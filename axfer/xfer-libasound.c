// SPDX-License-Identifier: GPL-2.0
//
// xfer-libasound.c - receive/transmit frames by alsa-lib.
//
// Copyright (c) 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp>
//
// Licensed under the terms of the GNU General Public License, version 2.

#include "xfer-libasound.h"
#include "misc.h"

static const char *const sched_model_labels [] = {
	[SCHED_MODEL_IRQ] = "irq",
	[SCHED_MODEL_TIMER] = "timer",
};

enum no_short_opts {
        // 200 or later belong to non us-ascii character set.
	OPT_PERIOD_SIZE = 200,
	OPT_BUFFER_SIZE,
	OPT_WAITER_TYPE,
	OPT_SCHED_MODEL,
	OPT_DISABLE_RESAMPLE,
	OPT_DISABLE_CHANNELS,
	OPT_DISABLE_FORMAT,
	OPT_DISABLE_SOFTVOL,
	OPT_FATAL_ERRORS,
	OPT_TEST_NOWAIT,
	// Obsoleted.
	OPT_TEST_POSITION,
	OPT_TEST_COEF,
};

#define S_OPTS	"D:NMF:B:A:R:T:m:"
static const struct option l_opts[] = {
	{"device",		1, 0, 'D'},
	{"nonblock",		0, 0, 'N'},
	{"mmap",		0, 0, 'M'},
	{"period-time",		1, 0, 'F'},
	{"buffer-time",		1, 0, 'B'},
	{"period-size",		1, 0, OPT_PERIOD_SIZE},
	{"buffer-size",		1, 0, OPT_BUFFER_SIZE},
	{"avail-min",		1, 0, 'A'},
	{"start-delay",		1, 0, 'R'},
	{"stop-delay",		1, 0, 'T'},
	{"waiter-type",		1, 0, OPT_WAITER_TYPE},
	{"sched-model",		1, 0, OPT_SCHED_MODEL},
	// For plugins in alsa-lib.
	{"disable-resample",	0, 0, OPT_DISABLE_RESAMPLE},
	{"disable-channels",	0, 0, OPT_DISABLE_CHANNELS},
	{"disable-format",	0, 0, OPT_DISABLE_FORMAT},
	{"disable-softvol",	0, 0, OPT_DISABLE_SOFTVOL},
	// For debugging.
	{"fatal-errors",	0, 0, OPT_FATAL_ERRORS},
	{"test-nowait",		0, 0, OPT_TEST_NOWAIT},
	// Obsoleted.
	{"chmap",		1, 0, 'm'},
	{"test-position",	0, 0, OPT_TEST_POSITION},
	{"test-coef",		1, 0, OPT_TEST_COEF},
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
	else if (key == 'N')
		state->nonblock = true;
	else if (key == 'M')
		state->mmap = true;
	else if (key == 'F')
		state->msec_per_period = arg_parse_decimal_num(optarg, &err);
	else if (key == 'B')
		state->msec_per_buffer = arg_parse_decimal_num(optarg, &err);
	else if (key == OPT_PERIOD_SIZE)
		state->frames_per_period = arg_parse_decimal_num(optarg, &err);
	else if (key == OPT_BUFFER_SIZE)
		state->frames_per_buffer = arg_parse_decimal_num(optarg, &err);
	else if (key == 'A')
		state->msec_for_avail_min = arg_parse_decimal_num(optarg, &err);
	else if (key == 'R')
		state->msec_for_start_threshold = arg_parse_decimal_num(optarg, &err);
	else if (key == 'T')
		state->msec_for_stop_threshold = arg_parse_decimal_num(optarg, &err);
	else if (key == OPT_WAITER_TYPE)
		state->waiter_type_literal = arg_duplicate_string(optarg, &err);
	else if (key == OPT_SCHED_MODEL)
		state->sched_model_literal = arg_duplicate_string(optarg, &err);
	else if (key == OPT_DISABLE_RESAMPLE)
		state->no_auto_resample = true;
	else if (key == OPT_DISABLE_CHANNELS)
		state->no_auto_channels = true;
	else if (key == OPT_DISABLE_FORMAT)
		state->no_auto_format = true;
	else if (key == OPT_DISABLE_SOFTVOL)
		state->no_softvol = true;
	else if (key == 'm' ||
		 key == OPT_TEST_POSITION ||
		 key == OPT_TEST_COEF)
		err = -EINVAL;
	else if (key == OPT_FATAL_ERRORS)
		state->finish_at_xrun = true;
	else if (key == OPT_TEST_NOWAIT)
		state->test_nowait = true;
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

	if (state->mmap && state->nonblock) {
		fprintf(stderr,
			"An option for mmap operation should not be used with "
			"nonblocking option.\n");
		return -EINVAL;
	}

	if (state->test_nowait) {
		if (!state->nonblock && !state->mmap) {
			fprintf(stderr,
				"An option for nowait test should be used with "
				"nonblock or mmap options.\n");
			return -EINVAL;
		}
	}

	if (state->msec_per_period > 0 && state->msec_per_buffer > 0) {
		if (state->msec_per_period > state->msec_per_buffer) {
			state->msec_per_period = state->msec_per_buffer;
			state->msec_per_buffer = 0;
		}
	}

	if (state->frames_per_period > 0 && state->frames_per_buffer > 0) {
		if (state->frames_per_period > state->frames_per_buffer) {
			state->frames_per_period = state->frames_per_buffer;
			state->frames_per_buffer = 0;
		}
	}

	state->sched_model = SCHED_MODEL_IRQ;
	if (state->sched_model_literal != NULL) {
		if (!strcmp(state->sched_model_literal, "timer")) {
			state->sched_model = SCHED_MODEL_TIMER;
			state->mmap = true;
			state->nonblock = true;
		}
	}

	if (state->waiter_type_literal != NULL) {
		if (state->test_nowait) {
			fprintf(stderr,
				"An option for waiter type should not be "
				"used with nowait test option.\n");
			return -EINVAL;
		}
		if (!state->nonblock && !state->mmap) {
			fprintf(stderr,
				"An option for waiter type should be used "
				"with nonblock or mmap or timer-based "
				"scheduling options.\n");
			return -EINVAL;
		}
		state->waiter_type =
			waiter_type_from_label(state->waiter_type_literal);
	} else {
		state->waiter_type = WAITER_TYPE_DEFAULT;
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
	if (state->mmap) {
		snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_INTERLEAVED);
		snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_NONINTERLEAVED);
	} else {
		snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_RW_INTERLEAVED);
		snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_RW_NONINTERLEAVED);
	}
	err = snd_pcm_hw_params_set_access_mask(state->handle, state->hw_params,
						mask);
	snd_pcm_access_mask_free(mask);

	return err;
}

