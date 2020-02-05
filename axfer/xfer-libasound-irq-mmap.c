// SPDX-License-Identifier: GPL-2.0
//
// xfer-libasound-irq-mmap.c - IRQ-based scheduling model for mmap operation.
//
// Copyright (c) 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp>
//
// Licensed under the terms of the GNU General Public License, version 2.

#include "xfer-libasound.h"
#include "misc.h"

struct map_layout {
	snd_pcm_status_t *status;

	char **vector;
	unsigned int samples_per_frame;
};

static int irq_mmap_pre_process(struct libasound_state *state)
{
	struct map_layout *layout = state->private_data;
	snd_pcm_access_t access;
	snd_pcm_uframes_t frame_offset;
	snd_pcm_uframes_t avail = 0;
	int i;
	int err;

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
		logging(state, "\n");
	}

	return 0;
}

static int irq_mmap_process_frames(struct libasound_state *state,
				   unsigned int *frame_count,
				   struct mapper_context *mapper,
				   struct container_context *cntrs)
{
	struct map_layout *layout = state->private_data;
	const snd_pcm_channel_area_t *areas;
	snd_pcm_uframes_t frame_offset;
	snd_pcm_uframes_t avail;
	unsigned int avail_count;
	void *frame_buf;
	snd_pcm_sframes_t consumed_count;
	int err;

	if (state->use_waiter) {
		unsigned int msec_per_buffer;
		unsigned short revents;

		// Wait during msec equivalent to all audio data frames in
		// buffer instead of period, for safe.
		err = snd_pcm_hw_params_get_buffer_time(state->hw_params,
							&msec_per_buffer, NULL);
		if (err < 0)
			return err;
		msec_per_buffer /= 1000;

		// Wait for hardware IRQ when no avail space in buffer.
		err = xfer_libasound_wait_event(state, msec_per_buffer,
						&revents);
		if (err == -ETIMEDOUT) {
			logging(state,
				"No event occurs for PCM substream during %u "
				"msec. The implementaion of kernel driver or "
				"userland backend causes this issue.\n",
				msec_per_buffer);
			return err;
		}
		if (err < 0)
			return err;
		if (revents & POLLERR) {
			// TODO: error reporting?
			return -EIO;
		}
		if (!(revents & (POLLIN | POLLOUT)))
			return -EAGAIN;

		// When rescheduled, current position of data transmission was
		// queried to actual hardware by a handler of IRQ. No need to
		// perform it; e.g. ioctl(2) with SNDRV_PCM_IOCTL_HWSYNC.
	}

	// Sync cache in user space to data in kernel space to calculate avail
	// frames according to the latest positions on PCM buffer.
	//
	// This has an additional advantage to handle libasound PCM plugins.
	// Most of libasound PCM plugins perform resampling in .avail_update()
	// callback for capture PCM substream, then update positions on buffer.
	//
	// MEMO: either snd_pcm_avail_update() and snd_pcm_mmap_begin() can
	// return the same number of available frames.
	avail = snd_pcm_avail_update(state->handle);
	if ((snd_pcm_sframes_t)avail < 0)
		return (int)avail;
	if (*frame_count < avail)
		avail = *frame_count;

	err = snd_pcm_mmap_begin(state->handle, &areas, &frame_offset, &avail);
	if (err < 0)
		return err;

	// Trim according up to expected frame count.
	if (*frame_count < avail)
		avail_count = *frame_count;
	else
		avail_count = (unsigned int)avail;

	// TODO: Perhaps, the complex layout can be supported as a variation of
	// vector type. However, there's no driver with this layout.
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

	err = mapper_context_process_frames(mapper, frame_buf, &avail_count,
					    cntrs);
	if (err < 0)
		return err;
	if (avail_count == 0) {
		*frame_count = 0;
		return 0;
	}

