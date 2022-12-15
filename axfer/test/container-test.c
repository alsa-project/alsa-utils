// SPDX-License-Identifier: GPL-2.0
//
// container-io.c - a unit test for parser/builder of supported containers.
//
// Copyright (c) 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp>
//
// Licensed under the terms of the GNU General Public License, version 2.

#include <aconfig.h>
#ifdef HAVE_MEMFD_CREATE
#define _GNU_SOURCE
#endif

#include "../container.h"
#include "../misc.h"

#include "generator.h"

#ifdef HAVE_MEMFD_CREATE
#include <sys/mman.h>
#endif

#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#include <assert.h>

struct container_trial {
	enum container_format format;

	struct container_context cntr;
	bool verbose;
};

static void test_builder(struct container_context *cntr, int fd,
			 enum container_format format,
			 snd_pcm_access_t access,
			 snd_pcm_format_t sample_format,
			 unsigned int samples_per_frame,
			 unsigned int frames_per_second,
			 void *frame_buffer, unsigned int frame_count,
			 bool verbose)
{
	snd_pcm_format_t sample;
	unsigned int channels;
	unsigned int rate;
	uint64_t max_frame_count;
	unsigned int handled_frame_count;
	uint64_t total_frame_count;
	int err;

	err = container_builder_init(cntr, fd, format, verbose);
	assert(err == 0);

	sample = sample_format;
	channels = samples_per_frame;
	rate = frames_per_second;
	max_frame_count = 0;
	err = container_context_pre_process(cntr, &sample, &channels, &rate,
					    &max_frame_count);
	assert(err == 0);
	assert(sample == sample_format);
	assert(channels == samples_per_frame);
	assert(rate == frames_per_second);
	assert(max_frame_count > 0);

	handled_frame_count = frame_count;
	err = container_context_process_frames(cntr, frame_buffer,
					       &handled_frame_count);
	assert(err == 0);
	assert(handled_frame_count > 0);
	assert(handled_frame_count <= frame_count);

	total_frame_count = 0;
	err = container_context_post_process(cntr, &total_frame_count);
	assert(err == 0);
	assert(total_frame_count == frame_count);

	container_context_destroy(cntr);
}

static void test_parser(struct container_context *cntr, int fd,
			enum container_format format,
		        snd_pcm_access_t access, snd_pcm_format_t sample_format,
		        unsigned int samples_per_frame,
		        unsigned int frames_per_second,
		        void *frame_buffer, unsigned int frame_count,
			bool verbose)
{
	snd_pcm_format_t sample;
	unsigned int channels;
	unsigned int rate;
	uint64_t total_frame_count;
	unsigned int handled_frame_count;
	int err;

	err = container_parser_init(cntr, fd, verbose);
	assert(err == 0);

	sample = sample_format;
	channels = samples_per_frame;
	rate = frames_per_second;
	total_frame_count = 0;
	err = container_context_pre_process(cntr, &sample, &channels, &rate,
					    &total_frame_count);
	assert(err == 0);
	assert(sample == sample_format);
	assert(channels == samples_per_frame);
	assert(rate == frames_per_second);
	assert(total_frame_count == frame_count);

	handled_frame_count = total_frame_count;
	err = container_context_process_frames(cntr, frame_buffer,
					       &handled_frame_count);
	assert(err == 0);
	assert(handled_frame_count == frame_count);

	total_frame_count = 0;
	err = container_context_post_process(cntr, &total_frame_count);
	assert(err == 0);
	assert(total_frame_count == handled_frame_count);

	container_context_destroy(cntr);
}

