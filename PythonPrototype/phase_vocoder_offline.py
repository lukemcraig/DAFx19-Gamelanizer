from __future__ import division

import fractions

import matplotlib.colors as colors
import matplotlib.pyplot as plt
import numpy as np
import scipy.signal


class PhaseVocoder:
    def __init__(self, time_scale_factor, pitch_shift_factor, analysis_window_size, analysis_overlap_factor,
                 sample_rate):
        self.window = scipy.signal.hann(M=analysis_window_size, sym=False)

        # zero padding the window
        # self.window = np.pad(self.window, int(len(self.window) * 1.5), 'constant', constant_values=0)
        pitch_shift_factor_ints = fractions.Fraction(pitch_shift_factor).limit_denominator(1000)
        self.pitch_shift_factor_down = pitch_shift_factor_ints.denominator
        self.pitch_shift_factor_up = pitch_shift_factor_ints.numerator
        self.time_scale_factor = pitch_shift_factor / time_scale_factor
        print('time_scale_factor:', self.time_scale_factor)
        self.analysis_window_size = len(self.window)

        self.analysis_overlap_factor = analysis_overlap_factor
        self.synthesis_overlap_factor = analysis_overlap_factor / self.time_scale_factor

        self.analysis_hop_size = int(self.analysis_window_size / analysis_overlap_factor)
        self.synthesis_hop_size = self.analysis_hop_size * self.time_scale_factor

        # Add 1 for the zero frequency bin
        self.n_bins = int(self.analysis_window_size / 2) + 1

        self.stft_function = self.__my_stft
        self.istft_function = self.__my_istft
        self.two_pi = 2 * np.pi

        # makes a big difference with robotization, but no apparent difference with time stretching
        self.fft_shift = True

        # Only used for plotting
        self.sample_rate = sample_rate
        return

    def __get_frame(self, audio_data, frame_index):
        start_sample = frame_index * self.analysis_hop_size
        end_sample = start_sample + self.analysis_window_size
        frame_data = audio_data[start_sample:end_sample]
        return frame_data, start_sample, end_sample

    def __get_windowed_frame(self, audio_data, frame_index):
        frame_data, start_sample, end_sample = self.__get_frame(audio_data, frame_index)
        if len(frame_data) < self.analysis_window_size:
            frame_data = np.pad(array=frame_data, pad_width=(0, self.analysis_window_size - len(frame_data)),
                                mode='constant')
        windowed_frame_data = frame_data * self.window
        # print("DBG: not windowing!")
        # windowed_frame_data = frame_data
        return windowed_frame_data, start_sample, end_sample

    def __get_stft(self, audio_data):
        """
        Short-term-fourier-transform
        :return: a 2D array where each row is a frame and each column is a frequency bin
        """
        # stft = self.__scipy_stft(audio_data)
        # stft = self.__my_stft(audio_data)
        stft = self.stft_function(audio_data)
        return stft

    def __get_istft(self, stft):
        """
        Inverse short-term-fourier-transform
        :return: a 1D array of the re-synthesized time domain audio data
        """
        # istft = self.__scipy_istft(stft)
        # istft = self.__my_istft(stft)
        istft = self.istft_function(stft)
        return istft

    def __my_stft(self, audio_data):
        padding_amount = ((np.ceil(len(audio_data) / self.analysis_window_size)) * self.analysis_window_size) - len(
            audio_data)
        zero_padded_audio = np.pad(audio_data, int(padding_amount * 0.5), 'constant', constant_values=0)

        # number_of_frames = int(np.ceil(len(audio_data) / self.analysis_hop_size)) + 1
        number_of_frames = int(len(zero_padded_audio) / self.analysis_hop_size)
        stft = np.zeros(shape=(self.n_bins, number_of_frames), dtype=np.complex128)
        for frame_index in range(0, number_of_frames):
            windowed_frame_data, start_sample, end_sample = self.__get_windowed_frame(zero_padded_audio, frame_index)
            if self.fft_shift:
                windowed_frame_data = np.fft.fftshift(windowed_frame_data)
            frame_fft = np.fft.rfft(windowed_frame_data, n=self.analysis_window_size, norm=None)
            stft[:, frame_index] = frame_fft
        return stft

    def __my_istft(self, stft):
        """

        """
        number_of_frames = stft.shape[1]
        # we only need as many columns as there are frames to overlap
        number_of_overlaping_frames = int(np.ceil(self.synthesis_overlap_factor))
        isft = np.zeros(shape=(
            int(np.ceil(number_of_frames * self.synthesis_hop_size) + (
                    self.analysis_window_size - self.synthesis_hop_size)),
            number_of_overlaping_frames))
        for frame_index in range(0, number_of_frames):
            start_sample = int(frame_index * self.synthesis_hop_size)
            end_sample = start_sample + self.analysis_window_size

            frame_ifft = np.fft.irfft(stft[:, frame_index], n=self.analysis_window_size, norm=None)
            if self.fft_shift:
                frame_ifft = np.fft.ifftshift(frame_ifft)
            # windowing on inverse stft
            frame_ifft *= self.window
            # amplitude compensation can happen on each frame or after summing. The result is the same
            compensated_frame_ifft = self.__amplitude_compensation(frame_ifft)
            isft[start_sample:end_sample, frame_index % number_of_overlaping_frames] = compensated_frame_ifft

        isft = isft.sum(axis=1)[int(self.analysis_window_size / 2):]

        return isft

    def __scipy_stft(self, audio_data):
        """

        """
        noverlap = self.analysis_window_size - self.analysis_hop_size
        # this is different than the default window
        f, t, stft = scipy.signal.stft(x=audio_data, nperseg=self.analysis_window_size, noverlap=noverlap,
                                       window=self.window)
        return stft

    def __scipy_istft(self, stft):
        """
        Unused for now
        """
        noverlap = self.analysis_window_size - self.synthesis_hop_size
        t, isft = scipy.signal.istft(stft, nperseg=self.analysis_window_size, noverlap=noverlap, window=self.window)
        return isft

    def __amplitude_compensation(self, frame_ifft):
        """
        References
        ----------
        .. [1] Pedersen, M. E. H.. "The Phase-Vocoder and its Realization." . 2003
                https://pdfs.semanticscholar.org/a73c/827cbaf9e0fc736e343724ad7037dcca1500.pdf
        """
        squared_window = np.square(self.window)
        summation = np.sum(squared_window)
        scale = summation / self.synthesis_hop_size
        frame_ifft /= scale
        return frame_ifft

    @staticmethod
    def __wrap_phase(phase_in):
        phase_out = ((phase_in + np.pi) % (-2.0 * np.pi)) + np.pi
        return phase_out

    def __adjust_phases_for_this_frequency_bin_across_all_frames(self, phases, w, k):
        """
        References
        ----------
        .. [1] Mark Dolson. The Phase Vocoder: A Tutorial. Computer Music Journal, 10(4):14-27, 1986.
        .. [2] Reiss, J.D. and McPherson, A. Audio Effects: Theory, Implementation and Application. 191-207 2014.
        .. [3] Pedersen, M. E. H.. "The Phase-Vocoder and its Realization." . 2003
                https://pdfs.semanticscholar.org/a73c/827cbaf9e0fc736e343724ad7037dcca1500.pdf
        """
        new_phases = phases.copy()
        # --------------------------------------------------------------------------------------------------------------
        for j in range(1, len(phases)):
            previous_phase = (phases[j - 1])
            current_phase = (phases[j])

            frequency_deviation = self.__calculate_frequency_deviation(previous_phase, current_phase, k)

            true_analysis_frequency = w + frequency_deviation
            true_analysis_bin = true_analysis_frequency * (self.analysis_window_size / self.two_pi)

            previous_new_phase = new_phases[j - 1]
            scaled_phase = true_analysis_bin * (self.two_pi / self.synthesis_overlap_factor) + previous_new_phase
            wrapped_scaled_phase = self.__wrap_phase(scaled_phase)
            new_phases[j] = wrapped_scaled_phase
        # --------------------------------------------------------------------------------------------------------------
        return new_phases

    def __calculate_frequency_deviation(self, previous_phase, current_phase, k):
        """
        References
        ----------
        .. [1] Pedersen, M. E. H.. "The Phase-Vocoder and its Realization." . 2003
                https://pdfs.semanticscholar.org/a73c/827cbaf9e0fc736e343724ad7037dcca1500.pdf
        """
        frequency_contribution = ((self.two_pi * k) / self.analysis_overlap_factor)
        numerator = current_phase - previous_phase - frequency_contribution
        if type(numerator) is np.ndarray:
            vec_wrap_phase = np.vectorize(self.__wrap_phase)
            wrapped_numerator = vec_wrap_phase(numerator)
        else:
            wrapped_numerator = self.__wrap_phase(numerator)
        frequency_deviation = wrapped_numerator / self.analysis_hop_size
        return frequency_deviation

    def __scale_phase(self, stft):
        """
        References
        ----------
        .. [1] Mark Dolson. The Phase Vocoder: A Tutorial. Computer Music Journal, 10(4):14-27, 1986.
        .. [2] Reiss, J.D. and McPherson, A. Audio Effects: Theory, Implementation and Application. 191-207 2014.
        .. [3] Pedersen, M. E. H.. "The Phase-Vocoder and its Realization." . 2003
                https://pdfs.semanticscholar.org/a73c/827cbaf9e0fc736e343724ad7037dcca1500.pdf
        """
        phase_scaled_stft = stft.copy()
        # iterate over all frames, one frequency bin at a time
        for freq_bin_index in range(self.n_bins):
            freq_bin_values = phase_scaled_stft[freq_bin_index]
            # cartesian to polar
            magnitudes = np.absolute(freq_bin_values)
            # real = np.real(freq_bin_values)
            # imaginary = np.imag(freq_bin_values)
            phases = np.angle(freq_bin_values)
            # phases = np.arctan2(imaginary, real)
            # assert np.all(phases == np.angle(freq_bin_values))
            frequency = (self.two_pi * freq_bin_index) / self.analysis_window_size
            new_phase = self.__adjust_phases_for_this_frequency_bin_across_all_frames(phases=phases, w=frequency,
                                                                                      k=freq_bin_index)
            # polar to cartesian
            # reals = magnitudes * np.cos(new_phase)
            # imaginaries = magnitudes * np.sin(new_phase)
            # new_frequency_bin_values = np.vectorize(complex)(reals, imaginaries)
            # euler's formula
            new_frequency_bin_values = magnitudes * np.exp(1j * new_phase)

            phase_scaled_stft[freq_bin_index] = new_frequency_bin_values
        return phase_scaled_stft

    # noinspection PyTypeChecker
    def __plot_spectrograms(self, stft, phase_scaled_stft, audio_data, istft):
        print("plotting")
        plt.suptitle(str(self.time_scale_factor))
        # fig, ((ax_analysis_mag, ax_scaled_phase_mag), (ax_analysis_phase, ax_scaled_phase_phase)) = plt.subplots(2, 2, sharey=True, sharex=True)
        grid_shape = (3, 3)
        # (row, column)
        ax_audio = plt.subplot2grid(shape=grid_shape, loc=(0, 0))
        ax_analysis_mag = plt.subplot2grid(shape=grid_shape, loc=(1, 0))
        ax_analysis_phase = plt.subplot2grid(shape=grid_shape, loc=(2, 0), sharey=ax_analysis_mag,
                                             sharex=ax_analysis_mag)

        ax_scaled_phase_mag = plt.subplot2grid(shape=grid_shape, loc=(1, 1), sharey=ax_analysis_mag,
                                               sharex=ax_analysis_mag)
        ax_scaled_phase_phase = plt.subplot2grid(shape=grid_shape, loc=(2, 1), sharey=ax_analysis_mag,
                                                 sharex=ax_analysis_mag)

        ax_resynth_audio = plt.subplot2grid(shape=grid_shape, loc=(0, 2), sharey=ax_audio)
        ax_resynth_mag = plt.subplot2grid(shape=grid_shape, loc=(1, 2))
        ax_resynth_phase = plt.subplot2grid(shape=grid_shape, loc=(2, 2), sharex=ax_resynth_mag, sharey=ax_resynth_mag)
        # X = np.linspace(0, stft.shape[1], stft.shape[1])
        # freqs = np.fft.rfftfreq(n=self.analysis_window_size)
        # Y = freqs * self.sample_rate
        aspect = 'auto'

        colors_log_norm = colors.LogNorm(vmin=0.000001, vmax=1)

        ax_audio.plot(audio_data)
        ax_audio.set_xlim(left=0, right=len(audio_data))
        ax_analysis_mag.imshow(np.absolute(stft), norm=colors_log_norm, aspect=aspect)
        # ax_analysis_mag.set_xticks(np.arange(len(audio_data)))
        # ax_analysis_mag.set_yticks(np.arange(len(Y)))
        # ax_analysis_mag.set_yticklabels(Y)
        ax_analysis_mag.set_title('stft - magnitude')
        ax_analysis_phase.imshow(np.angle(stft), aspect=aspect)
        ax_analysis_phase.set_title('stft - phase')

        ax_scaled_phase_mag.imshow(np.absolute(phase_scaled_stft), norm=colors_log_norm, aspect=aspect)
        ax_scaled_phase_mag.set_title('scaled phase stft - magnitude')
        ax_scaled_phase_phase.imshow(np.angle(phase_scaled_stft), aspect=aspect)
        ax_scaled_phase_phase.set_title('scaled phase stft - phase')

        ax_resynth_audio.plot(istft)
        ax_resynth_audio.set_xlim(left=0, right=len(istft))

        noverlap = self.analysis_window_size - self.synthesis_hop_size
        _, _, stft_resynth = scipy.signal.stft(x=istft, nperseg=self.analysis_window_size, noverlap=noverlap,
                                               window=self.window)
        ax_resynth_mag.imshow(np.absolute(stft_resynth), norm=colors_log_norm, aspect=aspect)
        ax_resynth_phase.imshow(np.angle(stft_resynth), aspect=aspect)

        print("plt.show()")
        plt.show()
        return

    def __resample(self, audio_data):
        """
        Resample the audio data in order to pitch shift it
        """
        audio_data = scipy.signal.resample_poly(audio_data, up=self.pitch_shift_factor_down,
                                                down=self.pitch_shift_factor_up)
        return audio_data

    def phase_vocode(self, audio_data=None, stft=None):
        """
        Main method
        """
        pitch_shift_after = False
        if not pitch_shift_after:
            # don't mess with the signal if you don't have to
            if self.pitch_shift_factor_down != 1 or self.pitch_shift_factor_up != 1:
                audio_data = self.__resample(audio_data)

        # don't mess with the signal if you don't have to
        if self.time_scale_factor != 1 or stft is not None:
            if stft is None:
                stft = self.__get_stft(audio_data=audio_data)

            phase_scaled_stft = self.__scale_phase(stft=stft)

            audio_data = self.__get_istft(stft=phase_scaled_stft)

            if False:
                self.__plot_spectrograms(stft, phase_scaled_stft, audio_data, audio_output)

        if pitch_shift_after:
            if self.pitch_shift_factor_down != 1 or self.pitch_shift_factor_up != 1:
                audio_data = self.__resample(audio_data)
        return audio_data

    def phase_vocode_peak_based(self, audio_data):
        """
        Main method
        """
        # if self.time_scale_factor != 1:
        stft = self.__get_stft(audio_data=audio_data)

        phase_scaled_stft = self.__scale_phase_peak_based(stft=stft)

        audio_data = self.__get_istft(stft=phase_scaled_stft)

        return audio_data

    def find_dominating_peak(self, peaks, i):
        if peaks[i]:
            return i
        for j in range(self.n_bins):
            if i - j >= 0 and peaks[i - j]:
                return i - j
            if i + j < len(peaks) and peaks[i + j]:
                return i + j
        # raise Exception("no dominating peak found")
        return i

    def find_peaks(self, spectrum):
        peaks = np.zeros(self.n_bins, dtype=bool)
        # mags_1 = np.zeros_like(mags)
        # mags_1[:self.n_bins - 1] = mags[1:]
        neighbors = 1
        for i in range(self.n_bins):
            peaks[i] = self.is_bin_a_peak(i, spectrum, neighbors)
        return peaks

    def is_bin_a_peak(self, i, mags, neighbors):
        for j in range(neighbors):
            if i > j:
                if mags[i] <= mags[i - (j + 1)]:
                    return False
        for j in range(neighbors):
            if i < self.n_bins - (1 + j):
                if mags[i] <= mags[i + (1 + j)]:
                    return False
        return True

    def __scale_phase_peak_based(self, stft):
        """
        References
        ----------
        .. [1] Mark Dolson. The Phase Vocoder: A Tutorial. Computer Music Journal, 10(4):14-27, 1986.
        .. [2] Reiss, J.D. and McPherson, A. Audio Effects: Theory, Implementation and Application. 191-207 2014.
        .. [3] Pedersen, M. E. H.. "The Phase-Vocoder and its Realization." . 2003
                https://pdfs.semanticscholar.org/a73c/827cbaf9e0fc736e343724ad7037dcca1500.pdf
        """
        p = 2
        # generate peak map for X
        all_mags = np.absolute(stft)

        dominant_peak = np.zeros_like(all_mags, dtype=int)

        for i in range(stft.shape[1]):
            peaks_row = self.find_peaks(all_mags[:, i])

            for j in range(stft.shape[0]):
                dominant_peak[j, i] = self.find_dominating_peak(peaks_row, j)
        # map each peak Xt[k1] to Yt[p*k1].
        original_dominant_peaks = dominant_peak.copy()
        dominant_peak *= p
        # create phasors for the chosen peaks
        # Z is the 'phasor'
        # https://pdfs.semanticscholar.org/a73c/827cbaf9e0fc736e343724ad7037dcca1500.pdf#page=14&zoom=250,54,646
        Z = np.ones_like(stft)
        for frame_index in range(1, stft.shape[1]):
            previous_Z = Z[:, frame_index - 1]
            for k in range(self.n_bins):
                k1 = dominant_peak[k, frame_index]
                b_delta = k1 * (p - 1)
                # b_delta = -10
                Z[k, frame_index] = previous_Z[k] * np.exp(1j * b_delta * ((2 * np.pi) / self.synthesis_overlap_factor))
        phase_scaled_stft = stft.copy() * 0
        for frame_index in range(1, stft.shape[1]):
            for k in range(self.n_bins):
                k1 = original_dominant_peaks[k, frame_index]
                b_delta = k1 * (p - 1)
                # b_delta = 10
                if k - b_delta >= 0:
                    print(k - b_delta)
                    phase_scaled_stft[k, frame_index] = Z[k, frame_index] * stft[k - b_delta, frame_index]
        return phase_scaled_stft
