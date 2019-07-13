/*
  ==============================================================================

	This file is part of Gamelanizer.
	Copyright (c) 2019 - Luke McDuffie Craig.

	Gamelanizer is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Gamelanizer is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Gamelanizer. If not, see <https://www.gnu.org/licenses/>.

  ==============================================================================
*/

#include "PhaseVocoder.h"
#include "ModuloSameSignAsDivisor.h"
#include "WindowingFunctions.h"
//==============================================================================
PhaseVocoder::PhaseVocoder(const int levelNumber,
                           const float effectiveTimeScaleFactor) : analysisFrames{levelNumber},
                                                                   effectiveTimeScaleFactor{effectiveTimeScaleFactor},
                                                                   resampler{analysisFrames.analysisHopSize}
{
    WindowingFunctions::fillWithNonsymmetricHannWindow(fft.window.data, fftSize);
}

void PhaseVocoder::initParams(const float initPitchShiftFactor)
{
    synthesisHopSize.reset();

    const auto pitchShiftFactorCentsLocal = 1200.0f * std::log2(initPitchShiftFactor);
    nextPitchShiftFactorCents.store(pitchShiftFactorCentsLocal);

    setParams(initPitchShiftFactor, pitchShiftFactorCentsLocal);
}

void PhaseVocoder::setParams(const float newPitchShiftFactor, const float newPitchShiftFactorCents)
{
    pitchShiftFactor = newPitchShiftFactor;

    pitchShiftFactorCents = newPitchShiftFactorCents;

    actualTimeScaleFactor = effectiveTimeScaleFactor * pitchShiftFactor;

    synthesisOverlapFactor = analysisFrames.analysisOverlapFactorActual / actualTimeScaleFactor;

    synthesisHopSize.exactValue = analysisFrames.analysisHopSize * actualTimeScaleFactor;

    resampler.updatePitchShiftFactor(newPitchShiftFactor);

    fft.window.amplitudeCompensationScale = static_cast<float>(synthesisHopSize.exactValue
        / FftStruct::FftWindow::squaredWindowSum);
}

void PhaseVocoder::loadNextParams()
{
    const auto nextPitchShiftFactorCentsLocal = nextPitchShiftFactorCents.load();
    if (nextPitchShiftFactorCentsLocal != pitchShiftFactorCents)
    {
        const auto nextPitchShiftFactorLocal = std::pow(2.0f, nextPitchShiftFactorCentsLocal / 1200.0f);
        setParams(nextPitchShiftFactorLocal, nextPitchShiftFactorCentsLocal);
    }
}

void PhaseVocoder::queueParams(const float newNextPitchShiftFactorCents)
{
    nextPitchShiftFactorCents.store(newNextPitchShiftFactorCents);
}

void PhaseVocoder::resetBetweenBeats()
{
    previousFramePhases.initialized = false;
    analysisFrames.reset();
    resampler.resetBetweenBeats();
    loadNextParams();
}

void PhaseVocoder::fullReset()
{
    resampler.fullReset();
    resetBetweenBeats();
}

//==============================================================================

void PhaseVocoder::pushResampledHopOnToAnalysisFrameBuffer()
{
    const auto newHop = resampler.getAnalysisHopBuffer();
    for (auto sampleValue : newHop)
    {
        analysisFrames.circularBuffer[analysisFrames.writePosition] = sampleValue;
        ++analysisFrames.writePosition;
        if (analysisFrames.writePosition == fftSize)
        {
            // wrap around
            analysisFrames.writePosition = 0;
            // flag that it's been filled once. (this will stay true until the PV is reset at a beat boundary)
            analysisFrames.initialized = true;
        }
        jassert(analysisFrames.writePosition<fftSize);
    }
}

void PhaseVocoder::copyAnalysisFrameToFftInOut()
{
    for (auto i = 0; i < fftSize; ++i)
    {
        const auto index = analysisFrames.writePosition - fftSize + i;
        const auto indexRewound = ModuloSameSignAsDivisor::mod(index, fftSize);
        jassert(indexRewound < fftSize && indexRewound >= 0);
        fft.inOut[i] = analysisFrames.circularBuffer[indexRewound];
    }
}

void PhaseVocoder::storePhasesInBuffer()
{
    // reinterpret_cast the fft buffer to complex
    auto* complexBins = reinterpret_cast<std::complex<float>*>(fft.inOut.data());

    for (auto k = 0; k < nComplexBins; ++k)
    {
        // calculate the phase of the current kth bin
        const auto currentPhase = complexBinPhase(complexBins[k]);
        // store current unaltered phase for next time
        previousFramePhases.unaltered[k] = currentPhase;
        // store current unaltered phase for next time
        previousFramePhases.scaled[k] = currentPhase;
    }
}

