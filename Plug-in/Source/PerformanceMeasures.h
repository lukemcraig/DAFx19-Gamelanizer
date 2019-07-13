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

#if MeasurePerformance
#include "../JuceLibraryCode/JuceHeader.h"
#include <chrono>

/** \addtogroup Utility
 *  @{
 */

/**
 * \brief A utility class to measure the time between two points in code.
 * This is similar to JUCE's PerformanceCounter class. The difference is that this 
 * stores every single measurement and writes it all to a file at once. Additionally,
 * The starting and ending time are not stored as member variables to prevent accidental
 * miscalculations (from returning early). Unless you definitely want to use this, the 
 * MeasurePerformance preprocessor definition should be set to 0 in the .jucer file, 
 * otherwise the plug-in will cause an audio dropout when the measurement file is written. 
 */
class PerformanceMeasures
{
public:
    PerformanceMeasures() = default;
    ~PerformanceMeasures() = default;
    //==============================================================================
    /**
     * \brief Call this so that multiple measurements can be recorded without having to reopen the session.
     * Ensures that bogus measurements from a previous run are not accidentally used.
     */
    void reset();
    /**
     * \brief Call at the start of every measurement block.
     * \return The starting time to be passed to finishMeasurements.
     */
    static std::chrono::steady_clock::time_point getNewStartingTime();
    /**
     * \brief Call at the end of every measurement block. 
     * When the sampleLocation reaches stopAtSample or nMeasurements have been made, a .csv will be written to the users desktop.
     * \param startingTime The value that getNewStartingTime returned at the beginning of the measured region.
     * \param sampleLocation The calculated host sample position. Used for x-axis plotting and verifying that block size is consistent.
     */
    void finishMeasurements(std::chrono::time_point<std::chrono::steady_clock> startingTime, int64 sampleLocation);
private:
    enum
    {
        // 32 beats at 80BPM, 44.1kHz
        stopAtSample = 1058400,
        // large enough array size for block size of 32 
        nMeasurements = 33100
    };

    std::array<double, nMeasurements> measurements{};
    std::array<int64, nMeasurements> measurementSampleLocations{};
    size_t measurementIndex{};
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PerformanceMeasures)
};
#endif

/** @}*/
