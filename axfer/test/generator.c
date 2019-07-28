// SPDX-License-Identifier: GPL-2.0
//
// allocator.h - a header of a generator for test with buffers of PCM frames.
//
// Copyright (c) 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp>
//
// Licensed under the terms of the GNU General Public License, version 2.

#include "generator.h"

#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>

int generator_context_init(struct test_generator *gen,
			   uint64_t access_mask, uint64_t sample_format_mask,
			   unsigned int min_samples_per_frame,
			   unsigned int max_samples_per_frame,
			   unsigned int min_frame_count,
			   unsigned int max_frame_count,
			   unsigned int step_frame_count,
			   unsigned int private_size)
{
	gen->fd = open("/dev/urandom", O_RDONLY);
	if (gen->fd < 0)
		return -errno;

	gen->private_data = malloc(private_size);
	if (gen->private_data == NULL)
		return -ENOMEM;
	memset(gen->private_data, 0, private_size);

	gen->access_mask = access_mask;
	gen->sample_format_mask = sample_format_mask;
	gen->min_samples_per_frame = min_samples_per_frame;
	gen->max_samples_per_frame = max_samples_per_frame;
	gen->min_frame_count = min_frame_count;
	gen->max_frame_count = max_frame_count;
	gen->step_frame_count = step_frame_count;

	return 0;
}

static void *allocate_buf(snd_pcm_access_t access,
			  snd_pcm_format_t sample_format,
			  unsigned int samples_per_frame,
			  unsigned int frame_count)
{
	unsigned int bytes_per_sample;

	bytes_per_sample = snd_pcm_format_physical_width(sample_format) / 8;

	return calloc(samples_per_frame * frame_count, bytes_per_sample);
}

static void *allocate_vector(snd_pcm_access_t access,
			     snd_pcm_format_t sample_format,
			     unsigned int samples_per_frame,
			     unsigned int frame_count)
{
	unsigned int bytes_per_sample;
	char **bufs;
	int i;

	bytes_per_sample = snd_pcm_format_physical_width(sample_format) / 8;

	bufs = calloc(samples_per_frame, sizeof(char *));
	if (bufs == NULL)
		return NULL;

	for (i = 0; i < samples_per_frame; ++i) {
		bufs[i] = calloc(frame_count, bytes_per_sample);
		if (bufs[i] == NULL) {
			for (; i >= 0; --i)
				free(bufs[i]);
			free(bufs);
			return NULL;
		}
	}

	return bufs;
}

static int fill_buf(int fd, void *frame_buffer, snd_pcm_access_t access,
		    snd_pcm_format_t sample_format,
		    unsigned int samples_per_frame, unsigned int frame_count)
{
	unsigned int size;
	int len;

	size = snd_pcm_format_physical_width(sample_format) / 8 *
						samples_per_frame * frame_count;
	while (size > 0) {
		len = read(fd, frame_buffer, size);
		if (len < 0)
			return len;
		size -= len;
	}

	return 0;
}

static int fill_vector(int fd, void *frame_buffer, snd_pcm_access_t access,
		       snd_pcm_format_t sample_format,
		       unsigned int samples_per_frame, unsigned int frame_count)
{
	char **bufs = frame_buffer;
	unsigned int size;
	int len;
	int i;

	for (i = 0; i < samples_per_frame; ++i) {
		size = frame_count *
			snd_pcm_format_physical_width(sample_format) / 8;

		while (size > 0) {
			len = read(fd, bufs[i], size);
			if (len < 0)
				return len;
			size -= len;
		}
	}

	return 0;
}

static void deallocate_buf(void *frame_buffer, unsigned int samples_per_frame)
{
	free(frame_buffer);
}

static void deallocate_vector(void *frame_buffer,
			      unsigned int samples_per_frame)
{
	char **bufs = frame_buffer;
	int i;

	for (i = 0; i < samples_per_frame; ++i)
		free(bufs[i]);

	free(bufs);
}

static int test_frame_count(struct test_generator *gen,
			    snd_pcm_access_t access,
			    snd_pcm_format_t sample_format,
			    unsigned int samples_per_frame)
{
	void *(*allocator)(snd_pcm_access_t access,
			   snd_pcm_format_t sample_format,
			   unsigned int samples_per_frame,
			   unsigned int frame_count);
	int (*fill)(int fd, void *frame_buffer, snd_pcm_access_t access,
		    snd_pcm_format_t sample_format,
		    unsigned int samples_per_frame, unsigned int frame_count);
	void (*deallocator)(void *frame_buffer, unsigned int samples_per_frame);
	void *frame_buffer;
	int i;
	int err = 0;

	if (access != SND_PCM_ACCESS_RW_NONINTERLEAVED) {
		allocator = allocate_buf;
		fill = fill_buf;
		deallocator = deallocate_buf;
	} else {
		allocator = allocate_vector;
		fill = fill_vector;
		deallocator = deallocate_vector;
	}

	frame_buffer = allocator(access, sample_format, samples_per_frame,
				 gen->max_frame_count);
	if (frame_buffer == NULL)
		return -ENOMEM;

	err = fill(gen->fd, frame_buffer, access, sample_format,
		   samples_per_frame, gen->max_frame_count);
	if (err < 0)
		goto end;


	for (i = gen->min_frame_count;
	     i <= gen->max_frame_count; i += gen->step_frame_count) {
		err = gen->cb(gen, access ,sample_format, samples_per_frame,
			      frame_buffer, i);
		if (err < 0)
			break;
	}
end:
	deallocator(frame_buffer, samples_per_frame);

	return err;
}

static int test_samples_per_frame(struct test_generator *gen,
				  snd_pcm_access_t access,
				  snd_pcm_format_t sample_format)
{
	int i;
	int err = 0;

	for (i = gen->min_samples_per_frame;
	     i <= gen->max_samples_per_frame; ++i) {
		err = test_frame_count(gen, access, sample_format, i);
		if (err < 0)
			break;
	}

	return err;
}

static int test_sample_format(struct test_generator *gen,
			      snd_pcm_access_t access)
{
	int i;
	int err = 0;

	for (i = 0; i <= SND_PCM_FORMAT_LAST; ++i) {
		if (!((1ull << i) & gen->sample_format_mask))
			continue;

		err = test_samples_per_frame(gen, access, i);
		if (err < 0)
			break;
	}

	return err;
}

static int test_access(struct test_generator *gen)
{
	int i;
	int err = 0;

	for (i = 0; i <= SND_PCM_ACCESS_LAST; ++i) {
		if (!((1ull << i) & gen->access_mask))
			continue;

		err = test_sample_format(gen, i);
		if (err < 0)
			break;
	}
	return err;
}

int generator_context_run(struct test_generator *gen, generator_cb_t cb)
{
	gen->cb = cb;
	return test_access(gen);
}

void generator_context_destroy(struct test_generator *gen)
{
	free(gen->private_data);
	close(gen->fd);
}