	consumed_count = snd_pcm_mmap_commit(state->handle, frame_offset,
					     avail_count);
	if (consumed_count < 0)
		return (int)consumed_count;
	if (consumed_count != avail_count)
		logging(state, "A bug of access plugin for this PCM node.\n");

	*frame_count = consumed_count;

	return 0;
}

static int irq_mmap_r_process_frames(struct libasound_state *state,
				     unsigned *frame_count,
				     struct mapper_context *mapper,
				     struct container_context *cntrs)
{
	struct map_layout *layout = state->private_data;
	snd_pcm_state_t s;
	int err;

	// To querying current status of hardware, we need to care of
	// synchronization between 3 levels:
	//  1. status to actual hardware by driver.
	//  2. status data in kernel space.
	//  3. status data in user space.
	//
	// Kernel driver query 1 and sync 2, according to requests of some
	// ioctl(2) commands. For synchronization between 2 and 3, ALSA PCM core
	// supports mmap(2) operation on cache coherent architectures, some
	// ioctl(2) commands on cache incoherent architecture. In usage of the
	// former mechanism, we need to care of concurrent access by IRQ context
	// and process context to the mapped page frame.
	// In a call of ioctl(2) with SNDRV_PCM_IOCTL_STATUS and
	// SNDRV_PCM_IOCTL_STATUS_EXT, the above care is needless because
	// mapped page frame is unused regardless of architectures in a point of
	// cache coherency.
	err = snd_pcm_status(state->handle, layout->status);
	if (err < 0)
		goto error;
	s = snd_pcm_status_get_state(layout->status);

	// TODO: if reporting something, do here with the status data.

	// For capture direction, need to start stream explicitly.
	if (s != SND_PCM_STATE_RUNNING) {
		if (s != SND_PCM_STATE_PREPARED) {
			err = -EPIPE;
			goto error;
		}

		err = snd_pcm_start(state->handle);
		if (err < 0)
			goto error;
	}

	err = irq_mmap_process_frames(state, frame_count, mapper, cntrs);
	if (err < 0)
		goto error;

	return 0;
error:
	*frame_count = 0;
	return err;
}

static int irq_mmap_w_process_frames(struct libasound_state *state,
				     unsigned *frame_count,
				     struct mapper_context *mapper,
				     struct container_context *cntrs)
{
	struct map_layout *layout = state->private_data;
	snd_pcm_state_t s;
	int err;

	// Read my comment in 'irq_mmap_r_process_frames().
	err = snd_pcm_status(state->handle, layout->status);
	if (err < 0)
		goto error;
	s = snd_pcm_status_get_state(layout->status);

	// TODO: if reporting something, do here with the status data.

	err = irq_mmap_process_frames(state, frame_count, mapper, cntrs);
	if (err < 0)
		goto error;

	// Need to start playback stream explicitly
	if (s != SND_PCM_STATE_RUNNING) {
		if (s != SND_PCM_STATE_PREPARED) {
			err = -EPIPE;
			goto error;
		}

		err = snd_pcm_start(state->handle);
		if (err < 0)
			goto error;
	}

	return 0;
error:
	*frame_count = 0;
	return err;
}

static void irq_mmap_post_process(struct libasound_state *state)
{
	struct map_layout *layout = state->private_data;

	if (layout->status)
		snd_pcm_status_free(layout->status);
	layout->status = NULL;

	free(layout->vector);
	layout->vector = NULL;
}

const struct xfer_libasound_ops xfer_libasound_irq_mmap_w_ops = {
	.pre_process	= irq_mmap_pre_process,
	.process_frames	= irq_mmap_w_process_frames,
	.post_process	= irq_mmap_post_process,
	.private_size	= sizeof(struct map_layout),
};

const struct xfer_libasound_ops xfer_libasound_irq_mmap_r_ops = {
	.pre_process	= irq_mmap_pre_process,
	.process_frames	= irq_mmap_r_process_frames,
	.post_process	= irq_mmap_post_process,
	.private_size	= sizeof(struct map_layout),
};
