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
#include "GamelanizerConstants.h"

/**
 * \brief An implementation of the phase vocoder technique tailored for Gamelanizer.
 * Each subdivision level will have a corresponding instance of this class.
 */
class PhaseVocoder
{
public:
    PhaseVocoder();
    ~PhaseVocoder();

    /**
     * \brief Must be called before playback begins. Effective time scale factor will never change but pitch shift can.
     * \param initPitchShiftFactor (2 is an octave, 3/2 is a fifth, 4/3 is a fourth)
     * \param initEffectiveTimeScaleFactor Should be a power of a 2. (2 makes the output twice as fast as the input)
     */
    void initParams(float initPitchShiftFactor, float initEffectiveTimeScaleFactor);
    /**
	 * \brief Thread safe way to set the next pitch shift factor to be used. 
	 * This will take effect in immediately after processing a phase vocoder frame
	 * so that the actual time scale factor corresponds with a single resampling rate.
	 * If this is called again before that takes effect, the previous request will be 
	 * discarded and only the current one used.
	 * \param newNextPitchShiftFactorCents (2 is an octave, 3/2 is a fifth, 4/3 is a fourth)
	 */
    void queueParams(float newNextPitchShiftFactorCents);
    /**
     * \brief This should be called immediately after processing a phase vocoder frame, in order to set the new pitch shift factor.
     * If the pitch shift factor is the same, no processing is done.
     */
    void loadNextParams();
    /**
     * \brief Call this at the beginning of each new beat to reinitialize the phases.
     */
    void reset();
    /**
     * \brief Call this at the beginning of playback or if the timeline position jumps around.
     */
    void fullReset();

    /**
     * \brief Push a single sample onto the resampler queue and possibly process a frame if possible.
     * \param sampleValue The audio data
     * \param skipProcessing True if this is just being called to get the state of the array indexes where they should be 
     * \return 0 if no new data available. Hop size if a new frame is available on fftInOut.
     */
    int processSample(float sampleValue, bool skipProcessing);

    const float* getFftInOutReadPointer() const { return fftInOut; }
    static int getFftSize() { return fftSize; }
    //==============================================================================
private:
    /**
     * \brief The FFT order
     */
    static constexpr int fftOrder{10};
    /**
     * \brief The FFT size (2^fftOrder)
     */
    static constexpr int fftSize{1 << fftOrder};
    /**
     * \brief The number of complex bins for the real FFT
     */
    static constexpr int nComplexBins{(fftSize >> 1) + 1};

    //==============================================================================

    /**
     * \brief Data related to the analysis frame that comes out of the resampler and goes into the time-stretching process
     */
    struct AnalysisFrames
    {
        /**
        * \brief The number of analysis frames overlapping at one time.
        */
        static constexpr double analysisOverlapFactor{4.0};

        /**
         * \brief The number of samples to hop for the overlapping analysis frames (after resampling, before FFT).
         * TODO accumulated rounding errors?
         */
        static constexpr int analysisHopSize{static_cast<int>(fftSize / analysisOverlapFactor)};

        /**
         * \brief The circular buffer for the (overlapping) time-domain frames that the resampler outputs.
         * Its data will be unwrapped onto fftInOut every new hop.
         */
        std::array<float, fftSize> circularBuffer{};

        /**
         * \brief The write position on the circularBuffer
         */
        int writePosition{};

        /**
         * \brief Whether the circularBuffer has initially been filled with a full frame (rather than just a hop).
         */
        bool initialized{};
    } analysisFrames;

    //==============================================================================
    /**
     * \brief This length is based on calculateMaximumNeededNumSamples. 
     * TODO (It actually could be smaller if we knew the level number)
     */
    static constexpr int resamplerLength = (GamelanizerConstants::maxLevels << 2) * AnalysisFrames::analysisHopSize
        + (GamelanizerConstants::maxLevels << 2) * 4 + 1;

    //==============================================================================

    /**
     * \brief The effective time scale factor, how much faster the output is than the input.
     * Should be a power of two for Gamelanizer
     * todo make this const and assigned with the constructor
     */
    double effectiveTimeScaleFactor{};
    /**
     * \brief The actual time scale factor that is applied after resampling is performed for pitch shifting. 
     */
    double actualTimeScaleFactor{};

