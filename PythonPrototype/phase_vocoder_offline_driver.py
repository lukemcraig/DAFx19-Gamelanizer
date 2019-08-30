from __future__ import division

import argparse
import os

import numpy as np
from scipy.io import wavfile as wavfile

from mixing_utils import get_full_scale_audio_and_max_int_and_sample_rate, floating_to_fixed_point_16bit
from phase_vocoder_offline import PhaseVocoder

parser = argparse.ArgumentParser(description='Phase vocode an input audio file. Output is stored in /output_pv/')
parser.add_argument('filename', type=str, help='the filename, in /input/, with extension')
parser.add_argument('-w', '--window_size', type=int, nargs='?', default=1024)
parser.add_argument('-d', '--overlap_factor', type=int, nargs='?', default=64)

args = parser.parse_args()
overlap_factor = args.overlap_factor
window_size = args.window_size

semitones = 7
pitch_shift_factor = 2 ** (semitones / 12)

time_scale_factor = 2

input_filename = args.filename
input_audio_fs, max_integer, sample_rate = get_full_scale_audio_and_max_int_and_sample_rate(filename=input_filename)

pv = PhaseVocoder(time_scale_factor, pitch_shift_factor, window_size, overlap_factor, sample_rate)

use_peak_based_method = False
if not use_peak_based_method:
    scaled_audio = pv.phase_vocode(input_audio_fs)
else:
    scaled_audio = pv.phase_vocode_peak_based(input_audio_fs)

fixed_point = floating_to_fixed_point_16bit(max_integer=max_integer, data_fs=scaled_audio)
output_folder = "output_pv/"
if not os.path.exists(output_folder):
    os.makedirs(output_folder)
wavfile.write(filename=output_folder + input_filename + "-pv_out.wav", rate=sample_rate,
              data=fixed_point.astype(np.int16))
