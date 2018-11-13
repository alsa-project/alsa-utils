// SPDX-License-Identifier: GPL-2.0
//
// xfer-libasound.c - receive/transmit frames by alsa-lib.
//
// Copyright (c) 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp>
//
// Licensed under the terms of the GNU General Public License, version 2.

#include "xfer-libasound.h"
#include "misc.h"

#define S_OPTS	"D:"
static const struct option l_opts[] = {
	{"device",		1, 0, 'D'},
};

static int xfer_libasound_init(struct xfer_context *xfer,
			       snd_pcm_stream_t direction)
{
	struct libasound_state *state = xfer->private_data;
	int err;

	err = snd_output_stdio_attach(&state->log, stderr, 0);
	if (err < 0)
		return err;

	err = snd_pcm_hw_params_malloc(&state->hw_params);
	if (err < 0)
		return err;

	return snd_pcm_sw_params_malloc(&state->sw_params);
}

static int xfer_libasound_parse_opt(struct xfer_context *xfer, int key,
				    const char *optarg)
{
	struct libasound_state *state = xfer->private_data;
	int err = 0;

	if (key == 'D')
		state->node_literal = arg_duplicate_string(optarg, &err);
	else
		err = -ENXIO;

	return err;
}

int xfer_libasound_validate_opts(struct xfer_context *xfer)
{
	struct libasound_state *state = xfer->private_data;
	int err = 0;

	state->verbose = xfer->verbose > 1;

	if (state->node_literal == NULL) {
		state->node_literal = strdup("default");
		if (state->node_literal == NULL)
			return -ENOMEM;
	}

	return err;
}

static int set_access_hw_param(struct libasound_state *state)
{
	snd_pcm_access_mask_t *mask;
	int err;

	err = snd_pcm_access_mask_malloc(&mask);
	if (err < 0)
		return err;
	snd_pcm_access_mask_none(mask);
	snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_RW_NONINTERLEAVED);
	err = snd_pcm_hw_params_set_access_mask(state->handle, state->hw_params,
						mask);
	snd_pcm_access_mask_free(mask);

	return err;
}

static int open_handle(struct xfer_context *xfer)
{
	struct libasound_state *state = xfer->private_data;
	int err;

	err = snd_pcm_open(&state->handle, state->node_literal, xfer->direction,
			   0);
	if (err < 0) {
		logging(state, "Fail to open libasound PCM node for %s: %s\n",
			snd_pcm_stream_name(xfer->direction),
			state->node_literal);
		return err;
	}

	err = snd_pcm_hw_params_any(state->handle, state->hw_params);
	if (err < 0)
		return err;

	// TODO: Applying NO_PERIOD_WAKEUP should be done here.

	return set_access_hw_param(state);
}

static int configure_hw_params(struct libasound_state *state,
			       snd_pcm_format_t format,
			       unsigned int samples_per_frame,
			       unsigned int frames_per_second)
{
	int err;

	// Configure sample format.
	if (format == SND_PCM_FORMAT_UNKNOWN) {
		snd_pcm_format_mask_t *mask;

		err = snd_pcm_format_mask_malloc(&mask);
		if (err < 0)
			return err;
		snd_pcm_hw_params_get_format_mask(state->hw_params, mask);
		for (format = 0; format <= SND_PCM_FORMAT_LAST; ++format) {
			if (snd_pcm_format_mask_test(mask, format))
				break;
		}
		snd_pcm_format_mask_free(mask);
		if (format > SND_PCM_FORMAT_LAST) {
			logging(state,
				"Any sample format is not available.\n");
			return -EINVAL;
		}
	}
	err = snd_pcm_hw_params_set_format(state->handle, state->hw_params,
					   format);
	if (err < 0) {
		logging(state,
			"Sample format '%s' is not available: %s\n",
			snd_pcm_format_name(format), snd_strerror(err));
		return err;
	}

	// Configure channels.
	if (samples_per_frame == 0) {
		err = snd_pcm_hw_params_get_channels_min(state->hw_params,
							 &samples_per_frame);
		if (err < 0) {
			logging(state,
				"Any channel number is not available.\n");
			return err;
		}
	}
	err = snd_pcm_hw_params_set_channels(state->handle, state->hw_params,
					     samples_per_frame);
	if (err < 0) {
		logging(state,
			"Channels count '%u' is not available: %s\n",
			samples_per_frame, snd_strerror(err));
		return err;
	}

	// Configure rate.
	if (frames_per_second == 0) {
		err = snd_pcm_hw_params_get_rate_min(state->hw_params,
						     &frames_per_second, NULL);
		if (err < 0) {
			logging(state,
				"Any rate is not available.\n");
			return err;
		}

	}
	err = snd_pcm_hw_params_set_rate(state->handle, state->hw_params,
					 frames_per_second, 0);
	if (err < 0) {
		logging(state,
			"Sampling rate '%u' is not available: %s\n",
			frames_per_second, snd_strerror(err));
		return err;
	}

	return snd_pcm_hw_params(state->handle, state->hw_params);
}

