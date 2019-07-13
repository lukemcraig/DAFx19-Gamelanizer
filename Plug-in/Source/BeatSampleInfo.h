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

/** \addtogroup Core
 *  @{
 */

/**
 * \brief Data about the length of a beat and our position in it.
 */
struct BeatSampleInfo
{
    /**
     * \brief This should be called when the BPM is changed. Starts this struct back at the beginning of the timeline.
     * \param newSamplesPerBeatFractional the exact samples per beat.
     */
    void reset(double newSamplesPerBeatFractional);

    /**
     * \brief Start this struct back at the beginning of the timeline.
     */
    void reset();

    /**
     * \brief This should be called when the host moves to a new beat.
     */
    void setNextBeatInfo();

    /**
	 * \brief This should be called after every sample is processed to internally track the host timeline position.
	 */
    void incrementSamplesIntoBeat() { ++samplesIntoBeat; }

    /**
	 * \return True if we're at a beat boundary
	 */
    [[nodiscard]] bool isPastBeatEnd() const { return samplesIntoBeat >= beatSampleLength; }

    /**
	 * \return The number of samples through the beat the host timeline has moved past
	 */
    [[nodiscard]] int getSamplesIntoBeat() const { return samplesIntoBeat; }

    /**
     * \return The sample position of start of the beat. Prevents accumulated rounding errors.
     */
    [[nodiscard]] int getBeatSampleStart() const { return beatSampleStart; }

    /**
     * \return The sample position of end of the beat. Prevents accumulated rounding errors.
     */
    [[nodiscard]] int getBeatSampleEnd() const { return beatSampleEnd; }

    /**
     * \brief This value changes from beat to beat to prevent accumulated rounding errors.
     * \return The length in whole samples of the current beat.
     */
    [[nodiscard]] int getBeatSampleLength() const { return beatSampleLength; }

    /**
	 * \return The number of the beat, starting from 0.
	 */
    [[nodiscard]] int getBeatNumber() const { return beatNumber; }

    /**
	 * \return True if the current beat is the second one in a pair of beats
	 */
    [[nodiscard]] int isBeatB() const { return beatB; }

private:
    double samplesPerBeatFractional{};
    int samplesIntoBeat{};
    int beatSampleStart{};
    int beatSampleEnd{};
    int beatSampleLength{};
    int beatNumber{};
    bool beatB{};
};

/** @}*/
