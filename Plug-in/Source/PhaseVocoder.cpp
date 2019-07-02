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
PhaseVocoder::PhaseVocoder()
{
    WindowingFunctions::fillWithNonsymmetricHannWindow(fft.window.data, fftSize);

    resampler.interpolator.reset();
}

PhaseVocoder::~PhaseVocoder()
{
}

void PhaseVocoder::initParams(const float initPitchShiftFactor, const float initEffectiveTimeScaleFactor)
{
    effectiveTimeScaleFactor = initEffectiveTimeScaleFactor;

    const auto pitchShiftFactorCentsLocal = 1200.0f * std::log2(initPitchShiftFactor);
    nextPitchShiftFactorCents.store(pitchShiftFactorCentsLocal);

    setParams(initPitchShiftFactor, pitchShiftFactorCentsLocal);
}

void PhaseVocoder::setParams(const float newPitchShiftFactor, const float newPitchShiftFactorCents)
{
    const auto oldPitchShiftFactor = pitchShiftFactor;
    pitchShiftFactor = newPitchShiftFactor;

    pitchShiftFactorCents = newPitchShiftFactorCents;

    actualTimeScaleFactor = effectiveTimeScaleFactor * pitchShiftFactor;
    synthesisOverlapFactor = AnalysisFrames::analysisOverlapFactor / actualTimeScaleFactor;

    synthesisHopSize = AnalysisFrames::analysisHopSize * actualTimeScaleFactor;

    resampler.maxNeedSamples = calculateMaximumNeededNumSamples(AnalysisFrames::analysisHopSize, oldPitchShiftFactor);

    fft.window.amplitudeCompensationScale = static_cast<float>(synthesisHopSize / FftStruct::FftWindow::squaredWindowSum);
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

int PhaseVocoder::calculateMaximumNeededNumSamples(const int desiredNumOut, const double oldPitchShiftFactor) const
{
    const auto neededNormally = desiredNumOut * pitchShiftFactor;
    // because the resampler doesn't get reset when the pitchShiftFactor parameter is changed, it might need these extra samples
    const auto neededForStatefulPosition = pitchShiftFactor * 2 + oldPitchShiftFactor * 2;
    return static_cast<int>(ceil(neededNormally + neededForStatefulPosition));
}


void PhaseVocoder::resampleHop()
{
    const auto numUsed = resampler.interpolator.process(pitchShiftFactor, resampler.queue.data.data(),
                                                        resampler.resamplerAnalysisHopBuffer,
                                                        AnalysisFrames::analysisHopSize);
    jassert(numUsed <= resampler.queue.writePosition);
    popUsedSamples(numUsed);
}

void PhaseVocoder::popUsedSamples(const int numUsed)
{
    const auto nUnconsumed = resampler.queue.writePosition - numUsed;
    jassert(nUnconsumed >= 0);

    // copy the unconsumed samples to the front of the queue
    for (auto i = 0; i < nUnconsumed; ++i)
        resampler.queue.data[i] = resampler.queue.data[numUsed + i];

    resampler.queue.writePosition -= numUsed;
}

void PhaseVocoder::pushResampledHopOnToAnalysisFrameBuffer()
{
    for (auto sampleValue : resampler.resamplerAnalysisHopBuffer)
    {
        analysisFrames.circularBuffer[analysisFrames.writePosition] = sampleValue;
        analysisFrames.writePosition += 1;
        if (analysisFrames.writePosition == fftSize)
        {
            // wrap around
            analysisFrames.writePosition = 0;
            // flag that its been filled once. (this will stay true until the PV is reset at a beat boundary)
            analysisFrames.initialized = true;
        }
    }
}

void PhaseVocoder::copyAnalysisFrameToFftInOut()
{
    for (auto i = 0; i < fftSize; ++i)
    {
        const auto index = analysisFrames.writePosition - fftSize + i;
        const auto fftQueueIndexRewound = ModuloSameSignAsDivisor::modInt(index, fftSize);
        jassert(fftQueueIndexRewound < fftSize && fftQueueIndexRewound >= 0);
        fft.inOut[i] = analysisFrames.circularBuffer[fftQueueIndexRewound];
    }
}

void PhaseVocoder::storePhasesInBuffer()
{
    // this is the first frame we've processed of this beat, so just store the phases without scaling them

    // reinterpret_cast the fft buffer to complex
    auto* complexBins = reinterpret_cast<std::complex<float>*>(fft.inOut);

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
    auto* complexBins = reinterpret_cast<std::complex<float>*>(fft.inOut);

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
    const auto freqDeviation = calculateFrequencyDeviation(oldPhase, currentPhase, k);
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
    // window fft buffer
    FloatVectorOperations::multiply(fft.inOut, fft.window.data.data(), fftSize);
    // rfft the fft buffer
    fft.instance.performRealOnlyForwardTransform(fft.inOut, true);

    if (previousFramePhases.isInitialized)
    {
        // alter the phases for the time stretching
        scaleAllFrequencyBinsAndStorePhaseBuffers();
    }
    else
    {
        // this is the first frame we've processed of this beat
        storePhasesInBuffer();
        // both phase buffers are now initialized
        previousFramePhases.isInitialized = true;
    }
    // inverse rfft the complex bins
    fft.instance.performRealOnlyInverseTransform(fft.inOut);
    // synthesis window
    FloatVectorOperations::multiply(fft.inOut, fft.window.data.data(), fftSize);
    // amplitude scaling
    FloatVectorOperations::multiply(fft.inOut, fft.window.amplitudeCompensationScale, fftSize);
}

void PhaseVocoder::reset()
{
    previousFramePhases.isInitialized = false;
    analysisFrames.initialized = false;
    analysisFrames.writePosition = 0;
    resampler.interpolator.reset();
}

void PhaseVocoder::fullReset()
{
    // TODO why not in reset?
    loadNextParams();
    //TODO why not in reset?
    resampler.queue.writePosition = 0;
    reset();
}

int PhaseVocoder::processSample(const float sampleValue, const bool skipProcessing)
{
    jassert(static_cast<size_t>(resampler.queue.writePosition) < resampler.queue.data.size());
    resampler.queue.data[resampler.queue.writePosition] = sampleValue;
    resampler.queue.writePosition += 1;

    // if the number of samples on the queue is at least the number needed to get the desired output length
    if (resampler.queue.writePosition > resampler.maxNeedSamples)
    {
        resampleHop();
        pushResampledHopOnToAnalysisFrameBuffer();
        if (analysisFrames.initialized)
        {
            if (!skipProcessing)
            {
                scaleAnalysisFrame();
            }
            // TODO track subsample position ?
            const auto stepSize = static_cast<int>(synthesisHopSize);
            return stepSize;
        }
    }
    return 0;
}

float PhaseVocoder::complexBinPhase(const std::complex<float>& complexBin)
{
    return std::arg(complexBin);
}

float PhaseVocoder::complexBinMag(const std::complex<float>& complexBin)
{
    return std::abs(complexBin);
}

float PhaseVocoder::calculateFrequencyDeviation(const float oldPhase, const float currentPhase,
                                                const int k)
{
    const auto frequencyContribution = MathConstants<float>::twoPi * static_cast<float>(k)
        / static_cast<float>(AnalysisFrames::analysisOverlapFactor);
    const auto numerator = currentPhase - oldPhase - frequencyContribution;
    const auto wrappedNumerator = wrapPhase(numerator);
    const auto frequencyDeviation = wrappedNumerator / static_cast<float>(AnalysisFrames::analysisHopSize);
    return frequencyDeviation;
}

float PhaseVocoder::wrapPhase(const float phaseIn)
{
    constexpr auto pi = MathConstants<float>::pi;
    constexpr auto twoPi = MathConstants<float>::twoPi;
    return ModuloSameSignAsDivisor::modFloat(phaseIn + pi, -twoPi) + pi;
}