static int retrieve_actual_hw_params(snd_pcm_hw_params_t *hw_params,
				     snd_pcm_format_t *format,
				     unsigned int *samples_per_frame,
				     unsigned int *frames_per_second,
				     snd_pcm_access_t *access,
				     snd_pcm_uframes_t *frames_per_buffer)
{
	int err;

	err = snd_pcm_hw_params_get_format(hw_params, format);
	if (err < 0)
		return err;

	err = snd_pcm_hw_params_get_channels(hw_params,
					     samples_per_frame);
	if (err < 0)
		return err;

	err = snd_pcm_hw_params_get_rate(hw_params, frames_per_second,
					 NULL);
	if (err < 0)
		return err;

	err = snd_pcm_hw_params_get_access(hw_params, access);
	if (err < 0)
		return err;

	return snd_pcm_hw_params_get_buffer_size(hw_params, frames_per_buffer);
}

static int configure_sw_params(struct libasound_state *state,
			       unsigned int frames_per_second,
			       unsigned int frames_per_buffer)
{
	return snd_pcm_sw_params(state->handle, state->sw_params);
}

static int xfer_libasound_pre_process(struct xfer_context *xfer,
				      snd_pcm_format_t *format,
				      unsigned int *samples_per_frame,
				      unsigned int *frames_per_second,
				      snd_pcm_access_t *access,
				      snd_pcm_uframes_t *frames_per_buffer)
{
	struct libasound_state *state = xfer->private_data;
	int err;

	err = open_handle(xfer);
	if (err < 0)
		return -ENXIO;

	err = configure_hw_params(state, *format, *samples_per_frame,
				  *frames_per_second);
	if (err < 0) {
		logging(state, "Current hardware parameters:\n");
		snd_pcm_hw_params_dump(state->hw_params, state->log);
		return err;
	}

	// Retrieve actual parameters.
	err = retrieve_actual_hw_params(state->hw_params, format,
					samples_per_frame, frames_per_second,
					access, frames_per_buffer);
	if (err < 0)
		return err;

	// Query software parameters.
	err = snd_pcm_sw_params_current(state->handle, state->sw_params);
	if (err < 0)
		return err;

	// Assign I/O operation.
	if (*access == SND_PCM_ACCESS_RW_INTERLEAVED ||
	    *access == SND_PCM_ACCESS_RW_NONINTERLEAVED) {
		state->ops = &xfer_libasound_irq_rw_ops;
	} else {
		return -ENXIO;
	}
	if (state->ops->private_size > 0) {
		state->private_data = malloc(state->ops->private_size);
		if (state->private_data == NULL)
			return -ENOMEM;
		memset(state->private_data, 0, state->ops->private_size);
	}
	err = state->ops->pre_process(state);
	if (err < 0)
		return err;

	err = configure_sw_params(state, *frames_per_second,
				  *frames_per_buffer);
	if (err < 0) {
		logging(state, "Current software parameters:\n");
		snd_pcm_sw_params_dump(state->sw_params, state->log);
		return err;
	}

	if (xfer->verbose > 0)
		snd_pcm_dump(state->handle, state->log);

	return 0;
}

