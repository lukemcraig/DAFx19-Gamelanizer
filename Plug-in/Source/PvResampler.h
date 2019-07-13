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

/** \addtogroup Core
 *  @{
 */

/**
* \brief Data related to the resampler that is used for pitch shifting
*/
class PvResampler
{
public:
    explicit PvResampler(int analysisHopSize);

    PvResampler(const PvResampler&) = delete;

    PvResampler& operator=(const PvResampler&) = delete;

    PvResampler(PvResampler&&) = delete;

    PvResampler& operator=(PvResampler&&) = delete;

    ~PvResampler() = default;

    //==============================================================================   

    void resetBetweenBeats();

    void fullReset();

    //==============================================================================   

    void updatePitchShiftFactor(double newPitchShiftFactor);

    //==============================================================================   

    void pushSample(float sampleValue);

    bool resampleHopToAnalysisHopBufferIfReady(double pitchShiftFactor);

    [[nodiscard]] std::vector<float> getAnalysisHopBuffer() const;

private:
    /**
    * \brief The actual interpolator instance to do pitch shifting before time stretching is done.
    */
    CatmullRomInterpolator interpolator;

    /**
    * \brief The maximum number of samples that the resampler could need in order to always output analysisHopSize samples.
    */
    int maxNeedSamples{};

    double currentPitchShiftFactor{16.0};

    /**
     * \brief Class for the FIFO queue of time domain data that's given as input to the resampler.
     */
    class Queue
    {
    public:
        explicit Queue(int resamplerLength);

        void popUsedSamples(int numUsed);

        void reset();

        /**
         * \brief The actual data of the queue
         */
        std::vector<float> data;

        /**
         * \brief The write position of the resamplerQueue
         */
        int writePosition{};
    };

    /**
     * \brief The FIFO queue of time domain data that's given as input to the resampler.
     */
    Queue inputQueue;

    /**
     * \brief The output of the resampler is written to this before being copied to PhaseVocoder::AnalysisFrames::circularBuffer.
     * This is necessary because it might need to wrap around PhaseVocoder::AnalysisFrames::circularBuffer
     * but the resampler class will not handle that.
     */
    std::vector<float> analysisHopBuffer;

    /**
     * \brief Calculate the maximum number of samples the resampler might need to produce desiredNumOut
     * \param desiredNumOut the analysis hop size
     * \param newPitchShiftFactor the new pitch shift factor
     * \param oldPitchShiftFactor the previous pitch shift factor
     * \return The maximum number of samples the resampler might need
     */
    static int calculateMaxNeededSamples(int desiredNumOut,
                                         double newPitchShiftFactor,
                                         double oldPitchShiftFactor);

    [[nodiscard]] bool readyToResampleHop() const;

    JUCE_LEAK_DETECTOR(PvResampler)
};

/** @}*/