static int disable_period_wakeup(struct libasound_state *state)
{
	int err;

	if (snd_pcm_type(state->handle) != SND_PCM_TYPE_HW) {
		logging(state,
			"Timer-based scheduling is only available for 'hw' "
			"PCM plugin.\n");
		return -ENXIO;
	}

	if (!snd_pcm_hw_params_can_disable_period_wakeup(state->hw_params)) {
		logging(state,
			"This hardware doesn't support the mode of no-period-"
			"wakeup. In this case, timer-based scheduling is not "
			"available.\n");
		return -EIO;
	}

	err = snd_pcm_hw_params_set_period_wakeup(state->handle,
						  state->hw_params, 0);
	if (err < 0) {
		logging(state,
			"Fail to disable period wakeup so that the hardware "
			"generates no IRQs during transmission of data "
			"frames.\n");
	}

	return err;
}

static int open_handle(struct xfer_context *xfer)
{
	struct libasound_state *state = xfer->private_data;
	int mode = 0;
	int err;

	if (state->nonblock)
		mode |= SND_PCM_NONBLOCK;
	if (state->no_auto_resample)
		mode |= SND_PCM_NO_AUTO_RESAMPLE;
	if (state->no_auto_channels)
		mode |= SND_PCM_NO_AUTO_CHANNELS;
	if (state->no_auto_format)
		mode |= SND_PCM_NO_AUTO_FORMAT;
	if (state->no_softvol)
		mode |= SND_PCM_NO_SOFTVOL;

	err = snd_pcm_open(&state->handle, state->node_literal, xfer->direction,
			   mode);
	if (err < 0) {
		logging(state, "Fail to open libasound PCM node for %s: %s\n",
			snd_pcm_stream_name(xfer->direction),
			state->node_literal);
		return err;
	}

	if ((state->nonblock || state->mmap) && !state->test_nowait)
		state->use_waiter = true;

	err = snd_pcm_hw_params_any(state->handle, state->hw_params);
	if (err < 0)
		return err;

	if (state->sched_model == SCHED_MODEL_TIMER) {
		err = disable_period_wakeup(state);
		if (err < 0)
			return err;
	}

	if (xfer->dump_hw_params) {
		logging(state, "Available HW Params of node: %s\n",
			snd_pcm_name(state->handle));
		snd_pcm_hw_params_dump(state->hw_params, state->log);
		// TODO: there're more parameters which are not dumped by
		// alsa-lib.
		return 0;
	}

	return set_access_hw_param(state);
}

