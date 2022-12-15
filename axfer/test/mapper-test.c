// SPDX-License-Identifier: GPL-2.0
//
// mapper-io.c - a unit test for muxer/demuxer for PCM frames on buffer.
//
// Copyright (c) 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp>
//
// Licensed under the terms of the GNU General Public License, version 2.

#include <aconfig.h>
#ifdef HAVE_MEMFD_CREATE
#define _GNU_SOURCE
#endif

#include "../mapper.h"
#include "../misc.h"

#include "generator.h"

#ifdef HAVE_MEMFD_CREATE
#include <sys/mman.h>
#endif

#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#include <assert.h>

struct mapper_trial {
	enum container_format cntr_format;
	struct container_context *cntrs;

	char **paths;

	struct mapper_context mapper;
	bool verbose;
};

static void test_demuxer(struct mapper_context *mapper, snd_pcm_access_t access,
			 unsigned int bytes_per_sample,
			 unsigned int samples_per_frame,
			 unsigned int frames_per_buffer,
			 void *frame_buffer, unsigned int frame_count,
			 struct container_context *cntrs,
			 unsigned int cntr_count, bool verbose)
{
	unsigned int total_frame_count;
	int err;

	err = mapper_context_init(mapper, MAPPER_TYPE_DEMUXER, cntr_count,
				  verbose);
	assert(err == 0);

	err = mapper_context_pre_process(mapper, access, bytes_per_sample,
					 samples_per_frame, frames_per_buffer,
					 cntrs);
	assert(err == 0);

	total_frame_count = frame_count;
	err = mapper_context_process_frames(mapper, frame_buffer,
					    &total_frame_count, cntrs);
	assert(err == 0);
	assert(total_frame_count == frame_count);

	mapper_context_post_process(mapper);
	mapper_context_destroy(mapper);
}

static int test_demux(struct mapper_trial *trial, snd_pcm_access_t access,
		      snd_pcm_format_t sample_format,
		      unsigned int samples_per_frame,
		      unsigned int frames_per_second,
		      unsigned int frames_per_buffer,
		      void *frame_buffer, unsigned int frame_count,
		      int *cntr_fds, unsigned int cntr_count)
{
	struct container_context *cntrs = trial->cntrs;
	enum container_format cntr_format = trial->cntr_format;
	unsigned int bytes_per_sample;
	uint64_t total_frame_count;
	int i;
	int err = 0;

	for (i = 0; i < cntr_count; ++i) {
		snd_pcm_format_t format;
		unsigned int channels;
		unsigned int rate;

		err = container_builder_init(cntrs + i, cntr_fds[i], cntr_format, 0);
		if (err < 0)
			goto end;

		format = sample_format;
		rate = frames_per_second;
		total_frame_count = frame_count;
		if (cntr_count > 1)
			channels = 1;
		else
			channels = samples_per_frame;
		err = container_context_pre_process(cntrs + i, &format,
						    &channels, &rate,
						    &total_frame_count);
		if (err < 0)
			goto end;
		assert(format == sample_format);
		assert(rate == frames_per_second);
		assert(total_frame_count >= 0);
		if (cntr_count > 1)
			assert(channels == 1);
		else
			assert(channels == samples_per_frame);
	}

	bytes_per_sample = snd_pcm_format_physical_width(sample_format) / 8;
	test_demuxer(&trial->mapper, access, bytes_per_sample,
		     samples_per_frame, frames_per_buffer, frame_buffer,
		     frame_count, cntrs, cntr_count, trial->verbose);

	for (i = 0; i < cntr_count; ++i) {
		container_context_post_process(cntrs + i, &total_frame_count);
		assert(total_frame_count == frame_count);
	}
end:
	for (i = 0; i < cntr_count; ++i)
		container_context_destroy(cntrs + i);

	return err;
}

