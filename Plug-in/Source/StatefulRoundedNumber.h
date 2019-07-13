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

/** \addtogroup Utility
 *  @{
 */

/**
 * \brief A utility class for storing a number that needs to be repeatedly used as an integral without accumulating rounding errors.
 * For example, for calculating a sample index accurately.
 */
class StatefulRoundedNumber
{
public:
    /**
     * \brief A zero-parameter method pointer.
     */
    typedef int (StatefulRoundedNumber::* RoundingMethod)();

    /**
     * \brief Default constructor uses the roundDown method.
     */
    StatefulRoundedNumber();

    /**
     * \brief Non-default constructor
     * \param roundingMethod The rounding function to use.
     */
    explicit StatefulRoundedNumber(RoundingMethod roundingMethod);

    /**
     * \brief Reset the state but keep the #exactValue
     */
    void reset();

    /**
     * \brief Use the unused portion of the last call with the actual value and round that. 
     * Store the new unused portion for the next call. 
     * \return The rounded int
     */
    int getNextInt();

    /**
     * \brief The actual value of the number
     */
    double exactValue{};

private:
    /**
     * \brief The unused portion accumulated each call to #getNextInt
     */
    double roundedOff{};

    const RoundingMethod roundingMethod;

    int roundDownMethod();

    int roundTowardsZeroMethod();

public:
    static constexpr RoundingMethod roundDown{&StatefulRoundedNumber::roundDownMethod};
    static constexpr RoundingMethod roundTowardsZero{&StatefulRoundedNumber::roundTowardsZeroMethod};
};

/** @}*/
