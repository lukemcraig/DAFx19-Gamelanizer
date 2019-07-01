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

/**
 * \brief A helper class for the storing the ID Strings of the parameters and building the parameter layout.
 * AudioProcessorValueTreeState and PluginEditor rely on this.
 */
class GamelanizerParameters
{
public:
    GamelanizerParameters();

    ~GamelanizerParameters();

    /**
     * \param level The subdivision level
     */
    String getGainId(int level);
    /**
     * \param level The subdivision level
     */
    String getMuteId(int level);
    /**
     * \param level The subdivision level
     */
    String getPanId(int level);
    /**
     * \param level The subdivision level
     */
    String getPitchId(int level);
    /**
     * \param level The subdivision level
     */
    String getTaperId(int level);
    /**
     * \param level The subdivision level
     */
    String getHpfId(int level);
    /**
     * \param level The subdivision level
     */
    String getLpfId(int level);
    /**
     * \param level The subdivision level
     * \param note The note number (0-3)
     */
    String getDropId(int level, int note);
    //==============================================================================
    /**
     * \brief Called by the AudioProcessorValueTreeState's constructor.
     * \return 
     */
    AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    //==============================================================================
private:
    //==============================================================================
    std::array<String, GamelanizerConstants::maxLevels + 1> gainIds;
    std::array<String, GamelanizerConstants::maxLevels + 1> muteIds;
    std::array<String, GamelanizerConstants::maxLevels + 1> panIds;

    std::array<String, GamelanizerConstants::maxLevels> pitchIds;
    std::array<String, GamelanizerConstants::maxLevels> taperIds;
    std::array<String, GamelanizerConstants::maxLevels> hpfIds;
    std::array<String, GamelanizerConstants::maxLevels> lpfIds;
    std::array<std::array<String, 4>, GamelanizerConstants::maxLevels> dropIds;
    //==============================================================================
    void addGainsToLayout(std::vector<std::unique_ptr<RangedAudioParameter>>& params);
    void addMutesToLayout(std::vector<std::unique_ptr<RangedAudioParameter>>& params);
    void addPansToLayout(std::vector<std::unique_ptr<RangedAudioParameter>>& params);
    void addTapersToLayout(std::vector<std::unique_ptr<RangedAudioParameter>>& params);
    void addFiltersToLayout(std::vector<std::unique_ptr<RangedAudioParameter>>& params);
    void addPitchesToLayout(std::vector<std::unique_ptr<RangedAudioParameter>>& params);
    void addDropsToLayout(std::vector<std::unique_ptr<RangedAudioParameter>>& params);
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GamelanizerParameters)
};
