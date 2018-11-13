// SPDX-License-Identifier: GPL-2.0
//
// frame-cache.c - maintainer of cache for data frame.
//
// Copyright (c) 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp>
//
// Licensed under the terms of the GNU General Public License, version 2.

#include "frame-cache.h"

static void align_frames_in_i(struct frame_cache *cache,
			      unsigned int consumed_count)
{
	char *buf = cache->buf;
	unsigned int offset;
	unsigned int size;

	cache->remained_count -= consumed_count;

	offset = cache->bytes_per_sample * cache->samples_per_frame *
		 consumed_count;
	size = cache->bytes_per_sample * cache->samples_per_frame *
	       cache->remained_count;
	memmove(buf, buf + offset, size);

	cache->buf_ptr = buf + size;
}

static void align_frames_in_n(struct frame_cache *cache,
			      unsigned int consumed_count)
{
	char **bufs = cache->buf;
	char **buf_ptrs = cache->buf_ptr;
	unsigned int offset;
	unsigned int size;
	int i;

	cache->remained_count -= consumed_count;

	for (i = 0; i < cache->samples_per_frame; ++i) {
		offset = cache->bytes_per_sample * consumed_count;
		size = cache->bytes_per_sample * cache->remained_count;
		memmove(bufs[i], bufs[i] + offset, size);
		buf_ptrs[i] = bufs[i] + size;
	}
}

int frame_cache_init(struct frame_cache *cache, snd_pcm_access_t access,
		     unsigned int bytes_per_sample,
		     unsigned int samples_per_frame,
		     unsigned int frames_per_cache)
{
	if (access == SND_PCM_ACCESS_RW_INTERLEAVED)
		cache->align_frames = align_frames_in_i;
	else if (access == SND_PCM_ACCESS_RW_NONINTERLEAVED)
		cache->align_frames = align_frames_in_n;
	else
		return -EINVAL;
	cache->access = access;

	if (access == SND_PCM_ACCESS_RW_INTERLEAVED) {
		char *buf;

		buf = calloc(frames_per_cache,
			     bytes_per_sample * samples_per_frame);
		if (buf == NULL)
			return -ENOMEM;
		cache->buf = buf;
		cache->buf_ptr = buf;
	} else {
		char **bufs;
		char **buf_ptrs;
		int i;

		bufs = calloc(samples_per_frame, sizeof(*bufs));
		if (bufs == NULL)
			return -ENOMEM;
		buf_ptrs = calloc(samples_per_frame, sizeof(*buf_ptrs));
		if (buf_ptrs == NULL)
			return -ENOMEM;
		for (i = 0; i < samples_per_frame; ++i) {
			bufs[i] = calloc(frames_per_cache, bytes_per_sample);
			if (bufs[i] == NULL)
				return -ENOMEM;
			buf_ptrs[i] = bufs[i];
		}
		cache->buf = bufs;
		cache->buf_ptr = buf_ptrs;
	}

	cache->remained_count = 0;
	cache->bytes_per_sample = bytes_per_sample;
	cache->samples_per_frame = samples_per_frame;
	cache->frames_per_cache = frames_per_cache;

	return 0;
}

void frame_cache_destroy(struct frame_cache *cache)
{
	if (cache->access == SND_PCM_ACCESS_RW_NONINTERLEAVED) {
		int i;
		for (i = 0; i < cache->samples_per_frame; ++i) {
			char **bufs = cache->buf;
			free(bufs[i]);
		}
		free(cache->buf_ptr);
	}
	free(cache->buf);
	memset(cache, 0, sizeof(*cache));
}