static void test_muxer(struct mapper_context *mapper, snd_pcm_access_t access,
		       unsigned int bytes_per_sample,
		       unsigned int samples_per_frame,
		       unsigned int frames_per_buffer,
		       void *frame_buffer, unsigned int frame_count,
		       struct container_context *cntrs,
		       unsigned int cntr_count, bool verbose)
{
	unsigned int total_frame_count;
	int err;

	err = mapper_context_init(mapper, MAPPER_TYPE_MUXER, cntr_count,
				  verbose);
	assert(err == 0);

	err = mapper_context_pre_process(mapper, access, bytes_per_sample,
					 samples_per_frame, frames_per_buffer,
					 cntrs);
	assert(err == 0);

	total_frame_count = frame_count;
	err = mapper_context_process_frames(mapper, frame_buffer,
					    &total_frame_count, cntrs);
	assert(err == 0);
	assert(total_frame_count == frame_count);

	mapper_context_post_process(mapper);
	mapper_context_destroy(mapper);
}

static int test_mux(struct mapper_trial *trial, snd_pcm_access_t access,
		    snd_pcm_format_t sample_format,
		    unsigned int samples_per_frame,
		    unsigned int frames_per_second,
		    unsigned int frames_per_buffer,
		    void *frame_buffer, unsigned int frame_count,
		    int *cntr_fds, unsigned int cntr_count)
{
	struct container_context *cntrs = trial->cntrs;
	unsigned int bytes_per_sample;
	uint64_t total_frame_count;
	int i;
	int err = 0;

	for (i = 0; i < cntr_count; ++i) {
		snd_pcm_format_t format;
		unsigned int channels;
		unsigned int rate;

		err = container_parser_init(cntrs + i, cntr_fds[i], 0);
		if (err < 0)
			goto end;

		format = sample_format;
		rate = frames_per_second;
		if (cntr_count > 1)
			channels = 1;
		else
			channels = samples_per_frame;
		err = container_context_pre_process(cntrs + i, &format,
						    &channels, &rate,
						    &total_frame_count);
		if (err < 0)
			goto end;

		assert(format == sample_format);
		assert(rate == frames_per_second);
		assert(total_frame_count == frame_count);
		if (cntr_count > 1)
			assert(channels == 1);
		else
			assert(channels == samples_per_frame);
	}

	bytes_per_sample = snd_pcm_format_physical_width(sample_format) / 8;
	test_muxer(&trial->mapper, access, bytes_per_sample, samples_per_frame,
		   frames_per_buffer, frame_buffer, frame_count, cntrs,
		   cntr_count, trial->verbose);

	for (i = 0; i < cntr_count; ++i) {
		container_context_post_process(cntrs + i, &total_frame_count);
		assert(total_frame_count == frame_count);
	}
end:
	for (i = 0; i < cntr_count; ++i)
		container_context_destroy(cntrs + i);

	return err;
}

static int test_mapper(struct mapper_trial *trial, snd_pcm_access_t access,
		    snd_pcm_format_t sample_format,
		    unsigned int samples_per_frame,
		    unsigned int frames_per_second, void *frame_buffer,
		    void *check_buffer, unsigned int frame_count,
		    unsigned int cntr_count)
{
	int *cntr_fds;
	unsigned int frames_per_buffer;
	int i;
	int err;

	// Use a buffer aligned by typical size of page frame.
	frames_per_buffer = ((frame_count + 4096) / 4096) * 4096;

	cntr_fds = calloc(cntr_count, sizeof(*cntr_fds));
	if (cntr_fds == NULL)
		return -ENOMEM;

	for (i = 0; i < cntr_count; ++i) {
		const char *path = trial->paths[i];

#ifdef HAVE_MEMFD_CREATE
		cntr_fds[i] = memfd_create(path, 0);
#else
		cntr_fds[i] = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
#endif
		if (cntr_fds[i] < 0) {
			err = -errno;
			goto end;
		}
	}

	err = test_demux(trial, access, sample_format, samples_per_frame,
			 frames_per_second, frames_per_buffer, frame_buffer,
			 frame_count, cntr_fds, cntr_count);
	if (err < 0)
		goto end;

	for (i = 0; i < cntr_count; ++i) {
		off_t pos = lseek(cntr_fds[i], 0, SEEK_SET);
		if (pos != 0) {
			err = -EIO;
			goto end;
		}
	}

	err = test_mux(trial, access, sample_format, samples_per_frame,
		       frames_per_second, frames_per_buffer, check_buffer,
		       frame_count, cntr_fds, cntr_count);
end:
	for (i = 0; i < cntr_count; ++i)
		close(cntr_fds[i]);

	free(cntr_fds);

	return err;
}

