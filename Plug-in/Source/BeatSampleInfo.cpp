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

#include "BeatSampleInfo.h"
#include "../JuceLibraryCode/JuceHeader.h"

void BeatSampleInfo::reset(const double newSamplesPerBeatFractional)
{
    samplesPerBeatFractional = newSamplesPerBeatFractional;
    reset();
}

void BeatSampleInfo::reset()
{
    beatSampleEnd = -1;
    beatNumber = 0;
    beatB = true;
    setNextBeatInfo();
}

void BeatSampleInfo::setNextBeatInfo()
{
    ++beatNumber;
    beatSampleStart = beatSampleEnd + 1;
    // this is only valid if the tempo never changes
    beatSampleEnd = static_cast<int>(std::round(samplesPerBeatFractional * beatNumber));
    beatSampleLength = beatSampleEnd - beatSampleStart;
    samplesIntoBeat = 0;
    beatB = !beatB;
}
