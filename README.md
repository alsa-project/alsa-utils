# alsa-utils
## Fork of alsa-utils, providing an improved version of `alsamixer`

Features
- Transparent color
- Configurable colors
- Configurable keybindings
- Full mouse support

Since `alsamixer` is distributed in a bundle with other alsa utilities
you may want to install this version of `alsamixer` as a separate binary (like `/bin/alsamixer2`).
This way you can use the improved version while retrieving updates of the original `alsa-utils` package.

See [alsamixer/alsamixer.rc](alsamixer/alsamixer.rc) for the default configuration file.
See [alsamixer/alsamixer.cust.rc](alsamixer/alsamixer.cust.rc) for the default configuration file in the image below.
![Screenshot 2017-03-20](http://pixelbanane.de/yafu/3015496499/alsamixer-improved.png)

# alsa-utils
## Advanced Linux Sound Architecture - Utilities

This package contains the command line utilities for the ALSA project.
The package can be compiled only with the installed ALSA driver and
the ALSA C library (alsa-lib).

Utility         | Description
----------------|----------------------------------------------------
alsaconf	| the ALSA driver configurator script
alsa-info       | a script to gather information about ALSA subsystem
alsactl		| an utility for soundcard settings management
aplay/arecord	| an utility for the playback / capture of .wav,.voc,.au files
axfer		| an utility to transfer audio data frame (enhancement of aplay)
amixer		| a command line mixer
alsamixer	| a ncurses mixer
amidi		| a utility to send/receive sysex dumps or other MIDI data
iecset		| a utility to show/set the IEC958 status bits
speaker-test    | a speaker test utility
alsaloop        | a software loopback for PCM devices
alsaucm         | Use Case Manager utility
alsabat         | a sound tester for ALSA sound card driver
alsatplg        | ALSA topology compiler

You may give a look for more information about the ALSA project to URL
http://www.alsa-project.org.
