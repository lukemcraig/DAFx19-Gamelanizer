from __future__ import division

import numpy as np
from scipy.io import wavfile as wavfile


def floating_to_fixed_point_16bit(max_integer, data_fs):
    data_fs = np.multiply(data_fs, max_integer)
    data_fixed_point = data_fs.astype(dtype=np.int16)
    return data_fixed_point


def get_full_scale_audio_and_max_int_and_sample_rate(filename):
    sample_rate, input_audio = wavfile.read("input/" + filename)
    bit_depth = 16
    maximum_integer = np.power(2, bit_depth - 1)
    input_audio_full_scale = np.true_divide(input_audio, maximum_integer)
    return input_audio_full_scale, maximum_integer, sample_rate
