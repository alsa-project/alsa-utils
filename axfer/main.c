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

static void print_version(const char *const cmdname)
{
	printf("%s: version %s\n", cmdname, SND_UTIL_VERSION_STR);
}

static void print_help(void)
{
	printf("help\n");
}

static void decide_subcmd(int argc, char *const *argv, enum subcmds *subcmd)
{
	static const char *const subcmds[] = {
		[SUBCMD_TRANSFER] = "transfer",
		[SUBCMD_LIST] = "list",
		[SUBCMD_HELP] = "help",
		[SUBCMD_VERSION] = "version",
	};
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

	if (argc == 1) {
		*subcmd = SUBCMD_HELP;
		return;
	}

	// sub-command system.
	for (i = 0; i < ARRAY_SIZE(subcmds); ++i) {
		if (!strcmp(argv[1], subcmds[i])) {
			*subcmd = i;
			return;
		}
	}

	// Original command system. For long options.
	for (i = 0; i < ARRAY_SIZE(long_opts); ++i) {
		for (j = 0; j < argc; ++j) {
			if (!strcmp(long_opts[i].name, argv[j])) {
				*subcmd = long_opts[i].subcmd;
				return;
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
					return;
				}
			}
		}
	}

	*subcmd = SUBCMD_TRANSFER;
}

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

int main(int argc, char *const *argv)
{
	snd_pcm_stream_t direction;
	enum subcmds subcmd;
	int err = 0;

	if (!decide_direction(argc, argv, &direction))
		subcmd = SUBCMD_HELP;
	else
		decide_subcmd(argc, argv, &subcmd);

	if (subcmd == SUBCMD_TRANSFER)
		printf("execute 'transfer' subcmd.\n");
	else if (subcmd == SUBCMD_LIST)
		printf("execute 'list' subcmd.\n");
	else if (subcmd == SUBCMD_VERSION)
		print_version(argv[0]);
	else
		print_help();
	if (err < 0)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
