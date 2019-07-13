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

#include "GamelanizerParameters.h"
//==============================================================================
GamelanizerParameters::GamelanizerParameters()
{
    // initialize parameterId arrays
    for (auto i = 0; i < GamelanizerConstants::maxLevels + 1; ++i)
    {
        gainIds[i] = "gain" + String(i);
        muteIds[i] = "mute" + String(i);
        panIds[i] = "pan" + String(i);
    }
    for (auto i = 0; i < GamelanizerConstants::maxLevels; ++i)
    {
        pitchIds[i] = "pitch" + String(i);
        taperIds[i] = "taper" + String(i);
        lpfIds[i] = "lpf" + String(i);
        hpfIds[i] = "hpf" + String(i);
        for (auto j = 0; j < 4; ++j)
            dropIds[i][j] = "drop" + String(i) + "_" + String(j);
    }
}

//==============================================================================
String GamelanizerParameters::getGainId(const int level) { return gainIds[level]; }

String GamelanizerParameters::getMuteId(const int level) { return muteIds[level]; }

String GamelanizerParameters::getPanId(const int level) { return panIds[level]; }

String GamelanizerParameters::getPitchId(const int level) { return pitchIds[level]; }

String GamelanizerParameters::getTaperId(const int level) { return taperIds[level]; }

String GamelanizerParameters::getHpfId(const int level) { return hpfIds[level]; }

String GamelanizerParameters::getLpfId(const int level) { return lpfIds[level]; }

String GamelanizerParameters::getDropId(const int level, const int note)
{
    return dropIds[level][note];
}

//==============================================================================
AudioProcessorValueTreeState::ParameterLayout GamelanizerParameters::createParameterLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    addGainsToLayout(params);
    addMutesToLayout(params);
    addPansToLayout(params);
    addTapersToLayout(params);
    addFiltersToLayout(params);
    addPitchesToLayout(params);
    addDropsToLayout(params);

    return {params.begin(), params.end()};
}

//==============================================================================
void GamelanizerParameters::addGainsToLayout(std::vector<std::unique_ptr<RangedAudioParameter>>& params)
{
    std::function<String(float value, int maximumStringLength)> gainToDbString = [
        ](float value, int /*maximumStringLength*/)
    {
        return Decibels::toString(Decibels::gainToDecibels(value), 1, -96.0f, true);
    };

    std::function<float(const String& text)> dBStringToGain = [
        ](const String& text)
    {
        return Decibels::decibelsToGain(text.getFloatValue());
    };

    std::array<float, GamelanizerConstants::maxLevels + 1> gainDefaults{
        Decibels::decibelsToGain(0.0f),
        Decibels::decibelsToGain(-2.0f),
        Decibels::decibelsToGain(-3.0f),
        Decibels::decibelsToGain(-10.0f),
        Decibels::decibelsToGain(-12.0f)
    };
    for (auto i = 0; i < GamelanizerConstants::maxLevels + 1; ++i)
    {
        params.push_back(std::make_unique<AudioParameterFloat>(
                getGainId(i),
                "Gain " + String(i),
                NormalisableRange<float>{0.0f, 1.0f, 0.0f, 0.5f},
                gainDefaults[i],
                "",
                AudioProcessorParameter::genericParameter,
                gainToDbString,
                dBStringToGain)
        );
    }
}

void GamelanizerParameters::addMutesToLayout(std::vector<std::unique_ptr<RangedAudioParameter>>& params)
{
    for (auto i = 0; i < GamelanizerConstants::maxLevels + 1; ++i)
    {
        params.push_back(std::make_unique<AudioParameterBool>(
                getMuteId(i),
                "Mute " + String(i),
                i >= 3) // default value
        );
    }
}

void GamelanizerParameters::addPansToLayout(std::vector<std::unique_ptr<RangedAudioParameter>>& params)
{
    std::function<String(float value, int maximumStringLength)> panValueToString = [
        ](float value, int /*maximumStringLength*/)
    {
        if (value == 0.0f)
            return String("C");
        if (value < 0.0f)
            return "L" + String(value * -1.0f, 1) + "%";
        return "R" + String(value, 1) + "%";
    };

    std::array<float, GamelanizerConstants::maxLevels + 1> panDefaults{
        0, -100.0f, 100.0f, -50.0f, 50.0f
    };

    for (auto i = 0; i < GamelanizerConstants::maxLevels + 1; ++i)
    {
        params.push_back(std::make_unique<AudioParameterFloat>(
                getPanId(i),
                "Pan " + String(i),
                NormalisableRange<float>{-100.0f, 100.0f},
                panDefaults[i],
                String(),
                AudioProcessorParameter::genericParameter,
                panValueToString,
                nullptr)
        );
    }
}

