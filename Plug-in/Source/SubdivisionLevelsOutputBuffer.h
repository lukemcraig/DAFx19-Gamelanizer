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
 * \brief Struct for the buffer where the subdivision level outputs are overlapped and added in the correct positions.
 */
struct SubdivisionLevelsOutputBuffer
{
    /**
     * \brief The actual buffer
     */
    AudioBuffer<float> data;

    /**
     * \brief The read position of every channel in the buffer is the same.
     */
    int readPosition{};

    JUCE_LEAK_DETECTOR(SubdivisionLevelsOutputBuffer)
};

/** @}*/
