#!/bin/bash

#GDB="gdb --args"

ALSA_CONFIG_UCM="$HOME/alsa/alsa-lib/test/ucm" \
LD_PRELOAD="$HOME/alsa/alsa-lib/src/.libs/libasound.so" \
$GDB ./alsaucm "$@"
