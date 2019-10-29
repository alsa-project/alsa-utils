// SPDX-License-Identifier: GPL-2.0
//
// xfer-libasound-irq-rw.c - IRQ-based scheduling model for read/write operation.
//
// Copyright (c) 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp>
//
// Licensed under the terms of the GNU General Public License, version 2.

#include "xfer-libasound.h"
#include "misc.h"
#include "frame-cache.h"

struct rw_closure {
	snd_pcm_access_t access;
	int (*process_frames)(struct libasound_state *state,
			      snd_pcm_state_t status, unsigned int *frame_count,
			      struct mapper_context *mapper,
		      struct container_context *cntrs);
	struct frame_cache cache;
};

static int wait_for_avail(struct libasound_state *state)
{
	unsigned int msec_per_buffer;
	unsigned short revents;
	unsigned short event;
	int err;

	// Wait during msec equivalent to all audio data frames in buffer
	// instead of period, for safe.
	err = snd_pcm_hw_params_get_buffer_time(state->hw_params,
						&msec_per_buffer, NULL);
	if (err < 0)
		return err;
	msec_per_buffer /= 1000;

	// Wait for hardware IRQ when no available space.
	err = xfer_libasound_wait_event(state, msec_per_buffer, &revents);
	if (err < 0)
		return err;

	// TODO: error reporting.
	if (revents & POLLERR)
		return -EIO;

	if (snd_pcm_stream(state->handle) == SND_PCM_STREAM_CAPTURE)
		event = POLLIN;
	else
		event = POLLOUT;

	if (!(revents & event))
		return -EAGAIN;

	return 0;
}

static int read_frames(struct libasound_state *state, unsigned int *frame_count,
		       unsigned int avail_count, struct mapper_context *mapper,
		       struct container_context *cntrs)
{
	struct rw_closure *closure = state->private_data;
	snd_pcm_sframes_t handled_frame_count;
	unsigned int consumed_count;
	int err;

	// Trim according up to expected frame count.
	if (*frame_count < avail_count)
		avail_count = *frame_count;

	// Cache required amount of frames.
	if (avail_count > frame_cache_get_count(&closure->cache)) {
		avail_count -= frame_cache_get_count(&closure->cache);

		// Execute write operation according to the shape of buffer.
		// These operations automatically start the substream.
		if (closure->access == SND_PCM_ACCESS_RW_INTERLEAVED) {
			handled_frame_count = snd_pcm_readi(state->handle,
							closure->cache.buf_ptr,
							avail_count);
		} else {
			handled_frame_count = snd_pcm_readn(state->handle,
							closure->cache.buf_ptr,
							avail_count);
		}
		if (handled_frame_count < 0) {
			err = handled_frame_count;
			return err;
		}
		frame_cache_increase_count(&closure->cache, handled_frame_count);
		avail_count = frame_cache_get_count(&closure->cache);
	}

	// Write out to file descriptors.
	consumed_count = avail_count;
	err = mapper_context_process_frames(mapper, closure->cache.buf,
					    &consumed_count, cntrs);
	if (err < 0)
		return err;

	frame_cache_reduce(&closure->cache, consumed_count);

	*frame_count = consumed_count;

	return 0;
}

static int r_process_frames_blocking(struct libasound_state *state,
				     snd_pcm_state_t status,
				     unsigned int *frame_count,
				     struct mapper_context *mapper,
				     struct container_context *cntrs)
{
	snd_pcm_sframes_t avail;
	snd_pcm_uframes_t avail_count;
	int err = 0;

	if (status == SND_PCM_STATE_RUNNING) {
		// Check available space on the buffer.
		avail = snd_pcm_avail(state->handle);
		if (avail < 0) {
			err = avail;
			goto error;
		}
		avail_count = (snd_pcm_uframes_t)avail;

		if (avail_count == 0) {
			// Request data frames so that blocking is just
			// released.
			err = snd_pcm_sw_params_get_avail_min(state->sw_params,
							      &avail_count);
			if (err < 0)
				goto error;
		}
	} else {
		// Request data frames so that the PCM substream starts.
		snd_pcm_uframes_t frame_count;
		err = snd_pcm_sw_params_get_start_threshold(state->sw_params,
							    &frame_count);
		if (err < 0)
			goto error;

		avail_count = (unsigned int)frame_count;
	}

