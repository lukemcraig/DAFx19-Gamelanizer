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
 * \brief Utility functions for calculating the modulus of x/y where the result has the same sign as y.
 * This is useful for wrapping indices of circular buffers.
 * This behaves the same as Python's % (modulo) operator and Matlab's mod().
 * It behaves differently from std::mod, std::remainder, and the C++ % operator.
 * 
 * \see https://stackoverflow.com/questions/1907565/c-and-python-different-behaviour-of-the-modulo-operation
 * \see https://www.mathworks.com/help/matlab/ref/mod.html
 * \see https://docs.scipy.org/doc/numpy/reference/generated/numpy.mod.html
 */
struct ModuloSameSignAsDivisor
{
    /**
     * \brief Same as (x % y) in Python or mod(x,y) in Matlab. For ints.
     */
    static int modInt(const int x, const int y) { return ((x % y) + y) % y; }
    /**
     * \brief Same as (x % y) in Python or mod(x,y) in Matlab. For floats.
     */
    static float modFloat(float x, float y);
};
