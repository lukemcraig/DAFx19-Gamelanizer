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
/**
 * \brief Data about the length of a beat and our position in it.
 */
struct BeatSampleInfo
{
    /**
     * \brief This should be called when the BPM, timeline position, or sample rate is changed.
     * \param samplesPerBeatFractional the exact samples per beat.
     */
    void reset(double samplesPerBeatFractional) ;

    /**
     * \brief This should be called when the host moves to a new beat.
     * \param samplesPerBeatFractional eventually this will allow changing tempos. For now it should be the same value that was called in reset.
     */
    void setNextBeatInfo(double samplesPerBeatFractional) ;

    /**
	 * \brief This should be called after every sample is processed to internally track the host timeline position.
	 */
	void incrementSamplesIntoBeat()  { ++samplesIntoBeat; }

    /**
	 * \return True if we're at a beat boundary
	 */
	bool isPastBeatEnd() const { return samplesIntoBeat >= beatSampleLength; }
    /**
	 * \return The number of samples through the beat the host timeline has moved past
	 */
	int getSamplesIntoBeat() const  { return samplesIntoBeat; }
	/**
     * \return The sample position of start of the beat. Prevents accumulated rounding errors.
     */
	int getBeatSampleStart() const  { return beatSampleStart; }
	/**
     * \return The sample position of end of the beat. Prevents accumulated rounding errors.
     */
	int getBeatSampleEnd() const  { return beatSampleEnd; }
	/**
     * \brief This value changes from beat to beat to prevent accumulated rounding errors.
     * \return The length in whole samples of the current beat.
     */
	int getBeatSampleLength() const  { return beatSampleLength; }
    /**
	 * \return The number of the beat, starting from 0.
	 */
	int getBeatNumber() const  { return beatNumber; }
    /**
	 * \return True if the current beat is the second one in a pair of beats
	 */
	int isBeatB() const  { return beatB; }
private:
	int samplesIntoBeat{};
	int beatSampleStart{};
	int beatSampleEnd{};
	int beatSampleLength{};
	int beatNumber{};
	bool beatB{};
};