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

/** \addtogroup GUI
 *  @{
 */

/**
 * \brief A rotary slider that snaps to the important semitones when the user holds modifier keys.
 */
class SliderToggleableSnap final : public Slider
{
public:
    SliderToggleableSnap() = default;

    SliderToggleableSnap(const SliderToggleableSnap&) = delete;

    SliderToggleableSnap& operator=(const SliderToggleableSnap&) = delete;

    SliderToggleableSnap(SliderToggleableSnap&&) = delete;

    SliderToggleableSnap& operator=(SliderToggleableSnap&&) = delete;

    ~SliderToggleableSnap() = default;

    double snapValue(double attemptedValue, DragMode dragMode) override;

private:
    JUCE_LEAK_DETECTOR(SliderToggleableSnap)
};

/** @}*/
