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
#include <cmath>
#include "StatefulRoundedNumber.h"

StatefulRoundedNumber::StatefulRoundedNumber(): roundingMethod(roundDown)
{
}

StatefulRoundedNumber::StatefulRoundedNumber(const RoundingMethod roundingMethod): roundingMethod(roundingMethod)
{
}

void StatefulRoundedNumber::reset()
{
    roundedOff = 0;
}

int StatefulRoundedNumber::roundDownMethod()
{
    const auto sum = exactValue + roundedOff;
    const auto intPart = std::floor(sum);
    // roundedOff will always be nonnegative
    roundedOff = sum - intPart;
    return static_cast<int>(intPart);
}

int StatefulRoundedNumber::roundTowardsZeroMethod()
{
    double intPart;
    roundedOff = std::modf(exactValue + roundedOff, &intPart);
    return static_cast<int>(intPart);
}

// ReSharper disable once CppMemberFunctionMayBeConst
int StatefulRoundedNumber::getNextInt()
{
    return (this->*roundingMethod)();
}
