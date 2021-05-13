// SPDX-License-Identifier: GPL-2.0
//
// xfer-libasound-irq-mmap.c - Timer-based scheduling model for mmap operation.
//
// Copyright (c) 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp>
//
// Licensed under the terms of the GNU General Public License, version 2.

#include "xfer-libasound.h"
#include "misc.h"

struct map_layout {
	snd_pcm_status_t *status;
	bool need_forward_or_rewind;
	char **vector;

	unsigned int frames_per_second;
	unsigned int samples_per_frame;
	unsigned int frames_per_buffer;
};

static int timer_mmap_pre_process(struct libasound_state *state)
{
	struct map_layout *layout = state->private_data;
	snd_pcm_access_t access;
	snd_pcm_uframes_t frame_offset;
	snd_pcm_uframes_t avail = 0;
	snd_pcm_uframes_t frames_per_buffer;
	int i;
	int err;

	// This parameter, 'period event', is a software feature in alsa-lib.
	// This switch a handler in 'hw' PCM plugin from irq-based one to
	// timer-based one. This handler has two file descriptors for
	// ALSA PCM character device and ALSA timer device. The latter is used
	// to catch suspend/resume events as wakeup event.
	err = snd_pcm_sw_params_set_period_event(state->handle,
						 state->sw_params, 1);
	if (err < 0)
		return err;

	err = snd_pcm_status_malloc(&layout->status);
	if (err < 0)
		return err;

	err = snd_pcm_hw_params_get_access(state->hw_params, &access);
	if (err < 0)
		return err;

	err = snd_pcm_hw_params_get_channels(state->hw_params,
					     &layout->samples_per_frame);
	if (err < 0)
		return err;

	err = snd_pcm_hw_params_get_rate(state->hw_params,
					 &layout->frames_per_second, NULL);
	if (err < 0)
		return err;

	err = snd_pcm_hw_params_get_buffer_size(state->hw_params,
						&frames_per_buffer);
	if (err < 0)
		return err;
	layout->frames_per_buffer = (unsigned int)frames_per_buffer;

	if (access == SND_PCM_ACCESS_MMAP_NONINTERLEAVED) {
		layout->vector = calloc(layout->samples_per_frame,
					sizeof(*layout->vector));
		if (layout->vector == NULL)
			return err;
	}

	if (state->verbose) {
		const snd_pcm_channel_area_t *areas;
		err = snd_pcm_mmap_begin(state->handle, &areas, &frame_offset,
					 &avail);
		if (err < 0)
			return err;

		logging(state, "attributes for mapped page frame:\n");
		for (i = 0; i < layout->samples_per_frame; ++i) {
			const snd_pcm_channel_area_t *area = areas + i;

			logging(state, "  sample number: %d\n", i);
			logging(state, "    address: %p\n", area->addr);
			logging(state, "    bits for offset: %u\n", area->first);
			logging(state, "    bits/frame: %u\n", area->step);
		}
	}

	return 0;
}

static void *get_buffer(struct libasound_state *state,
			const snd_pcm_channel_area_t *areas,
			snd_pcm_uframes_t frame_offset)
{
	struct map_layout *layout = state->private_data;
	void *frame_buf;

	if (layout->vector == NULL) {
		char *buf;
		buf = areas[0].addr;
		buf += snd_pcm_frames_to_bytes(state->handle, frame_offset);
		frame_buf = buf;
	} else {
		int i;
		for (i = 0; i < layout->samples_per_frame; ++i) {
			layout->vector[i] = areas[i].addr;
			layout->vector[i] += snd_pcm_samples_to_bytes(
						state->handle, frame_offset);
		}
		frame_buf = layout->vector;
	}

	return frame_buf;
}

static int timer_mmap_process_frames(struct libasound_state *state,
				     unsigned int *frame_count,
				     struct mapper_context *mapper,
				     struct container_context *cntrs)
{
	struct map_layout *layout = state->private_data;
	snd_pcm_uframes_t planned_count;
	snd_pcm_sframes_t avail;
	snd_pcm_uframes_t avail_count;
	const snd_pcm_channel_area_t *areas;
	snd_pcm_uframes_t frame_offset;
	void *frame_buf;
	snd_pcm_sframes_t consumed_count;
	int err;

	// Retrieve avail space on PCM buffer between kernel/user spaces.
	// On cache incoherent architectures, still care of data
	// synchronization.
	avail = snd_pcm_avail_update(state->handle);
	if (avail < 0)
		return (int)avail;

	// Retrieve pointers of the buffer and left space up to the boundary.
	avail_count = (snd_pcm_uframes_t)avail;
	err = snd_pcm_mmap_begin(state->handle, &areas, &frame_offset,
				 &avail_count);
	if (err < 0)
		return err;

