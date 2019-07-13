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
#include "PhaseVocoder.h"
#include "BeatSampleInfo.h"
#include "GamelanizerParametersVTSHelper.h"
#include "SubdivisionLevelsOutputBuffer.h"

/** \addtogroup Core
 *  @{
 */

/**
 * \brief A class representing a subdivision level processor.
 */
class SubdivisionLevel
{
public:
    /**
     * \brief Creates a subdivision level.
     * \param levelNumber 0 based index
     * \param bsi a reference to GamelanizerAudioProcessor::beatSampleInfo
     * \param gpvh a reference to GamelanizerAudioProcessor::gamelanizerParametersVtsHelper
     * \param lob a reference to GamelanizerAudioProcessor::levelsOutputBuffer
     * \param hostSampleRate a reference to GamelanizerAudioProcessor::hostSampleRate
     */
    SubdivisionLevel(int levelNumber,
                     BeatSampleInfo& bsi,
                     GamelanizerParametersVtsHelper& gpvh,
                     SubdivisionLevelsOutputBuffer& lob,
                     double& hostSampleRate);

    SubdivisionLevel(const SubdivisionLevel&) = delete;

    SubdivisionLevel& operator=(const SubdivisionLevel&) = delete;

    SubdivisionLevel(SubdivisionLevel&&) = delete;

    SubdivisionLevel& operator=(SubdivisionLevel&&) = delete;

    ~SubdivisionLevel() = default;

    //==============================================================================

    /**
     * \brief Pass a sample to the #pv and add a new synthesis frame to this level's output buffer if it's ready.
     * \param sampleValue The sample date
     */
    void processSample(float sampleValue);

    /**
     * \brief Push 0s to all of the PVs until they process whatever extra data they have. 
     */
    void processFinalHop();

    /**
     * \brief Should this note be skipped over. Corresponds to the GamelanizerAudioProcessorEditor::dropButtons
     * \param copyNumber (the fifth note of the second subdivision level is the third copy of the first note, so this would be 2 (0 based indexing))
     * \return True if this note should be skipped
     */
    [[nodiscard]] bool shouldDropThisNote(int copyNumber) const;

    /**
     * \brief Only call this method at the END of beat b 
     * \f[{w}[i] \leftarrow {w}[i] + {s}[i](2^{i+1}-2)\f]
     */
    void moveWritePosOnBeatB();

    /**
     * \brief Adjust writePosition to the correct starting positions of the new beat.
     * \f[{w}[i] \leftarrow {w}[i]+({s}[i]-{\Delta w}[i])\f]
     */
    void fastForwardWriteHeadsToNextBeat();

    /**
     * \brief Reset the phase vocoder and state
     */
    void fullReset();

    //==============================================================================

    /**
     * \brief Update the high and low-pass filter coefficients.
     * This method is called every sample but will only take effect every GamelanizerParametersVtsHelper::filterUpdateRateInSamples
     * and will not calculate anything if the cutoff frequency has not changed.
     */
    void updateFilters();

    /**
     * \brief Prepare the filters when GamelanizerAudioProcessor::prepareToPlay is called
     * \param samplesPerBlock the maximum samples per block the host will give
     */
    void prepareFilters(int samplesPerBlock);

    /**
     * \brief Reset the high-pass and low-pass filters.
     */
    void resetFilters();

    /**
     * \brief Prevent instability from limit cycles because the filter objects are processing a sample at a time and so do not handle this themselves.
     * \see http://www.wlxt.uestc.edu.cn/wlxt/ncourse/dsp/web/kj/chapter%209/9.4%20Limit%20Cycles%20in%20IIR%20Digital%20Filters.htm
     * \see https://forum.juce.com/t/dsp-snaptozero-need-help-to-understand/33033/2
     */
    void snapFiltersToZero();

    //==============================================================================
    /**
     * \brief initialize the time and pitch shift settings of the PV
     */
    void preparePhaseVocoder();

    /**
     * \brief queue the pitch shift parameter for the PV to change to when it's ready 
     */
    void queuePhaseVocoderNextParams();

private:
    /**
     * \brief \f[i\f] 0 indexed in code. 1 indexed in the paper and formulas.
     */
    const int levelNumber;

    /**
     * \brief \f[2^{i}\f] cached because it is used repeatedly
     */
    const int powerOfTwo;

    /**
     * \brief \f[2^{i+1}-2\f]
     */
    const int numberOfNotesToJumpOver;

    //==============================================================================
public:
    /**
     * \brief The Phase Vocoder instance of this subdivision level. Used for pitch shifting and time scaling.
     */
    PhaseVocoder pv;

    //==============================================================================

    /**
     * \brief The lead write head for this level.
     * \f[w[i]\f]
     */
    int writePosition{};

    /**
     * \brief The number of samples per subdivided note of this level.
     */
    int noteLengthInSamples{};

    /**
     * \brief The exact number of samples per subdivided note of this level.
     * \f[s[i]=\frac{s_b}{2^i}\f]
     */
    double noteLengthInSamplesFractional{};

    //==============================================================================

    /**
     * \brief Low pass filters for this subdivision level.
     */
    dsp::StateVariableFilter::Filter<float> lpFilter;

    /**
     * \brief High pass filters for this subdivision level.
     */
    dsp::StateVariableFilter::Filter<float> hpFilter;

private:

    /**
     * \brief The number of samples written of this subdivision level before changing to the next beat.
     * \f[\Delta w[i]\f]
     */
    int accumulatedSamples{};

    //==============================================================================    
    /**
     * \brief Reference to GamelanizerAudioProcessor::beatSampleInfo
     */
    BeatSampleInfo& beatSampleInfo;

    /**
     * \brief Reference to GamelanizerAudioProcessor::gamelanizerParametersVtsHelper
     */
    GamelanizerParametersVtsHelper& gamelanizerParametersVtsHelper;

    /**
     * \brief Reference to GamelanizerAudioProcessor::levelsOutputBuffer
     */
    SubdivisionLevelsOutputBuffer& levelsOutputBuffer;

    /**
     * \brief Reference to GamelanizerAudioProcessor::hostSampleRate
     */
    double& hostSampleRate;

    //==============================================================================   

    /**
     * \brief Overlap-and-add and duplicate the phase vocoded notes in the correct positions.
     * (Algorithm 1 in the paper)
     * \param samples The audio data.
     * \param nSamples The number of samples in the first parameter.
     */
    void addSamplesToLevelsOutputBuffer(const float* samples, int nSamples) const;

    /**
     * \brief \f[w[i] \leftarrow w[i]+h_s[i]\f]
     * \f[\Delta w[i] \leftarrow \Delta w[i]+h_s[i]\f]
     * \param hop the synthesis hop size from PhaseVocoder::synthesisHopSize
     */
    void moveWriteHeadOneHop(int hop);

    /**
     * \brief Adjust the lead write head position for the circular buffer
     */
    void wrapLevelWritePosition();

    /**
     * \brief Helper function for construction of #numberOfNotesToJumpOver .
     * \param levelNumber The level number (0 indexed)
     * \return The number of notes to jump over
     */
    static int calculateNumberOfNotesToJumpOver(int levelNumber);

    //==============================================================================    
    JUCE_LEAK_DETECTOR(SubdivisionLevel)
};

/** @}*/
