// SPDX-License-Identifier: GPL-2.0
// main.c - an entry point for this program.
//
// Copyright (c) 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp>
//
// Originally written as 'aplay', by Michael Beck and Jaroslav Kysela.
//
// Licensed under the terms of the GNU General Public License, version 2.

#include "subcmd.h"
#include "misc.h"

#include "version.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

enum subcmds {
	SUBCMD_TRANSFER = 0,
	SUBCMD_LIST,
	SUBCMD_HELP,
	SUBCMD_VERSION,
};

char *arg_duplicate_string(const char *str, int *err)
{
	char *ptr;

	// For safe.
	if (strlen(str) > 1024) {
		*err = -EINVAL;
		return NULL;
	}

	ptr = strdup(str);
	if (ptr == NULL)
		*err = -ENOMEM;

	return ptr;
}

long arg_parse_decimal_num(const char *str, int *err)
{
	long val;
	char *endptr;

	errno = 0;
	val = strtol(str, &endptr, 0);
	if (errno > 0) {
		*err = -errno;
		return 0;
	}
	if (*endptr != '\0') {
		*err = -EINVAL;
		return 0;
	}

	return val;
}

static void print_version(const char *const cmdname)
{
	printf("%s: version %s\n", cmdname, SND_UTIL_VERSION_STR);
}

static void print_help(void)
{
	printf(
"Usage:\n"
"  axfer transfer DIRECTION OPTIONS\n"
"  axfer list DIRECTION OPTIONS\n"
"  axfer version\n"
"  axfer help\n"
"\n"
"  where:\n"
"    DIRECTION = capture | playback\n"
"    OPTIONS = -h | --help | (subcommand specific)\n"
	);
}

// Backward compatibility to aplay(1).
static bool decide_subcmd(int argc, char *const *argv, enum subcmds *subcmd)
{
	static const struct {
		const char *const name;
		enum subcmds subcmd;
	} long_opts[] = {
		{"--list-devices",	SUBCMD_LIST},
		{"--list-pcms",		SUBCMD_LIST},
		{"--help",  		SUBCMD_HELP},
		{"--version",  		SUBCMD_VERSION},
	};
	static const struct {
		unsigned char c;
		enum subcmds subcmd;
	} short_opts[] = {
		{'l', SUBCMD_LIST},
		{'L', SUBCMD_LIST},
		{'h', SUBCMD_HELP},
	};
	char *pos;
	int i, j;

	if (argc == 1)
		return false;

	// Original command system. For long options.
	for (i = 0; i < ARRAY_SIZE(long_opts); ++i) {
		for (j = 0; j < argc; ++j) {
			if (!strcmp(long_opts[i].name, argv[j])) {
				*subcmd = long_opts[i].subcmd;
				return true;
			}
		}
	}

	// Original command system. For short options.
	for (i = 1; i < argc; ++i) {
		// Pick up short options only.
		if (argv[i][0] != '-' || argv[i][0] == '\0' ||
		    argv[i][1] == '-' || argv[i][1] == '\0')
			continue;
		for (pos = argv[i]; *pos != '\0'; ++pos) {
			for (j = 0; j < ARRAY_SIZE(short_opts); ++j) {
				if (*pos == short_opts[j].c) {
					*subcmd = short_opts[j].subcmd;
					return true;
				}
			}
		}
	}

	return false;
}

