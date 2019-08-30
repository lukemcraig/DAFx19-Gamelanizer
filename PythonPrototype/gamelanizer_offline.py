from __future__ import division

import os

import numpy as np
import scipy.io.wavfile as wavfile
from scipy import signal

from mixing_utils import get_full_scale_audio_and_max_int_and_sample_rate, floating_to_fixed_point_16bit
from negative_delay import do_all_negative_delays
from phase_vocoder_offline import PhaseVocoder


class GamelanizerOffline:
    def __init__(self, sample_rate, window_size, hop_size_denominator,
                 bpm, num_subdivision_levels, input_mix_balance,
                 subdivision_levels_mix_balance,
                 num_output_channels):
        self.pvs = []
        for current_level in range(0, num_subdivision_levels):
            # ---------
            power = np.power(2, current_level + 1)
            time_scale_factor = power
            pv = PhaseVocoder(time_scale_factor=time_scale_factor, pitch_shift_factor=4 / 3,
                              analysis_window_size=window_size, analysis_overlap_factor=hop_size_denominator,
                              sample_rate=sample_rate)
            self.pvs.append(pv)

        self.input_mix_balance = input_mix_balance
        self.subdivision_levels_mix_balance = subdivision_levels_mix_balance
        self.num_output_channels = num_output_channels
        self.bpm = bpm
        self.sample_rate = sample_rate
        self.num_subdivision_levels = num_subdivision_levels

    def make_beat_array(self, input_audio, number_of_beats, samples_per_beat):
        beat_array = np.zeros(shape=(number_of_beats, int(np.ceil(samples_per_beat))))
        for beat in np.arange(0, number_of_beats):
            starting_sample = beat * samples_per_beat
            ending_sample = (beat + 1) * samples_per_beat
            current_beat = input_audio[int(starting_sample):int(ending_sample)]
            beat_array[beat, 0:len(current_beat)] = current_beat
        return beat_array

    def concatenate_subdivision_level_beats_into_channels(self, subdivided_levels_separate_beats):
        out_level_audios = []
        for i in range(self.num_subdivision_levels):
            out_level_audios.append(np.concatenate(subdivided_levels_separate_beats[i]))
        return out_level_audios

    def scale_and_subdivide_levels(self, beat_array):
        subdivided_levels = [[] for _ in range(self.num_subdivision_levels)]
        iterator = iter(range(0, len(beat_array)))
        for i in iterator:
            pair_indices = (i, next(iterator))
            for current_level in range(0, self.num_subdivision_levels):
                # ---------
                power = np.power(2, current_level + 1)

                scaled_beat_a = self.pvs[current_level].phase_vocode(beat_array[pair_indices[0]])
                scaled_beat_b = self.pvs[current_level].phase_vocode(beat_array[pair_indices[1]])

                scaled_beat_a = self.taper_beats(scaled_beat_a, len(scaled_beat_a))
                scaled_beat_b = self.taper_beats(scaled_beat_b, len(scaled_beat_b))

                for j in range(0, power):
                    subdivided_levels[current_level].append(scaled_beat_a)
                    subdivided_levels[current_level].append(scaled_beat_b)
                # -----------
        return subdivided_levels

    def get_num_beats_and_samples_per_beat(self, input_audio_fs):
        one_beat_in_seconds = 60 / self.bpm
        samples_per_beat = one_beat_in_seconds * self.sample_rate
        number_of_beats = int(np.ceil(len(input_audio_fs) / samples_per_beat))
        if number_of_beats % 2 != 0:
            number_of_beats -= 1
        return number_of_beats, samples_per_beat

    def slice_beats_and_subdivide(self, input_audio_fs):
        number_of_beats, samples_per_beat = self.get_num_beats_and_samples_per_beat(
            input_audio_fs=input_audio_fs)
        beat_array = self.make_beat_array(input_audio=input_audio_fs, number_of_beats=number_of_beats,
                                          samples_per_beat=samples_per_beat)

        subdivided_levels_separate_beats = self.scale_and_subdivide_levels(beat_array=beat_array)
        return subdivided_levels_separate_beats, samples_per_beat

    def taper_beats(self, beat_array, samples_per_beat):
        fade_out_window = signal.tukey(int(samples_per_beat), alpha=0.01)
        beat_array *= fade_out_window
        return beat_array

    def process_audio(self, input_audio_fs):
        subdivided_levels_beats, samples_per_beat = self.slice_beats_and_subdivide(input_audio_fs=input_audio_fs)
        subdivided_levels_channels = self.concatenate_subdivision_level_beats_into_channels(
            subdivided_levels_separate_beats=subdivided_levels_beats)
        base_level = do_all_negative_delays(input_audio_fs=input_audio_fs,
                                            num_subdivision_levels=self.num_subdivision_levels,
                                            samples_per_beat=samples_per_beat,
                                            subdivided_levels_channels=subdivided_levels_channels)
        return subdivided_levels_channels, base_level


def main(input_filename, bpm, window_size, hop_size_denominator, num_subdivision_levels, num_output_channels,
         input_mix_balance, subdivision_levels_mix_balance):
    input_audio_fs, max_integer, sample_rate = get_full_scale_audio_and_max_int_and_sample_rate(
        filename=input_filename)

    gamelanizer_offline = GamelanizerOffline(sample_rate=sample_rate,
                             window_size=window_size,
                             hop_size_denominator=hop_size_denominator, bpm=bpm,
                             num_subdivision_levels=num_subdivision_levels,
                             input_mix_balance=input_mix_balance,
                             subdivision_levels_mix_balance=subdivision_levels_mix_balance,
                             num_output_channels=num_output_channels)

    subdivided_levels_channels, base_level = gamelanizer_offline.process_audio(input_audio_fs=input_audio_fs)

    output_folder = "output_offline/"
    if not os.path.exists(output_folder):
        os.makedirs(output_folder)

    base_out = floating_to_fixed_point_16bit(max_integer=max_integer, data_fs=base_level)
    wavfile.write(filename=output_folder + input_filename + "-out.wav", rate=sample_rate, data=base_out.T)

    for i, channel in enumerate(subdivided_levels_channels):
        channel_out = floating_to_fixed_point_16bit(max_integer=max_integer, data_fs=channel)
        wavfile.write(filename=output_folder + input_filename + str(i) + "-out.wav", rate=sample_rate,
                      data=channel_out.T)
    return