void PhaseVocoder::scaleAllFrequencyBinsAndStorePhaseBuffers()
{
    // reinterpret_cast the fft buffer to complex
    auto* complexBins = reinterpret_cast<std::complex<float>*>(fft.inOut.data());

    for (auto k = 0; k < nComplexBins; ++k)
    {
        // get phases
        const auto currentPhase = complexBinPhase(complexBins[k]);
        // previous frame's unaltered phases
        const auto oldPhase = previousFramePhases.unaltered[k];
        // store current unaltered phase for next time
        previousFramePhases.unaltered[k] = currentPhase;
        // calculate the magnitudes
        const auto currentMagnitudes = complexBinMag(complexBins[k]);
        // store the scaled complex bins back on inOut
        complexBins[k] = scaleFrequencyBin(k, currentMagnitudes, currentPhase, oldPhase);
    }
}

std::complex<float> PhaseVocoder::scaleFrequencyBin(const int k, const float mag, const float currentPhase,
                                                    const float oldPhase)
{
    const auto freqDeviation = calculateFrequencyDeviation(oldPhase, currentPhase, k,
                                                           analysisFrames.analysisOverlapFactorActual,
                                                           static_cast<float>(analysisFrames.analysisHopSize));
    const auto omega = (MathConstants<float>::twoPi * static_cast<float>(k)) / fftSize;
    const auto trueFreq = omega + freqDeviation;
    const auto trueBinIndex = trueFreq * (fftSize / MathConstants<float>::twoPi);
    // get the previous scaled phase from scaled phase buffer
    const auto previousScaledPhase = previousFramePhases.scaled[k];
    // calculate the current frame's scaled phase 
    auto scaledPhase = trueBinIndex * (MathConstants<float>::twoPi / static_cast<float>(synthesisOverlapFactor)) +
        previousScaledPhase;
    // wrap the new scaled phase 
    scaledPhase = wrapPhase(scaledPhase);
    //set the previous scaled phase buffer to the new scaled phase
    previousFramePhases.scaled[k] = scaledPhase;
    // combine the original magnitude with the scaled phase
    return std::polar(mag, scaledPhase);
}

void PhaseVocoder::scaleAnalysisFrame()
{
    copyAnalysisFrameToFftInOut();

    // window the FFT buffer
    FloatVectorOperations::multiply(fft.inOut.data(), fft.window.data.data(), fftSize);
    // RFFT the FFT buffer
    fft.instance.performRealOnlyForwardTransform(fft.inOut.data(), true);

    if (previousFramePhases.initialized)
    {
        // alter the phases for the time stretching
        scaleAllFrequencyBinsAndStorePhaseBuffers();
    }
    else
    {
        // this is the first frame we've processed of this beat
        storePhasesInBuffer();
        // both phase buffers are now initialized
        previousFramePhases.initialized = true;
    }
    // inverse RFFT the complex bins
    fft.instance.performRealOnlyInverseTransform(fft.inOut.data());
    // synthesis window
    FloatVectorOperations::multiply(fft.inOut.data(), fft.window.data.data(), fftSize);
    // amplitude scaling
    FloatVectorOperations::multiply(fft.inOut.data(), fft.window.amplitudeCompensationScale, fftSize);
}

int PhaseVocoder::processSample(const float sampleValue)
{
    resampler.pushSample(sampleValue);

    // if the number of samples on the inputQueue is at least the number needed to get the desired output length
    const auto newHopAvailable = resampler.resampleHopToAnalysisHopBufferIfReady(pitchShiftFactor);
    if (newHopAvailable)
    {
        pushResampledHopOnToAnalysisFrameBuffer();
        if (analysisFrames.initialized)
        {
            scaleAnalysisFrame();
            return synthesisHopSize.getNextInt();
        }
    }
    return 0;
}

//==============================================================================

float PhaseVocoder::complexBinPhase(const std::complex<float> complexBin)
{
    return std::arg(complexBin);
}

float PhaseVocoder::complexBinMag(const std::complex<float> complexBin)
{
    return std::abs(complexBin);
}

float PhaseVocoder::calculateFrequencyDeviation(const float oldPhase, const float currentPhase,
                                                const int k, const float analysisOverlapFactor,
                                                const float analysisHopSize)
{
    const auto frequencyContribution = MathConstants<float>::twoPi * static_cast<float>(k)
        / analysisOverlapFactor;
    const auto numerator = currentPhase - oldPhase - frequencyContribution;
    const auto wrappedNumerator = wrapPhase(numerator);
    const auto frequencyDeviation = wrappedNumerator / analysisHopSize;
    return frequencyDeviation;
}

float PhaseVocoder::wrapPhase(const float phaseIn)
{
    constexpr auto pi = MathConstants<float>::pi;
    constexpr auto twoPi = MathConstants<float>::twoPi;
    return ModuloSameSignAsDivisor::mod(phaseIn + pi, -twoPi) + pi;
}

//==============================================================================

PhaseVocoder::AnalysisFrames::AnalysisFrames(const int level): analysisOverlapFactor{jmax(4.0, std::pow(2, 4 - level))},
                                                               analysisHopSize{
                                                                   static_cast<int>(std::round(
                                                                       fftSize / analysisOverlapFactor))
                                                               },
                                                               analysisOverlapFactorActual{
                                                                   static_cast<float>(fftSize) / static_cast<float>(
                                                                       analysisHopSize)
                                                               }
{
    // it should be ok if this is false, but it will be nice to know if that ever is the case.
    jassert(analysisOverlapFactorActual == analysisOverlapFactor);
}

//==============================================================================