	// MEMO: Use the amount of data frames as you like.
	planned_count = layout->frames_per_buffer * random() / RAND_MAX;
	if (frame_offset + planned_count > layout->frames_per_buffer)
		planned_count = layout->frames_per_buffer - frame_offset;

	// Trim up to expected frame count.
	if (*frame_count < planned_count)
		planned_count = *frame_count;

	// Yield this CPU till planned amount of frames become available.
	if (avail_count < planned_count) {
		unsigned short revents;
		int timeout_msec;

		// TODO; precise granularity of timeout; e.g. ppoll(2).
		// Furthermore, wrap up according to granularity of reported
		// value for hw_ptr.
		timeout_msec = ((planned_count - avail_count) * 1000 +
				layout->frames_per_second - 1) /
			       layout->frames_per_second;

		// TODO: However, experimentally, the above is not enough to
		// keep planned amount of frames when waking up. I don't know
		// exactly the mechanism yet.
		err = xfer_libasound_wait_event(state, timeout_msec,
						&revents);
		// MEMO: timeout is expected since the above call is just to measure time elapse.
		if (err < 0 && err != -ETIMEDOUT)
			return err;
		if (revents & POLLERR) {
			// TODO: error reporting.
			return -EIO;
		}
		if (!(revents & (POLLIN | POLLOUT)))
			return -EAGAIN;

		// MEMO: Need to perform hwsync explicitly because hwptr is not
		// synchronized to actual position of data frame transmission
		// on hardware because IRQ handlers are not used in this
		// scheduling strategy.
		avail = snd_pcm_avail(state->handle);
		if (avail < 0)
			return (int)avail;
		if (avail < planned_count) {
			logging(state,
				"Wake up but not enough space: %lu %lu %u\n",
				planned_count, avail, timeout_msec);
			planned_count = avail;
		}
	}

	// Let's process data frames.
	*frame_count = planned_count;
	frame_buf = get_buffer(state, areas, frame_offset);
	err = mapper_context_process_frames(mapper, frame_buf, frame_count,
					    cntrs);
	if (err < 0)
		return err;

	consumed_count = snd_pcm_mmap_commit(state->handle, frame_offset,
					     *frame_count);
	if (consumed_count != *frame_count) {
		logging(state,
			"A bug of 'hw' PCM plugin or driver for this PCM "
			"node.\n");
	}
	*frame_count = consumed_count;

	return 0;
}

static int forward_appl_ptr(struct libasound_state *state)
{
	struct map_layout *layout = state->private_data;
	snd_pcm_uframes_t forwardable_count;
	snd_pcm_sframes_t forward_count;

	forward_count = snd_pcm_forwardable(state->handle);
	if (forward_count < 0)
		return (int)forward_count;
	forwardable_count = forward_count;

	// No need to add safe-gurard because hwptr goes ahead.
	forward_count = snd_pcm_forward(state->handle, forwardable_count);
	if (forward_count < 0)
		return (int)forward_count;

	if (state->verbose) {
		logging(state,
			"  forwarded: %lu/%u\n",
			forward_count, layout->frames_per_buffer);
	}

	return 0;
}

static int timer_mmap_r_process_frames(struct libasound_state *state,
				       unsigned *frame_count,
				       struct mapper_context *mapper,
				       struct container_context *cntrs)
{
	struct map_layout *layout = state->private_data;
	snd_pcm_state_t s;
	int err;

	// SNDRV_PCM_HW_PARAMS_NO_PERIOD_WAKEUP suppresses any IRQ to notify
	// period elapse for data transmission, therefore no need to care of
	// concurrent access by IRQ context and process context, unlike
	// IRQ-based operations.
	// Here, this is just to query current status to hardware, for later
	// processing.
	err = snd_pcm_status(state->handle, layout->status);
	if (err < 0)
		goto error;
	s = snd_pcm_status_get_state(layout->status);

	// TODO: if reporting something, do here with the status data.

	if (s == SND_PCM_STATE_RUNNING) {
		// Reduce delay between sampling on hardware and handling by
		// this program.
		if (layout->need_forward_or_rewind) {
			err = forward_appl_ptr(state);
			if (err < 0)
				goto error;
			layout->need_forward_or_rewind = false;
		}

		err = timer_mmap_process_frames(state, frame_count, mapper,
						cntrs);
		if (err < 0)
			goto error;
	} else {
		if (s == SND_PCM_STATE_PREPARED) {
			// For capture direction, need to start stream
			// explicitly.
			err = snd_pcm_start(state->handle);
			if (err < 0)
				goto error;
			layout->need_forward_or_rewind = true;
			// Not yet.
			*frame_count = 0;
		} else {
			err = -EPIPE;
			goto error;
		}
	}

