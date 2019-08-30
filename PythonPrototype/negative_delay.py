from __future__ import division

import numpy as np


def do_all_negative_delays(input_audio_fs, num_subdivision_levels, samples_per_beat, subdivided_levels_channels):
    input_audio_fs = zero_pad_endings(input_audio_fs, subdivided_levels_channels, samples_per_beat,
                                      num_subdivision_levels)

    base_level = negative_delay_input_audio(input_audio_full_scale=input_audio_fs,
                                            num_subdivision_levels=num_subdivision_levels,
                                            samples_per_beat=samples_per_beat)
    negative_delay_subdivision_levels(subdivided_levels_channels=subdivided_levels_channels,
                                      num_subdivision_levels=num_subdivision_levels,
                                      samples_per_beat=samples_per_beat)
    return base_level


def zero_pad_endings(input_audio_fs, subdivided_levels_channels, samples_per_beat, num_subdivision_levels):
    amount_of_negative_delay = samples_per_beat * .5
    for _ in range(num_subdivision_levels - 1):
        amount_of_negative_delay += amount_of_negative_delay * .5
    amount_of_negative_delay = int(np.round(amount_of_negative_delay))
    input_audio_fs = np.pad(array=input_audio_fs, pad_width=(0, amount_of_negative_delay), mode='constant',
                            constant_values=0)
    for i in range(num_subdivision_levels):
        subdivided_levels_channels[i] = np.pad(array=subdivided_levels_channels[i],
                                               pad_width=(0, amount_of_negative_delay),
                                               mode='constant',
                                               constant_values=0)
    return input_audio_fs


def negative_delay_input_audio(input_audio_full_scale, num_subdivision_levels, samples_per_beat):
    for i in range(num_subdivision_levels):
        power = np.power(2, i + 1)
        beat_fraction_in_samples = int(samples_per_beat / power)
        input_audio_full_scale = np.roll(input_audio_full_scale, beat_fraction_in_samples)
        input_audio_full_scale[:beat_fraction_in_samples] = 0
    return input_audio_full_scale


def negative_delay_subdivision_levels(subdivided_levels_channels, num_subdivision_levels, samples_per_beat):
    for i in range(num_subdivision_levels - 1):
        power = np.power(2, i + 2)
        beat_fraction_in_samples = int(samples_per_beat / power)
        subdivided_levels_channels[i] = np.roll(subdivided_levels_channels[i], beat_fraction_in_samples)
        subdivided_levels_channels[i][:beat_fraction_in_samples] = 0