// Backward compatibility to aplay(1).
static bool decide_direction(int argc, char *const *argv,
			     snd_pcm_stream_t *direction)
{
	static const struct {
		const char *const name;
		snd_pcm_stream_t direction;
	} long_opts[] = {
		{"--capture",	SND_PCM_STREAM_CAPTURE},
		{"--playback",	SND_PCM_STREAM_PLAYBACK},
	};
	static const struct {
		unsigned char c;
		snd_pcm_stream_t direction;
	} short_opts[] = {
		{'C',		SND_PCM_STREAM_CAPTURE},
		{'P',		SND_PCM_STREAM_PLAYBACK},
	};
	static const char *const aliases[] = {
		[SND_PCM_STREAM_CAPTURE] = "arecord",
		[SND_PCM_STREAM_PLAYBACK] = "aplay",
	};
	int i, j;
	char *pos;

	// Original command system. For long options.
	for (i = 0; i < ARRAY_SIZE(long_opts); ++i) {
		for (j = 0; j < argc; ++j) {
			if (!strcmp(long_opts[i].name, argv[j])) {
				*direction = long_opts[i].direction;
				return true;
			}
		}
	}

	// Original command system. For short options.
	for (i = 1; i < argc; ++i) {
		// Pick up short options only.
		if (argv[i][0] != '-' || argv[i][0] == '\0' ||
		    argv[i][1] == '-' || argv[i][1] == '\0')
			continue;
		for (pos = argv[i]; *pos != '\0'; ++pos) {
			for (j = 0; j < ARRAY_SIZE(short_opts); ++j) {
				if (*pos == short_opts[j].c) {
					*direction = short_opts[j].direction;
					return true;
				}
			}
		}
	}

	// If not decided yet, judge according to command name.
	for (i = 0; i < ARRAY_SIZE(aliases); ++i) {
		for (pos = argv[0] + strlen(argv[0]); pos != argv[0]; --pos) {
			if (strstr(pos, aliases[i]) != NULL) {
				*direction = i;
				return true;
			}
		}
	}

	return false;
}

static bool detect_subcmd(int argc, char *const *argv, enum subcmds *subcmd)
{
	static const char *const subcmds[] = {
		[SUBCMD_TRANSFER] = "transfer",
		[SUBCMD_LIST] = "list",
		[SUBCMD_HELP] = "help",
		[SUBCMD_VERSION] = "version",
	};
	int i;

	if (argc < 2)
		return false;

	for (i = 0; i < ARRAY_SIZE(subcmds); ++i) {
		if (!strcmp(argv[1], subcmds[i])) {
			*subcmd = i;
			return true;
		}
	}

	return false;
}

static bool detect_direction(int argc, char *const *argv,
			     snd_pcm_stream_t *direction)
{
	if (argc < 3)
		return false;

	if (!strcmp(argv[2], "capture")) {
		*direction = SND_PCM_STREAM_CAPTURE;
		return true;
	}

	if (!strcmp(argv[2], "playback")) {
		*direction = SND_PCM_STREAM_PLAYBACK;
		return true;
	}

	return false;
}

int main(int argc, char *const *argv)
{
	snd_pcm_stream_t direction;
	enum subcmds subcmd;
	int err = 0;

	// For compatibility to aplay(1) implementation.
	if (strstr(argv[0], "arecord") == argv[0] + strlen(argv[0]) - 7 ||
	    strstr(argv[0], "aplay") == argv[0] + strlen(argv[0]) - 5) {
		if (!decide_direction(argc, argv, &direction))
			direction = SND_PCM_STREAM_PLAYBACK;
		if (!decide_subcmd(argc, argv, &subcmd))
			subcmd = SUBCMD_TRANSFER;
	} else {
		// The first option should be one of subcommands.
		if (!detect_subcmd(argc, argv, &subcmd))
			subcmd = SUBCMD_HELP;
		// The second option should be either 'capture' or 'direction'
		// if subcommand is neither 'version' nor 'help'.
		if (subcmd != SUBCMD_VERSION && subcmd != SUBCMD_HELP) {
			if (!detect_direction(argc, argv, &direction)) {
				subcmd = SUBCMD_HELP;
			} else {
				// argv[0] is needed for unparsed option to use
				// getopt_long(3).
				argc -= 2;
				argv += 2;
			}
		}
	}

	if (subcmd == SUBCMD_TRANSFER)
		err = subcmd_transfer(argc, argv, direction);
	else if (subcmd == SUBCMD_LIST)
		err = subcmd_list(argc, argv, direction);
	else if (subcmd == SUBCMD_VERSION)
		print_version(argv[0]);
	else
		print_help();
	if (err < 0)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