static int prepare_waiter(struct libasound_state *state)
{
	unsigned int pfd_count;
	int err;

	// Nothing to do for dafault waiter (=snd_pcm_wait()).
	if (state->waiter_type == WAITER_TYPE_DEFAULT)
		return 0;

	err = snd_pcm_poll_descriptors_count(state->handle);
	if (err < 0)
		return err;
	if (err == 0)
		return -ENXIO;
	pfd_count = (unsigned int)err;

	state->waiter = malloc(sizeof(*state->waiter));
	if (state->waiter == NULL)
		return -ENOMEM;

	err = waiter_context_init(state->waiter, state->waiter_type, pfd_count);
	if (err < 0)
		return err;

	err = snd_pcm_poll_descriptors(state->handle, state->waiter->pfds,
				       pfd_count);
	if (err < 0)
		return err;

	return waiter_context_prepare(state->waiter);
}

int xfer_libasound_wait_event(struct libasound_state *state, int timeout_msec,
			      unsigned short *revents)
{
	int count;

	if (state->waiter_type != WAITER_TYPE_DEFAULT) {
		struct waiter_context *waiter = state->waiter;
		int err;

		count = waiter_context_wait_event(waiter, timeout_msec);
		if (count < 0)
			return count;
		if (count == 0 && timeout_msec > 0)
			return -ETIMEDOUT;

		err = snd_pcm_poll_descriptors_revents(state->handle,
				waiter->pfds, waiter->pfd_count, revents);
		if (err < 0)
			return err;
	} else {
		count = snd_pcm_wait(state->handle, timeout_msec);
		if (count < 0)
			return count;
		if (count == 0 && timeout_msec > 0)
			return -ETIMEDOUT;

		if (snd_pcm_stream(state->handle) == SND_PCM_STREAM_PLAYBACK)
			*revents = POLLOUT;
		else
			*revents = POLLIN;
	}

	return 0;
}

