#!/bin/bash

#GDB="gdb --args"
#GDB="strace"
#GDB="valgrind --leak-check=yes --show-reachable=yes"
GDB="perf stat"

#ALSA_CONFIG_UCM="$HOME/alsa/alsa-ucm-conf/ucm" \
ALSA_CONFIG_UCM2="$HOME/alsa/alsa-ucm-conf/ucm2" \
LD_PRELOAD="$HOME/alsa/alsa-lib/src/.libs/libasound.so" \
$GDB ./alsaucm "$@"
