// SPDX-License-Identifier: GPL-2.0
//
// subcmd-list.c - operations for list sub command.
//
// Copyright (c) 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp>
//
// Licensed under the terms of the GNU General Public License, version 2.

#include "subcmd.h"
#include "misc.h"

#include <getopt.h>
#include <stdbool.h>

enum list_op {
	LIST_OP_DEVICE = 0,
	LIST_OP_PCM,
	LIST_OP_HELP,
};

static int dump_device(snd_ctl_t *handle, const char *id, const char *name,
		       snd_pcm_stream_t direction, snd_pcm_info_t *info)
{
	unsigned int count;
	int i;
	int err;

	printf("card %i: %s [%s], device %i: %s [%s]\n",
	       snd_pcm_info_get_card(info), id, name,
	       snd_pcm_info_get_device(info), snd_pcm_info_get_id(info),
	       snd_pcm_info_get_name(info));

	count = snd_pcm_info_get_subdevices_count(info);
	printf("  Subdevices: %i/%u\n",
	       snd_pcm_info_get_subdevices_avail(info), count);

	for (i = 0; i < count; ++i) {
		snd_pcm_info_set_subdevice(info, i);

		err = snd_ctl_pcm_info(handle, info);
		if (err < 0) {
			printf("control digital audio playback info (%i): %s",
			       snd_pcm_info_get_card(info), snd_strerror(err));
			continue;
		}

		printf("  Subdevice #%i: %s\n",
		       i, snd_pcm_info_get_subdevice_name(info));
	}

	return 0;
}

static int dump_devices(snd_ctl_t *handle, const char *id, const char *name,
			snd_pcm_stream_t direction)
{
	snd_pcm_info_t *info;
	int device = -1;
	int err;

	err = snd_pcm_info_malloc(&info);
	if (err < 0)
		return err;

	while (1) {
		err = snd_ctl_pcm_next_device(handle, &device);
		if (err < 0)
			break;
		if (device < 0)
			break;

		snd_pcm_info_set_device(info, device);
		snd_pcm_info_set_subdevice(info, 0);
		snd_pcm_info_set_stream(info, direction);
		err = snd_ctl_pcm_info(handle, info);
		if (err < 0)
			continue;

		err = dump_device(handle, id, name, direction, info);
		if (err < 0)
			break;
	}

	free(info);
	return err;
}

static int list_devices(snd_pcm_stream_t direction)
{
	int card = -1;
	char name[32];
	snd_ctl_t *handle;
	snd_ctl_card_info_t *info;
	int err;

	err = snd_ctl_card_info_malloc(&info);
	if (err < 0)
		return err;

	// Not found.
	if (snd_card_next(&card) < 0 || card < 0)
		goto end;

	printf("**** List of %s Hardware Devices ****\n",
	       snd_pcm_stream_name(direction));

	while (card >= 0) {
		sprintf(name, "hw:%d", card);
		err = snd_ctl_open(&handle, name, 0);
		if (err < 0) {
			printf("control open (%i): %s",
			       card, snd_strerror(err));
		} else {
			err = snd_ctl_card_info(handle, info);
			if (err < 0) {
				printf("control hardware info (%i): %s",
				       card, snd_strerror(err));
			} else {
				err = dump_devices(handle,
					snd_ctl_card_info_get_id(info),
					snd_ctl_card_info_get_name(info),
					direction);
			}
			snd_ctl_close(handle);
		}

		if (err < 0)
			break;

		// Go to next.
		if (snd_card_next(&card) < 0) {
			printf("snd_card_next");
			break;
		}
	}
end:
	free(info);
	return err;
}

static int list_pcms(snd_pcm_stream_t direction)
{
	static const char *const filters[] = {
		[SND_PCM_STREAM_CAPTURE]	= "Input",
		[SND_PCM_STREAM_PLAYBACK]	= "Output",
	};
	const char *filter;
	void **hints;
	void **n;
	char *io;
	char *name;
	char *desc;

	if (snd_device_name_hint(-1, "pcm", &hints) < 0)
		return -EINVAL;

	filter = filters[direction];

	for (n = hints; *n != NULL; ++n) {
		io = snd_device_name_get_hint(*n, "IOID");
		if (io != NULL && strcmp(io, filter) != 0) {
			free(io);
			continue;
		}

		name = snd_device_name_get_hint(*n, "NAME");
		desc = snd_device_name_get_hint(*n, "DESC");

		printf("%s\n", name);
		if (desc == NULL) {
			free(name);
			free(desc);
			continue;
		}


		printf("    ");
		while (*desc) {
			if (*desc == '\n')
				printf("\n    ");
			else
				putchar(*desc);
			desc++;
		}
		putchar('\n');
	}

	snd_device_name_free_hint(hints);

	return 0;
}

static void print_help(void)
{
	printf(
"Usage:\n"
"  axfer list DIRECTION TARGET\n"
"\n"
"  where:\n"
"    DIRECTION = capture | playback\n"
"    TARGET = device | pcm\n"
	);
}

// Backward compatibility to aplay(1).
static bool decide_operation(int argc, char *const *argv, enum list_op *op)
{
	static const char *s_opts = "hlL";
	static const struct option l_opts[] = {
		{"list-devices",	0, NULL, 'l'},
		{"list-pcms",		0, NULL, 'L'},
		{NULL,			0, NULL, 0}
	};

	optind = 0;
	opterr = 0;
	while (1) {
		int c = getopt_long(argc, argv, s_opts, l_opts, NULL);
		if (c < 0)
			break;
		if (c == 'l') {
			*op = LIST_OP_DEVICE;
			return true;
		}
		if (c == 'L') {
			*op = LIST_OP_PCM;
			return true;
		}
	}

	return false;
}

static int detect_operation(int argc, char *const *argv, enum list_op *op)
{
	static const char *const ops[] = {
		[LIST_OP_DEVICE] = "device",
		[LIST_OP_PCM] = "pcm",
	};
	int i;

	if (argc < 2)
		return false;

	for (i = 0; i < ARRAY_SIZE(ops); ++i) {
		if (!strcmp(argv[1], ops[i])) {
			*op = i;
			return true;
		}
	}

	return false;
}

int subcmd_list(int argc, char *const *argv, snd_pcm_stream_t direction)
{
	enum list_op op = LIST_OP_HELP;
	int err = 0;

	// Renewed command system.
	if (!detect_operation(argc, argv, &op) &&
	    !decide_operation(argc, argv, &op))
			err = -EINVAL;

	if (op == LIST_OP_DEVICE)
		err = list_devices(direction);
	else if (op == LIST_OP_PCM)
		err = list_pcms(direction);
	else
		print_help();

	return err;
}