	err = read_frames(state, frame_count, avail_count, mapper, cntrs);
	if (err < 0)
		goto error;

	return 0;
error:
	*frame_count = 0;
	return err;
}

static int r_process_frames_nonblocking(struct libasound_state *state,
					snd_pcm_state_t status,
					unsigned int *frame_count,
					struct mapper_context *mapper,
					struct container_context *cntrs)
{
	snd_pcm_sframes_t avail;
	snd_pcm_uframes_t avail_count;
	int err = 0;

	if (status != SND_PCM_STATE_RUNNING) {
		err = snd_pcm_start(state->handle);
		if (err < 0)
			goto error;
	}

	if (state->use_waiter) {
		err = wait_for_avail(state);
		if (err < 0)
			goto error;
	}

	// Check available space on the buffer.
	avail = snd_pcm_avail(state->handle);
	if (avail < 0) {
		err = avail;
		goto error;
	}
	avail_count = (snd_pcm_uframes_t)avail;

	if (avail_count == 0) {
		// Let's go to a next iteration.
		err = 0;
		goto error;
	}

	err = read_frames(state, frame_count, avail_count, mapper, cntrs);
	if (err < 0)
		goto error;

	return 0;
error:
	*frame_count = 0;
	return err;
}

static int write_frames(struct libasound_state *state,
			unsigned int *frame_count, unsigned int avail_count,
			struct mapper_context *mapper,
			struct container_context *cntrs)
{
	struct rw_closure *closure = state->private_data;
	snd_pcm_uframes_t consumed_count;
	snd_pcm_sframes_t handled_frame_count;
	int err;

	// Trim according up to expected frame count.
	if (*frame_count < avail_count)
		avail_count = *frame_count;

	// Cache required amount of frames.
	if (avail_count > frame_cache_get_count(&closure->cache)) {
		avail_count -= frame_cache_get_count(&closure->cache);

		// Read frames to transfer.
		err = mapper_context_process_frames(mapper,
				closure->cache.buf_ptr, &avail_count, cntrs);
		if (err < 0)
			return err;
		frame_cache_increase_count(&closure->cache, avail_count);
		avail_count = frame_cache_get_count(&closure->cache);
	}

	// Execute write operation according to the shape of buffer. These
	// operations automatically start the stream.
	consumed_count = avail_count;
	if (closure->access == SND_PCM_ACCESS_RW_INTERLEAVED) {
		handled_frame_count = snd_pcm_writei(state->handle,
					closure->cache.buf, consumed_count);
	} else {
		handled_frame_count = snd_pcm_writen(state->handle,
					closure->cache.buf, consumed_count);
	}
	if (handled_frame_count < 0) {
		err = handled_frame_count;
		return err;
	}

	consumed_count = handled_frame_count;
	frame_cache_reduce(&closure->cache, consumed_count);

	*frame_count = consumed_count;

	return 0;
}

static int w_process_frames_blocking(struct libasound_state *state,
				     snd_pcm_state_t status,
				     unsigned int *frame_count,
				     struct mapper_context *mapper,
				     struct container_context *cntrs)
{
	snd_pcm_sframes_t avail;
	unsigned int avail_count;
	int err;

	if (status == SND_PCM_STATE_RUNNING) {
		// Check available space on the buffer.
		avail = snd_pcm_avail(state->handle);
		if (avail < 0) {
			err = avail;
			goto error;
		}
		avail_count = (unsigned int)avail;

		if (avail_count == 0) {
			// Fill with data frames so that blocking is just
			// released.
			snd_pcm_uframes_t avail_min;
			err = snd_pcm_sw_params_get_avail_min(state->sw_params,
							      &avail_min);
			if (err < 0)
				goto error;
			avail_count = (unsigned int)avail_min;
		}
	} else {
		snd_pcm_uframes_t frames_for_start_threshold;
		snd_pcm_uframes_t frames_per_period;

		// Fill with data frames so that the PCM substream starts.
		err = snd_pcm_sw_params_get_start_threshold(state->sw_params,
						&frames_for_start_threshold);
		if (err < 0)
			goto error;

		// But the above number can be too small and cause XRUN because
		// I/O operation is done per period.
		err = snd_pcm_hw_params_get_period_size(state->hw_params,
						&frames_per_period, NULL);
		if (err < 0)
			goto error;

		// Use larger one to prevent from both of XRUN and successive
		// blocking.
		if (frames_for_start_threshold > frames_per_period)
			avail_count = (unsigned int)frames_for_start_threshold;
		else
			avail_count = (unsigned int)frames_per_period;
	}

