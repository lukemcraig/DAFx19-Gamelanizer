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

#include "WindowingFunctions.h"

float WindowingFunctions::tukeyWindow(const int x, const int length, const float alpha)
{
    if (alpha == 0.0f)
    {
        return 1.0f;
    }
    if (x < 0)
    {
        return 0.0f;
    }
    if (static_cast<float>(x) < alpha * (static_cast<float>(length) - 1.0f) * .5f)
    {
        const auto b = (2.0f * static_cast<float>(x)) / (alpha * (static_cast<float>(length) - 1.0f));
        const auto nCos = dsp::FastMathApproximations::cos(MathConstants<float>::pi * (b - 1.0f));
        return .5f * (1.0f + nCos);
    }
    if (static_cast<float>(x) <= (static_cast<float>(length) - 1.0f) * (1.0f - (alpha * .5f)))
    {
        return 1.0f;
    }
    if (x <= length - 1)
    {
        const auto b = (2.0f * static_cast<float>(x)) / (alpha * (static_cast<float>(length) - 1.0f));
        const auto nCos = dsp::FastMathApproximations::cos(MathConstants<float>::pi * (b - (2.0f / alpha) + 1));
        return .5f * (1.0f + nCos);
    }
    return 0.0f;
}