static int configure_hw_params(struct libasound_state *state,
			       snd_pcm_format_t format,
			       unsigned int samples_per_frame,
			       unsigned int frames_per_second,
			       unsigned int msec_per_period,
			       unsigned int msec_per_buffer,
			       snd_pcm_uframes_t frames_per_period,
			       snd_pcm_uframes_t frames_per_buffer)
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

	// Keep one of 'frames_per_buffer' and 'msec_per_buffer'.
	if (frames_per_buffer == 0) {
		if (msec_per_buffer == 0) {
			err = snd_pcm_hw_params_get_buffer_time_max(
				state->hw_params, &msec_per_buffer, NULL);
			if (err < 0) {
				logging(state,
					"The maximum msec per buffer is not "
					"available.\n");
				return err;
			}
			if (msec_per_buffer > 500000)
				msec_per_buffer = 500000;
		}
	} else if (msec_per_buffer > 0) {
		uint64_t msec;

		msec = 1000000 * frames_per_buffer / frames_per_second;
		if (msec < msec_per_buffer)
			msec_per_buffer = 0;
	}

	// Keep one of 'frames_per_period' and 'msec_per_period'.
	if (frames_per_period == 0) {
		if (msec_per_period == 0) {
			if (msec_per_buffer > 0)
				msec_per_period = msec_per_buffer / 4;
			else
				frames_per_period = frames_per_buffer / 4;
		}
	} else if (msec_per_period > 0) {
		uint64_t msec;

		msec = 1000000 * frames_per_period / frames_per_second;
		if (msec < msec_per_period)
			msec_per_period = 0;
	}

	if (msec_per_period) {
		err = snd_pcm_hw_params_set_period_time_near(state->handle,
				state->hw_params, &msec_per_period, NULL);
		if (err < 0) {
			logging(state,
				"Fail to configure period time: %u msec\n",
				msec_per_period);
			return err;
		}
	} else {
		err = snd_pcm_hw_params_set_period_size_near(state->handle,
				state->hw_params, &frames_per_period, NULL);
		if (err < 0) {
			logging(state,
				"Fail to configure period size: %lu frames\n",
				frames_per_period);
			return err;
		}
	}

	if (msec_per_buffer) {
		err = snd_pcm_hw_params_set_buffer_time_near(state->handle,
				state->hw_params, &msec_per_buffer, NULL);
		if (err < 0) {
			logging(state,
				"Fail to configure buffer time: %u msec\n",
				msec_per_buffer);
			return err;
		}
	} else {
		err = snd_pcm_hw_params_set_buffer_size_near(state->handle,
					state->hw_params, &frames_per_buffer);
		if (err < 0) {
			logging(state,
				"Fail to configure buffer size: %lu frames\n",
				frames_per_buffer);
			return err;
		}
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
			       unsigned int frames_per_buffer,
			       unsigned int msec_for_avail_min,
			       unsigned int msec_for_start_threshold,
			       unsigned int msec_for_stop_threshold)
{
	snd_pcm_uframes_t frame_count;
	int err;

	if (msec_for_avail_min > 0) {
		frame_count = msec_for_avail_min * frames_per_second / 1000000;
		if (frame_count == 0 || frame_count > frames_per_buffer) {
			logging(state,
				"The msec for 'avail_min' is too %s: %u "
				"msec (%lu frames at %u).\n",
				frame_count == 0 ? "small" : "large",
				msec_for_avail_min, frame_count,
				frames_per_second);
			return -EINVAL;
		}
		err = snd_pcm_sw_params_set_avail_min(state->handle,
						state->sw_params, frame_count);
		if (err < 0) {
			logging(state,
				"Fail to configure 'avail-min'.\n");
			return -EINVAL;
		}
	}

	if (msec_for_start_threshold > 0) {
		frame_count = msec_for_start_threshold * frames_per_second /
			      1000000;
		if (frame_count == 0 || frame_count > frames_per_buffer) {
			logging(state,
				"The msec for 'start-delay' is too %s: %u "
				"msec (%lu frames at %u).\n",
				frame_count == 0 ? "small" : "large",
				msec_for_start_threshold, frame_count,
				frames_per_second);
			return -EINVAL;
		}
		err = snd_pcm_sw_params_set_start_threshold(state->handle,
						state->sw_params, frame_count);
		if (err < 0) {
			logging(state,
				"Fail to configure 'start-delay'.\n");
			return -EINVAL;
		}
	}

	if (msec_for_stop_threshold > 0) {
		frame_count = msec_for_stop_threshold * frames_per_second /
			      1000000;
		if (frame_count == 0 || frame_count > frames_per_buffer) {
			logging(state,
				"The msec for 'stop-delay' is too %s: %u "
				"msec (%lu frames at %u).\n",
				frame_count == 0 ? "small" : "large",
				msec_for_stop_threshold, frame_count,
				frames_per_second);
			return -EINVAL;
		}
		err = snd_pcm_sw_params_set_stop_threshold(state->handle,
						state->sw_params, frame_count);
		if (err < 0) {
			logging(state,
				"Fail to configure 'stop-delay'.\n");
			return -EINVAL;
		}
	}

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
	unsigned int flag;
	int err;

	err = open_handle(xfer);
	if (err < 0)
		return -ENXIO;

	err = configure_hw_params(state, *format, *samples_per_frame,
				  *frames_per_second,
				  state->msec_per_period,
				  state->msec_per_buffer,
				  state->frames_per_period,
				  state->frames_per_buffer);
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
	err = snd_pcm_hw_params_get_period_wakeup(state->handle,
						  state->hw_params, &flag);
	if (err < 0)
		return err;

	if (flag) {
		if (*access == SND_PCM_ACCESS_RW_INTERLEAVED ||
		    *access == SND_PCM_ACCESS_RW_NONINTERLEAVED) {
			state->ops = &xfer_libasound_irq_rw_ops;
		} else if (*access == SND_PCM_ACCESS_MMAP_INTERLEAVED ||
			   *access == SND_PCM_ACCESS_MMAP_NONINTERLEAVED) {
			if (snd_pcm_stream(state->handle) == SND_PCM_STREAM_CAPTURE)
				state->ops = &xfer_libasound_irq_mmap_r_ops;
			else
				state->ops = &xfer_libasound_irq_mmap_w_ops;
		} else {
			return -ENXIO;
		}
	} else {
		if (*access == SND_PCM_ACCESS_MMAP_INTERLEAVED ||
		    *access == SND_PCM_ACCESS_MMAP_NONINTERLEAVED) {
			if (snd_pcm_stream(state->handle) == SND_PCM_STREAM_CAPTURE)
				state->ops = &xfer_libasound_timer_mmap_r_ops;
			else
				state->ops = &xfer_libasound_timer_mmap_w_ops;
		} else {
			return -ENXIO;
		}
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
				  *frames_per_buffer,
				  state->msec_for_avail_min,
				  state->msec_for_start_threshold,
				  state->msec_for_stop_threshold);
	if (err < 0) {
		logging(state, "Current software parameters:\n");
		snd_pcm_sw_params_dump(state->sw_params, state->log);
		return err;
	}

	if (xfer->verbose > 0) {
		snd_pcm_dump(state->handle, state->log);
		logging(state, "Scheduling model:\n");
		logging(state, "  %s\n", sched_model_labels[state->sched_model]);
	}

	if (state->use_waiter) {
		// NOTE: This should be after configuring sw_params due to
		// timer descriptor for time-based scheduling model.
		err = prepare_waiter(state);
		if (err < 0)
			return err;

		if (xfer->verbose > 0) {
			logging(state, "Waiter type:\n");
			logging(state,
				"  %s\n",
				waiter_label_from_type(state->waiter_type));
		}
	}

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
		if (err == -EPIPE && !state->finish_at_xrun) {
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
		if (snd_pcm_stream(state->handle) == SND_PCM_STREAM_CAPTURE ||
		    state->ops == &xfer_libasound_timer_mmap_w_ops) {
			err = snd_pcm_drop(state->handle);
			if (err < 0)
				logging(state, "snd_pcm_drop(): %s\n",
				       snd_strerror(err));
		} else {
			// TODO: this is a bug in kernel land.
			if (state->nonblock)
				snd_pcm_nonblock(state->handle, 0);
			err = snd_pcm_drain(state->handle);
			if (state->nonblock)
				snd_pcm_nonblock(state->handle, 1);
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
	free(state->waiter_type_literal);
	free(state->sched_model_literal);
	state->node_literal = NULL;
	state->waiter_type_literal = NULL;
	state->sched_model_literal = NULL;

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

static void xfer_libasound_help(struct xfer_context *xfer)
{
	printf(
"      [BASICS]\n"
"        -D, --device          select node by name in coniguration space\n"
"        -N, --nonblock        nonblocking mode\n"
"        -M, --mmap            use mmap(2) for zero copying technique\n"
"        -F, --period-time     interval between interrupts (msec unit)\n"
"        --period-size         interval between interrupts (frame unit)\n"
"        -B, --buffer-time     size of buffer for frame(msec unit)\n"
"        --buffer-size         size of buffer for frame(frame unit)\n"
"        --waiter-type         type of waiter to handle available frames\n"
"        --sched-model         model of process scheduling\n"
"      [SOFTWARE FEATURES]\n"
"        -A, --avail-min       threshold of frames to wake up process\n"
"        -R, --start-delay     threshold of frames to start PCM substream\n"
"        -T, --stop-delay      threshold of frames to stop PCM substream\n"
"      [LIBASOUND PLUGIN OPTIONS]\n"
"        --disable-resample    disable rate conversion for plug plugin\n"
"        --disable-channels    disable channel conversion for plug plugin\n"
"        --disable-format      disable format conversion for plug plugin\n"
"        --disable-softvol     disable software volume for sofvol plugin\n"
"      [DEBUG ASSISTANT]\n"
"        --fatal-errors        finish at XRUN\n"
"        --test-nowait         busy poll without any waiter\n"
	);
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
		.help		= xfer_libasound_help,
	},
	.private_size = sizeof(struct libasound_state),
};