	err = write_frames(state, frame_count, avail_count, mapper, cntrs);
	if (err < 0)
		goto error;

	return 0;
error:
	*frame_count = 0;
	return err;
}

static int w_process_frames_nonblocking(struct libasound_state *state,
					snd_pcm_state_t status,
					unsigned int *frame_count,
					struct mapper_context *mapper,
					struct container_context *cntrs)
{
	snd_pcm_sframes_t avail;
	unsigned int avail_count;
	int err;

	if (state->use_waiter) {
		err = wait_for_avail(state);
		if (err < 0)
			goto error;
	}

	// Check available space on the buffer.
	avail = snd_pcm_avail(state->handle);
	if (avail < 0) {
		err = avail;
		goto error;
	}
	avail_count = (unsigned int)avail;

	if (avail_count == 0) {
		// Let's go to a next iteration.
		err = 0;
		goto error;
	}

	err = write_frames(state, frame_count, avail_count, mapper, cntrs);
	if (err < 0)
		goto error;

	// NOTE: The substream starts automatically when the accumulated number
	// of queued data frame exceeds start_threshold.

	return 0;
error:
	*frame_count = 0;
	return err;
}

static int irq_rw_pre_process(struct libasound_state *state)
{
	struct rw_closure *closure = state->private_data;
	snd_pcm_format_t format;
	snd_pcm_uframes_t frames_per_buffer;
	int bytes_per_sample;
	unsigned int samples_per_frame;
	int err;

	err = snd_pcm_hw_params_get_format(state->hw_params, &format);
	if (err < 0)
		return err;
	bytes_per_sample = snd_pcm_format_physical_width(format) / 8;
	if (bytes_per_sample <= 0)
		return -ENXIO;

	err = snd_pcm_hw_params_get_channels(state->hw_params,
					     &samples_per_frame);
	if (err < 0)
		return err;

	err = snd_pcm_hw_params_get_buffer_size(state->hw_params,
						&frames_per_buffer);
	if (err < 0)
		return err;

	err = snd_pcm_hw_params_get_access(state->hw_params, &closure->access);
	if (err < 0)
		return err;

	err = frame_cache_init(&closure->cache, closure->access,
			       bytes_per_sample, samples_per_frame,
			       frames_per_buffer);
	if (err < 0)
		return err;

	if (snd_pcm_stream(state->handle) == SND_PCM_STREAM_CAPTURE) {
		if (state->nonblock)
			closure->process_frames = r_process_frames_nonblocking;
		else
			closure->process_frames = r_process_frames_blocking;
	} else {
		if (state->nonblock)
			closure->process_frames = w_process_frames_nonblocking;
		else
			closure->process_frames = w_process_frames_blocking;
	}

	return 0;
}

static int irq_rw_process_frames(struct libasound_state *state,
				unsigned int *frame_count,
				struct mapper_context *mapper,
				struct container_context *cntrs)
{
	struct rw_closure *closure = state->private_data;
	snd_pcm_state_t status;

	// Need to recover the stream.
	status = snd_pcm_state(state->handle);
	if (status != SND_PCM_STATE_RUNNING && status != SND_PCM_STATE_PREPARED)
		return -EPIPE;

	// NOTE: Actually, status can be shift always.
	return closure->process_frames(state, status, frame_count, mapper, cntrs);
}

static void irq_rw_post_process(struct libasound_state *state)
{
	struct rw_closure *closure = state->private_data;

	frame_cache_destroy(&closure->cache);
}

const struct xfer_libasound_ops xfer_libasound_irq_rw_ops = {
	.pre_process	= irq_rw_pre_process,
	.process_frames	= irq_rw_process_frames,
	.post_process	= irq_rw_post_process,
	.private_size	= sizeof(struct rw_closure),
};
