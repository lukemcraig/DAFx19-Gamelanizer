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

#include "PluginEditor.h"

//==============================================================================
GamelanizerAudioProcessorEditor::GamelanizerAudioProcessorEditor(GamelanizerAudioProcessor& p,
                                                                 AudioProcessorValueTreeState& vts,
                                                                 GamelanizerParameters& gp)
    : AudioProcessorEditor(&p), processor(p), gamelanizerParameters(gp), valueTreeState(vts)
{
    LookAndFeel::setDefaultLookAndFeel(&cleanLookAndFeel);
    //setLookAndFeel(&cleanLookAndFeel);

    GlyphArrangement glyph;
    glyph.addLineOfText(Font(32, Font::bold), "GAMELANIZER", 12, 38);
    glyph.createPath(titlePath);

    nameLabel.setText("Luke M. Craig", dontSendNotification);
    nameLabel.setJustificationType(Justification::bottomLeft);
    addAndMakeVisible(nameLabel);

    aboutButton.setButtonText("?");

    // TODO Async?
    aboutButton.onClick = [this]
    {
        AlertWindow::showMessageBox(AlertWindow::QuestionIcon, "About",
                                    "Gamelanizer " + String(JucePlugin_VersionString) +
                                    ", made by Luke M. Craig for DAFx19.\n"
                                    + "Code repo available at\n"
                                    + "https://github.com/lukemcraig/DAFx19-Gamelanizer\n"
#ifdef JUCE_DEBUG
                                    + "DEBUG BUILD: " + __TIMESTAMP__
#endif
                                    , "OK",
                                    this);
    };

    addAndMakeVisible(aboutButton);

    for (auto i = 0; i < GamelanizerConstants::maxLevels; ++i)
    {
        pitchSliders[i].setSliderStyle(Slider::SliderStyle::Rotary);
        pitchSliders[i].setName("pitch" + String(i));
        pitchSliders[i].setTextBoxStyle(Slider::TextEntryBoxPosition::TextBoxAbove, false, 100, 20);
        addAndMakeVisible(pitchSliders[i]);

        pitchSliderAttachments[i].reset(
            new SliderAttachment(valueTreeState, gamelanizerParameters.getPitchId(i), pitchSliders[i]));

        pitchSliderLabels[i].setText("Pitch Shift", dontSendNotification);
        pitchSliderLabels[i].setJustificationType(Justification::horizontallyCentred);
        addAndMakeVisible(pitchSliderLabels[i]);

        levelGroups[i].setText("Level " + String(i + 1));
        addAndMakeVisible(levelGroups[i]);

        dropLabels[i].setText("Drop Note:", dontSendNotification);
        addAndMakeVisible(dropLabels[i]);
        for (auto j = 0; j < 4; ++j)
        {
            dropButtons[i][j].setClickingTogglesState(true);
            dropButtons[i][j].setButtonText(String(j + 1));
            dropAttachments[i][j].reset(
                new ButtonAttachment(valueTreeState, gamelanizerParameters.getDropId(i, j), dropButtons[i][j]));
            addAndMakeVisible(dropButtons[i][j]);
        }
    }

    baseGroup.setText("Base");
    addAndMakeVisible(baseGroup);

    for (auto i = 0; i < GamelanizerConstants::maxLevels + 1; ++i)
    {
        gainSliders[i].setSliderStyle(Slider::SliderStyle::LinearVertical);
        gainSliders[i].setTextBoxStyle(Slider::TextEntryBoxPosition::TextBoxAbove, false, 80, 20);
        addAndMakeVisible(gainSliders[i]);

        gainAttachments[i].reset(new SliderAttachment(valueTreeState, gamelanizerParameters.getGainId(i),
                                                      gainSliders[i]));
        gainSlidersLabel[i].setJustificationType(Justification::horizontallyCentred);
        gainSlidersLabel[i].setText("Gain", dontSendNotification);

        addAndMakeVisible(gainSlidersLabel[i]);
    }
    for (auto i = 0; i < GamelanizerConstants::maxLevels + 1; ++i)
    {
        muteButtons[i].setClickingTogglesState(true);
        muteButtons[i].setButtonText("M");
        muteAttachments[i].reset(new ButtonAttachment(valueTreeState, gamelanizerParameters.getMuteId(i),
                                                      muteButtons[i]));
        addAndMakeVisible(muteButtons[i]);
    }

    for (auto i = 0; i < GamelanizerConstants::maxLevels + 1; ++i)
    {
        panSliders[i].setSliderStyle(Slider::SliderStyle::Rotary);
        panSliders[i].setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
        panSliders[i].setPopupDisplayEnabled(true, true, this);
        addAndMakeVisible(panSliders[i]);
        const auto textColour = getLookAndFeel().findColour(Slider::textBoxTextColourId);
        panSliders[i].setColour(TooltipWindow::textColourId, textColour);

        panAttachments[i].reset(new SliderAttachment(valueTreeState, gamelanizerParameters.getPanId(i), panSliders[i]));

        panSlidersLabel[i].setText("Pan", dontSendNotification);
        panSlidersLabel[i].setJustificationType(Justification::horizontallyCentred);
        addAndMakeVisible(panSlidersLabel[i]);
    }

    for (auto i = 0; i < GamelanizerConstants::maxLevels; ++i)
    {
        addAndMakeVisible(taperControls[i]);

        taperAttachments[i].reset(taperControls[i].attachSlider(valueTreeState, gamelanizerParameters.getTaperId(i)));

        taperLabels[i].setText("Taper", dontSendNotification);
        taperLabels[i].setJustificationType(Justification::horizontallyCentred);
        addAndMakeVisible(taperLabels[i]);
    }

    for (auto i = 0; i < GamelanizerConstants::maxLevels; ++i)
    {
        lpfSliders[i].setSliderStyle(Slider::SliderStyle::Rotary);
        lpfSliders[i].setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
        lpfSliders[i].setPopupDisplayEnabled(true, true, this);
        const auto textColour = getLookAndFeel().findColour(Slider::textBoxTextColourId);
        lpfSliders[i].setColour(TooltipWindow::textColourId, textColour);

        addAndMakeVisible(lpfSliders[i]);

        lpfSliderAttachments[i].reset(
            new SliderAttachment(valueTreeState, gamelanizerParameters.getLpfId(i), lpfSliders[i]));

        lpfLabels[i].setText("LPF", dontSendNotification);
        lpfLabels[i].setJustificationType(Justification::horizontallyCentred);
        addAndMakeVisible(lpfLabels[i]);
    }

    for (auto i = 0; i < GamelanizerConstants::maxLevels; ++i)
    {
        hpfSliders[i].setSliderStyle(Slider::SliderStyle::Rotary);
        hpfSliders[i].setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
        hpfSliders[i].setPopupDisplayEnabled(true, true, this);
        const auto textColour = getLookAndFeel().findColour(Slider::textBoxTextColourId);
        hpfSliders[i].setColour(TooltipWindow::textColourId, textColour);

        addAndMakeVisible(hpfSliders[i]);

        hpfSliderAttachments[i].reset(
            new SliderAttachment(valueTreeState, gamelanizerParameters.getHpfId(i), hpfSliders[i]));

        hpfLabels[i].setText("HPF", dontSendNotification);
        hpfLabels[i].setJustificationType(Justification::horizontallyCentred);
        addAndMakeVisible(hpfLabels[i]);
    }

    tempoEditor.setText(String(processor.getCurrentBpm()));
    addAndMakeVisible(tempoEditor);
    tempoEditor.addListener(this);
    tempoEditorLabel.setText("BPM", dontSendNotification);
    tempoEditorLabel.attachToComponent(&tempoEditor, false);

    // make it so clicking outside the text editor makes it lose focus
    setWantsKeyboardFocus(true);
    startTimer(200);
    setResizable(true, true);
    setResizeLimits(400, 400, 1680, 1050);
    setSize(1200, 500);
}

