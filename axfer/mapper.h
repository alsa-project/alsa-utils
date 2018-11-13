// SPDX-License-Identifier: GPL-2.0
//
// mapper.h - an interface of muxer/demuxer between buffer with data frames and
//	      formatted files.
//
// Copyright (c) 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp>
//
// Licensed under the terms of the GNU General Public License, version 2.

#ifndef __ALSA_UTILS_AXFER_MAPPER__H_
#define __ALSA_UTILS_AXFER_MAPPER__H_

#include "container.h"

enum mapper_type {
	MAPPER_TYPE_MUXER = 0,
	MAPPER_TYPE_DEMUXER,
	MAPPER_TYPE_COUNT,
};

enum mapper_target {
	MAPPER_TARGET_SINGLE = 0,
	MAPPER_TARGET_MULTIPLE,
	MAPPER_TARGET_COUNT,
};

struct mapper_ops;

struct mapper_context {
	enum mapper_type type;
	enum mapper_target target;
	const struct mapper_ops *ops;
	unsigned int private_size;

	void *private_data;
	unsigned int cntr_count;

	// A part of parameters of PCM substream.
	snd_pcm_access_t access;
	unsigned int bytes_per_sample;
	unsigned int samples_per_frame;
	snd_pcm_uframes_t frames_per_buffer;

	unsigned int verbose;
};

int mapper_context_init(struct mapper_context *mapper,
			enum mapper_type type, unsigned int cntr_count,
			unsigned int verbose);
int mapper_context_pre_process(struct mapper_context *mapper,
			       snd_pcm_access_t access,
			       unsigned int bytes_per_sample,
			       unsigned int samples_per_frame,
			       unsigned int frames_per_buffer,
			       struct container_context *cntrs);
int mapper_context_process_frames(struct mapper_context *mapper,
				  void *frame_buffer,
				  unsigned int *frame_count,
				  struct container_context *cntrs);
void mapper_context_post_process(struct mapper_context *mapper);
void mapper_context_destroy(struct mapper_context *mapper);

// For internal use in 'mapper' module.

struct mapper_ops {
	int (*pre_process)(struct mapper_context *mapper,
			   struct container_context *cntrs,
			   unsigned int cntr_count);
	int (*process_frames)(struct mapper_context *mapper,
			      void *frame_buffer, unsigned int *frame_count,
			      struct container_context *cntrs,
			      unsigned int cntr_count);
	void (*post_process)(struct mapper_context *mapper);
};

struct mapper_data {
	struct mapper_ops ops;
	unsigned int private_size;
};

extern const struct mapper_data mapper_muxer_single;
extern const struct mapper_data mapper_demuxer_single;

extern const struct mapper_data mapper_muxer_multiple;
extern const struct mapper_data mapper_demuxer_multiple;

#endif
