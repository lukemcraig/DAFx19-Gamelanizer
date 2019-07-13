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
#include "GamelanizerParameters.h"
#include "GamelanizerConstants.h"
#include "GamelanizerParametersVTSHelper.h"
#include "BeatSampleInfo.h"
#include "SubdivisionLevel.h"
#include "SubdivisionLevelsOutputBuffer.h"
#if MeasurePerformance
#include "PerformanceMeasures.h"
#endif

/** \addtogroup Core
 *  @{
 */

/**
 * \brief The main class for the plug-in
 */
class GamelanizerAudioProcessor final : public AudioProcessor
{
public:
    //==============================================================================
    /**
     * \brief Constructor.
     */
    GamelanizerAudioProcessor();

    /**
     * \brief Non-copyable.
     */
    GamelanizerAudioProcessor(const GamelanizerAudioProcessor&) = delete;

    /**
     * \brief Non-copyable.
     */
    GamelanizerAudioProcessor& operator=(const GamelanizerAudioProcessor&) = delete;

    /**
     * \brief Non-movable.
     */
    GamelanizerAudioProcessor(GamelanizerAudioProcessor&&) = delete;

    /**
     * \brief Non-movable.
     */
    GamelanizerAudioProcessor& operator=(GamelanizerAudioProcessor&&) = delete;

    /**
     * \brief Default destructor.
     */
    ~GamelanizerAudioProcessor() = default;

    //==============================================================================

    /**
     * \brief Called by the host before first playback and when its sample rate or HW buffer size changes.
     * 
     * Setup that depends on the sample rate is done here.
     */
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;

    /**
     * \brief Called by the host. Reaper seems to consistently call this on playback and timeline jumps 
     * but other hosts may not.
     * 
     * Resets the filters.
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
    bool getPreventGuiBpmChange() const { return preventGuiBpmChange.load(); }

    /**
     * \brief Thread safe way for the GUI to display the processor's BPM.
     * \return 
     */
    float getCurrentBpm() const { return currentBpm.load(); }

    /**
     * \brief Thread safe way for the GUI to change the processor's BPM. 
     * \warning This should not be called during playback!
     * \param newBpm the new tempo in beats per minute
     */
    void setCurrentBpm(float newBpm);

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

private:
    //==============================================================================
#if MeasurePerformance
    PerformanceMeasures performanceMeasures;
#endif
    //==============================================================================
    /**
     * \brief The sample rate that the host reports in prepareToPlay().
     * \f[f_s\f]
     *  
     * Used for #samplesPerBeatFractional and the SubdivisionLevel::lpFilter and SubdivisionLevel::hpFilter.
     */
    double hostSampleRate{};

    /**
     * \brief The BPM set by the GUI. Is not allowed to changed during playback.
     * \f[t\f]
     */
    std::atomic<float> currentBpm{};

    /**
     * \brief The exact number of samples per beat.
     * \f[s_b=\frac{60 f_s}{t}\f]
     */
    double samplesPerBeatFractional{};

    /**
     * \brief Timeline-aware state information
     */
    BeatSampleInfo beatSampleInfo;

    //==============================================================================

    /**
     * \brief The internal count of the sample position of the host.
     * 
     * The host only reports this once per block so we have to calculate it ourselves.
     */
    int64 hostSampleOughtToBe{};

    /**
     * \brief Used to determine playback state changes.
     */
    bool hostIsPlaying{};

    /**
     * \brief Used to prevent the GUI from changing the BPM during playback.
     */
    std::atomic<bool> preventGuiBpmChange{};

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
     * \brief Parameter IDs and layout
     */
    GamelanizerParameters gamelanizerParameters{};
    /**
     * \brief Parameter states
     */
    AudioProcessorValueTreeState audioProcessorValueTreeState;
    /**
     * \brief Parameter interface
     */
    GamelanizerParametersVtsHelper gamelanizerParametersVtsHelper;

    //==============================================================================

    /**
     * \brief The buffer where the subdivision level outputs are overlapped and added in the correct positions.
     * \f[B\f] 
     */
    SubdivisionLevelsOutputBuffer levelsOutputBuffer;

    /**
     * \brief The subdivision level processors
     */
    std::array<SubdivisionLevel, GamelanizerConstants::maxLevels> subdivisionLevels{
        {
            {0, beatSampleInfo, gamelanizerParametersVtsHelper, levelsOutputBuffer, hostSampleRate},
            {1, beatSampleInfo, gamelanizerParametersVtsHelper, levelsOutputBuffer, hostSampleRate},
            {2, beatSampleInfo, gamelanizerParametersVtsHelper, levelsOutputBuffer, hostSampleRate},
            {3, beatSampleInfo, gamelanizerParametersVtsHelper, levelsOutputBuffer, hostSampleRate}
        }
    };

