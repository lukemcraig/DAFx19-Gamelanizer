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

#include "PvResampler.h"

PvResampler::PvResampler(const int analysisHopSize): inputQueue{
                                                         calculateMaxNeededSamples(analysisHopSize, 16.0, 16.0) + 1
                                                     },
                                                     analysisHopBuffer(analysisHopSize)
{
    interpolator.reset();
}

void PvResampler::pushSample(const float sampleValue)
{
    inputQueue.data[inputQueue.writePosition] = sampleValue;
    ++inputQueue.writePosition;
}

int PvResampler::calculateMaxNeededSamples(const int desiredNumOut,
                                           const double newPitchShiftFactor,
                                           const double oldPitchShiftFactor)
{
    const auto neededNormally = desiredNumOut * newPitchShiftFactor;
    // because the resampler doesn't get reset when the pitchShiftFactor parameter is changed, it might need these extra samples
    const auto neededForStatefulPosition = newPitchShiftFactor * 2 + oldPitchShiftFactor * 2;
    return static_cast<int>(ceil(neededNormally + neededForStatefulPosition));
}

void PvResampler::updatePitchShiftFactor(const double newPitchShiftFactor)
{
    const auto oldPitchShiftFactor = currentPitchShiftFactor;
    currentPitchShiftFactor = newPitchShiftFactor;
    maxNeedSamples = calculateMaxNeededSamples(static_cast<int>(analysisHopBuffer.size()),
                                               newPitchShiftFactor,
                                               oldPitchShiftFactor);
}

bool PvResampler::readyToResampleHop() const
{
    return inputQueue.writePosition > maxNeedSamples;
}

bool PvResampler::resampleHopToAnalysisHopBufferIfReady(const double pitchShiftFactor)
{
    jassert(pitchShiftFactor==currentPitchShiftFactor);
    if (!readyToResampleHop())
        return false;

    const auto numUsed = interpolator.process(pitchShiftFactor,
                                              inputQueue.data.data(),
                                              analysisHopBuffer.data(),
                                              static_cast<int>(analysisHopBuffer.size()));

    jassert(numUsed <= inputQueue.writePosition);
    inputQueue.popUsedSamples(numUsed);
    return true;
}

std::vector<float> PvResampler::getAnalysisHopBuffer() const
{
    return analysisHopBuffer;
}

void PvResampler::resetBetweenBeats()
{
    interpolator.reset();
}

void PvResampler::fullReset()
{
    inputQueue.reset();
    analysisHopBuffer.assign(analysisHopBuffer.size(), 0);
    resetBetweenBeats();
}

//==============================================================================
PvResampler::Queue::Queue(const int resamplerLength): data(resamplerLength)
{
}

void PvResampler::Queue::popUsedSamples(const int numUsed)
{
    const auto nUnconsumed = writePosition - numUsed;
    jassert(nUnconsumed >= 0);

    // copy the unconsumed samples to the front of the queue
    for (auto i = 0; i < nUnconsumed; ++i)
        data[i] = data[numUsed + i];

    writePosition -= numUsed;
}

void PvResampler::Queue::reset()
{
    writePosition = 0;
}
