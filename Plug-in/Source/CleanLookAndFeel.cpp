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

#include "CleanLookAndFeel.h"

CleanLookAndFeel::CleanLookAndFeel() : LookAndFeel_V4({
    0xfff8f8f8, // background
    0xffdadada, // widgetBackground (sliders off)
    0xff050505, // menuBackground
    0xffc4c4c4, // outline
    0xff050505, // defaultText
    0xff424242, // defaultFill (knobs)
    0xffe4e2e2, // highlightedText (Buttons On and Popup)
    0xff050505, // highlightedFill (sliders on)
    0xffd7d7d7 // menuText
})
{
}

//==============================================================================
void CleanLookAndFeel::drawLinearSlider(Graphics& g, int x, int y, int width, int height,
                                        float sliderPos,
                                        float /*minSliderPos*/,
                                        float /*maxSliderPos*/,
                                        const Slider::SliderStyle /*style*/, Slider& slider)
{
    // most of this is copied from the overridden library code
    const auto trackWidth = jmin(6.0f, slider.isHorizontal() ? height * 0.25f : width * 0.25f);

    const Point<float> startPoint(slider.isHorizontal() ? x : x + width * 0.5f,
                                  slider.isHorizontal() ? y + height * 0.5f : height + y);

    const Point<float> endPoint(slider.isHorizontal() ? width + x : startPoint.x,
                                slider.isHorizontal() ? startPoint.y : y);

    Path backgroundTrack;
    backgroundTrack.startNewSubPath(startPoint);
    backgroundTrack.lineTo(endPoint);
    const auto backgroundColour = slider.findColour(Slider::backgroundColourId);
    g.setColour(backgroundColour);
    g.strokePath(backgroundTrack, {trackWidth, PathStrokeType::curved, PathStrokeType::rounded});

    // added this
    g.setColour(backgroundColour.darker(trackDarkenAmount));
    g.strokePath(backgroundTrack, {valueTrackWidth * innerTrackScale, PathStrokeType::curved, PathStrokeType::rounded});

    const auto kx = slider.isHorizontal() ? sliderPos : (x + width * 0.5f);
    const auto ky = slider.isHorizontal() ? (y + height * 0.5f) : sliderPos;

    const auto minPoint = startPoint;
    const Point<float> maxPoint = {kx, ky};

    const auto thumbWidth = static_cast<float>(getSliderThumbRadius(slider));

    Path valueTrack;
    valueTrack.startNewSubPath(minPoint);
    valueTrack.lineTo(maxPoint);
    const auto trackColour = slider.findColour(Slider::trackColourId);
    g.setColour(trackColour);
    // changed this
    g.strokePath(valueTrack, {valueTrackWidth, PathStrokeType::curved, PathStrokeType::rounded});

    drawThumbKnob(g, slider, backgroundColour, valueTrackWidth, maxPoint, thumbWidth);
}