GamelanizerAudioProcessorEditor::~GamelanizerAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
    //LookAndFeel::setDefaultLookAndFeel(nullptr);
}

//==============================================================================
void GamelanizerAudioProcessorEditor::paint(Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));

    g.setColour(getLookAndFeel().findColour(Label::textColourId));

    g.strokePath(titlePath, PathStrokeType(0.5f));
}

void GamelanizerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(10);
    auto titleArea = area.removeFromTop(32);
    titleArea.removeFromLeft(222);
    nameLabel.setBounds(titleArea.removeFromLeft(100));
    titleArea.removeFromLeft(10);
    tempoEditor.setBounds(titleArea.removeFromLeft(100).withTrimmedTop(16));
    aboutButton.setBounds(titleArea.removeFromRight(32));
    area.removeFromTop(5);

    auto baseBounds = area.removeFromLeft(100);
    baseGroup.setBounds(baseBounds);
    baseBounds.removeFromTop(16);
    gainSlidersLabel[0].setBounds(baseBounds.removeFromTop(16));
    gainSliders[0].setBounds(baseBounds.removeFromTop(baseBounds.proportionOfHeight(0.75)));
    muteButtons[0].setBounds(baseBounds.removeFromTop(18).reduced(12, 0));

    panSlidersLabel[0].setBounds(baseBounds.removeFromTop(16));
    panSliders[0].setBounds(baseBounds);

    const auto pitchAreaWidth = area.getWidth();
    auto pitchArea = area.removeFromLeft(pitchAreaWidth);
    for (auto i = 0; i < GamelanizerConstants::maxLevels; ++i)
    {
        auto subdivisionLevelArea = pitchArea.removeFromLeft(pitchAreaWidth / GamelanizerConstants::maxLevels);
        subdivisionLevelArea.reduce(2, 2);
        levelGroups[i].setBounds(subdivisionLevelArea);
        subdivisionLevelArea.reduce(10, 10);

        subdivisionLevelArea.removeFromTop(10);

        auto gainAndPanSliderArea = subdivisionLevelArea.removeFromRight(subdivisionLevelArea.proportionOfWidth(0.25f));
        gainSlidersLabel[i + 1].setBounds(gainAndPanSliderArea.removeFromTop(16));
        gainSliders[i + 1].
            setBounds(gainAndPanSliderArea.removeFromTop(gainAndPanSliderArea.proportionOfHeight(0.75f)));

        muteButtons[i + 1].setBounds(gainAndPanSliderArea.removeFromTop(18));

        panSlidersLabel[i + 1].setBounds(gainAndPanSliderArea.removeFromTop(16));
        panSliders[i + 1].setBounds(gainAndPanSliderArea);

        auto dropButtonsArea = subdivisionLevelArea.removeFromBottom(30).reduced(0, 5);
        const auto dropButtonWidth = dropButtonsArea.proportionOfWidth(1.0 / 4.0);
        for (auto j = 0; j < 4; ++j)
        {
            dropButtons[i][j].setBounds(dropButtonsArea.removeFromLeft(dropButtonWidth));
        }
        dropLabels[i].setBounds(subdivisionLevelArea.removeFromBottom(16));

        taperControls[i].setBounds(
            subdivisionLevelArea.removeFromBottom(subdivisionLevelArea.proportionOfHeight(0.2f)));
        taperLabels[i].setBounds(subdivisionLevelArea.removeFromBottom(16));

        auto filterArea = subdivisionLevelArea.removeFromBottom(100);
        auto hpfFilterArea = filterArea.removeFromLeft(filterArea.proportionOfWidth(0.5f));
        hpfLabels[i].setBounds(hpfFilterArea.removeFromTop(16));
        hpfSliders[i].setBounds(hpfFilterArea);

        lpfLabels[i].setBounds(filterArea.removeFromTop(16));
        lpfSliders[i].setBounds(filterArea);

        pitchSliderLabels[i].setBounds(subdivisionLevelArea.removeFromTop(16));
        pitchSliders[i].setBounds(subdivisionLevelArea);
    }
}

//==============================================================================

void GamelanizerAudioProcessorEditor::timerCallback()
{
    // disable the tempo field if the DAW is playing    
    tempoEditor.setEnabled(!processor.getPreventGuiBpmChange());
}

void GamelanizerAudioProcessorEditor::updateProcessorTempo()
{
    auto newBpm = tempoEditor.getText().getFloatValue();
    newBpm = jmin(newBpm, GamelanizerConstants::maxBpm);
    newBpm = jmax(newBpm, GamelanizerConstants::minBpm);
    tempoEditor.setText(String(newBpm));
    processor.setCurrentBpm(newBpm);
}

void GamelanizerAudioProcessorEditor::textEditorFocusLost(TextEditor& textEditor)
{
    if (&textEditor == &tempoEditor)
    {
        updateProcessorTempo();
    }
}

void GamelanizerAudioProcessorEditor::textEditorEscapeKeyPressed(TextEditor& textEditor)
{
    if (&textEditor == &tempoEditor)
    {
        updateProcessorTempo();
    }
}

void GamelanizerAudioProcessorEditor::textEditorReturnKeyPressed(TextEditor& textEditor)
{
    if (&textEditor == &tempoEditor)
    {
        updateProcessorTempo();
    }
}
