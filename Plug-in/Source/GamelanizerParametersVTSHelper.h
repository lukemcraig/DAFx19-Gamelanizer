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

#pragma once
#include "../JuceLibraryCode/JuceHeader.h"
#include "GamelanizerConstants.h"
#include "GamelanizerParameters.h"

/** \addtogroup Parameters
 *  @{
 */

/**
 * \brief A helper class for parameter related methods that depend on the AudioProcessorValueTreeState
 */
class GamelanizerParametersVtsHelper
{
public:
    GamelanizerParametersVtsHelper(AudioProcessorValueTreeState&, GamelanizerParameters&);

    GamelanizerParametersVtsHelper(const GamelanizerParametersVtsHelper&) = delete;

    GamelanizerParametersVtsHelper& operator=(const GamelanizerParametersVtsHelper&) = delete;

    GamelanizerParametersVtsHelper(GamelanizerParametersVtsHelper&&) = delete;

    GamelanizerParametersVtsHelper& operator=(GamelanizerParametersVtsHelper&&) = delete;

    ~GamelanizerParametersVtsHelper() = default;

    //==============================================================================
    /**
     * \brief A struct for checking whether a returned parameter was changed in order to prevent unnecessary calculations.
     */
    struct ParameterAndWasChanged
    {
        float value;
        bool wasChanged;
    };

    //==============================================================================
    void resetSmoothers(double sampleRate);

    /**
     * \brief setCurrentAndTargetValue for all the smoothers. 
     * Called from the constructor and from setStateInformation
     */
    void instantlyUpdateSmoothers();

    void updateSmoothers();

    //==============================================================================
    float getGain(int level);

    float getPan(int level);

    float getMute(int level);

    //==============================================================================	
    float getDropNote(int level, int note);

    float getTaper(int level);

    ParameterAndWasChanged getPitch(int level, bool smoothed = true);

    ParameterAndWasChanged getLpFilterCutoff(int level);

    ParameterAndWasChanged getHpFilterCutoff(int level);

    //==============================================================================
private:
    //==============================================================================
    GamelanizerParameters& gamelanizerParameters;
    AudioProcessorValueTreeState& valueTreeState;
    //==============================================================================
    typedef SmoothedValue<float, ValueSmoothingTypes::Linear> SmoothFloat;

    std::array<float*, GamelanizerConstants::maxLevels + 1> gainParamRawPointers{};
    std::array<SmoothFloat, GamelanizerConstants::maxLevels + 1> gainsSmooth{};

    std::array<float*, GamelanizerConstants::maxLevels + 1> panParamRawPointers{};
    std::array<SmoothFloat, GamelanizerConstants::maxLevels + 1> pansSmooth{};

    std::array<float*, GamelanizerConstants::maxLevels + 1> muteParamRawPointers{};
    std::array<SmoothFloat, GamelanizerConstants::maxLevels + 1> mutesSmooth{};

    std::array<float*, GamelanizerConstants::maxLevels> taperParamRawPointers{};
    std::array<SmoothFloat, GamelanizerConstants::maxLevels> tapersSmooth{};

    std::array<float*, GamelanizerConstants::maxLevels> pitchParamRawPointers{};
    std::array<SmoothFloat, GamelanizerConstants::maxLevels> pitchesSmooth{};
    std::array<float, GamelanizerConstants::maxLevels> pitchesPrevious{};
    //==============================================================================

    /**
     * \brief How many samples before the filter coefficients are allowed to change
     */
    static constexpr int filterUpdateRateInSamples{32};

    std::array<float*, GamelanizerConstants::maxLevels> lpfParamRawPointers{};
    std::array<SmoothFloat, GamelanizerConstants::maxLevels> lpfSmooth{};
    std::array<float, GamelanizerConstants::maxLevels> lpfPrevious{};
    std::array<int, GamelanizerConstants::maxLevels> lpfSamplesSinceChange{};

    std::array<float*, GamelanizerConstants::maxLevels> hpfParamRawPointers{};
    std::array<SmoothFloat, GamelanizerConstants::maxLevels> hpfSmooth{};
    std::array<float, GamelanizerConstants::maxLevels> hpfPrevious{};
    std::array<int, GamelanizerConstants::maxLevels> hpfSamplesSinceChange{};
    //==============================================================================
    std::array<std::array<float*, 4>, GamelanizerConstants::maxLevels> dropParamRawPointers{};
    //==============================================================================
    JUCE_LEAK_DETECTOR(GamelanizerParametersVtsHelper)
};

/** @}*/
