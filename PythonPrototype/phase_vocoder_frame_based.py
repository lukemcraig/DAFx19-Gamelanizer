from __future__ import division

import fractions

import matplotlib.pyplot as plt
import numpy as np
import scipy.signal

from catmull_rom_interpolator import CatmullRomInterpolator


class PhaseVocoderFrameBased:
    def __init__(self, time_scale_factor, pitch_shift_factor, analysis_window_size, analysis_overlap_factor):
        self.window = scipy.signal.hann(M=analysis_window_size, sym=False)

        # zero padding the window
        # self.window = np.pad(self.window, int(len(self.window) * 1.5), 'constant', constant_values=0)
        pitch_shift_factor_ints = fractions.Fraction(pitch_shift_factor).limit_denominator(1000)
        self.pitch_shift_factor_down = pitch_shift_factor_ints.denominator
        self.pitch_shift_factor_up = pitch_shift_factor_ints.numerator
        self.time_scale_factor = pitch_shift_factor / time_scale_factor
        print(self.time_scale_factor)
        self.analysis_window_size = len(self.window)

        self.analysis_overlap_factor = analysis_overlap_factor
        self.synthesis_overlap_factor = analysis_overlap_factor / self.time_scale_factor

        self.analysis_hop_size = int(self.analysis_window_size / analysis_overlap_factor)
        self.synthesis_hop_size = self.analysis_hop_size * self.time_scale_factor

        # Add 1 for the zero frequency bin
        self.n_bins = int(self.analysis_window_size / 2) + 1

        self.two_pi = 2 * np.pi

        self.scaled_buffer_write_pos = 0
        self.fft_queue = np.zeros(self.analysis_window_size)
        self.fft_queue_index = 0
        self.fft_in_out = np.zeros(self.analysis_window_size)
        self.fft_data_complex = np.zeros(self.n_bins, dtype=np.complex)
        self.phase_buffer = np.zeros(self.n_bins)
        self.scaled_phase_buffer = np.zeros(self.n_bins)
        # set the initial phases
        self.ready = False
        # filled the fft queue with one full window
        self.fft_queue_ready = False

        self.interpolator = CatmullRomInterpolator()
        # TODO better maximum length
        self.resampler_queue = np.zeros(8192)
        self.resampler_queue_write = 0
        self.resampler_analysis_hop_buffer = np.zeros(self.analysis_hop_size)

        self.max_need_samples = self.calculate_maximum_needed_num_samples(self.analysis_hop_size)

        # only works for hann window
        squared_window_sum = self.analysis_window_size * .375
        self.amplitude_compensation_scale = self.synthesis_hop_size / squared_window_sum

        self.plot_buffers = False
        if self.plot_buffers:
            self.plot_interval = 0.1
            _, self.axes = plt.subplots(5, 1, sharex='all', sharey='all')
            plt.tight_layout()
            _, self.axes1 = plt.subplots(1, 1, sharex='all', sharey='all')
        return

    def reset(self):
        """
        call this at the beginning of a beat
        """
        self.ready = False
        # worrying about filling the fft queue
        self.fft_queue_ready = False
        # self.fft_queue[:] = 0
        self.fft_queue_index = 0
        # self.fft_in_out[:] = 0
        # self.fft_data_complex[:] = 0
        # self.phase_buffer[:] = 0
        # self.scaled_phase_buffer[:] = 0
        self.interpolator.reset()
        # TODO remove after implementing peak locking
        self.scaled_buffer_write_pos = 0
        return

    @staticmethod
    def __wrap_phase(phase_in):
        """
        in the range (-pi,pi]
        """
        phase_out = ((phase_in + np.pi) % (-2.0 * np.pi)) + np.pi
        return phase_out

    def copy_fft_queue_to_fft_in_out(self):
        # fft_queue_index_rewound = (self.fft_queue_index - self.analysis_window_size + np.arange(
        #     self.analysis_window_size)) % self.analysis_window_size
        # self.fft_in_out[:] = self.fft_queue[fft_queue_index_rewound]
        for i in range(self.analysis_window_size):
            fft_queue_index_rewound = (self.fft_queue_index - self.analysis_window_size + i) % self.analysis_window_size
            self.fft_in_out[i] = self.fft_queue[fft_queue_index_rewound]
        return

    def scale_whats_on_the_fft_queue(self):
        self.copy_fft_queue_to_fft_in_out()
        self.plot_fft_data()
        self.fft_in_out[0:self.analysis_window_size] *= self.window
        self.plot_fft_data()
        self.fft_data_complex[0:self.n_bins] = np.fft.rfft(self.fft_in_out[0:self.analysis_window_size])
        if self.ready:
            self.scale_all_frequency_bins_and_store_phase_buffers()
        else:
            # if this is the first frame we've processed of this beat
            self.store_phases_in_buffer()
            self.ready = True

        # inverse fft
        self.fft_in_out[0:self.analysis_window_size] = np.fft.irfft(self.fft_data_complex)
        self.plot_fft_data()
        # synthesis windowing
        self.fft_in_out[0:self.analysis_window_size] *= self.window
        self.plot_fft_data()
        # amplitude compensation
        self.fft_in_out[0:self.analysis_window_size] *= self.amplitude_compensation_scale
        self.plot_fft_data()
        return

    def store_phases_in_buffer(self):
        # this is the first frame we've processed of this beat, so just store the phases
        current_phases = np.angle(self.fft_data_complex)
        self.phase_buffer[:] = current_phases
        self.scaled_phase_buffer[:] = current_phases
        return

    def scale_all_frequency_bins_and_store_phase_buffers(self):
        current_phases = np.angle(self.fft_data_complex)
        old_phases = self.phase_buffer.copy()
        self.phase_buffer[:] = current_phases
        magnitudes = np.abs(self.fft_data_complex)
        for k in range(self.n_bins):
            self.scale_frequency_bin(k, magnitudes[k], current_phases[k], old_phases[k])
        return

    def scale_frequency_bin(self, k, magnitude, current_phase, old_phase):
        frequency_deviation = self.calculate_frequency_deviation(old_phase, current_phase, k)
        omega = (self.two_pi * k) / self.analysis_window_size
        true_freq = omega + frequency_deviation
        true_bin_index = true_freq * (self.analysis_window_size / self.two_pi)
        previous_scaled_phase = self.scaled_phase_buffer[k]
        scaled_phase = true_bin_index * (self.two_pi / self.synthesis_overlap_factor) + previous_scaled_phase
        scaled_phase = self.__wrap_phase(scaled_phase)
        self.scaled_phase_buffer[k] = scaled_phase
        self.fft_data_complex[k] = magnitude * np.exp(1j * scaled_phase)
        return

    def calculate_frequency_deviation(self, old_phase, current_phase, k):
        frequency_contribution = (self.two_pi * k) / self.analysis_overlap_factor
        numerator = current_phase - old_phase - frequency_contribution
        wrapped_numerator = self.__wrap_phase(numerator)
        frequency_deviation = wrapped_numerator / self.analysis_hop_size
        return frequency_deviation

    def calculate_minimum_needed_num_samples(self, desired_num_out):
        # for scipy's polyphase resampling
        up = self.pitch_shift_factor_down
        down = self.pitch_shift_factor_up
        a = ((desired_num_out - 1) * down) / up
        a_ceil = np.ceil(a)
        if a_ceil > a:
            return int(a_ceil)
        else:
            return int(a_ceil + 1)

    def calculate_maximum_needed_num_samples(self, desired_num_out):
        # for scipy's polyphase resampling
        up = self.pitch_shift_factor_down
        down = self.pitch_shift_factor_up
        b = (desired_num_out * down) / up
        return int(np.floor(b))

    def push_sample_onto_resamp_queue(self, sample):
        self.resampler_queue[self.resampler_queue_write] = sample
        self.resampler_queue_write += 1
        # if the number of samples on the queue is the number needed
        if self.resampler_queue_write >= self.max_need_samples + 1:
            self.resample_hop()
            self.push_analysis_hop_onto_fft_queue()
            if self.fft_queue_ready:
                self.scale_whats_on_the_fft_queue()
                # TODO track subsample position
                return int(self.synthesis_hop_size)
        return 0

    def resample_hop(self):
        speed_ratio = self.pitch_shift_factor_up / self.pitch_shift_factor_down
        num_used = self.interpolator.process(actual_ratio=speed_ratio, input_buffer=self.resampler_queue,
                                             output_buffer=self.resampler_analysis_hop_buffer,
                                             num_out=self.analysis_hop_size)
        self.pop_used_samples(num_used)
        return

    def pop_used_samples(self, num_used):
        num_unconsumed = self.resampler_queue_write - num_used
        assert num_unconsumed >= 0
        # copy the unused data on top of the used data
        self.resampler_queue[0:num_unconsumed] = self.resampler_queue[num_used:self.resampler_queue_write]
        # zero out where the unused data was
        # (not actually necessary if we set the available and wrap parameters)
        self.resampler_queue[num_unconsumed:self.resampler_queue_write + num_used] = 0
        self.resampler_queue_write -= num_used
        return

    def push_analysis_hop_onto_fft_queue(self):
        for i in range(self.analysis_hop_size):
            self.fft_queue[self.fft_queue_index] = self.resampler_analysis_hop_buffer[i]
            self.fft_queue_index += 1
            if self.fft_queue_index == self.analysis_window_size:
                self.fft_queue_ready = True
                self.fft_queue_index = 0
        return

    # region DebugPlotting
    def plot_fft_data(self):
        if self.plot_buffers:
            self.axes[4].clear()
            self.axes[4].set_title('fft data')
            self.axes[4].plot(self.fft_in_out[0:self.analysis_window_size])
            plt.pause(self.plot_interval)
        return

    def plot_scaled_write_buffer(self, scaled_write_buffer):
        if self.plot_buffers:
            self.axes1.clear()
            self.axes1.set_title('scaled write buffer')
            self.axes1.plot(scaled_write_buffer)
            plt.pause(self.plot_interval)
        return

    def plot_fft_queue(self):
        if self.plot_buffers:
            self.axes[3].clear()
            self.axes[3].set_title('fft input ring buffer')
            self.axes[3].plot(self.fft_queue)
            self.axes[3].axvline(x=self.fft_queue_index)
            plt.pause(self.plot_interval)
        return

    def plot_input_to_resampler(self):
        if self.plot_buffers:
            self.axes[1].clear()
            self.axes[1].set_title('resampler input queue')

            self.axes[1].plot(self.resampler_queue)
            self.axes[1].axvline(x=self.resampler_queue_write)
            plt.pause(self.plot_interval)
        return

    def plot_resampled_data(self, resampled_data):
        if self.plot_buffers:
            self.axes[2].clear()
            self.axes[2].set_title('resampled data')

            self.axes[2].plot(resampled_data)
            plt.pause(self.plot_interval)
        return

    def plot_input_buffer(self, input_buffer):
        if self.plot_buffers:
            self.axes[0].clear()
            self.axes[0].set_title('input buffer')

            self.axes[0].plot(input_buffer)
            plt.pause(self.plot_interval)
        return

    # endregion

    # region Peak Locking
    def scale_whats_on_the_fft_queue_peak_based(self, scaled_buffer_write):
        self.copy_fft_queue_to_fft_in_out()
        self.fft_in_out[0:self.analysis_window_size] *= self.window
        self.fft_data_complex[0:self.n_bins] = np.fft.rfft(self.fft_in_out[0:self.analysis_window_size])
        peaks, dominating_peaks = self.find_peaks()
        all_mags = np.abs(self.fft_data_complex[0:self.n_bins])
        plt.plot(all_mags)
        peak_indicies = np.where(peaks)
        plt.scatter(x=peak_indicies, y=all_mags[peak_indicies])
        plt.scatter(x=np.arange(len(dominating_peaks)), y=all_mags[dominating_peaks], alpha=0.2)
        plt.show()
        if self.ready:
            # https://pdfs.semanticscholar.org/a73c/827cbaf9e0fc736e343724ad7037dcca1500.pdf#page=14&zoom=280,70,620
            # 'phasor' Z
            Z = np.zeros_like(self.fft_data_complex[0:self.n_bins])
            for k1 in range(self.n_bins):
                if peaks[k1]:
                    # magnitude = np.abs(self.fft_data_complex[k1])
                    current_phase = np.angle(self.fft_data_complex[k1])
                    old_phase = self.phase_buffer[k1]
                    self.phase_buffer[k1] = current_phase
                    frequency_deviation = self.calculate_frequency_deviation(old_phase, current_phase, k1)
                    omega = (self.two_pi * k1) / self.analysis_window_size
                    true_freq = omega + frequency_deviation
                    true_bin_index = true_freq * (self.analysis_window_size / self.two_pi)
                    previous_scaled_phase = self.scaled_phase_buffer[k1]
                    scaled_phase = true_bin_index * (
                            self.two_pi / self.synthesis_overlap_factor) + previous_scaled_phase
                    scaled_phase = self.__wrap_phase(scaled_phase)
                    self.scaled_phase_buffer[k1] = scaled_phase
                    # self.fft_data_complex[k1] = magnitude * np.exp(1j * scaled_phase)
                    theta = scaled_phase - current_phase
                    Z[k1] = np.exp(1j * theta)
            for k in range(self.n_bins):
                current_phase = np.angle(self.fft_data_complex[k])
                self.phase_buffer[k] = current_phase
                k1 = dominating_peaks[k]
                self.fft_data_complex[k] = Z[k1] * self.fft_data_complex[k]
                self.scaled_phase_buffer[k] = np.angle(self.fft_data_complex[k])

        else:
            for k in range(self.n_bins):
                current_phase = np.angle(self.fft_data_complex[k])
                self.phase_buffer[k] = current_phase
                # beacuse this is the first frame we've processed of this beat
                self.scaled_phase_buffer[k] = current_phase
            self.ready = True

        self.fft_in_out[0:self.analysis_window_size] = np.fft.irfft(self.fft_data_complex)
        self.fft_in_out[0:self.analysis_window_size] *= self.window
        self.fft_in_out[0:self.analysis_window_size] *= self.amplitude_compensation_scale

        scaled_buffer_write[
        self.scaled_buffer_write_pos:self.scaled_buffer_write_pos + self.analysis_window_size] += self.fft_in_out[
                                                                                                  0:self.analysis_window_size]
        old_write_pos = self.scaled_buffer_write_pos
        self.scaled_buffer_write_pos += int(self.synthesis_hop_size)
        return old_write_pos, self.scaled_buffer_write_pos

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

    def find_dominating_peak(self, peaks, i):
        if peaks[i]:
            return i
        for j in range(1, self.n_bins):
            if i - j >= 0 and peaks[i - j]:
                return i - j
            if i + j < len(peaks) and peaks[i + j]:
                return i + j
        # raise Exception("no dominating peak found")
        return i

    def find_peaks(self):
        peaks = np.zeros(self.n_bins, dtype=bool)
        mags = np.abs(self.fft_data_complex[0:self.n_bins])
        # mags_1 = np.zeros_like(mags)
        # mags_1[:self.n_bins - 1] = mags[1:]
        neighbors = 8
        for i in range(self.n_bins):
            peaks[i] = self.is_bin_a_peak(i, mags, neighbors)
        dominating_peak = np.empty(self.n_bins, dtype=int)
        dominating_peak[:] = -1
        for i in range(self.n_bins):
            dominating_peak[i] = self.find_dominating_peak(peaks, i)
        return peaks, dominating_peak

    def phase_vocode_peak_based(self, input_buffer, scaled_buffer_write):
        """
        Main method
        """
        for i in range(self.analysis_hop_size):
            self.fft_queue[self.fft_queue_index] = input_buffer[i]
            # scaled_buffer_write[self.scaled_buffer_read_pos] = 0
            # self.scaled_buffer_read_pos += 1
            self.fft_queue_index += 1
            if self.fft_queue_index == self.analysis_window_size:
                self.fft_queue_ready = True
                self.fft_queue_index = 0

        if self.fft_queue_ready:
            return self.scale_whats_on_the_fft_queue_peak_based(scaled_buffer_write)
        else:
            return None
    # endregion
