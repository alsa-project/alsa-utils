// SPDX-License-Identifier: GPL-2.0
//
// xfer.h - a header for receiver/transmiter of data frames.
//
// Copyright (c) 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp>
//
// Licensed under the terms of the GNU General Public License, version 2.

#ifndef __ALSA_UTILS_AXFER_XFER__H_
#define __ALSA_UTILS_AXFER_XFER__H_

#include "mapper.h"

#include <getopt.h>

enum xfer_type {
	XFER_TYPE_UNSUPPORTED = -1,
	XFER_TYPE_COUNT,
};

struct xfer_ops;

struct xfer_context {
	snd_pcm_stream_t direction;
	enum xfer_type type;
	const struct xfer_ops *ops;
	void *private_data;

	unsigned int verbose;
};

enum xfer_type xfer_type_from_label(const char *label);

int xfer_context_init(struct xfer_context *xfer, enum xfer_type type,
		      snd_pcm_stream_t direction, int argc, char *const *argv);
void xfer_context_destroy(struct xfer_context *xfer);
int xfer_context_pre_process(struct xfer_context *xfer,
			     snd_pcm_format_t *format,
			     unsigned int *samples_per_frame,
			     unsigned int *frames_per_second,
			     snd_pcm_access_t *access,
			     snd_pcm_uframes_t *frames_per_buffer);
int xfer_context_process_frames(struct xfer_context *xfer,
				struct mapper_context *mapper,
				struct container_context *cntrs,
				unsigned int *frame_count);
void xfer_context_pause(struct xfer_context *xfer, bool enable);
void xfer_context_post_process(struct xfer_context *xfer);

// For internal use in 'xfer' module.

struct xfer_ops {
	int (*init)(struct xfer_context *xfer, snd_pcm_stream_t direction);
	int (*parse_opt)(struct xfer_context *xfer, int key, const char *optarg);
	int (*validate_opts)(struct xfer_context *xfer);
	int (*pre_process)(struct xfer_context *xfer, snd_pcm_format_t *format,
			   unsigned int *samples_per_frame,
			   unsigned int *frames_per_second,
			   snd_pcm_access_t *access,
			   snd_pcm_uframes_t *frames_per_buffer);
	int (*process_frames)(struct xfer_context *xfer,
			      unsigned int *frame_count,
			      struct mapper_context *mapper,
			      struct container_context *cntrs);
	void (*post_process)(struct xfer_context *xfer);
	void (*destroy)(struct xfer_context *xfer);
	void (*pause)(struct xfer_context *xfer, bool enable);
};

struct xfer_data {
	const char *s_opts;
	const struct option *l_opts;
	unsigned int l_opts_count;
	struct xfer_ops ops;
	unsigned int private_size;
};

extern const struct xfer_data xfer_alsa;

#endif
