// SPDX-License-Identifier: GPL-2.0
//
// container-raw.c - a parser/builder for a container with raw data frame.
//
// Copyright (c) 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp>
//
// Licensed under the terms of the GNU General Public License, version 2.

#include "container.h"
#include "misc.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static int raw_builder_pre_process(struct container_context *cntr,
				   snd_pcm_format_t *sample_format,
				   unsigned int *samples_per_frame,
				   unsigned int *frames_per_second,
				   uint64_t *byte_count)
{
	*byte_count = UINT64_MAX;

	return 0;
}

static int raw_parser_pre_process(struct container_context *cntr,
				  snd_pcm_format_t *sample_format,
				  unsigned int *samples_per_frame,
				  unsigned int *frames_per_second,
				  uint64_t *byte_count)
{
	struct stat buf = {0};
	int err;

	if (cntr->stdio) {
		*byte_count = UINT64_MAX;
		return 0;
	}

	err = fstat(cntr->fd, &buf);
	if (err < 0)
		return err;

	*byte_count = buf.st_size;
	if (*byte_count == 0)
		*byte_count = UINT64_MAX;

	return 0;
}

const struct container_parser container_parser_raw = {
	.format = CONTAINER_FORMAT_RAW,
	.max_size = UINT64_MAX,
	.ops = {
		.pre_process = raw_parser_pre_process,
	},
};

const struct container_builder container_builder_raw = {
	.format = CONTAINER_FORMAT_RAW,
	.max_size = UINT64_MAX,
	.ops = {
		.pre_process = raw_builder_pre_process,
	},
};
