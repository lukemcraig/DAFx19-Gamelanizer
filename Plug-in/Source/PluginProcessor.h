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
#include "PhaseVocoder.h"
#include "GamelanizerParameters.h"
#include "GamelanizerConstants.h"
#include "GamelanizerParametersVTSHelper.h"
#include "BeatSampleInfo.h"
#if MeasurePerformance
#include "PerformanceMeasures.h"
#endif

//==============================================================================
/**
 * \brief The main class for the plug-in
 */
class GamelanizerAudioProcessor : public AudioProcessor
{
public:
    //==============================================================================
    GamelanizerAudioProcessor();
    ~GamelanizerAudioProcessor();

    //==============================================================================
    void initAllPhaseVocoders();
    void queuePhaseVocoderNextParams(int level);
    //==============================================================================

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;

    void releaseResources() override;

    void reset() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(AudioBuffer<float>&, MidiBuffer&) override;

    void updateLpFilter(int level);

    void updateHpFilter(int level);
    //==============================================================================
    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    //==============================================================================
    const String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;
    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const String getProgramName(int index) override;
    void changeProgramName(int index, const String& newName) override;
    //==============================================================================
    /**
     * \brief Saves the parameters and BPM. Called by the host.
     */
    void getStateInformation(MemoryBlock& destData) override;
    /**
     * \brief Loads the parameters and BPM. Called by the host.
     */
    void setStateInformation(const void* data, int sizeInBytes) override;
    //==============================================================================
    /**
     * \brief Thread safe way for the GUI to know if it should disable the BPM text field. 
     * \return 
     */
    bool getHostIsPlaying() const { return hostIsPlaying.load(); }
    //==============================================================================
    /**
     * \brief Thread safe way for the GUI to display the processor's BPM.
     * \return 
     */
    float getCurrentBpm() const { return currentBpm.load(); }
    /**
     * \brief Thread safe way for the GUI to change the processor's BPM. 
     * This should not be called during playback!
     * \param newBpm the new tempo in beats per minute
     */
    void setCurrentBpm(float newBpm);
    //==============================================================================
private:
    //==============================================================================
#if MeasurePerformance
    PerformanceMeasures performanceMeasures;
#endif
    //==============================================================================
    /**
     * \brief The sample rate that the host reports in prepareToPlay.
     * Used for samplesPerBeatFractional and the filters.
     */
    double hostSampleRate{};
    float nyquist{};
    std::atomic<float> currentBpm{};
    double samplesPerBeatFractional{};
    //==============================================================================
    int64 hostSampleOughtToBe{};
    std::atomic<bool> hostIsPlaying{};
    //==============================================================================
    BeatSampleInfo beatSampleInfo;

    //==============================================================================
    enum InitWAndDelayMethod
    {
        threeBeats,
        // this is the one from the paper
        earliestAWithC,
        // this is the one I've been using the longest
        earliestABeforeC // this is the one that uses the least number of samples
    };

    static constexpr InitWAndDelayMethod initWAndDelayMethod = threeBeats;

    AudioBuffer<float> delayBuffer;
    int dlyWritePos{};
    int dlyReadPos{};
    //==============================================================================
    AudioBuffer<float> outputBuffer;
    int outReadPos{};
    // TODO this don't wrap itself. Should use int64 or wrap internally. Test to see where it breaks
    std::array<int, GamelanizerConstants::maxLevels> levelWritePositions{};
    std::array<int, GamelanizerConstants::maxLevels> levelNoteSampleLengths{};
    std::array<int, GamelanizerConstants::maxLevels> levelAccumulatedSamples{};
    //==============================================================================
    std::array<int, GamelanizerConstants::maxLevels> powers{};
    //==============================================================================
    std::array<PhaseVocoder, GamelanizerConstants::maxLevels> pvs;
    //==============================================================================
    std::array<dsp::StateVariableFilter::Filter<float>, GamelanizerConstants::maxLevels> lpFilters;
    std::array<dsp::StateVariableFilter::Filter<float>, GamelanizerConstants::maxLevels> hpFilters;
    //==============================================================================
    GamelanizerParameters gamelanizerParameters;
    AudioProcessorValueTreeState audioProcessorValueTreeState;
    GamelanizerParametersVTSHelper gamelanizerParametersVtsHelper;
    //==============================================================================
    void processSamples(int64 numSamples, const float* monoDelayBufferWrite, float* dlyBufferData,
                        float** multiOutWrite, float** outputBufferWrite, bool skipProcessing);
    void processSample(int level, float sampleValue, bool skipProcessing);
    void addSamplesToOutputBuffer(int level, const float* samples, int nSamples);
    //==============================================================================
    void nextBeat();
    void moveWritePosOnBeatB();
    void fastForwardWriteHeadsToNextBeat();
    void updateLevelSampleWidths();
    /**
     * \brief calculates #samplesPerBeatFractional and resets the #beatSampleInfo
     *  This should not be called during playback!
     */
    void prepareSamplesPerBeat();
    //==============================================================================
    int getLatencyNeeded();
    void initDlyReadPos();
    void initWritePositions();
    //==============================================================================
    bool shouldDropThisNote(int level, int copyNumber);
    //==============================================================================
    void prepareFilters(double sampleRate, int samplesPerBlock);
    void snapFiltersToZero();
    //==============================================================================
    /**
     * \brief Checks if playback is starting from a sample other than 0 and corrects the internal state.
     * \return True if we should stop the processing loop early
     */
    bool handleTimelineStateChange();
    /**
     * \brief Checks if the host is not playing and cleans up the internal buffers if it is
     * \param cpi The hosts #CurrentPositionInfo
     * \return True if the host is not playing
     */
    bool handleNotPlaying(const AudioPlayHead::CurrentPositionInfo& cpi);
    /**
     * \brief Checks if the host has changed timeline positions without pausing and cleans up the internal buffers if it has
     * \param hostTimeInSamples The sample position that the host reports with #CurrentPositionInfo
     * \param hostIsPlayingLocal The cached value of #hostIsPlaying
     */
    void handleTimelineJump(const int64 hostTimeInSamples, bool hostIsPlayingLocal);
    /**
     * \brief Checks if the plugin is running as a standalone and also makes some sanity checks in debug mode.
     * \return True if this is running in standalone
     */
    bool handleStandaloneApp() const;

    /**
     * \brief Runs through the main processing loop to correct the internal state, without doing much work
     * \param hostTimeInSamples The sample position that the host reports with #CurrentPositionInfo
     */
    void simulateProcessing(int64 hostTimeInSamples);

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GamelanizerAudioProcessor)
};
