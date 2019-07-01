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
#include <array>
#include "../JuceLibraryCode/JuceHeader.h"
/**
 * \brief Utility windowing functions
 */
struct WindowingFunctions
{
    /**
     * \brief A Tukey (cosine-tapered) window.
     * \param x The input argument to evaluate (the position along the window) .
     * \param length The length of the window.
     * \param alpha 0 is rectangular (no windowing). 1 is a Hann window.
     * \return 
     * \url https://en.wikipedia.org/wiki/Window_function#Tukey_window
     * \url https://www.mathworks.com/help/signal/ref/tukeywin.html
     * \url http://web.mit.edu/xiphmont/Public/windows.pdf
     */
    static float tukeyWindow(int x, int length, float alpha);

    /**
     * \brief Fill an array with a nonsymmetric Hann window.     
     * \param window The array to fill.
     * \param fftSize The FFT length. Make it window.size() -1 to make it symmetric.
     * 
     * \url https://en.wikipedia.org/wiki/Hann_function
     */
    template <typename T, std::size_t Size>
    static void fillWithNonsymmetricHannWindow(std::array<T, Size>& window, int fftSize)
    {
        const auto twoPiOverM = juce::MathConstants<T>::twoPi / static_cast<T>(fftSize);
        for (auto n = 0; n < window.size(); n++)
        {
            window[n] = 0.5f - 0.5f * std::cos(twoPiOverM * n);
        }
    }
};
