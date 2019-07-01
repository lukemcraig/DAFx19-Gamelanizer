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
#include "PluginProcessor.h"
#include "TaperControls.h"
#include "CleanLookAndFeel.h"
#include "SliderToggleableSnap.h"


/**
 * \brief The GUI for the plug-in
 */
class Gamelanizer2AudioProcessorEditor : public AudioProcessorEditor,
                                         public TextEditor::Listener,
                                         public Timer
{
public:
    Gamelanizer2AudioProcessorEditor(GamelanizerAudioProcessor&, AudioProcessorValueTreeState&, GamelanizerParameters&);
    ~Gamelanizer2AudioProcessorEditor();

    //==============================================================================
    void paint(Graphics&) override;
    void resized() override;

    void timerCallback() override;

    void textEditorFocusLost(TextEditor&) override;
    void textEditorEscapeKeyPressed(TextEditor&) override;
    void textEditorReturnKeyPressed(TextEditor&) override;

private:
    typedef AudioProcessorValueTreeState::SliderAttachment SliderAttachment;
    typedef AudioProcessorValueTreeState::ButtonAttachment ButtonAttachment;

    GamelanizerAudioProcessor& processor;
    GamelanizerParameters& gamelanizerParameters;
    AudioProcessorValueTreeState& valueTreeState;

    CleanLookAndFeel cleanLookAndFeel;

    Path titlePath;
    Label nameLabel;
    TextButton aboutButton;

    TextEditor tempoEditor;
    Label tempoEditorLabel;

    GroupComponent baseGroup;
    std::array<GroupComponent, GamelanizerConstants::maxLevels> levelGroups;

    std::array<TextButton, GamelanizerConstants::maxLevels + 1> muteButtons; // add +1 for the input level gain
    std::array<std::unique_ptr<ButtonAttachment>, GamelanizerConstants::maxLevels + 1> muteAttachments;

    std::array<Slider, GamelanizerConstants::maxLevels + 1> gainSliders; // add +1 for the input level gain
    std::array<Label, GamelanizerConstants::maxLevels + 1> gainSlidersLabel;
    std::array<std::unique_ptr<SliderAttachment>, GamelanizerConstants::maxLevels + 1> gainAttachments;

    std::array<Slider, GamelanizerConstants::maxLevels + 1> panSliders; // add +1 for the input level pan
    std::array<Label, GamelanizerConstants::maxLevels + 1> panSlidersLabel;
    std::array<std::unique_ptr<SliderAttachment>, GamelanizerConstants::maxLevels + 1> panAttachments;

    std::array<SliderToggleableSnap, GamelanizerConstants::maxLevels> pitchSliders;
    std::array<Label, GamelanizerConstants::maxLevels> pitchSliderLabels;
    std::array<std::unique_ptr<SliderAttachment>, GamelanizerConstants::maxLevels> pitchSliderAttachments;

    std::array<Label, GamelanizerConstants::maxLevels> dropLabels;
    std::array<std::array<TextButton, 4>, GamelanizerConstants::maxLevels> dropButtons;
    std::array<std::array<std::unique_ptr<ButtonAttachment>, 4>, GamelanizerConstants::maxLevels> dropAttachments;

    std::array<Slider, GamelanizerConstants::maxLevels> lpfSliders;
    std::array<Label, GamelanizerConstants::maxLevels> lpfLabels;
    std::array<std::unique_ptr<SliderAttachment>, GamelanizerConstants::maxLevels> lpfSliderAttachments;

    std::array<Slider, GamelanizerConstants::maxLevels> hpfSliders;
    std::array<Label, GamelanizerConstants::maxLevels> hpfLabels;
    std::array<std::unique_ptr<SliderAttachment>, GamelanizerConstants::maxLevels> hpfSliderAttachments;

    std::array<TaperControls, GamelanizerConstants::maxLevels> taperControls;
    std::array<Label, GamelanizerConstants::maxLevels> taperLabels;
    std::array<std::unique_ptr<SliderAttachment>, GamelanizerConstants::maxLevels> taperAttachments;

    //==============================================================================
    void updateProcessorTempo();
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Gamelanizer2AudioProcessorEditor)
};