void GamelanizerParameters::addTapersToLayout(std::vector<std::unique_ptr<RangedAudioParameter>>& params)
{
    std::function<String(float value, int maximumStringLength)> taperValueToString = [
        ](float value, int /*maximumStringLength*/)
    {
        return String(value * 100.0f, 1) + "%";
    };
    std::array<float, GamelanizerConstants::maxLevels> taperDefaults{
        0.0f, 0.0f, 0.1f, 0.2f
    };
    for (auto i = 0; i < GamelanizerConstants::maxLevels; ++i)
    {
        params.push_back(std::make_unique<AudioParameterFloat>(
                getTaperId(i),
                "Taper " + String(i + 1),
                NormalisableRange<float>(0.0f, 1.0f),
                taperDefaults[i], // default value
                String(),
                AudioProcessorParameter::genericParameter,
                taperValueToString,
                nullptr)
        );
    }
}

void GamelanizerParameters::addFiltersToLayout(std::vector<std::unique_ptr<RangedAudioParameter>>& params)
{
    std::function<String(float value, int maximumStringLength)> frequencyValueToString = [
        ](float value, int /*maximumStringLength*/)
    {
        if (value >= 1000.0f)
        {
            return String(value / 1000.0f, 1) + " kHz";
        }
        return String(value, 1) + " Hz";
    };
    for (auto i = 0; i < GamelanizerConstants::maxLevels; ++i)
    {
        params.push_back(std::make_unique<AudioParameterFloat>(
                getHpfId(i),
                "HighPass " + String(i + 1),
                NormalisableRange<float>(10.0f, 20000.0f, 0.0f, 0.25f),
                10.0f, // default value
                String(),
                AudioProcessorParameter::genericParameter,
                frequencyValueToString,
                nullptr)
        );
    }
    std::array<float, GamelanizerConstants::maxLevels> lpfDefaults{
        10000.0f, 9000.0f, 6000.0f, 5000.0f
    };
    for (auto i = 0; i < GamelanizerConstants::maxLevels; ++i)
    {
        params.push_back(std::make_unique<AudioParameterFloat>(
                getLpfId(i),
                "LowPass " + String(i + 1),
                NormalisableRange<float>(10.0f, 20000.0f, 0.0f, 0.25f),
                lpfDefaults[i], // default value
                String(),
                AudioProcessorParameter::genericParameter,
                frequencyValueToString,
                nullptr)
        );
    }
}

void GamelanizerParameters::addPitchesToLayout(std::vector<std::unique_ptr<RangedAudioParameter>>& params)
{
    std::function<String(float value, int maximumStringLength)> pitchShiftValueToString = [
        ](float value, int /*maximumStringLength*/)
    {
        return String(value, 0) + " cents";
    };
    for (auto i = 0; i < GamelanizerConstants::maxLevels; ++i)
    {
        params.push_back(std::make_unique<AudioParameterFloat>(
                getPitchId(i),
                "Pitch " + String(i + 1),
                NormalisableRange<float>{
                    GamelanizerConstants::minPitchShiftCents,
                    GamelanizerConstants::maxPitchShiftCents,
                    1.0f
                },
                1200.0f * (i + 1.0f), // default value  
                String(),
                AudioProcessorParameter::genericParameter,
                pitchShiftValueToString,
                nullptr)
        );
    }
}

void GamelanizerParameters::addDropsToLayout(std::vector<std::unique_ptr<RangedAudioParameter>>& params)
{
    std::array<std::array<float, 4>, GamelanizerConstants::maxLevels> dropNoteDefaults{
        {
            {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}
        }
    };
    for (auto i = 0; i < GamelanizerConstants::maxLevels; ++i)
    {
        for (auto j = 0; j < 4; ++j)
        {
            params.push_back(std::make_unique<AudioParameterBool>(
                    getDropId(i, j),
                    "Drop " + String(i + 1) + " " + String(j + 1),
                    dropNoteDefaults[i][j]) // default value
            );
        }
    }
}

//==============================================================================