    //==============================================================================

    /**
     * \brief If the host is actually playing, process the samples given from processBlock
     * \param numSamples The number of samples to process
     * \param monoInputRead A read only pointer to the incoming audio block (mono)
     * \param multiOutWrite Write pointers to the outgoing audio block (stereo + 5 mono)
     * \param baseDelayBufferReadWrite A write pointer to BaseDelayBuffer::data
     * \param levelsBufferReadWrite Write pointers to #SubdivisionLevelsOutputBuffer::data
     * \param skipProcessing If true, skip intense processing in order to get the buffer position states correct
     */
    void processSamples(int64 numSamples, const float* monoInputRead, float** multiOutWrite,
                        float* baseDelayBufferReadWrite, float** levelsBufferReadWrite, bool skipProcessing);

    //==============================================================================
    /**
     * \brief The host timeline is on a beat boundary, so change the internal state to the next beat.
     */
    void nextBeat();

    //==============================================================================
    /**
     * \brief calculates #samplesPerBeatFractional and resets #beatSampleInfo.
     * 
     *  This should not be called during playback!
     */
    void prepareSamplesPerBeat();

    /**
     * \brief Set all the SubdivisionLevel::noteLengthInSamples according to #samplesPerBeatFractional.
     */
    void initLevelNoteSampleLengths();

    //==============================================================================

    /**
     * \brief Different ways of calculating the delay/latency compensation and initial write positions.
     * 
     * The practical differences between them are negligible however.
     */
    enum InitWriteHeadsAndLatencyMethod
    {
        /**
         * \brief The methods from the paper.
         * \f[w[i]=2 s_b + s[M] + \sum_{j=i+1}^{M}s[j]\f]
         * \f[l=3s_b\f]
         */
        threeBeats,

        /**
         * \brief These methods have been tested the most.
         * \f[w[i]=2 s_b + \sum_{j=i+1}^{M}s[j]\f]
         * \f[l=2 s_b + \sum_{j=1}^{M}s[j]\f]
         */
        earliestAWithC,

        /**
         * \brief These methods use the minimum number of samples.
         * \f[w[i]=2 s_b + s[M] - \sum_{j=i+1}^{M}s[j]\f]
         * \f[l=2 s_b + \sum_{j=1}^{M-1}s[j]\f]
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
     * \brief Set the BaseDelayBuffer::readPosition and request latency compensation from the host.
     */
    void initDlyReadPos();

    /**
     * \brief Set the SubdivisionLevel::writePosition for #levelsOutputBuffer
     */
    void initWritePositions();

    //==============================================================================

    /**
     * \brief Checks if playback is starting from a sample other than 0 and corrects the internal state.
     * \return True if we should stop the processing loop early
     */
    bool handleTimelineStateChange();

    /**
     * \brief Checks if the host is not playing and cleans up the internal buffers if it is.
     * \param cpi The host's CurrentPositionInfo
     * \return True if the host is not playing
     * \see https://docs.juce.com/master/classAudioPlayHead.html#ae8ff79b6ec79fbecb1e8276ad9867cd2
     */
    bool handleNotPlaying(const AudioPlayHead::CurrentPositionInfo& cpi);

    /**
     * \brief Checks if the host has changed timeline positions without pausing and cleans up the internal buffers if it has.
     * \param hostTimeInSamples The sample position that the host reports with CurrentPositionInfo
     * \see https://docs.juce.com/master/classAudioPlayHead.html#ae8ff79b6ec79fbecb1e8276ad9867cd2
     */
    void handleTimelineJump(int64 hostTimeInSamples);

    /**
     * \brief Checks if the plugin is running as a standalone and also makes some sanity checks in debug mode.
     * \return True if this is running in standalone
     */
    bool handleStandaloneApp() const;

    /**
     * \brief Runs through the main processing loop to correct the internal state, without doing much work.
     * \param hostTimeInSamples The sample position that the host reports with CurrentPositionInfo
     * \see https://docs.juce.com/master/classAudioPlayHead.html#ae8ff79b6ec79fbecb1e8276ad9867cd2
     */
    void simulateProcessing(int64 hostTimeInSamples);

    //==============================================================================
    JUCE_LEAK_DETECTOR(GamelanizerAudioProcessor)

    //==============================================================================
public:
    /// @cond BOILERPLATE
    /**
     * \brief Called by the host before prepareToPlay() when the sample rate changes. 
     * Unused.
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
};

/** @}*/