void CleanLookAndFeel::drawRotarySlider(Graphics& g, int x, int y, int width, int height, float sliderPos,
                                        const float rotaryStartAngle, const float rotaryEndAngle, Slider& slider)
{
    // most of this is copied from the overridden library code
    const auto outline = slider.findColour(Slider::rotarySliderOutlineColourId);
    const auto fill = slider.findColour(Slider::rotarySliderFillColourId);

    auto bounds = Rectangle<int>(x, y, width, height).toFloat().reduced(10);

    const auto radius = jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const auto lineW = jmin(8.0f, radius * 0.5f);
    // changed this
    const auto valueLineW = jmin(valueTrackWidthRotary, radius * 0.5f);
    const auto arcRadius = radius - lineW * 0.5f;

    if (slider.getName().startsWith("pitch"))
    {
        const auto centerX = x + width * 0.5f;
        const auto centerY = y + height * 0.5f;
        // todo clean this up

        // draw octave lines	
        const std::array<float, 7> octaves{
            -2400.0f, -1200.0f, 0.0f, 1200.0f,
            2400.0f, 3600.0f, 4800.0f
        };
        drawSemitoneLines(g, rotaryStartAngle, rotaryEndAngle, outline, centerX, centerY,
                          radius * 0.25f, radius, octaves);

        // draw 5ths
        const std::array<float, 14> fifths{
            -2000.0f, -1500.0f, -1000.0f,
            -500.0f, 0.0f, 500.0f,
            1000.0f, 1500.0f, 2000.0f,
            2500.0f, 3000.0f, 3500.0f,
            4000.0f, 4500.0f,

        };
        drawSemitoneLines(g, rotaryStartAngle, rotaryEndAngle, outline, centerX, centerY,
                          radius * 0.8f, radius, fifths);

        // draw 4ths
        const std::array<float, 10> fourths{
            -2100.0f, -1400.0f, -700.0f, 0.0f,
            700.0f, 1400.0f, 2100.0f,
            2800.0f, 3500.0f, 4200.0f
        };
        drawSemitoneLines(g, rotaryStartAngle, rotaryEndAngle, outline, centerX, centerY,
                          radius, radius * 1.1f, fourths);
    }

    {
        Path backgroundArc;
        backgroundArc.addCentredArc(bounds.getCentreX(),
                                    bounds.getCentreY(),
                                    arcRadius,
                                    arcRadius,
                                    0.0f,
                                    rotaryStartAngle,
                                    rotaryEndAngle,
                                    true);

        g.setColour(outline);
        g.strokePath(backgroundArc, PathStrokeType(lineW, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour(outline.darker(trackDarkenAmount));
        g.strokePath(backgroundArc,
                     PathStrokeType(valueLineW * innerTrackScale, PathStrokeType::curved, PathStrokeType::rounded));
    }
    if (slider.isEnabled())
    {
        Path valueArc;
        valueArc.addCentredArc(bounds.getCentreX(),
                               bounds.getCentreY(),
                               arcRadius,
                               arcRadius,
                               0.0f,
                               rotaryStartAngle,
                               toAngle,
                               true);

        g.setColour(fill);
        g.strokePath(valueArc, PathStrokeType(valueLineW, PathStrokeType::curved, PathStrokeType::rounded));
    }

    const Point<float> thumbPoint(bounds.getCentreX() + arcRadius * std::cos(toAngle - MathConstants<float>::halfPi),
                                  bounds.getCentreY() + arcRadius * std::sin(toAngle - MathConstants<float>::halfPi));

    const auto thumbWidth = lineW * 2.0f;
    // changed this
    drawThumbKnob(g, slider, outline, valueLineW, thumbPoint, thumbWidth);
}

void CleanLookAndFeel::drawGroupComponentOutline(Graphics& g, int width, int height,
                                                 const String& text, const Justification& position,
                                                 GroupComponent& group)
{
    // most of this is copied from the overridden library code
    const auto textH = 15.0f;
    const auto indent = 3.0f;
    const auto textEdgeGap = 4.0f;
    auto cs = 5.0f;

    Font f(textH);

    Path p;
    auto x = indent;
    auto y = f.getAscent() - 3.0f;
    auto w = jmax(0.0f, width - x * 2.0f);
    auto h = jmax(0.0f, height - y - indent);
    cs = jmin(cs, w * 0.5f, h * 0.5f);
    auto cs2 = 2.0f * cs;

    auto textW = text.isEmpty()
                     ? 0
                     : jlimit(0.0f, jmax(0.0f, w - cs2 - textEdgeGap * 2), f.getStringWidth(text) + textEdgeGap * 2.0f);
    auto textX = cs + textEdgeGap;

    if (position.testFlags(Justification::horizontallyCentred))
        textX = cs + (w - cs2 - textW) * 0.5f;
    else if (position.testFlags(Justification::right))
        textX = w - cs - textW - textEdgeGap;

    p.startNewSubPath(x + textX + textW, y);
    p.lineTo(x + w - cs, y);

    p.addArc(x + w - cs2, y, cs2, cs2, 0, MathConstants<float>::halfPi);
    p.lineTo(x + w, y + h - cs);

    p.addArc(x + w - cs2, y + h - cs2, cs2, cs2, MathConstants<float>::halfPi, MathConstants<float>::pi);
    p.lineTo(x + cs, y + h);

    p.addArc(x, y + h - cs2, cs2, cs2, MathConstants<float>::pi, MathConstants<float>::pi * 1.5f);
    p.lineTo(x, y + cs);

    p.addArc(x, y, cs2, cs2, MathConstants<float>::pi * 1.5f, MathConstants<float>::twoPi);
    p.lineTo(x + textX, y);

    auto alpha = group.isEnabled() ? 1.0f : 0.5f;

    g.setColour(group.findColour(GroupComponent::outlineColourId)
                     .withMultipliedAlpha(alpha));
    // changed this
    g.strokePath(p, PathStrokeType(groupStrokeThickness));

    g.setColour(group.findColour(GroupComponent::textColourId)
                     .withMultipliedAlpha(alpha));
    g.setFont(f);
    g.drawText(text,
               roundToInt(x + textX), 0,
               roundToInt(textW),
               roundToInt(textH),
               Justification::centred, true);
}

//==============================================================================
void CleanLookAndFeel::drawThumbKnob(Graphics& g, Slider& slider, const Colour outline, const float valueLineW,
                                     const Point<float> thumbPoint, const float thumbWidth) const
{
    const auto thumbArea = Rectangle<float>(thumbWidth, thumbWidth).withCentre(thumbPoint);
    Path thumbPath;
    thumbPath.addEllipse(thumbArea);

    g.setColour(outline);
    g.fillPath(thumbPath);

    g.setColour(slider.findColour(Slider::thumbColourId));
    g.strokePath(thumbPath, PathStrokeType(valueLineW));

    thumbPath.applyTransform(AffineTransform::scale(0.66f, 0.66f, thumbPoint.x, thumbPoint.y));
    g.fillPath(thumbPath);
}

template <std::size_t Size>
void CleanLookAndFeel::drawSemitoneLines(Graphics& g, const float rotaryStartAngle, const float rotaryEndAngle,
                                         const Colour colour, const float centerX, const float centerY,
                                         const float startRadius, const float endRadius,
                                         std::array<float, Size> importantSemitones) const
{
    for (auto& t : importantSemitones)
    {
        t += 2400.0f;
        t /= 7200.0f;
    }
    std::array<float, Size> importantPitchAngles{};
    for (auto i = 0; i < Size; ++i)
    {
        importantPitchAngles[i] = (1.0f - importantSemitones[i]) * rotaryStartAngle
            + importantSemitones[i] * rotaryEndAngle;
    }
    g.setColour(colour);
    for (auto& theta : importantPitchAngles)
    {
        Path p;
        const auto startX = centerX + startRadius * std::cos(theta);
        const auto startY = centerY + startRadius * std::sin(theta);
        const auto endX = centerX + endRadius * std::cos(theta);
        const auto endY = centerY + endRadius * std::sin(theta);
        auto rotation = AffineTransform::rotation(-float_Pi * 0.5f, centerX, centerY);
        Line<float> l{{startX, startY}, {endX, endY}};
        l.applyTransform(rotation);
        p.addLineSegment(l, valueTrackWidth);
        g.fillPath(p);
    }
}
