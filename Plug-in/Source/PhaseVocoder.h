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
#include "PvResampler.h"
#include "StatefulRoundedNumber.h"

/** \addtogroup Core
 *  @{
 */

/**
 * \brief An implementation of the phase vocoder technique tailored for Gamelanizer.
 * Each subdivision level will have a corresponding instance of this class.
 */
class PhaseVocoder
{
public:
    PhaseVocoder(int levelNumber, float effectiveTimeScaleFactor);

    PhaseVocoder(const PhaseVocoder&) = delete;

    PhaseVocoder& operator=(const PhaseVocoder&) = delete;

    PhaseVocoder(PhaseVocoder&&) = delete;

    PhaseVocoder& operator=(PhaseVocoder&&) = delete;

    ~PhaseVocoder() = default;

    /**
     * \brief Must be called before playback begins. Effective time scale factor will never change but pitch shift can.
     * \param initPitchShiftFactor (2 is an octave, 3/2 is a fifth, 4/3 is a fourth)
     */
    void initParams(float initPitchShiftFactor);

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
    void resetBetweenBeats();

    /**
     * \brief Call this at the beginning of playback or if the timeline position jumps around.
     */
    void fullReset();

    /**
     * \brief Push a single sample onto the resampler inputQueue and resample a hop and process a frame if possible.
     * \param sampleValue The audio data   
     * \return 0 if no new data available. Hop size if a new frame is available on inOut.
     */
    int processSample(float sampleValue);

    [[nodiscard]] const float* getFftInOutReadPointer() const { return fft.inOut.data(); }

    static int getFftSize() { return fftSize; }
    //==============================================================================
private:
    /**
     * \brief The FFT order
     */
    static constexpr int fftOrder{10};
    /**
     * \brief The FFT size \f[N\f].
     * 
     */
    static constexpr int fftSize{1 << fftOrder};
    /**
     * \brief The number of complex bins for the real FFT
     */
    static constexpr int nComplexBins{(fftSize >> 1) + 1};

    //==============================================================================

    /**
     * \brief Data related to the analysis frame, which comes out of the resampler and goes into the time-stretching process.
     */
    struct AnalysisFrames
    {
        explicit AnalysisFrames(int level);

        /**
         * \brief Flag the frame buffer as uninitialized and discard its data
         */
        void reset()
        {
            initialized = false;
            writePosition = 0;
        }

        /**
         * \brief The circular buffer for the (overlapping) time-domain frames that the resampler outputs.
         * Its data will be unwrapped onto inOut every new hop.
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

    private:
        /**
        * \brief The number of analysis frames overlapping at one time, \f$o_a\f$.
        * The 4th subdivision level does not need more than 4. 
        * The 1st subdivision level needs 16 to sound smooth at 4800 cents but only 4 for less than 1200 cents.
        * 
        */
        const double analysisOverlapFactor;

    public:
        /**
         * \brief The number of samples to hop for the overlapping analysis frames (after resampling, before FFT).
         * \f[h_a=\frac{N}{o_a}\f]
         */
        const int analysisHopSize;

        /**
        * \brief The actual overlap factor used due to the rounding of #analysisHopSize.
        * This probably won't actually matter because the fft size should divide evenly by the overlap factor
        */
        const float analysisOverlapFactorActual;
    } analysisFrames;

    //==============================================================================

    /**
     * \brief The effective time scale factor: how much faster the output is than the input.
     * Should be a power of two for Gamelanizer (2 makes the output twice as fast as the input)
     * \f[r[i]=\frac{1}{2^i}\f]
     */
    const double effectiveTimeScaleFactor;
    /**
     * \brief The actual time scale factor that is applied after resampling is performed for pitch shifting. 
     * \f[v[i]=r[i] p[i]\f]
     */
    double actualTimeScaleFactor{};

    /**
     * \brief The number of synthesis frames (after IFFT) overlapping frames at one.
     * \f[o_s[i]=\frac{o_a}{v[i]}\f]
     */
    double synthesisOverlapFactor{};

    /**
     * \brief The number of samples to hop for the overlapping synthesis frames (after IFFT).
     * \f[h_s[i]=h_a v[i]\f]
     */
    StatefulRoundedNumber synthesisHopSize{StatefulRoundedNumber::roundDown};

    /**
    * \brief The current pitch shift factor used by the resampler
    *  \f[p[i]=2^{\frac{c[i]}{1200}}\f]
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

    PvResampler resampler;

    /**
     * \brief The FFT object and related data
     */
    struct FftStruct
    {
        /**
         * \brief The Fast Fourier Transform object.
         */
        dsp::FFT instance{fftOrder};

        /**
         * \brief The time-domain data and the complex frequency domain data will both be on here.
         * It has to be twice the fftSize in order to work with JUCE's FFT class. 
         * The time domain data will occupy the first half and the complex data will occupy 
         * the whole thing after forward transforming.
         * reinterpret_cast is used on this. 
         */
        std::array<float, 2 * fftSize> inOut{};

        //float inOut[2 * fftSize]{};

        /**
         * \brief The Hann Window and its #amplitudeCompensationScale factor
         */
        struct FftWindow
        {
            /**
             * \brief The Hann Window that's applied to time-domain frames
             *
             */
            std::array<float, fftSize> data{};

            /**
             * \brief the sum of the squared non-symmetric Hann window. Used to calculate #amplitudeCompensationScale
             */
            static constexpr double squaredWindowSum = fftSize * .375;

            /**
             * \brief Amplitude scaling factor based on the window
             */
            float amplitudeCompensationScale{};
        } window;
    } fft;

    /**
     * \brief Data related to the phases of the previous frame
     */
    struct PreviousFramePhases
    {
        /**
         * \brief The unaltered phases of the previous frame.
         */
        std::array<float, nComplexBins> unaltered{};

        /**
         * \brief The scaled phases of the previous frame.
         */
        std::array<float, nComplexBins> scaled{};

        /**
         * \brief False for the first frame of every beat
         */
        bool initialized{};
    } previousFramePhases;

    //==============================================================================
    /**
     * \brief Set new pitch shift factor and related member variables
     * \param newPitchShiftFactor 
     * \param newPitchShiftFactorCents 
     */
    void setParams(float newPitchShiftFactor, float newPitchShiftFactorCents);

    //==============================================================================

    void pushResampledHopOnToAnalysisFrameBuffer();

    //==============================================================================
    void scaleAnalysisFrame();

    void copyAnalysisFrameToFftInOut();

    void scaleAllFrequencyBinsAndStorePhaseBuffers();

    std::complex<float> scaleFrequencyBin(int k, float mag, float currentPhase, float oldPhase);

    /**
     * \brief if this is the first frame we've processed of this beat, just store the phases without scaling them
     */
    void storePhasesInBuffer();

    //==============================================================================
    /**
     * \brief Calculate the phase of a complex frequency bin
     * \return The phase
     */
    static float complexBinPhase(std::complex<float> complexBin);

    /**
     * \brief Calculate the magnitude of a complex frequency bin
     * \return The magnitude
     */
    static float complexBinMag(std::complex<float> complexBin);

    static float calculateFrequencyDeviation(float oldPhase, float currentPhase, int k,
                                             float analysisOverlapFactor, float analysisHopSize);

    static float wrapPhase(float phaseIn);

    //==============================================================================
    JUCE_LEAK_DETECTOR(PhaseVocoder)
};

/** @}*/
