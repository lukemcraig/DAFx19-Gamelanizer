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

/**
 * \brief A greyscale LookAndFeel made to print in a paper without wasting ink.
 * The overriding methods are mostly just tweaked versions of the base class in the JUCE library.
 */
class CleanLookAndFeel : public LookAndFeel_V4
{
public:
    CleanLookAndFeel();

    /**
     * \brief The default linear slider with narrower lines and a subtle groove and a cool knob
     */
    void drawLinearSlider(Graphics&, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                          float maxSliderPos, const Slider::SliderStyle, Slider&) override;
    /**
   * \brief The default rotary slider with narrower lines and a subtle groove and a cool knob. Also lines to important indicate semitones.
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

    //==============================================================================
    /**
     * \brief Draw lines on the pitch rotary sliders to show 4ths, 5ths, and octaves.
     * \param importantSemitones 
     */
    template <std::size_t Size>
    void drawSemitoneLines(Graphics& g, const float rotaryStartAngle, const float rotaryEndAngle, const Colour colour,
                           const float centerX, const float centerY, const float startRadius, const float endRadius,
                           std::array<float, Size> importantSemitones) const;
    /**
     * \brief A cooler thumb knob for sliders.
     */
    void drawThumbKnob(Graphics& g, Slider& slider, Colour outline, float valueLineW, Point<float> thumbPoint,
                       float thumbWidth) const;
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CleanLookAndFeel)
};
