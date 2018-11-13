// SPDX-License-Identifier: GPL-2.0
//
// frame-cache.h - maintainer of cache for data frame.
//
// Copyright (c) 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp>
//
// Licensed under the terms of the GNU General Public License, version 2.

#include <alsa/asoundlib.h>

struct frame_cache {
	void *buf;
	void *buf_ptr;

	unsigned int remained_count;

	snd_pcm_access_t access;
	unsigned int bytes_per_sample;
	unsigned int samples_per_frame;
	unsigned int frames_per_cache;

	void (*align_frames)(struct frame_cache *cache,
			     unsigned int consumed_count);
};

int frame_cache_init(struct frame_cache *cache, snd_pcm_access_t access,
		     unsigned int bytes_per_sample,
		     unsigned int samples_per_frame,
		     unsigned int frames_per_cache);
void frame_cache_destroy(struct frame_cache *cache);

static inline unsigned int frame_cache_get_count(struct frame_cache *cache)
{
	return cache->remained_count;
}

static inline void frame_cache_increase_count(struct frame_cache *cache,
					      unsigned int frame_count)
{
	cache->remained_count += frame_count;
}

static inline void frame_cache_reduce(struct frame_cache *cache,
				      unsigned int consumed_count)
{
	cache->align_frames(cache, consumed_count);
}