static int callback(struct test_generator *gen, snd_pcm_access_t access,
		    snd_pcm_format_t sample_format,
		    unsigned int samples_per_frame, void *frame_buffer,
		    unsigned int frame_count)
{
	static const unsigned int entries[] = {
		[0] = 44100,
		[1] = 48000,
		[2] = 88200,
		[3] = 96000,
		[4] = 176400,
		[5] = 192000,
	};
	struct container_trial *trial = gen->private_data;
	unsigned int frames_per_second;
	const char *const name = "hoge";
	unsigned int size;
	void *buf;
	int i;
	int err = 0;

	size = frame_count * samples_per_frame *
			snd_pcm_format_physical_width(sample_format) / 8;
	buf = malloc(size);
	if (buf == NULL)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(entries); ++i) {
		int fd;
		off_t pos;

		frames_per_second = entries[i];

#ifdef HAVE_MEMFD_CREATE
		fd = memfd_create(name, 0);
#else
		fd = open(name, O_RDWR | O_CREAT | O_TRUNC, 0644);
#endif
		if (fd < 0) {
			err = -errno;
			break;
		}

		test_builder(&trial->cntr, fd, trial->format, access,
			     sample_format, samples_per_frame,
			     frames_per_second, frame_buffer, frame_count,
			     trial->verbose);

		pos = lseek(fd, 0, SEEK_SET);
		if (pos < 0) {
			err = -errno;
			break;
		}

		test_parser(&trial->cntr, fd, trial->format, access,
			    sample_format, samples_per_frame, frames_per_second,
			    buf, frame_count, trial->verbose);

		err = memcmp(buf, frame_buffer, size);
		assert(err == 0);

		close(fd);
	}

	free(buf);

	return err;
}

