// SPDX-License-Identifier: GPL-2.0
//
// subcmd.h - a header for each sub-commands.
//
// Copyright (c) 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp>
//
// Licensed under the terms of the GNU General Public License, version 2.

#ifndef __ALSA_UTILS_AXFER_SUBCMD__H_
#define __ALSA_UTILS_AXFER_SUBCMD__H_

#include <alsa/asoundlib.h>

int subcmd_list(int argc, char *const *argv, snd_pcm_stream_t direction);

int subcmd_transfer(int argc, char *const *argv, snd_pcm_stream_t direction);

#endif
