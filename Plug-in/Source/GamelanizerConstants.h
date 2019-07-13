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
 * \brief Constants that are used in multiple places. 
 */
struct GamelanizerConstants
{
    /**
	 * \brief Number of subdivision levels, \f$M\f$.
	 */
    static constexpr int maxLevels{4};

    /**
	 * \brief Minimum BPM that the GUI is allowed to set. 
	 * The sizes of various buffers are based on this.
	 */
    static constexpr float minBpm{30.0f};
    /**
     * \brief Maximum BPM that the GUI is allowed to set.
     * This isn't actually necessary for operation and could be taken out.
     */
    static constexpr float maxBpm{1000.0f};

    /**
	 * \brief The minimum pitch shift in cents allowable.
	 */
    static constexpr float minPitchShiftCents{-2400.0f};

    /**
	 * \brief The maximum pitch shift in cents allowable.
	 */
    static constexpr float maxPitchShiftCents{4800.0f};
};

/** @}*/
