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

#include "SliderToggleableSnap.h"

double SliderToggleableSnap::snapValue(const double attemptedValue, DragMode)
{
    // snap to octaves
    if (ModifierKeys::currentModifiers.isShiftDown())
        return std::round(attemptedValue / 1200.0) * 1200.0;
    // snap to fifths
    if (ModifierKeys::currentModifiers.isCtrlDown() || ModifierKeys::currentModifiers.isCommandDown())
        return std::round(attemptedValue / 700.0) * 700.0;
    // snap to fourths
    if (ModifierKeys::currentModifiers.isAltDown())
        return std::round(attemptedValue / 500.0) * 500.0;
    return attemptedValue;
}
