from __future__ import division

import os
import time

import matplotlib.pyplot as plt
import numpy as np
from scipy.io import wavfile

from audio_play_head import AudioPlayHead
from mixing_utils import get_full_scale_audio_and_max_int_and_sample_rate, floating_to_fixed_point_16bit
from phase_vocoder_frame_based import PhaseVocoderFrameBased
from song import Song
from tempo_marker import TempoMarker


class GamelanizerFrameBased:
    def __init__(self, hw_buffer_size, analysis_window_size, analysis_overlap_factor, num_subdivision_levels,
                 sample_rate, song, pitch_shift_factor):

        self.filename = song.filename
        self.audio_play_head = AudioPlayHead(song.tempo_markers)

        self.hw_buffer_size = hw_buffer_size
        self.sample_rate = sample_rate

        self.num_subdivision_levels = num_subdivision_levels
        self.analysis_window_size = analysis_window_size
        self.analysis_hop_size = int(self.analysis_window_size / analysis_overlap_factor)
        self.pvs = []
        self.powers = [np.power(2, current_level + 1) for current_level in range(self.num_subdivision_levels)]
        for current_level in range(0, self.num_subdivision_levels):
            # ---------
            time_scale_factor = self.powers[current_level]
            pv = PhaseVocoderFrameBased(time_scale_factor=time_scale_factor,
                                        pitch_shift_factor=pitch_shift_factor ** (current_level + 1),
                                        analysis_window_size=analysis_window_size,
                                        analysis_overlap_factor=analysis_overlap_factor)
            self.pvs.append(pv)
            pass
        # in C++ version, this could be a (non-realtime) setting, rather than hard coded
        max_samples_per_beat = 400000
        self.scaled_buffer = np.zeros((int((max_samples_per_beat * .5) + 1), self.num_subdivision_levels))
        self.samples_into_beat = 0
        self.beat_sample_indices = [0, 0]
        self._debug_beat_pos_start = []
        self._debug_beat_pos_end = []
        self.beat_number = 0
        self.beat_b = False

        self.level_sample_widths = [0 for _ in range(self.num_subdivision_levels)]
        self.lvl_write_pos = [0 for _ in range(self.num_subdivision_levels)]
        self.lvl_accumulated = [0 for _ in range(self.num_subdivision_levels)]

        self.output_buffer = np.zeros((int(max_samples_per_beat * 4), self.num_subdivision_levels))
        self.out_read_pos = 0

        # delay buffer time = (2 * samples_per_beat) + sum(level_sample_widths)
        self.delay_buffer = np.zeros(max_samples_per_beat * 3)

        self.dly_in_write_pos = 0
        self.dly_out_read_pos = 0

        self.is_playing = False
        self._debug_delay_time = 0
        self._debug_fig_num = 0
        self.plot_buffers = False
        self._debug_input_audio = None
        self._debug_calculated_absolute_sample_position = 0
        self._debug_first_pos = [0 for _ in range(self.num_subdivision_levels)]
        return

    def main(self):
        input_audio_fs, max_integer, sample_rate = get_full_scale_audio_and_max_int_and_sample_rate(
            filename=("%s" % self.filename))

        amount_to_pad = int(
            0.5 + (np.ceil(len(input_audio_fs) / self.hw_buffer_size) * self.hw_buffer_size) - len(input_audio_fs))
        input_audio_fs = np.pad(input_audio_fs, pad_width=(0, amount_to_pad), mode='constant',
                                constant_values=0)
        input_audio_fs = np.pad(input_audio_fs, pad_width=(0, int(len(input_audio_fs) / 2)), mode='constant',
                                constant_values=0)
        self._debug_input_audio = input_audio_fs.copy()
        num_blocks = len(input_audio_fs) // self.hw_buffer_size
        assert num_blocks == np.floor(num_blocks)
        num_blocks = int(num_blocks)
        process_block_times = []
        for i in range(num_blocks):
            print(i, "/", num_blocks)
            start_sample = i * self.hw_buffer_size
            end_sample = (i + 1) * self.hw_buffer_size
            channel_data = input_audio_fs[start_sample:end_sample]
            self.audio_play_head.move_to_sample(start_sample, sample_rate)
            start_time = time.time()
            self.process_block(channel_data)
            end_time = time.time()
            process_block_times.append(end_time - start_time)
        plt.plot(process_block_times)
        plt.show()
        fixed_point = floating_to_fixed_point_16bit(max_integer=max_integer, data_fs=input_audio_fs)
        fn, ext = os.path.splitext(self.filename)
        output_folder = "output_frame_based/"
        if not os.path.exists(output_folder):
            os.makedirs(output_folder)
        wavfile.write(filename=output_folder + fn + "-gam_out" + ext, rate=sample_rate,
                      data=fixed_point.astype(np.int16))

        return

    def process_block(self, channel_data):
        bpm, ppq, current_sample, current_time = self.audio_play_head.get_current_position()
        if not self.is_playing:
            self.is_playing = True
            actual_samples_per_beat = (self.sample_rate * (60 / bpm))
            self.beat_number = 0
            self.samples_into_beat = 0
            self.out_read_pos = 0
            self.initialize_beat_sample_start(actual_samples_per_beat)
            self.update_beat_sample_end(actual_samples_per_beat)

            self.update_level_sample_widths(actual_samples_per_beat)

            self.initialize_lvl_write_pos(actual_samples_per_beat)
            # TODO handling tempo changes
            delay_time = int(np.ceil((actual_samples_per_beat * 2) + sum(self.level_sample_widths)))
            self._debug_delay_time = delay_time
            self.dly_out_read_pos = (self.dly_in_write_pos - delay_time) % len(self.delay_buffer)
            self._debug_first_pos = self.lvl_write_pos.copy()
            self._plot_output_positions()
            pass

        # actual processing
        for sample in range(self.hw_buffer_size):
            input_sample = channel_data[sample]
            self.delay_buffer[self.dly_in_write_pos] = input_sample

            channel_data[sample] = 0
            # output the subdivision levels
            channel_data[sample] += self.output_buffer[self.out_read_pos, :].sum()

            # output the delayed base
            channel_data[sample] += self.delay_buffer[self.dly_out_read_pos]

            # beat_width = self.beat_sample_indices[1] - self.beat_sample_indices[0]
            for current_level in range(self.num_subdivision_levels):
                hop = self.process_sample(current_level, input_sample)
                self.lvl_accumulated[current_level] += hop

            # if we're on a beat boundary
            calculated_absolute_sample_position = self.samples_into_beat + self.beat_sample_indices[0]
            self._debug_calculated_absolute_sample_position = calculated_absolute_sample_position
            if calculated_absolute_sample_position >= self.beat_sample_indices[1]:
                self._plot_output_positions()
                for current_level in range(self.num_subdivision_levels):
                    beat_width = self.level_sample_widths[current_level]
                    missing = int(beat_width - self.lvl_accumulated[current_level])
                    self.lvl_write_pos[current_level] += missing
                    self.lvl_accumulated[current_level] = 0
                self.next_beat(bpm)
                self._plot_output_positions()
            else:
                self.samples_into_beat += 1

            self.out_read_pos += 1

            self.dly_in_write_pos += 1
            self.dly_out_read_pos += 1

            if self.out_read_pos == len(self.output_buffer):
                self.out_read_pos = 0

            if self.dly_in_write_pos == len(self.delay_buffer):
                self.dly_in_write_pos = 0
            if self.dly_out_read_pos == len(self.delay_buffer):
                self.dly_out_read_pos = 0
        return

    def process_sample(self, current_level, input_sample):
        pv = self.pvs[current_level]
        hop = pv.push_sample_onto_resamp_queue(input_sample)

        if hop > 0:
            self.add_samples_to_output_buffer(current_level, pv.fft_in_out)
            # self._plot_output_positions()
            self.lvl_write_pos[current_level] += hop
            # self._plot_output_positions()
        return hop

    def add_samples_to_output_buffer(self, current_level, samples):

        power = self.powers[current_level]
        beat_length = self.beat_sample_indices[1] - self.beat_sample_indices[0]
        beat_length_scaled = (beat_length / power)
        beat_length_scaled_times_two = (beat_length_scaled * 2)
        for i in range(power):
            # multiple write heads for each copy of the scaled beat, depending on the subdivision lvl
            write_head = self.lvl_write_pos[current_level] + int(beat_length_scaled_times_two * i)
            write_head_array = np.arange(write_head, write_head + len(samples)) % len(self.output_buffer)
            self.output_buffer[write_head_array, current_level] += samples
        return

    def next_beat(self, bpm):
        # self._plot_delay_buffer()
        # self._plot_output_positions()
        for pv in self.pvs:
            pv.reset()
        actual_samples_per_beat = (self.sample_rate * (60 / bpm))
        self.update_level_sample_widths(actual_samples_per_beat)
        # only call this at the END of beat b
        if self.beat_b:
            self.move_write_pos_on_beat_b()
        self.samples_into_beat = 0
        self.beat_number += 1
        print("beat number", self.beat_number)
        self.beat_sample_indices[0] = self.beat_sample_indices[1]
        self._debug_beat_pos_start.append(self.beat_sample_indices[0])
        self.update_beat_sample_end(actual_samples_per_beat)
        self.beat_b = not self.beat_b
        return

    def move_write_pos_on_beat_b(self):
        beat_length = self.beat_sample_indices[1] - self.beat_sample_indices[0]
        for current_level in range(0, self.num_subdivision_levels):
            beat_length_scaled = (beat_length / self.powers[current_level])
            # we have to add 2 because we're starting from 0.
            # If we counted the base level as 0, we would only add 1
            number_notes_in_this_lvl_equal_to_two_in_the_original = 2 ** (2 + current_level)
            # by now, the lead write head is at the end of the 2nd note, so we don't have to jump past those 2 notes
            number_of_notes_to_jump_over = number_notes_in_this_lvl_equal_to_two_in_the_original - 2
            samples_to_jump = int(beat_length_scaled * number_of_notes_to_jump_over)
            new_lvl_write_pos = self.lvl_write_pos[current_level] + samples_to_jump
            self.lvl_write_pos[current_level] = new_lvl_write_pos  # % len(self.output_buffer)
        return

    def initialize_lvl_write_pos(self, actual_samples_per_beat):
        for i in range(self.num_subdivision_levels):
            self.lvl_write_pos[i] = 0
        self.lvl_write_pos[-1] = int(np.round(actual_samples_per_beat * 2))
        for i in range(self.num_subdivision_levels - 2, -1, -1):
            self.lvl_write_pos[i] += self.lvl_write_pos[i + 1] + self.level_sample_widths[i + 1]
        return

    def initialize_beat_sample_start(self, actual_samples_per_beat):
        good_rounding = int(np.round(actual_samples_per_beat * self.beat_number))
        self.beat_sample_indices[0] = good_rounding
        self._debug_beat_pos_start.append(self.beat_sample_indices[0])
        return

    def update_beat_sample_end(self, actual_samples_per_beat):
        # this won't handle changing tempos.
        good_rounding = int(np.round(actual_samples_per_beat * (self.beat_number + 1)))
        self.beat_sample_indices[1] = good_rounding
        self._debug_beat_pos_end.append(self.beat_sample_indices[1])
        return

    def update_level_sample_widths(self, actual_samples_per_beat):
        for x in range(self.num_subdivision_levels):
            self.level_sample_widths[x] = int(np.round(actual_samples_per_beat * (1 / (2 ** (x + 1)))))
        return

    def _plot_delay_buffer(self):
        if self.plot_buffers:
            plt.clf()
            plt.plot(self.delay_buffer, zorder=1)
            # write is red
            plt.scatter(x=self.dly_in_write_pos, y=0, c='r', zorder=2)
            # read is orange
            plt.scatter(x=self.dly_out_read_pos, y=0, c='orange', zorder=2)
            plt.show()
        return

    def _plot_output_positions(self):
        if self.plot_buffers:
            plt.clf()
            fig, axes = plt.subplots(2, 1, sharex='all', sharey='all', figsize=(30, 15))
            axes[0].plot(self._debug_input_audio, zorder=1)
            axes[0].scatter(x=self._debug_calculated_absolute_sample_position, y=0, c='r', zorder=2)
            axes[1].scatter(x=self.lvl_write_pos[0], y=0, c='r', zorder=2)
            axes[1].axvline(x=self._debug_first_pos[0], c='blue')
            for line in self._debug_beat_pos_start:
                axes[0].axvline(x=line, c='g')
                axes[1].axvline(x=line + self._debug_delay_time, c='g')
            for line in self._debug_beat_pos_end:
                axes[0].axvline(x=line, c='r', linestyle='--')
                axes[1].axvline(x=line + self._debug_delay_time, c='r', linestyle='--')
            plt.plot(self.output_buffer, zorder=1)
            # plt.xlim(50000, 120000)
            # plt.xlim(-1000, 120000)
            plt.xlim(66000, 72000)
            plt.savefig(str(self._debug_fig_num) + '.png')
            self._debug_fig_num += 1
            # plt.pause(.01)
        return


if __name__ == '__main__':
    longtest120 = Song('longtest120.wav', [TempoMarker(0.0, 120.0, 4, 4)])
    longtest62 = Song('longtest62.wav', [TempoMarker(0.0, 62.0, 4, 4)])
    notenames97 = Song('notenames97.wav', [TempoMarker(0.0, 97.0, 4, 4)])
    notenameslong97 = Song('notenameslong97.wav', [TempoMarker(0.0, 97.0, 4, 4)])
    fourths240 = Song('fourthpattern.wav', [TempoMarker(0.0, 240.0, 4, 4)])
    compare = Song('compare.wav', [TempoMarker(0.0, 80.0, 4, 4)])
    # (5/4)=3rd, (4/3)=4th, (3/2)=5th, (2/1)=octave
    gamelanizer_frame_based = GamelanizerFrameBased(hw_buffer_size=1024, analysis_window_size=1024,
                                                    analysis_overlap_factor=4,
                                                    num_subdivision_levels=2, sample_rate=44100, song=compare,
                                                    pitch_shift_factor=(4 / 3))
    gamelanizer_frame_based.main()
