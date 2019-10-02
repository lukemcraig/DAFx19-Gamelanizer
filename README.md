# DAFx19-Gamelanizer
[This repository](https://github.com/lukemcraig/DAFx19-Gamelanizer) contains the accompanying material to the DAFx19 paper 'A Real-Time Audio Effect Plug-In Inspired by the Processes of Traditional Indonesian Gamelan Music.'

[Detailed documentation can be found here.](https://codedocs.xyz/lukemcraig/DAFx19-Gamelanizer/)

## Demonstrations

[Listen to input and output audio examples here.](https://soundcloud.com/lukemcraig/sets/gamelanizer-audio-examples)

[Here is a YouTube video showing the plug-in operating.](https://youtu.be/dPmx4D8Ckos)

<a href="https://youtu.be/dPmx4D8Ckos" target="_blank"><img src="https://img.youtube.com/vi/dPmx4D8Ckos/maxresdefault.jpg" alt="Gamelanizer Video"  border="10" /></a>

## Usage
Builds of the plug-in VSTs for Windows and macOS can be found in [the releases tab of the repo](https://github.com/lukemcraig/DAFx19-Gamelanizer/releases). The VSTs have only been tested in the latest version of REAPER. If you try them in another DAW feel free to let us know if they work. 

To use them, copy them to the directory where you DAW expects VSTs. When the plug-in is detected by your DAW it can be inserted on any audio track. Before beginning playback, you should set the BPM field in the plug-in GUI to match the host, assuming the input audio is in quarter-notes. If it's in half-notes, set the plug-in BPM to half the DAW BPM. If it's in eighth-notes, set the plug-in BPM to twice the DAW BPM. Sessions with changing tempos are not currently supported. For best results, the input audio should be quantized to the grid.

The pitch shift rotary knobs indicate perfect fourth, fifths, and octaves. To snap to these intervals, hold the modifier keys while dragging. Try stacking fourth and fifths or try unison by setting all the knobs to 0. Setting the sliders below 0 will use slightly more processing power. The high-pass and low-pass filters can be useful for putting the different subdivision levels further in the background. The "Drop Note" buttons mute the respective notes from being output (dropping note 4 from Level 2 means that every 4th sixteenth-note becomes a rest). 

The plug-in currently only supports mono input. There are 7 output buses. Buses 1+2 are the default stereo out. Bus 3 is the input signal. Bus 4,5,6,7 are the mono outputs of Level 1,2,3,4. Use these buses if you want to combine Gamelanizer with other plug-ins and complicated routing. During playback, the filters and gain sliders should apply instantly if you move them. Changes to the pitch shift knobs will be delayed by a significant amount. However, if you write the automation in the DAW, then it will occur when it ought to.

## Building
If you want to build Gamelanizer yourself, you'll need the latest version of [JUCE](https://github.com/WeAreROLI/JUCE). Open the included [Gamelanizer.jucer](https://github.com/lukemcraig/DAFx19-Gamelanizer/blob/master/Plug-in/Gamelanizer.jucer) file with the [Projucer](https://github.com/WeAreROLI/JUCE/tree/master/extras/Projucer) application to easily generate the correct Visual Studio or Xcode projects. 
#### Optional
If you want to use the MKL FFT and [have it installed on your computer](https://software.intel.com/en-us/mkl), go to the juce_dsp module page in the Projucer and set `JUCE_DSP_USE_INTEL_MKL` to `Enabled`. If you're building on macOS make sure you build with `Release - MKL` in Xcode. If you're building on Windows, follow the instructions in the `Notes` section of `Release - MKL` configuration in the Projucer.

![screenshot](https://github.com/lukemcraig/DAFx19-Gamelanizer/raw/master/screenshot.PNG)

## License
Gamelanizer is licensed under the GNU General Public License v3.0. See [LICENSE](https://github.com/lukemcraig/DAFx19-Gamelanizer/blob/master/LICENSE) for more.
