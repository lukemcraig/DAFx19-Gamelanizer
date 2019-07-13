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

#include "GamelanizerParametersVTSHelper.h"

GamelanizerParametersVtsHelper::GamelanizerParametersVtsHelper(AudioProcessorValueTreeState& vts,
                                                               GamelanizerParameters& gp) :
    gamelanizerParameters(gp), valueTreeState(vts)
{
    for (auto i = 0; i < GamelanizerConstants::maxLevels + 1; ++i)
    {
        muteParamRawPointers[i] = valueTreeState.getRawParameterValue(gamelanizerParameters.getMuteId(i));
        gainParamRawPointers[i] = valueTreeState.getRawParameterValue(gamelanizerParameters.getGainId(i));
        panParamRawPointers[i] = valueTreeState.getRawParameterValue(gamelanizerParameters.getPanId(i));
    }

    for (auto i = 0; i < GamelanizerConstants::maxLevels; ++i)
    {
        taperParamRawPointers[i] = valueTreeState.getRawParameterValue(gamelanizerParameters.getTaperId(i));
        pitchParamRawPointers[i] = valueTreeState.getRawParameterValue(gamelanizerParameters.getPitchId(i));
        lpfParamRawPointers[i] = valueTreeState.getRawParameterValue(gamelanizerParameters.getLpfId(i));
        hpfParamRawPointers[i] = valueTreeState.getRawParameterValue(gamelanizerParameters.getHpfId(i));

        for (auto j = 0; j < 4; j++)
            dropParamRawPointers[i][j] = valueTreeState.getRawParameterValue(gamelanizerParameters.getDropId(i, j));
    }
    instantlyUpdateSmoothers();
}

//==============================================================================
void GamelanizerParametersVtsHelper::resetSmoothers(const double sampleRate)
{
    for (auto& smoother : gainsSmooth)
        smoother.reset(sampleRate, 0.1);

    for (auto& smoother : pansSmooth)
        smoother.reset(sampleRate, 0.1);

    for (auto& smoother : mutesSmooth)
        smoother.reset(sampleRate, 0.1);

    for (auto& smoother : tapersSmooth)
        smoother.reset(sampleRate, 0.1);

    for (auto& smoother : pitchesSmooth)
        smoother.reset(sampleRate, 0.1);

    for (auto& smoother : lpfSmooth)
        smoother.reset(sampleRate, 0.01);

    for (auto& smoother : hpfSmooth)
        smoother.reset(sampleRate, 0.01);
}

void GamelanizerParametersVtsHelper::instantlyUpdateSmoothers()
{
    for (auto i = 0; i < GamelanizerConstants::maxLevels + 1; ++i)
        gainsSmooth[i].setCurrentAndTargetValue(*gainParamRawPointers[i]);

    for (auto i = 0; i < GamelanizerConstants::maxLevels + 1; ++i)
        pansSmooth[i].setCurrentAndTargetValue(*panParamRawPointers[i]);

    for (auto i = 0; i < GamelanizerConstants::maxLevels + 1; ++i)
        mutesSmooth[i].setCurrentAndTargetValue(*muteParamRawPointers[i]);

    for (auto i = 0; i < GamelanizerConstants::maxLevels; ++i)
        tapersSmooth[i].setCurrentAndTargetValue(*taperParamRawPointers[i]);

    for (auto i = 0; i < GamelanizerConstants::maxLevels; ++i)
        pitchesSmooth[i].setCurrentAndTargetValue(*pitchParamRawPointers[i]);

    for (auto i = 0; i < GamelanizerConstants::maxLevels; ++i)
        lpfSmooth[i].setCurrentAndTargetValue(*lpfParamRawPointers[i]);

    for (auto i = 0; i < GamelanizerConstants::maxLevels; ++i)
        hpfSmooth[i].setCurrentAndTargetValue(*hpfParamRawPointers[i]);
}

