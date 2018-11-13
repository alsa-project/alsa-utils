// SPDX-License-Identifier: GPL-2.0
//
// xfer-libasound.h - a header for receiver/transmitter of frames by alsa-lib.
//
// Copyright (c) 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp>
//
// Licensed under the terms of the GNU General Public License, version 2.

#ifndef __ALSA_UTILS_AXFER_XFER_LIBASOUND__H_
#define __ALSA_UTILS_AXFER_XFER_LIBASOUND__H_

#include "xfer.h"

#define logging(state, ...) \
	snd_output_printf(state->log, __VA_ARGS__)

struct xfer_libasound_ops;

struct libasound_state {
	snd_pcm_t *handle;

	snd_output_t *log;
	snd_pcm_hw_params_t *hw_params;
	snd_pcm_sw_params_t *sw_params;

	const struct xfer_libasound_ops *ops;
	void *private_data;

	bool verbose;

	char *node_literal;

	bool finish_at_xrun:1;
	bool nonblock:1;
	bool mmap:1;
};

// For internal use in 'libasound' module.

struct xfer_libasound_ops {
	int (*pre_process)(struct libasound_state *state);
	int (*process_frames)(struct libasound_state *state,
			      unsigned int *frame_count,
			      struct mapper_context *mapper,
			      struct container_context *cntrs);
	void (*post_process)(struct libasound_state *state);
	unsigned int private_size;
};

extern const struct xfer_libasound_ops xfer_libasound_irq_rw_ops;

extern const struct xfer_libasound_ops xfer_libasound_irq_mmap_r_ops;
extern const struct xfer_libasound_ops xfer_libasound_irq_mmap_w_ops;

#endif
