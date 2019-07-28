// SPDX-License-Identifier: GPL-2.0
//
// mapper-multiple.c - a muxer/demuxer for multiple containers.
//
// Copyright (c) 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp>
//
// Licensed under the terms of the GNU General Public License, version 2.

#include "mapper.h"
#include "misc.h"

struct multiple_state {
	void (*align_frames)(void *frame_buf, unsigned int frame_count,
			     char **buf, unsigned int bytes_per_sample,
			     struct container_context *cntrs,
			     unsigned int cntr_count);
	char **bufs;
	unsigned int cntr_count;
};

static void align_to_i(void *frame_buf, unsigned int frame_count,
		       char **src_bufs, unsigned int bytes_per_sample,
		       struct container_context *cntrs, unsigned int cntr_count)
{
	char *dst = frame_buf;
	char *src;
	unsigned int dst_pos;
	unsigned int src_pos;
	struct container_context *cntr;
	int i, j;

	// src: first channel in each of interleaved buffers in containers =>
	// dst:interleaved.
	for (i = 0; i < cntr_count; ++i) {
		src = src_bufs[i];
		cntr = cntrs + i;

		for (j = 0; j < frame_count; ++j) {
			// Use first src channel for each of dst channel.
			src_pos = bytes_per_sample * cntr->samples_per_frame * j;
			dst_pos = bytes_per_sample * (cntr_count * j + i);

			memcpy(dst + dst_pos, src + src_pos, bytes_per_sample);
		}
	}
}

static void align_from_i(void *frame_buf, unsigned int frame_count,
			 char **dst_bufs, unsigned int bytes_per_sample,
			 struct container_context *cntrs,
			 unsigned int cntr_count)
{
	char *src = frame_buf;
	char *dst;
	unsigned int src_pos;
	unsigned int dst_pos;
	struct container_context *cntr;
	int i, j;

	for (i = 0; i < cntr_count; ++i) {
		dst = dst_bufs[i];
		cntr = cntrs + i;

		for (j = 0; j < frame_count; ++j) {
			// Use first src channel for each of dst channel.
			src_pos = bytes_per_sample * (cntr_count * j + i);
			dst_pos = bytes_per_sample * cntr->samples_per_frame * j;

			memcpy(dst + dst_pos, src + src_pos, bytes_per_sample);
		}
	}
}

static int multiple_pre_process(struct mapper_context *mapper,
				struct container_context *cntrs,
				unsigned int cntr_count)
{
	struct multiple_state *state = mapper->private_data;
	struct container_context *cntr;
	int i;

	// Additionally, format of samples in the containers should be the same
	// as the format in PCM substream.
	for (i = 0; i < cntr_count; ++i) {
		cntr = cntrs + i;
		if (mapper->bytes_per_sample != cntr->bytes_per_sample)
			return -EINVAL;
	}
	state->cntr_count = cntr_count;

	// Decide method to align frames.
	if (mapper->type == MAPPER_TYPE_DEMUXER) {
		if (mapper->access == SND_PCM_ACCESS_RW_INTERLEAVED ||
		    mapper->access == SND_PCM_ACCESS_MMAP_INTERLEAVED)
			state->align_frames = align_from_i;
		else if (mapper->access == SND_PCM_ACCESS_RW_NONINTERLEAVED ||
			 mapper->access == SND_PCM_ACCESS_MMAP_NONINTERLEAVED)
			state->align_frames = NULL;
		else
			return -EINVAL;
	} else {
		if (mapper->access == SND_PCM_ACCESS_RW_INTERLEAVED ||
		    mapper->access == SND_PCM_ACCESS_MMAP_INTERLEAVED)
			state->align_frames = align_to_i;
		else if (mapper->access == SND_PCM_ACCESS_RW_NONINTERLEAVED ||
			 mapper->access == SND_PCM_ACCESS_MMAP_NONINTERLEAVED)
			state->align_frames = NULL;
		else
			return -EINVAL;
	}

	if (state->align_frames) {
		// Furthermore, in demuxer case, each container should be
		// configured to store one sample per frame.
		if (mapper->type == MAPPER_TYPE_DEMUXER) {
			for (i = 0; i < cntr_count; ++i) {
				cntr = cntrs + i;
				if (cntr->samples_per_frame != 1)
					return -EINVAL;
			}
		}

		state->bufs = calloc(cntr_count, sizeof(char *));
		if (state->bufs == NULL)
			return -ENOMEM;

		for (i = 0; i < cntr_count; ++i) {
			unsigned int bytes_per_buffer;

			// Allocate intermediate buffer as the same size as a
			// period for each of containers.
			cntr = cntrs + i;

			bytes_per_buffer = mapper->bytes_per_sample *
					   cntr->samples_per_frame *
					   mapper->frames_per_buffer;

			state->bufs[i] = malloc(bytes_per_buffer);
			if (state->bufs[i] == NULL)
				return -ENOMEM;
			memset(state->bufs[i], 0, bytes_per_buffer);
		}
	}

	return 0;
}