void GamelanizerParametersVtsHelper::updateSmoothers()
{
    for (auto i = 0; i < GamelanizerConstants::maxLevels + 1; ++i)
        gainsSmooth[i].setTargetValue(*gainParamRawPointers[i]);

    for (auto i = 0; i < GamelanizerConstants::maxLevels + 1; ++i)
        pansSmooth[i].setTargetValue(*panParamRawPointers[i]);

    for (auto i = 0; i < GamelanizerConstants::maxLevels + 1; ++i)
        mutesSmooth[i].setTargetValue(*muteParamRawPointers[i]);

    for (auto i = 0; i < GamelanizerConstants::maxLevels; ++i)
        tapersSmooth[i].setTargetValue(*taperParamRawPointers[i]);

    for (auto i = 0; i < GamelanizerConstants::maxLevels; ++i)
        pitchesSmooth[i].setTargetValue(*pitchParamRawPointers[i]);

    for (auto i = 0; i < GamelanizerConstants::maxLevels; ++i)
        lpfSmooth[i].setTargetValue(*lpfParamRawPointers[i]);

    for (auto i = 0; i < GamelanizerConstants::maxLevels; ++i)
        hpfSmooth[i].setTargetValue(*hpfParamRawPointers[i]);
}

//==============================================================================
float GamelanizerParametersVtsHelper::getGain(const int level) { return gainsSmooth[level].getNextValue(); }

float GamelanizerParametersVtsHelper::getPan(const int level) { return pansSmooth[level].getNextValue(); }

float GamelanizerParametersVtsHelper::getMute(const int level) { return mutesSmooth[level].getNextValue(); }

float GamelanizerParametersVtsHelper::getTaper(const int level) { return tapersSmooth[level].getNextValue(); }

GamelanizerParametersVtsHelper::ParameterAndWasChanged GamelanizerParametersVtsHelper::getPitch(
    const int level, const bool smoothed)
{
    if (!smoothed)
    {
        const auto target = pitchesSmooth[level].getTargetValue();
        pitchesSmooth[level].setCurrentAndTargetValue(target);
        pitchesPrevious[level] = target;
        return {target, true};
    }
    const auto previousPitch = pitchesPrevious[level];
    const auto nextPitch = pitchesSmooth[level].getNextValue();
    pitchesPrevious[level] = nextPitch;
    return {nextPitch, nextPitch != previousPitch};
}

GamelanizerParametersVtsHelper::ParameterAndWasChanged GamelanizerParametersVtsHelper::getLpFilterCutoff(
    const int level)
{
    const auto previousCutoff = lpfPrevious[level];
    const auto nextCutoff = lpfSmooth[level].getNextValue();

    // this allows the smoothing to happen internally every sample, but not update the coefficients every sample
    if (lpfSamplesSinceChange[level] < filterUpdateRateInSamples)
    {
        ++lpfSamplesSinceChange[level];
        return {previousCutoff, false};
    }
    lpfSamplesSinceChange[level] = 0;

    lpfPrevious[level] = nextCutoff;
    return {nextCutoff, previousCutoff != nextCutoff};
}

GamelanizerParametersVtsHelper::ParameterAndWasChanged GamelanizerParametersVtsHelper::getHpFilterCutoff(
    const int level)
{
    const auto previousCutoff = hpfPrevious[level];
    const auto nextCutoff = hpfSmooth[level].getNextValue();

    // this allows the smoothing to happen internally every sample, but not update the coefficients every sample
    if (hpfSamplesSinceChange[level] < filterUpdateRateInSamples)
    {
        ++hpfSamplesSinceChange[level];
        return {previousCutoff, false};
    }
    hpfSamplesSinceChange[level] = 0;

    hpfPrevious[level] = nextCutoff;
    return {nextCutoff, previousCutoff != nextCutoff};
}

float GamelanizerParametersVtsHelper::getDropNote(const int level, const int note)
{
    return *dropParamRawPointers[level][note];
}

//==============================================================================