    /**
     * \brief The number of synthesis frames (after IFFT) overlapping frames at one.
     */
    double synthesisOverlapFactor{};

    /**
     * \brief The number of samples to hop for the overlapping synthesis frames (after IFFT).
     */
    double synthesisHopSize{};

    /**
    * \brief The current pitch shift factor used by the resampler
    */
    double pitchShiftFactor{};

    /**
    * \brief The current pitch shift factor in cents
    * We store it in cents so we can compare it with #nextPitchShiftFactorCents and avoid calling std::pow unnecessarily
    */
    double pitchShiftFactorCents{};

    /**
     * \brief The queued pitch shift factor, in cents, that is set by queueParams.
     */
    std::atomic<float> nextPitchShiftFactorCents{};


    //==============================================================================
    /**
     * \brief Data related to the resampler that is used for pitch shifting
     */
    struct Resampler
    {
        /**
        * \brief The actual interpolator instance to do pitch shifting before time stretching is done.
        */
        CatmullRomInterpolator interpolator;

        /**
        * \brief The maximum number of samples that the resampler could need in order to always output analysisHopSize samples.
        */
        int maxNeedSamples{};

        /**
         * \brief The FIFO queue of time domain data that's given as input to the resampler.
         */
        struct Queue
        {
            /**
             * \brief The actual data of the queue
             */
            std::array<float, resamplerLength> data{};

            /**
             * \brief The write position of the resamplerQueue
             */
            int writePosition{};
        } queue;

        /**
         * \brief The output of the resampler is written to this before being copied to fftQueue.
         * This is necessary because it might need to wrap around fftQueue, but the resampler class will not handle that
         * todo std::array?
         */
        float resamplerAnalysisHopBuffer[AnalysisFrames::analysisHopSize]{};
    } resampler;

    //==============================================================================

    /**
     * \brief The Fast Fourier Transform object.
     */
    dsp::FFT fft;

    /**
     * \brief The Hann Window and its #amplitudeCompensationScale factor
     */
    struct FftWindow
    {
        /**
         * \brief The Hann Window that's applied to time-domain frames
         *
         */
        std::array<float, fftSize> window{};

        /**
         * \brief the sum of the squared non-symmetric Hann window. Used to calculate #amplitudeCompensationScale
         */
        static constexpr double squaredWindowSum = fftSize * .375;

        /**
         * \brief Amplitude scaling factor based on the window
         */
        float amplitudeCompensationScale{};
    } fftWindow;

    /**
     * \brief The time-domain data and the complex frequency domain data will both be on here.
     * It has to be twice the fftSize in order to work with JUCE's FFT class. 
     * The time domain data will occupy the first half and the complex data will occupy the whole thing after FFTing.
     * reinterpret_cast is used on this. 
     */
    float fftInOut[2 * fftSize]{};

    /**
     * \brief Data related to the phases of the previous frame
     */
    struct PreviousFramePhases
    {
        /**
         * \brief The unaltered phases of the previous frame.
         * todo std::array?
         */
        float unaltered[nComplexBins]{};
        /**
         * \brief The scaled phases of the previous frame.
         * todo std::array?
         */
        float scaled[nComplexBins]{};
        /**
         * \brief False for the first frame of every beat
         */
        bool isInitialized{};
    } previousFramePhases;

    //==============================================================================
    void setParams(const float newPitchShiftFactor, const float newPitchShiftFactorCents);
    int calculateMaximumNeededNumSamples(int desiredNumOut, double oldPitchShiftFactor) const;
    void resampleHop();
    void popUsedSamples(int numUsed);
    void pushAnalysisHopOnToFftQueue();
    //==============================================================================
    void copyFftQueueToFftInOut();
    void scaleAllFrequencyBinsAndStorePhaseBuffers();
    std::complex<float> scaleFrequencyBin(int k, float mag, float currentPhase, float oldPhase);
    void storePhasesInBuffer();
    void scaleWhatsOnTheFftQueue();
    //==============================================================================
    static float complexBinPhase(const std::complex<float>& complexBin);
    static float complexBinMag(const std::complex<float>& complexBin);
    static float calculateFrequencyDeviation(float oldPhase, float currentPhase, int k);
    static float wrapPhase(float phaseIn);
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhaseVocoder)
};
