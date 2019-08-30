# Gamelanizer - Python Implementations/Prototypes
This directory contains the Python implementations of Gamelanizer that were created for prototyping. These are included for educational purposes and not meant to be used in production.

The original prototype processes the entire vector of audio data at one time.
* gamelanizer_offline_prototype_driver.py : A command-line interface for gamelanizer_offline_prototype.py
* gamelanizer_offline_prototype.py : Performs the 'Gamelanization' on the input audio data, a beat at a time, over the entire audio data vector. Uses PhaseVocoder.py
* phase_vocoder_offline_driver.py : A command-line interface for PhaseVocoder.py 
* phase_vocoder_offline.py : Performs time scaling and pitch shifting using the standard phase vocoder technique. Processes the entire array of audio passed into it at once.
* negative_delay.py : Applies 'negative' delay (or latency compensation) to align the levels like gamelan music.
----
The second prototype simulates the block-based processing of a real-time plug-in and processes FFT frames instead of entire beats.
* gamelanizer_frame_based.py : Performs the 'Gamelanization' on the input audio data, one FFT frame at a time. Uses PhaseVocoderBuffered.py.
* phase_vocoder_frames.py : Performs time scaling and pitch shifting using the standard phase vocoder technique. Processes a frame at a time and maintains a state.
* catmull_rom_interpolator.py : A Python translation of one of JUCE's resampling classes.
----
The remaining are utility modules.
* mixing_utils.py : Utility functions to emulate basic DAW mixing functionality.
* song.py : A data class for storing song filename and tempo markers to make testing faster.
* tempo_marker.py : A data class for simulating tempo changes.

## Dependencies
* python 3.6
* numpy 1.14.2
* scipy 1.2.0
* matplotlib 3.0.2