static int process_containers(char **src_bufs, unsigned int *frame_count,
			      struct container_context *cntrs,
			      unsigned int cntr_count)
{
	struct container_context *cntr;
	char *src;
	int i;
	int err = 0;

	// TODO: arrangement for *frame_count.
	for (i = 0; i < cntr_count; ++i) {
		cntr = &cntrs[i];
		src = src_bufs[i];

		err = container_context_process_frames(cntr, src, frame_count);
		if (err < 0)
			break;
	}

	return err;
}

static int multiple_muxer_process_frames(struct mapper_context *mapper,
					 void *frame_buf,
					 unsigned int *frame_count,
					 struct container_context *cntrs,
					 unsigned int cntr_count)
{
	struct multiple_state *state = mapper->private_data;
	char **src_bufs;
	int err;

	// If need to align PCM frames, process PCM frames to the intermediate
	// buffer once.
	if (!state->align_frames) {
		// The most likely.
		src_bufs = frame_buf;
	} else {
		src_bufs = state->bufs;
	}
	err = process_containers(src_bufs, frame_count, cntrs, cntr_count);
	if (err < 0)
		return err;

	// Unlikely.
	if (src_bufs != frame_buf && *frame_count > 0) {
		state->align_frames(frame_buf, *frame_count, src_bufs,
				    mapper->bytes_per_sample, cntrs,
				    cntr_count);
	}

	return 0;
}

static int multiple_demuxer_process_frames(struct mapper_context *mapper,
					   void *frame_buf,
					   unsigned int *frame_count,
					   struct container_context *cntrs,
					   unsigned int cntr_count)
{
	struct multiple_state *state = mapper->private_data;
	char **dst_bufs;

	// If need to align PCM frames, process PCM frames to the intermediate
	// buffer once.
	if (!state->align_frames) {
		// The most likely.
		dst_bufs = frame_buf;
	} else {
		dst_bufs = state->bufs;
		state->align_frames(frame_buf, *frame_count, dst_bufs,
				    mapper->bytes_per_sample, cntrs,
				    cntr_count);
	}

	return process_containers(dst_bufs, frame_count, cntrs, cntr_count);
}

static void multiple_post_process(struct mapper_context *mapper)
{
	struct multiple_state *state = mapper->private_data;
	int i;

	if (state->bufs) {
		for (i = 0; i < state->cntr_count; ++i) {
			if (state->bufs[i])
				free(state->bufs[i]);
		}
		free(state->bufs);
	}

	state->bufs = NULL;
	state->align_frames = NULL;
}

const struct mapper_data mapper_muxer_multiple = {
	.ops = {
		.pre_process = multiple_pre_process,
		.process_frames = multiple_muxer_process_frames,
		.post_process = multiple_post_process,
	},
	.private_size = sizeof(struct multiple_state),
};

const struct mapper_data mapper_demuxer_multiple = {
	.ops = {
		.pre_process = multiple_pre_process,
		.process_frames = multiple_demuxer_process_frames,
		.post_process = multiple_post_process,
	},
	.private_size = sizeof(struct multiple_state),
};