int main(int argc, const char *argv[])
{
	static const uint64_t sample_format_masks[] = {
		[CONTAINER_FORMAT_RIFF_WAVE] =
			(1ull << SND_PCM_FORMAT_U8) |
			(1ull << SND_PCM_FORMAT_S16_LE) |
			(1ull << SND_PCM_FORMAT_S16_BE) |
			(1ull << SND_PCM_FORMAT_S24_LE) |
			(1ull << SND_PCM_FORMAT_S24_BE) |
			(1ull << SND_PCM_FORMAT_S32_LE) |
			(1ull << SND_PCM_FORMAT_S32_BE) |
			(1ull << SND_PCM_FORMAT_FLOAT_LE) |
			(1ull << SND_PCM_FORMAT_FLOAT_BE) |
			(1ull << SND_PCM_FORMAT_FLOAT64_LE) |
			(1ull << SND_PCM_FORMAT_FLOAT64_BE) |
			(1ull << SND_PCM_FORMAT_MU_LAW) |
			(1ull << SND_PCM_FORMAT_A_LAW) |
			(1ull << SND_PCM_FORMAT_S24_3LE) |
			(1ull << SND_PCM_FORMAT_S24_3BE) |
			(1ull << SND_PCM_FORMAT_S20_3LE) |
			(1ull << SND_PCM_FORMAT_S20_3BE) |
			(1ull << SND_PCM_FORMAT_S18_3LE) |
			(1ull << SND_PCM_FORMAT_S18_3BE),
		[CONTAINER_FORMAT_AU] =
			(1ull << SND_PCM_FORMAT_S8) |
			(1ull << SND_PCM_FORMAT_S16_BE) |
			(1ull << SND_PCM_FORMAT_S32_BE) |
			(1ull << SND_PCM_FORMAT_FLOAT_BE) |
			(1ull << SND_PCM_FORMAT_FLOAT64_BE) |
			(1ull << SND_PCM_FORMAT_MU_LAW) |
			(1ull << SND_PCM_FORMAT_A_LAW),
		[CONTAINER_FORMAT_VOC] =
			(1ull << SND_PCM_FORMAT_U8) |
			(1ull << SND_PCM_FORMAT_S16_LE) |
			(1ull << SND_PCM_FORMAT_MU_LAW) |
			(1ull << SND_PCM_FORMAT_A_LAW),
		[CONTAINER_FORMAT_RAW] =
			(1ull << SND_PCM_FORMAT_S8) |
			(1ull << SND_PCM_FORMAT_U8) |
			(1ull << SND_PCM_FORMAT_S16_LE) |
			(1ull << SND_PCM_FORMAT_S16_BE) |
			(1ull << SND_PCM_FORMAT_U16_LE) |
			(1ull << SND_PCM_FORMAT_U16_BE) |
			(1ull << SND_PCM_FORMAT_S24_LE) |
			(1ull << SND_PCM_FORMAT_S24_BE) |
			(1ull << SND_PCM_FORMAT_U24_LE) |
			(1ull << SND_PCM_FORMAT_U24_BE) |
			(1ull << SND_PCM_FORMAT_S32_LE) |
			(1ull << SND_PCM_FORMAT_S32_BE) |
			(1ull << SND_PCM_FORMAT_U32_LE) |
			(1ull << SND_PCM_FORMAT_U32_BE) |
			(1ull << SND_PCM_FORMAT_FLOAT_LE) |
			(1ull << SND_PCM_FORMAT_FLOAT_BE) |
			(1ull << SND_PCM_FORMAT_FLOAT64_LE) |
			(1ull << SND_PCM_FORMAT_FLOAT64_BE) |
			(1ull << SND_PCM_FORMAT_IEC958_SUBFRAME_LE) |
			(1ull << SND_PCM_FORMAT_IEC958_SUBFRAME_BE) |
			(1ull << SND_PCM_FORMAT_MU_LAW) |
			(1ull << SND_PCM_FORMAT_A_LAW) |
			(1ull << SND_PCM_FORMAT_S24_3LE) |
			(1ull << SND_PCM_FORMAT_S24_3BE) |
			(1ull << SND_PCM_FORMAT_U24_3LE) |
			(1ull << SND_PCM_FORMAT_U24_3BE) |
			(1ull << SND_PCM_FORMAT_S20_3LE) |
			(1ull << SND_PCM_FORMAT_S20_3BE) |
			(1ull << SND_PCM_FORMAT_U20_3LE) |
			(1ull << SND_PCM_FORMAT_U20_3BE) |
			(1ull << SND_PCM_FORMAT_S18_3LE) |
			(1ull << SND_PCM_FORMAT_S18_3BE) |
			(1ull << SND_PCM_FORMAT_U18_3LE) |
			(1ull << SND_PCM_FORMAT_U18_3BE) |
			(1ull << SND_PCM_FORMAT_DSD_U8) |
			(1ull << SND_PCM_FORMAT_DSD_U16_LE) |
			(1ull << SND_PCM_FORMAT_DSD_U32_LE) |
			(1ull << SND_PCM_FORMAT_DSD_U16_BE) |
			(1ull << SND_PCM_FORMAT_DSD_U32_BE),
	};
	static const uint64_t access_mask =
		(1ull << SND_PCM_ACCESS_MMAP_INTERLEAVED) |
		(1ull << SND_PCM_ACCESS_RW_INTERLEAVED);
	struct test_generator gen = {0};
	struct container_trial *trial;
	int i;
	int begin;
	int end;
	bool verbose;
	int err;

	if (argc > 1) {
		char *term;
		begin = strtol(argv[1], &term, 10);
		if (errno || *term != '\0')
			return EXIT_FAILURE;
		if (begin < CONTAINER_FORMAT_RIFF_WAVE &&
		    begin > CONTAINER_FORMAT_RAW)
			return -EXIT_FAILURE;
		end = begin + 1;
		verbose = true;
	} else {
		begin = CONTAINER_FORMAT_RIFF_WAVE;
		end = CONTAINER_FORMAT_RAW + 1;
		verbose = false;
	}

	for (i = begin; i < end; ++i) {
		err = generator_context_init(&gen, access_mask,
					     sample_format_masks[i],
					     1, 32, 23, 3000, 512,
					     sizeof(struct container_trial));
		if (err >= 0) {
			trial = gen.private_data;
			trial->format = i;
			trial->verbose = verbose;
			err = generator_context_run(&gen, callback);
		}

		generator_context_destroy(&gen);

		if (err < 0)
			break;
	}

	if (err < 0) {
		printf("%s\n", strerror(-err));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
