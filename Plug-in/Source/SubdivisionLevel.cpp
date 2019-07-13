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

#include "SubdivisionLevel.h"

SubdivisionLevel::SubdivisionLevel(const int levelNumber, BeatSampleInfo& bsi, GamelanizerParametersVtsHelper& gpvh,
                                   SubdivisionLevelsOutputBuffer& lob, double& hostSampleRate):
    levelNumber(levelNumber),
    powerOfTwo{
        static_cast<int>(std::pow(2, levelNumber + 1))
    },
    numberOfNotesToJumpOver{calculateNumberOfNotesToJumpOver(levelNumber)},
    pv(levelNumber, 1.0f / static_cast<float>(powerOfTwo)),
    beatSampleInfo(bsi),
    gamelanizerParametersVtsHelper(gpvh),
    levelsOutputBuffer(lob),
    hostSampleRate{hostSampleRate}
{
    hpFilter.parameters->type = dsp::StateVariableFilter::Parameters<float>::Type::highPass;
}

int SubdivisionLevel::calculateNumberOfNotesToJumpOver(const int levelNumber)
{
    // we have to add 2 because we're starting from 0. If we counted the base level as 0, we would only add 1
    const auto numberNotesInThisLvlEqualToTwoInTheOriginal = static_cast<int>(std::pow(2, levelNumber + 2));
    // when jumps happen the lead write head is at the end of the 2nd note, so we don't have to jump past those 2 notes
    return numberNotesInThisLvlEqualToTwoInTheOriginal - 2;
}

void SubdivisionLevel::moveWriteHeadOneHop(const int hop)
{
    accumulatedSamples += hop;
    writePosition += hop;

    wrapLevelWritePosition();
}

void SubdivisionLevel::processSample(const float sampleValue)
{
    const auto hop = pv.processSample(sampleValue);
    // hop is greater than 0 when the phase vocoder has new data for us to OLA
    if (hop > 0)
    {
        addSamplesToLevelsOutputBuffer(pv.getFftInOutReadPointer(), PhaseVocoder::getFftSize());
        pv.loadNextParams();

        moveWriteHeadOneHop(hop);
    }
}

void SubdivisionLevel::processFinalHop()
{
    auto hop = 0;
    // hop returns greater than 0 when the phase vocoder has new data for us to OLA
    while (hop == 0)
    {
        hop = pv.processSample(0);
    }

    addSamplesToLevelsOutputBuffer(pv.getFftInOutReadPointer(), PhaseVocoder::getFftSize());
    pv.loadNextParams();

    moveWriteHeadOneHop(hop);
}

bool SubdivisionLevel::shouldDropThisNote(const int copyNumber) const
{
    // drop the 1st note
    const auto onFirstNote = !beatSampleInfo.isBeatB() && (copyNumber % 2 == 0);
    if (onFirstNote && (gamelanizerParametersVtsHelper.getDropNote(levelNumber, 0) != 0))
        return true;

    // drop the 2nd note
    const auto onSecondNote = beatSampleInfo.isBeatB() && (copyNumber % 2 == 0);
    if (onSecondNote && (gamelanizerParametersVtsHelper.getDropNote(levelNumber, 1) != 0))
        return true;

    // drop the 3rd note
    const auto onThirdNote = !beatSampleInfo.isBeatB() && (copyNumber % 2 == 1);
    if (onThirdNote && (gamelanizerParametersVtsHelper.getDropNote(levelNumber, 2) != 0))
        return true;

    // drop the 4th note
    const auto onFourthNote = beatSampleInfo.isBeatB() && (copyNumber % 2 == 1);
    return onFourthNote && (gamelanizerParametersVtsHelper.getDropNote(levelNumber, 3) != 0);
}

