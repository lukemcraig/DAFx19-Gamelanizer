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
#include "GamelanizerConstants.h"
//==============================================================================

/** \addtogroup GUI
 *  @{
 */

/**
 * \brief Scale an array of semitones at compile time between the min and max allowable.
 */
template <size_t... Is, size_t N>
constexpr std::array<float, N> normalizeSemitones(std::array<float, N> const& semitones, std::index_sequence<Is...>)
{
    constexpr auto range = GamelanizerConstants::maxPitchShiftCents - GamelanizerConstants::minPitchShiftCents;
    return std::array<float, N>{
        {
            ((semitones[Is] - GamelanizerConstants::minPitchShiftCents) / range)...
        }
    };
}

/**
 * \brief Calculate the angles corresponding to an array of semitones for a rotary slider in the default rotation at compile time.
 */
template <size_t... Is, size_t N>
constexpr std::array<float, N> semitoneAngles(std::array<float, N> const& semitones, std::index_sequence<Is...>)
{
    constexpr auto rotaryStartAngle = 3.76991153f;
    constexpr auto rotaryEndAngle = 8.79645920f;

    return std::array<float, N>{
        {((1.0f - semitones[Is]) * rotaryStartAngle + semitones[Is] * rotaryEndAngle)...}
    };
}

/**
 * \brief Normalize and calculate the angles for the pitch sliders at compile time
 * \see https://stackoverflow.com/questions/34031231/multiplying-each-element-of-an-stdarray-at-compile-time 
 */
template <size_t... Is, size_t N>
constexpr std::array<float, N> normalizeSemitoneAngles(std::array<float, N> const& semitones,
                                                       std::index_sequence<Is...>)
{
    std::array<float, N> normalized = normalizeSemitones(semitones, std::make_index_sequence<N>{});
    return semitoneAngles(normalized, std::make_index_sequence<N>{});
}

//==============================================================================

/**
 * \brief A greyscale LookAndFeel made to print in a paper without wasting ink.
 * 
 * The overriding methods are mostly just tweaked versions of the base class in the JUCE library. 
 */
class CleanLookAndFeel final : public LookAndFeel_V4
{
public:
    CleanLookAndFeel();

    CleanLookAndFeel(const CleanLookAndFeel&) = delete;

    CleanLookAndFeel& operator=(const CleanLookAndFeel&) = delete;

    CleanLookAndFeel(CleanLookAndFeel&&) = delete;

    CleanLookAndFeel& operator=(CleanLookAndFeel&&) = delete;

    ~CleanLookAndFeel() = default;

    /**
     * \brief The default linear slider but with narrower lines and a subtle groove and a cool knob.
     */
    void drawLinearSlider(Graphics&, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                          float maxSliderPos, Slider::SliderStyle, Slider&) override;

    /**
   * \brief The default rotary slider with narrower lines and a subtle groove and a cool knob. 
   * Also lines to indicate important indicate semitones.
   */
    void drawRotarySlider(Graphics&, int x, int y, int width, int height, float sliderPosProportional,
                          float rotaryStartAngle, float rotaryEndAngle, Slider&) override;

    /**
    * \brief The default component outline with the thickness of #groupStrokeThickness
    */
    void drawGroupComponentOutline(Graphics& g, int width, int height, const String& text,
                                   const Justification& position, GroupComponent& group) override;

private:
    static constexpr float valueTrackWidth{2.0f};
    static constexpr float valueTrackWidthRotary{2.0f};
    static constexpr float trackDarkenAmount{0.33f};
    static constexpr float innerTrackScale{0.5f};
    static constexpr float groupStrokeThickness{1.0f};

    static constexpr std::array<float, 7> octaves{
        -2400.0f, -1200.0f, 0.0f, 1200.0f,
        2400.0f, 3600.0f, 4800.0f
    };

    static constexpr std::array<float, 14> fifths{
        -2000.0f, -1500.0f, -1000.0f,
        -500.0f, 0.0f, 500.0f,
        1000.0f, 1500.0f, 2000.0f,
        2500.0f, 3000.0f, 3500.0f,
        4000.0f, 4500.0f
    };

    static constexpr std::array<float, 10> fourths{
        -2100.0f, -1400.0f, -700.0f, 0.0f,
        700.0f, 1400.0f, 2100.0f,
        2800.0f, 3500.0f, 4200.0f
    };

    static constexpr auto octavesAngles = normalizeSemitoneAngles(octaves, std::make_index_sequence<octaves.size()>{});
    static constexpr auto fifthsAngles = normalizeSemitoneAngles(fifths, std::make_index_sequence<fifths.size()>{});
    static constexpr auto fourthsAngles = normalizeSemitoneAngles(fourths, std::make_index_sequence<fourths.size()>{});

    //==============================================================================
    /**
     * \brief Draw lines on the pitch rotary sliders to show 4ths, 5ths, or octaves.
     * \param g The graphics context.
     * \param colour The colour of a line.
     * \param centerX The center x position of the radial slider
     * \param centerY The center y position of the radial slider
     * \param startRadius The radius from the center where a line begins
     * \param endRadius The radius from the center where a line ends
     * \param importantSemitones the 4ths and 5ths or octaves array
     */
    template <std::size_t Size>
    void drawSemitoneLines(Graphics& g, Colour colour, float centerX, float centerY, float startRadius,
                           float endRadius, std::array<float, Size> importantSemitones) const;

    /**
     * \brief A cooler thumb knob for sliders.
     */
    static void drawThumbKnob(Graphics& g, Slider& slider, Colour outline,
                              float valueLineW, Point<float> thumbPoint,
                              float thumbWidth);

    //==============================================================================
    JUCE_LEAK_DETECTOR(CleanLookAndFeel)
};

/** @}*/
