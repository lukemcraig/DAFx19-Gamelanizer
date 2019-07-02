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
    /**
     * \brief initialize the time and pitch shift settings of all the subdivision levels 
     */
    void initAllPhaseVocoders();

    /**
     * \brief queue the current pitch shift parameter for the subdivision level to change to when it's ready 
     * \param level the subdivision level
     */
    void queuePhaseVocoderNextParams(int level);
    //==============================================================================

    /**
     * \brief Called by the host before first playback and when the sample rate or HW buffer size changes.
     * Setup that depends on the sample rate is done here.
     */
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;

    /**
     * \brief Called by the host. Reaper seems to consistently call this on playback and timeline jumps but other hosts may not.
     * Reset the filters.
     */
    void reset() override;

    /**
     * \brief The main processing method that is called by the host with every block of audio data.
     */
    void processBlock(AudioBuffer<float>&, MidiBuffer&) override;

    //==============================================================================
    /**
     * \brief Thread safe way for the GUI to know if it should disable the BPM text field. 
     * \return 
     */
    bool getHostIsPlaying() const { return hostIsPlaying.load(); }

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
    //==============================================================================
    /**
     * \brief Saves the parameters and BPM into the session. Called by the host.
     */
    void getStateInformation(MemoryBlock& destData) override;

    /**
     * \brief Loads the previously stored parameters and BPM. Called by the host.
     */
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================   
    /**
     * \brief Called several times by the host to determine valid bus layouts.
     * \return True for a valid bus layout. 
     */
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    //==============================================================================   
    /**
     * \brief Creates an instance of the GUI every time the host wants to display it
     */
    AudioProcessorEditor* createEditor() override;
    //==============================================================================
    /// @cond BOILERPLATE
    /**
     * \brief Called by the host before #prepareToPlay when the sample rate changes. Unused.
     */
    void releaseResources() override { ; }
    const String getName() const override { return JucePlugin_Name; }
    bool hasEditor() const override { return true; }
    double getTailLengthSeconds() const override { return 0; }
    //==============================================================================
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    //==============================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override { ; }
    const String getProgramName(int) override { return {}; }
    void changeProgramName(int, const String&) override { ; }
    /// @endcond
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
    /**
     * \brief Half the sampling rate. Used for validating filter settings.
     */
    float nyquist{};
    /**
     * \brief The BPM set by the GUI. Is not allowed to changed during playback.
     */
    std::atomic<float> currentBpm{};
    /**
     * \brief The exact number of samples per beat.
     */
    double samplesPerBeatFractional{};
    /**
     * \brief Timeline-aware state information
     */
    BeatSampleInfo beatSampleInfo;
    //==============================================================================
    /**
     * \brief The internal count of the sample position of the host.
     * The host only reports this once per block so we have to calculate it ourselves.
     */
    int64 hostSampleOughtToBe{};
    /**
     * \brief Used to determine playback state changes.
     * Also used to prevent the GUI from changing the BPM during playback.
     */
    std::atomic<bool> hostIsPlaying{};

    //==============================================================================
    /**
     * \brief The circular buffer for delaying the base level.
     */
    struct BaseDelayBuffer
    {
        /**
         * \brief The actual buffer
         */
        AudioBuffer<float> data;
        /**
         * \brief The write position in the delay buffer.
         */
        int writePosition{};
        /**
         * \brief The read position in the base delay buffer.
         */
        int readPosition{};
    } baseDelayBuffer;

    //==============================================================================

    /**
     * \brief The buffer where the subdivision level outputs are overlapped and added in the correct positions.
     */
    struct SubdivisionLevelsOutputBuffer
    {
        /**
         * \brief The actual buffer
         */
        AudioBuffer<float> data;

        /**
         * \brief The read position of every channel in the buffer is the same.
         */
        int readPosition{};
    } levelsOutputBuffer;

    /**
     * \brief The lead write heads for each level.
     * TODO this does not wrap itself. Should probably use int64 or wrap internally. Test to see if it breaks
     */
    std::array<int, GamelanizerConstants::maxLevels> levelWritePositions{};
    /**
     * \brief The number of samples per subdivided note of each level.
     */
    std::array<int, GamelanizerConstants::maxLevels> levelNoteSampleLengths{};
    /**
     * \brief The number of samples written of each subdivision level before changing to the next beat.
     */
    std::array<int, GamelanizerConstants::maxLevels> levelAccumulatedSamples{};
    //==============================================================================
    /**
     * \brief Cached powers of 2 that are used repeatedly.
     */
    std::array<int, GamelanizerConstants::maxLevels> levelPowers{};
    //==============================================================================
    /**
     * \brief The Phase Vocoder instance of each subdivision level. Used for pitch shifting and time scaling.
     */
    std::array<PhaseVocoder, GamelanizerConstants::maxLevels> pvs;
    //==============================================================================
    /**
     * \brief Low pass filters for each subdivision level.
     */
    std::array<dsp::StateVariableFilter::Filter<float>, GamelanizerConstants::maxLevels> lpFilters;
    /**
     * \brief High pass filters for each subdivision level.
     */
    std::array<dsp::StateVariableFilter::Filter<float>, GamelanizerConstants::maxLevels> hpFilters;

    //==============================================================================
    /**
     * \brief Parameter IDs and layout
     */
    GamelanizerParameters gamelanizerParameters;
    /**
     * \brief Parameter states
     */
    AudioProcessorValueTreeState audioProcessorValueTreeState;
    /**
     * \brief Parameter interface
     */
    GamelanizerParametersVTSHelper gamelanizerParametersVtsHelper;


    //==============================================================================
    /**
     * \brief If the host is actually playing, process the samples given from processBlock
     * \param skipProcessing If true, skip intense processing in order to get the buffer position states correct
     */
    void processSamples(int64 numSamples, const float* baseDelayBufferWrite, float* dlyBufferData,
                        float** multiOutWrite, float** outputBufferWrite, bool skipProcessing);
    /**
     * \brief Process an individual sample.
     * \param level The subdivision level number.
     * \param sampleValue The audio data.
     * \param skipProcessing If true, skip intense processing in order to get the buffer position states correct
     */
    void processSample(int level, float sampleValue, bool skipProcessing);
    /**
     * \brief Overlap-and-add and duplicate the phase vocoded notes in the correct positions
     * \param level The subdivision level number.
     * \param samples The audio data.
     * \param nSamples The number of samples in the second parameter.
     */
    void addSamplesToLevelsOutputBuffer(int level, const float* samples, int nSamples);

    //==============================================================================
    /**
     * \brief The host timeline is on a beat boundary, so change the internal state to the next beat.
     */
    void nextBeat();

    /**
     * \brief Only call this method at the END of beat b 
     */
    void moveWritePosOnBeatB();

    /**
     * \brief Adjust #levelWritePositions to the correct starting positions of the new beat. 
     */
    void fastForwardWriteHeadsToNextBeat();

    //==============================================================================
    /**
     * \brief calculates #samplesPerBeatFractional and resets the #beatSampleInfo
     *  This should not be called during playback!
     */
    void prepareSamplesPerBeat();

    /**
     * \brief Set the #levelNoteSampleLengths according to #samplesPerBeatFractional
     */
    void initLevelNoteSampleLengths();

    //==============================================================================

    /**
     * \brief Different ways of calculating the latency/delay and initial write positions.
     * The practical differences between them are negligible however.
     */
    enum InitWriteHeadsAndLatencyMethod
    {
        /**
         * \brief The method from the paper.
         */
        threeBeats,

        /**
         * \brief This method has been tested the most.
         */
        earliestAWithC,

        /**
         * \brief This method uses the minimum number of samples.
         */
        earliestABeforeC
    };

    static constexpr InitWriteHeadsAndLatencyMethod initWriteHeadsAndLatencyMethod = earliestAWithC;

    /**
     * \brief calculate the latency/delay needed for Gamelanizer. 
     * \return the latency needed
     */
    int calculateLatencyNeeded();

    /**
     * \brief Set the BaseDelayBuffer.readPosition and request latency compensation from the host.
     */
    void initDlyReadPos();

    /**
     * \brief Set the #levelWritePositions for #levelsOutputBuffer
     */
    void initWritePositions();

    //==============================================================================
    /**
     * \brief Should this note be skipped over. Corresponds to the #dropButtons in the GUI
     * \param level the subdivision level number
     * \param copyNumber (the fifth note of the second subdivision level is the third copy of the first note, so this would be 2 (0 based indexing))
     * \return True if this note should be skipped
     */
    bool shouldDropThisNote(int level, int copyNumber);
    //==============================================================================
    /**
     * \brief Prepare the filters when #prepareToPlay is called
     * \param sampleRate the host sample rate
     * \param samplesPerBlock the maximum samples per block the host will give
     */
    void prepareFilters(double sampleRate, int samplesPerBlock);

    /**
     * \brief Prevent instability from limit cycles because the filter objects are processing a sample at a time and so do not handle this themselves.
     * \see http://www.wlxt.uestc.edu.cn/wlxt/ncourse/dsp/web/kj/chapter%209/9.4%20Limit%20Cycles%20in%20IIR%20Digital%20Filters.htm
     * \see https://forum.juce.com/t/dsp-snaptozero-need-help-to-understand/33033/2
     */
    void snapFiltersToZero();

    /**
     * \brief Update the low-pass filter coefficients. 
     * This method is called every sample but will only take effect every #filterUpdateRateInSamples 
     * and will not calculate anything if the cutoff frequency has not changed.
     * \param level The subdivision level
     */
    void updateLpFilter(int level);

    /**
     * \brief Update the high-pass filter coefficients. 
     * This method is called every sample but will only take effect every #filterUpdateRateInSamples 
     * and will not calculate anything if the cutoff frequency has not changed.
     * \param level The subdivision level
     */
    void updateHpFilter(int level);

    //==============================================================================
    /**
     * \brief Checks if playback is starting from a sample other than 0 and corrects the internal state.
     * \return True if we should stop the processing loop early
     */
    bool handleTimelineStateChange();

    /**
     * \brief Checks if the host is not playing and cleans up the internal buffers if it is
     * \param cpi The host's CurrentPositionInfo
     * \return True if the host is not playing
     */
    bool handleNotPlaying(const AudioPlayHead::CurrentPositionInfo& cpi);

    /**
     * \brief Checks if the host has changed timeline positions without pausing and cleans up the internal buffers if it has
     * \param hostTimeInSamples The sample position that the host reports with CurrentPositionInfo
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
