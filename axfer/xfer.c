// SPDX-License-Identifier: GPL-2.0
//
// xfer.c - receiver/transmiter of data frames.
//
// Copyright (c) 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp>
//
// Licensed under the terms of the GNU General Public License, version 2.

#include "xfer.h"
#include "misc.h"

#include <stdio.h>

static const char *const xfer_type_labels[] = {
	[XFER_TYPE_COUNT] = "",
};

enum xfer_type xfer_type_from_label(const char *label)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(xfer_type_labels); ++i) {
		if (!strcmp(xfer_type_labels[i], label))
			return i;
	}

	return XFER_TYPE_UNSUPPORTED;
}

int xfer_context_init(struct xfer_context *xfer, enum xfer_type type,
		      snd_pcm_stream_t direction, int argc, char *const *argv)
{
	struct {
		enum xfer_type type;
		const struct xfer_data *data;
	} *entry, entries[] = {
		{XFER_TYPE_COUNT, NULL},
	};
	int i;
	int err;

	assert(xfer);
	assert(direction >= SND_PCM_STREAM_PLAYBACK);
	assert(direction <= SND_PCM_STREAM_CAPTURE);

	for (i = 0; i < ARRAY_SIZE(entries); ++i) {
		if (entries[i].type == type)
			break;
	}
	if (i == ARRAY_SIZE(entries))
		return -EINVAL;
	entry = &entries[i];

	xfer->direction = direction;
	xfer->type = type;
	xfer->ops = &entry->data->ops;

	xfer->private_data = malloc(entry->data->private_size);
	if (xfer->private_data == NULL)
		return -ENOMEM;
	memset(xfer->private_data, 0, entry->data->private_size);

	err = xfer->ops->init(xfer, direction);
	if (err < 0)
		return err;

	return xfer->ops->validate_opts(xfer);
}

void xfer_context_destroy(struct xfer_context *xfer)
{
	assert(xfer);

	if (!xfer->ops)
		return;

	if (xfer->ops->destroy)
		xfer->ops->destroy(xfer);
	if (xfer->private_data)
		free(xfer->private_data);
}

int xfer_context_pre_process(struct xfer_context *xfer,
			     snd_pcm_format_t *format,
			     unsigned int *samples_per_frame,
			     unsigned int *frames_per_second,
			     snd_pcm_access_t *access,
			     snd_pcm_uframes_t *frames_per_buffer)
{
	int err;

	assert(xfer);
	assert(format);
	assert(samples_per_frame);
	assert(frames_per_second);
	assert(access);
	assert(frames_per_buffer);

	if (!xfer->ops)
		return -ENXIO;

	err = xfer->ops->pre_process(xfer, format, samples_per_frame,
				     frames_per_second, access,
				     frames_per_buffer);
	if (err < 0)
		return err;

	assert(*format >= SND_PCM_FORMAT_S8);
	assert(*format <= SND_PCM_FORMAT_LAST);
	assert(*samples_per_frame > 0);
	assert(*frames_per_second > 0);
	assert(*access >= SND_PCM_ACCESS_MMAP_INTERLEAVED);
	assert(*access <= SND_PCM_ACCESS_LAST);
	assert(*frames_per_buffer > 0);

	if (xfer->verbose > 1) {
		fprintf(stderr, "Transfer: %s\n",
			xfer_type_labels[xfer->type]);
		fprintf(stderr, "  access: %s\n",
			snd_pcm_access_name(*access));
		fprintf(stderr, "  sample format: %s\n",
			snd_pcm_format_name(*format));
		fprintf(stderr, "  bytes/sample: %u\n",
		       snd_pcm_format_physical_width(*format) / 8);
		fprintf(stderr, "  samples/frame: %u\n",
			*samples_per_frame);
		fprintf(stderr, "  frames/second: %u\n",
			*frames_per_second);
		fprintf(stderr, "  frames/buffer: %lu\n",
			*frames_per_buffer);
	}

	return 0;
}

int xfer_context_process_frames(struct xfer_context *xfer,
				struct mapper_context *mapper,
				struct container_context *cntrs,
				unsigned int *frame_count)
{
	assert(xfer);
	assert(mapper);
	assert(cntrs);
	assert(frame_count);

	if (!xfer->ops)
		return -ENXIO;

	return xfer->ops->process_frames(xfer, frame_count, mapper, cntrs);
}

void xfer_context_pause(struct xfer_context *xfer, bool enable)
{
	assert(xfer);

	if (!xfer->ops)
		return;

	xfer->ops->pause(xfer, enable);
}

void xfer_context_post_process(struct xfer_context *xfer)
{
	assert(xfer);

	if (!xfer->ops)
		return;

	xfer->ops->post_process(xfer);
}
