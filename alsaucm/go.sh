#!/bin/bash

#GDB="gdb --args"
#GDB="strace"
#GDB="valgrind --leak-check=yes --show-reachable=yes"
#GDB="perf stat"
PROG=./alsaucm
#PROG=/home/perex/git/pipewire/builddir/spa/plugins/alsa/spa-acp-tool
#PROG="$HOME/git/pulseaudio/build/src/daemon/pulseaudio -n -F $HOME/git/pulseaudio/build/src/daemon/default.pa -p $HOME/git/pulseaudio/build/src/modules/"
#PROG=pulseaudio

#ALSA_CONFIG_UCM="$HOME/alsa/alsa-ucm-conf/ucm" \
ALSA_CONFIG_UCM2="$HOME/alsa/alsa-ucm-conf/ucm2" \
LD_PRELOAD="$HOME/alsa/alsa-lib/src/.libs/libasound.so" \
$GDB $PROG "$@"
