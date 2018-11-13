// SPDX-License-Identifier: GPL-2.0
//
// mapper-single.c - a muxer/demuxer for single containers.
//
// Copyright (c) 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp>
//
// Licensed under the terms of the GNU General Public License, version 2.

#include "mapper.h"
#include "misc.h"

struct single_state {
	void (*align_frames)(void *frame_buf, unsigned int frame_count,
			     char *buf, unsigned int bytes_per_sample,
			     unsigned int samples_per_frame);
	char *buf;
};

static void align_to_vector(void *frame_buf, unsigned int frame_count,
			    char *src, unsigned int bytes_per_sample,
			    unsigned samples_per_frame)
{
	char **dst_bufs = frame_buf;
	char *dst;
	unsigned int src_pos;
	unsigned int dst_pos;
	int i, j;

	// src: interleaved => dst: a set of interleaved buffers.
	for (i = 0; i < samples_per_frame; ++i) {
		dst = dst_bufs[i];
		for (j = 0; j < frame_count; ++j) {
			src_pos = bytes_per_sample * (samples_per_frame * j + i);
			dst_pos = bytes_per_sample * j;

			memcpy(dst + dst_pos, src + src_pos, bytes_per_sample);
		}
	}
}

static void align_from_vector(void *frame_buf, unsigned int frame_count,
			      char *dst, unsigned int bytes_per_sample,
			      unsigned int samples_per_frame)
{
	char **src_bufs = frame_buf;
	char *src;
	unsigned int dst_pos;
	unsigned int src_pos;
	int i, j;

	// src: a set of interleaved buffers => dst:interleaved.
	for (i = 0; i < samples_per_frame; ++i) {
		src = src_bufs[i];
		for (j = 0; j < frame_count; ++j) {
			src_pos = bytes_per_sample * j;
			dst_pos = bytes_per_sample * (samples_per_frame * j + i);

			memcpy(dst + dst_pos, src + src_pos, bytes_per_sample);
		}
	}
}

static int single_pre_process(struct mapper_context *mapper,
			      struct container_context *cntrs,
			      unsigned int cntr_count)
{
	struct single_state *state = mapper->private_data;
	unsigned int bytes_per_buffer;

	if (cntrs->bytes_per_sample != mapper->bytes_per_sample ||
	    cntrs->samples_per_frame != mapper->samples_per_frame)
		return -EINVAL;

	// Decide method to align frames.
	if (mapper->type == MAPPER_TYPE_DEMUXER) {
		if (mapper->access == SND_PCM_ACCESS_RW_NONINTERLEAVED ||
		    mapper->access == SND_PCM_ACCESS_MMAP_NONINTERLEAVED)
			state->align_frames = align_from_vector;
		else if (mapper->access == SND_PCM_ACCESS_RW_INTERLEAVED ||
			 mapper->access == SND_PCM_ACCESS_MMAP_INTERLEAVED)
			state->align_frames = NULL;
		else
			return -EINVAL;
	} else {
		if (mapper->access == SND_PCM_ACCESS_RW_NONINTERLEAVED ||
		    mapper->access == SND_PCM_ACCESS_MMAP_NONINTERLEAVED)
			state->align_frames = align_to_vector;
		else if (mapper->access == SND_PCM_ACCESS_RW_INTERLEAVED ||
			 mapper->access == SND_PCM_ACCESS_MMAP_INTERLEAVED)
			state->align_frames = NULL;
		else
			return -EINVAL;
	}

	if (state->align_frames) {
		// Allocate intermediate buffer as the same size as a period.
		bytes_per_buffer = mapper->bytes_per_sample *
				   mapper->samples_per_frame *
				   mapper->frames_per_buffer;
		state->buf = malloc(bytes_per_buffer);
		if (state->buf == NULL)
			return -ENOMEM;
		memset(state->buf, 0, bytes_per_buffer);
	}

	return 0;
}

static int single_muxer_process_frames(struct mapper_context *mapper,
				       void *frame_buf,
				       unsigned int *frame_count,
				       struct container_context *cntrs,
				       unsigned int cntr_count)
{
	struct single_state *state = mapper->private_data;
	void *src;
	int err;

	// If need to align PCM frames, process PCM frames to the intermediate
	// buffer once.
	if (!state->align_frames) {
		// The most likely.
		src = frame_buf;
	} else {
		src = state->buf;
	}
	err = container_context_process_frames(cntrs, src, frame_count);
	if (err < 0)
		return err;

	// Unlikely.
	if (src != frame_buf && *frame_count > 0)
		state->align_frames(frame_buf, *frame_count, src,
				    mapper->bytes_per_sample,
				    mapper->samples_per_frame);

	return 0;
}

static int single_demuxer_process_frames(struct mapper_context *mapper,
					 void *frame_buf,
					 unsigned int *frame_count,
					 struct container_context *cntrs,
					 unsigned int cntr_count)
{
	struct single_state *state = mapper->private_data;
	void *dst;

	// If need to align PCM frames, process PCM frames to the intermediate
	// buffer once.
	if (!state->align_frames) {
		// The most likely.
		dst = frame_buf;
	} else {
		state->align_frames(frame_buf, *frame_count, state->buf,
				    mapper->bytes_per_sample,
				    mapper->samples_per_frame);
		dst = state->buf;
	}

	return container_context_process_frames(cntrs, dst, frame_count);
}

static void single_post_process(struct mapper_context *mapper)
{
	struct single_state *state = mapper->private_data;

	if (state->buf)
		free(state->buf);

	state->buf = NULL;
	state->align_frames = NULL;
}

const struct mapper_data mapper_muxer_single = {
	.ops = {
		.pre_process = single_pre_process,
		.process_frames = single_muxer_process_frames,
		.post_process = single_post_process,
	},
	.private_size = sizeof(struct single_state),
};

const struct mapper_data mapper_demuxer_single = {
	.ops = {
		.pre_process = single_pre_process,
		.process_frames = single_demuxer_process_frames,
		.post_process = single_post_process,
	},
	.private_size = sizeof(struct single_state),
};