void SubdivisionLevel::addSamplesToLevelsOutputBuffer(const float* samples, const int nSamples) const
{
    const auto noteLength = static_cast<double>(beatSampleInfo.getBeatSampleLength()) / powerOfTwo;
    const auto twoNoteLengths = noteLength * 2;

    const auto numCopies = powerOfTwo;

    const auto levelBufLength = levelsOutputBuffer.data.getNumSamples();
    for (auto i = 0; i < numCopies; ++i)
    {
        if (shouldDropThisNote(i))
            continue;

        // multiple write heads for each copy of the scaled beat, depending on the subdivision lvl
        //todo subsample
        const auto writeHeadUnwrapped = writePosition + static_cast<int>(twoNoteLengths * i);
        jassert(writeHeadUnwrapped >= 0);
        jassert(levelBufLength > 0);
        const auto writeHead = writeHeadUnwrapped % levelBufLength;
        jassert(writeHead >= 0);
        jassert(writeHead < levelBufLength);

        // if the samples array doesn't need to wrap around the output buffer
        if (writeHead + nSamples < levelBufLength)
        {
            levelsOutputBuffer.data.addFrom(levelNumber, writeHead, samples, nSamples);
        }
            // the samples array does need to wrap around the output buffer
        else
        {
            // number of samples on the left part of the samples array
            const auto nSamplesLeft = levelBufLength - writeHead;
            jassert(nSamplesLeft > 0);
            jassert(nSamplesLeft < nSamples);
            // make sure we aren't writing past the read head
            if (writeHead < levelsOutputBuffer.readPosition)
            {
                jassert(writeHead + nSamplesLeft < levelsOutputBuffer.readPosition);
            }
            levelsOutputBuffer.data.addFrom(levelNumber, writeHead, samples, nSamplesLeft);
            // number of samples on the right part of the samples array
            const auto nSamplesRight = nSamples - nSamplesLeft;
            jassert(nSamplesRight > 0);
            jassert(nSamplesRight < nSamples);
            // make sure we aren't writing past the read head
            jassert(nSamplesLeft + nSamplesRight < levelsOutputBuffer.readPosition);
            levelsOutputBuffer.data.addFrom(levelNumber, 0, samples + nSamplesLeft, nSamplesRight);
        }
    }
}

void SubdivisionLevel::wrapLevelWritePosition()
{
    const auto levelBufLength = levelsOutputBuffer.data.getNumSamples();
    if (writePosition >= levelBufLength)
    {
        writePosition -= levelBufLength;
    }
}

void SubdivisionLevel::moveWritePosOnBeatB()
{
    // TODO subsample        
    const auto samplesToJump = static_cast<int>(std::round(noteLengthInSamplesFractional * numberOfNotesToJumpOver));
    writePosition += samplesToJump;
    wrapLevelWritePosition();
}

void SubdivisionLevel::fastForwardWriteHeadsToNextBeat()
{
    //TODO subsample
    const auto missing = noteLengthInSamples - accumulatedSamples;
    writePosition += missing;
    wrapLevelWritePosition();
    accumulatedSamples = 0;
}

void SubdivisionLevel::fullReset()
{
    pv.fullReset();
    accumulatedSamples = 0;
}

//==============================================================================

void SubdivisionLevel::prepareFilters(const int samplesPerBlock)
{
    lpFilter.prepare({
        hostSampleRate, static_cast<uint32>(samplesPerBlock), static_cast<uint32>(1)
    });

    hpFilter.prepare({
        hostSampleRate, static_cast<uint32>(samplesPerBlock), static_cast<uint32>(1)
    });
}

void SubdivisionLevel::resetFilters()
{
    lpFilter.reset();
    hpFilter.reset();
}

void SubdivisionLevel::snapFiltersToZero()
{
    lpFilter.snapToZero();
    hpFilter.snapToZero();
}

void SubdivisionLevel::preparePhaseVocoder()
{
    const auto pitchParam = gamelanizerParametersVtsHelper.getPitch(levelNumber, false);
    const auto pitchShiftFactor = std::pow(2.0, pitchParam.value / 1200.0);
    pv.initParams(static_cast<float>(pitchShiftFactor));
}

void SubdivisionLevel::queuePhaseVocoderNextParams()
{
    const auto newPitch = gamelanizerParametersVtsHelper.getPitch(levelNumber, true);
    if (newPitch.wasChanged)
    {
        pv.queueParams(newPitch.value);
    }
}

// ReSharper disable once CppMemberFunctionMayBeConst
void SubdivisionLevel::updateFilters()
{
    const auto nyquist = static_cast<float>(hostSampleRate * 0.5f);

    const auto lpFilterCutoff = gamelanizerParametersVtsHelper.getLpFilterCutoff(levelNumber);
    if (lpFilterCutoff.wasChanged)
        lpFilter.parameters->setCutOffFrequency(hostSampleRate, jmin(nyquist, lpFilterCutoff.value));

    const auto hpFilterCutoff = gamelanizerParametersVtsHelper.getHpFilterCutoff(levelNumber);
    if (hpFilterCutoff.wasChanged)
        hpFilter.parameters->setCutOffFrequency(hostSampleRate, jmin(nyquist, hpFilterCutoff.value));
}
