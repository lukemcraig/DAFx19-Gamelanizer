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

#include "../JuceLibraryCode/JuceHeader.h"
#include "TaperControls.h"
#include "WindowingFunctions.h"

//==============================================================================
TaperControls::TaperControls()
{
    addAndMakeVisible(taperSlider);
    taperSlider.setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
    taperSlider.setPopupDisplayEnabled(true, true, this);

    taperSlider.addListener(this);
}

TaperControls::~TaperControls()
{
    taperSlider.removeListener(this);
    // taperSlider.setLookAndFeel(nullptr);
    // setLookAndFeel(nullptr);
}

void TaperControls::visibilityChanged()
{
    // have to put this here to use the right colour
    const auto textColour = getLookAndFeel().findColour(Slider::textBoxTextColourId);
    taperSlider.setColour(TooltipWindow::textColourId, textColour);
}

void TaperControls::paint(Graphics& g)
{
    updateWindowPath(static_cast<float>(taperSlider.getValue()));

    g.setColour(Colours::black);

    g.strokePath(windowPath, {1.0f, PathStrokeType::curved, PathStrokeType::rounded});
    g.setColour(getLookAndFeel().findColour(Slider::backgroundColourId));
    g.fillPath(windowPath);
}

void TaperControls::resized()
{
    // set the bounds of any child components
    auto area = getLocalBounds();
    taperSlider.setBounds(area.removeFromBottom(20));
    chartArea = area;
}

void TaperControls::sliderValueChanged(Slider* slider)
{
    if (slider == &taperSlider)
    {
        repaint();
    }
}

void TaperControls::updateWindowPath(const float windowAlpha)
{
    windowPath.clear();

    const auto rectW = chartArea.getWidth() * 0.9f;
    const auto rectH = chartArea.getHeight() * 0.9f;

    const auto xOffset = (chartArea.getWidth() - rectW) * 0.5f;
    const auto yOffset = (chartArea.getHeight() - rectH) * 0.5f;

    windowPath.startNewSubPath(xOffset, rectH + yOffset);

    for (auto n = 0; n < rectW; n++)
    {
        const auto y = rectH * WindowingFunctions::tukeyWindow(n, static_cast<int>(rectW), windowAlpha);
        windowPath.lineTo(n + xOffset, rectH - y + yOffset);
    }
    windowPath.lineTo(rectW + xOffset, rectH + yOffset);
}

/**
 * \brief Make sure that the attachment gets destroyed before this object!
 */
TaperControls::SliderAttachment* TaperControls::attachSlider(AudioProcessorValueTreeState& valueTreeState,
                                                             const String& pid)
{
    return new SliderAttachment(valueTreeState, pid, taperSlider);
}