static int test_i_buf(struct mapper_trial *trial, snd_pcm_access_t access,
		      snd_pcm_format_t sample_format,
		      unsigned int samples_per_frame,
		      unsigned int frames_per_second, void *frame_buffer,
		      unsigned int frame_count, unsigned int cntr_count)
{
	unsigned int size;
	char *buf;
	int err;

	size = frame_count * samples_per_frame *
			snd_pcm_format_physical_width(sample_format) / 8;
	buf = malloc(size);
	if (buf == 0)
		return -ENOMEM;
	memset(buf, 0, size);

	// Test multiple target.
	err = test_mapper(trial, access, sample_format, samples_per_frame,
			  frames_per_second, frame_buffer, buf,
			  frame_count, cntr_count);
	if (err < 0)
		goto end;
	err = memcmp(frame_buffer, buf, size);
	assert(err == 0);

	// Test single target.
	err = test_mapper(trial, access, sample_format, samples_per_frame,
			  frames_per_second, frame_buffer, buf,
			  frame_count, 1);
	if (err < 0)
		goto end;
	err = memcmp(frame_buffer, buf, size);
	assert(err == 0);
end:
	free(buf);

	return err;
}

static int test_vector(struct mapper_trial *trial, snd_pcm_access_t access,
		       snd_pcm_format_t sample_format,
		       unsigned int samples_per_frame,
		       unsigned int frames_per_second, void *frame_buffer,
		       unsigned int frame_count, unsigned int cntr_count)
{
	unsigned int size;
	char **bufs;
	int i;
	int err;

	bufs = calloc(cntr_count, sizeof(*bufs));
	if (bufs == NULL)
		return -ENOMEM;

	size = frame_count * snd_pcm_format_physical_width(sample_format) / 8;

	for (i = 0; i < cntr_count; ++i) {
		bufs[i] = malloc(size);
		if (bufs[i] == NULL) {
			err = -ENOMEM;
			goto end;
		}
		memset(bufs[i], 0, size);
	}

	// Test multiple target.
	err = test_mapper(trial, access, sample_format, samples_per_frame,
			  frames_per_second, frame_buffer, bufs,
			  frame_count, cntr_count);
	if (err < 0)
		goto end;
	for (i = 0; i < cntr_count; ++i) {
		char **target = frame_buffer;
		err = memcmp(target[i], bufs[i], size);
		assert(err == 0);
	}

	// Test single target.
	err = test_mapper(trial, access, sample_format, samples_per_frame,
			  frames_per_second, frame_buffer, bufs,
			  frame_count, 1);
	if (err < 0)
		goto end;
	for (i = 0; i < cntr_count; ++i) {
		char **target = frame_buffer;
		err = memcmp(target[i], bufs[i], size);
		assert(err == 0);
	}
end:
	for (i = 0; i < cntr_count; ++i) {
		if (bufs[i])
			free(bufs[i]);
	}
	free(bufs);

	return err;
}

static int test_n_buf(struct mapper_trial *trial, snd_pcm_access_t access,
		      snd_pcm_format_t sample_format,
		      unsigned int samples_per_frame,
		      unsigned int frames_per_second, void *frame_buffer,
		      unsigned int frame_count, unsigned int cntr_count)
{
	char *test_buf = frame_buffer;
	unsigned int size;
	char **test_vec;
	int i;
	int err;

	size = frame_count * snd_pcm_format_physical_width(sample_format) / 8;

	test_vec = calloc(cntr_count * 2, sizeof(*test_vec));
	if (test_vec == NULL)
		return -ENOMEM;

	for (i = 0; i < cntr_count; ++i)
		test_vec[i] = test_buf + size * i;

	err = test_vector(trial, access, sample_format, samples_per_frame,
			  frames_per_second, test_vec, frame_count, cntr_count);
	free(test_vec);

	return err;
}

