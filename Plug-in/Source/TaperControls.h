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

/** \addtogroup GUI
 *  @{
 */

/**
 * \brief Component class for drawing and changing the Tukey windows of each subdivision level
 */
class TaperControls final : public Component,
                            public Slider::Listener
{
public:
    TaperControls();

    TaperControls(const TaperControls&) = delete;

    TaperControls& operator=(const TaperControls&) = delete;

    TaperControls(TaperControls&&) = delete;

    TaperControls& operator=(TaperControls&&) = delete;

    ~TaperControls();

    void paint(Graphics&) override;

    void resized() override;

    void sliderValueChanged(Slider* slider) override;

    void visibilityChanged() override;

    typedef AudioProcessorValueTreeState::SliderAttachment SliderAttachment;

    SliderAttachment* attachSlider(AudioProcessorValueTreeState& valueTreeState, const String& pid);

private:
    Slider taperSlider;

    Rectangle<int> chartArea;
    Path windowPath;

    void updateWindowPath(float windowAlpha);

    JUCE_LEAK_DETECTOR(TaperControls)
};

/** @}*/