static int xfer_libasound_process_frames(struct xfer_context *xfer,
					 unsigned int *frame_count,
					 struct mapper_context *mapper,
					 struct container_context *cntrs)
{
	struct libasound_state *state = xfer->private_data;
	int err;

	if (state->handle == NULL)
		return -ENXIO;

	err = state->ops->process_frames(state, frame_count, mapper, cntrs);
	if (err < 0) {
		if (err == -EAGAIN)
			return err;
		if (err == -EPIPE) {
			// Recover the stream and continue processing
			// immediately. In this program -EPIPE comes from
			// libasound implementation instead of file I/O.
			err = snd_pcm_prepare(state->handle);
		}

		if (err < 0) {
			// TODO: -EIO from libasound for hw PCM node means
			// that IRQ disorder. This should be reported to help
			// developers for drivers.
			logging(state, "Fail to process frames: %s\n",
				snd_strerror(err));
		}
	}

	return err;
}

static void xfer_libasound_pause(struct xfer_context *xfer, bool enable)
{
	struct libasound_state *state = xfer->private_data;
	snd_pcm_state_t s = snd_pcm_state(state->handle);
	int err;

	if (state->handle == NULL)
		return;

	if (enable) {
		if (s != SND_PCM_STATE_RUNNING)
			return;
	} else {
		if (s != SND_PCM_STATE_PAUSED)
			return;
	}

	// Not supported. Leave the substream to enter XRUN state.
	if (!snd_pcm_hw_params_can_pause(state->hw_params))
		return;

	err = snd_pcm_pause(state->handle, enable);
	if (err < 0 && state->verbose) {
		logging(state, "snd_pcm_pause(): %s\n", snd_strerror(err));
	}
}

static void xfer_libasound_post_process(struct xfer_context *xfer)
{
	struct libasound_state *state = xfer->private_data;
	snd_pcm_state_t pcm_state;
	int err;

	if (state->handle == NULL)
		return;

	pcm_state = snd_pcm_state(state->handle);
	if (pcm_state != SND_PCM_STATE_OPEN &&
	    pcm_state != SND_PCM_STATE_DISCONNECTED) {
		if (snd_pcm_stream(state->handle) == SND_PCM_STREAM_CAPTURE) {
			err = snd_pcm_drop(state->handle);
			if (err < 0)
				logging(state, "snd_pcm_drop(): %s\n",
				       snd_strerror(err));
		} else {
			err = snd_pcm_drain(state->handle);
			if (err < 0)
				logging(state, "snd_pcm_drain(): %s\n",
				       snd_strerror(err));
		}
	}

	err = snd_pcm_hw_free(state->handle);
	if (err < 0)
		logging(state, "snd_pcm_hw_free(): %s\n", snd_strerror(err));

	snd_pcm_close(state->handle);
	state->handle = NULL;

	if (state->ops && state->ops->post_process)
		state->ops->post_process(state);
	free(state->private_data);
	state->private_data = NULL;

	// Free cache of content for configuration files so that memory leaks
	// are not detected.
	snd_config_update_free_global();
}

static void xfer_libasound_destroy(struct xfer_context *xfer)
{
	struct libasound_state *state = xfer->private_data;

	free(state->node_literal);
	state->node_literal = NULL;

	if (state->hw_params)
		snd_pcm_hw_params_free(state->hw_params);
	if (state->sw_params)
		snd_pcm_sw_params_free(state->sw_params);
	state->hw_params = NULL;
	state->sw_params = NULL;

	if (state->log)
		snd_output_close(state->log);
	state->log = NULL;
}

const struct xfer_data xfer_libasound = {
	.s_opts = S_OPTS,
	.l_opts = l_opts,
	.l_opts_count = ARRAY_SIZE(l_opts),
	.ops = {
		.init		= xfer_libasound_init,
		.parse_opt	= xfer_libasound_parse_opt,
		.validate_opts	= xfer_libasound_validate_opts,
		.pre_process	= xfer_libasound_pre_process,
		.process_frames	= xfer_libasound_process_frames,
		.pause		= xfer_libasound_pause,
		.post_process	= xfer_libasound_post_process,
		.destroy	= xfer_libasound_destroy,
	},
	.private_size = sizeof(struct libasound_state),
};