static int callback(struct test_generator *gen, snd_pcm_access_t access,
		    snd_pcm_format_t sample_format,
		    unsigned int samples_per_frame, void *frame_buffer,
		    unsigned int frame_count)
{

	int (*handler)(struct mapper_trial *trial, snd_pcm_access_t access,
		       snd_pcm_format_t sample_format,
		       unsigned int samples_per_frame,
		       unsigned int frames_per_second, void *frame_buffer,
		       unsigned int frame_count, unsigned int cntr_count);
	struct mapper_trial *trial = gen->private_data;

	if (access == SND_PCM_ACCESS_RW_NONINTERLEAVED)
		handler = test_vector;
	else if (access == SND_PCM_ACCESS_MMAP_NONINTERLEAVED)
		handler = test_n_buf;
	else
		handler = test_i_buf;

	return handler(trial, access, sample_format, samples_per_frame, 48000,
		       frame_buffer, frame_count, samples_per_frame);
};

int main(int argc, const char *argv[])
{
	// Test 8/16/18/20/24/32/64 bytes per sample.
	static const uint64_t sample_format_mask =
			(1ull << SND_PCM_FORMAT_U8) |
			(1ull << SND_PCM_FORMAT_S16_LE) |
			(1ull << SND_PCM_FORMAT_S18_3LE) |
			(1ull << SND_PCM_FORMAT_S20_3LE) |
			(1ull << SND_PCM_FORMAT_S24_LE) |
			(1ull << SND_PCM_FORMAT_S32_LE) |
			(1ull << SND_PCM_FORMAT_FLOAT64_LE);
	uint64_t access_mask;
	struct test_generator gen = {0};
	struct mapper_trial *trial;
	struct container_context *cntrs;
	unsigned int samples_per_frame;
	char **paths = NULL;
	snd_pcm_access_t access;
	bool verbose;
	int i;
	int err;

	// Test up to 32 channels.
	samples_per_frame = 32;
	cntrs = calloc(samples_per_frame, sizeof(*cntrs));
	if (cntrs == NULL)
		return -ENOMEM;

	paths = calloc(samples_per_frame, sizeof(*paths));
	if (paths == NULL) {
		err = -ENOMEM;
		goto end;
	}
	for (i = 0; i < samples_per_frame; ++i) {
		paths[i] = malloc(8);
		if (paths[i] == NULL) {
			err = -ENOMEM;
			goto end;
		}
		snprintf(paths[i], 8, "hoge%d", i);
	}

	if (argc > 1) {
		char *term;
		access = strtol(argv[1], &term, 10);
		if (errno != 0 || *term != '\0') {
			err = -EINVAL;;
			goto end;
		}
		if (access < SND_PCM_ACCESS_MMAP_INTERLEAVED &&
		    access > SND_PCM_ACCESS_RW_NONINTERLEAVED) {
			err = -EINVAL;
			goto end;
		}
		if (access == SND_PCM_ACCESS_MMAP_COMPLEX) {
			err = -EINVAL;
			goto end;
		}

		access_mask = 1ull << access;
		verbose = true;
	} else {
		access_mask = (1ull << SND_PCM_ACCESS_MMAP_INTERLEAVED) |
			      (1ull << SND_PCM_ACCESS_MMAP_NONINTERLEAVED) |
			      (1ull << SND_PCM_ACCESS_RW_INTERLEAVED) |
			      (1ull << SND_PCM_ACCESS_RW_NONINTERLEAVED);
		verbose = false;
	}

	err = generator_context_init(&gen, access_mask, sample_format_mask,
				     1, samples_per_frame,
				     23, 4500, 1024,
				     sizeof(struct mapper_trial));
	if (err < 0)
		goto end;

	trial = gen.private_data;
	trial->cntrs = cntrs;
	trial->cntr_format = CONTAINER_FORMAT_RIFF_WAVE;
	trial->paths = paths;
	trial->verbose = verbose;
	err = generator_context_run(&gen, callback);

	generator_context_destroy(&gen);
end:
	if (paths) {
		for (i = 0; i < samples_per_frame; ++i)
			free(paths[i]);
		free(paths);
	}
	free(cntrs);

	if (err < 0) {
		printf("%s\n", strerror(-err));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