	return 0;
error:
	*frame_count = 0;
	return err;
}

static int rewind_appl_ptr(struct libasound_state *state)
{
	struct map_layout *layout = state->private_data;
	snd_pcm_uframes_t rewindable_count;
	snd_pcm_sframes_t rewind_count;

	rewind_count = snd_pcm_rewindable(state->handle);
	if (rewind_count < 0)
		return (int)rewind_count;
	rewindable_count = rewind_count;

	// If appl_ptr were rewound just to position of hw_ptr, at next time,
	// hw_ptr could catch up appl_ptr. This is overrun. We need a space
	// between these two pointers to prevent this XRUN.
	// This space is largely affected by time to process data frames later.
	//
	// TODO: a generous way to estimate a good value.
	if (rewindable_count < 32)
		return 0;
	rewindable_count -= 32;

	rewind_count = snd_pcm_rewind(state->handle, rewindable_count);
	if (rewind_count < 0)
		return (int)rewind_count;

	if (state->verbose) {
		logging(state,
			"  rewound: %lu/%u\n",
			rewind_count, layout->frames_per_buffer);
	}

	return 0;
}

static int fill_buffer_with_zero_samples(struct libasound_state *state)
{
	struct map_layout *layout = state->private_data;
	const snd_pcm_channel_area_t *areas;
	snd_pcm_uframes_t frame_offset;
	snd_pcm_uframes_t avail_count;
	snd_pcm_format_t sample_format;
	snd_pcm_uframes_t consumed_count;
	int err;

	err = snd_pcm_hw_params_get_buffer_size(state->hw_params,
						&avail_count);
	if (err < 0)
		return err;

	err = snd_pcm_mmap_begin(state->handle, &areas, &frame_offset,
				 &avail_count);
	if (err < 0)
		return err;

	err = snd_pcm_hw_params_get_format(state->hw_params, &sample_format);
	if (err < 0)
		return err;

	err = snd_pcm_areas_silence(areas, frame_offset,
				    layout->samples_per_frame, avail_count,
				    sample_format);
	if (err < 0)
		return err;

	consumed_count = snd_pcm_mmap_commit(state->handle, frame_offset,
					     avail_count);
	if (consumed_count != avail_count)
		logging(state, "A bug of access plugin for this PCM node.\n");

	return 0;
}

static int timer_mmap_w_process_frames(struct libasound_state *state,
				       unsigned *frame_count,
				       struct mapper_context *mapper,
				       struct container_context *cntrs)
{
	struct map_layout *layout = state->private_data;
	snd_pcm_state_t s;
	int err;

	// Read my comment in 'timer_mmap_w_process_frames()'.
	err = snd_pcm_status(state->handle, layout->status);
	if (err < 0)
		goto error;
	s = snd_pcm_status_get_state(layout->status);

	// TODO: if reporting something, do here with the status data.

	if (s == SND_PCM_STATE_RUNNING) {
		// Reduce delay between queueing by this program and presenting
		// on hardware.
		if (layout->need_forward_or_rewind) {
			err = rewind_appl_ptr(state);
			if (err < 0)
				goto error;
			layout->need_forward_or_rewind = false;
		}

		err = timer_mmap_process_frames(state, frame_count, mapper,
						cntrs);
		if (err < 0)
			goto error;
	} else {
		// Need to start playback stream explicitly
		if (s == SND_PCM_STATE_PREPARED) {
			err = fill_buffer_with_zero_samples(state);
			if (err < 0)
				goto error;

			err = snd_pcm_start(state->handle);
			if (err < 0)
				goto error;

			layout->need_forward_or_rewind = true;
			// Not yet.
			*frame_count = 0;
		} else {
			err = -EPIPE;
			goto error;
		}
	}

	return 0;
error:
	*frame_count = 0;
	return err;
}

static void timer_mmap_post_process(struct libasound_state *state)
{
	struct map_layout *layout = state->private_data;

	if (layout->status)
		snd_pcm_status_free(layout->status);
	layout->status = NULL;

	if (layout->vector)
		free(layout->vector);
	layout->vector = NULL;
}

const struct xfer_libasound_ops xfer_libasound_timer_mmap_w_ops = {
	.pre_process	= timer_mmap_pre_process,
	.process_frames	= timer_mmap_w_process_frames,
	.post_process	= timer_mmap_post_process,
	.private_size	= sizeof(struct map_layout),
};

const struct xfer_libasound_ops xfer_libasound_timer_mmap_r_ops = {
	.pre_process	= timer_mmap_pre_process,
	.process_frames	= timer_mmap_r_process_frames,
	.post_process	= timer_mmap_post_process,
	.private_size	= sizeof(struct map_layout),
};
