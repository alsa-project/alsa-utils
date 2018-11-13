// SPDX-License-Identifier: GPL-2.0
//
// misc.h - a header file for miscellaneous tools.
//
// Copyright (c) 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp>
//
// Licensed under the terms of the GNU General Public License, version 2.

#ifndef __ALSA_UTILS_AXFER_MISC__H_
#define __ALSA_UTILS_AXFER_MISC__H_

#include <gettext.h>

#define ARRAY_SIZE(array)	(sizeof(array)/sizeof(array[0]))

char *arg_duplicate_string(const char *str, int *err);
long arg_parse_decimal_num(const char *str, int *err);

#endif
