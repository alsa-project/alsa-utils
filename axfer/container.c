// SPDX-License-Identifier: GPL-2.0
//
// container.c - an interface of parser/builder for formatted files.
//
// Copyright (c) 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp>
//
// Licensed under the terms of the GNU General Public License, version 2.

#include "container.h"
#include "misc.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>

static const char *const cntr_type_labels[] = {
	[CONTAINER_TYPE_PARSER] = "parser",
	[CONTAINER_TYPE_BUILDER] = "builder",
};

static const char *const cntr_format_labels[] = {
	[CONTAINER_FORMAT_RIFF_WAVE] = "riff/wave",
	[CONTAINER_FORMAT_AU] = "au",
	[CONTAINER_FORMAT_VOC] = "voc",
	[CONTAINER_FORMAT_RAW] = "raw",
};

static const char *const suffixes[] = {
	[CONTAINER_FORMAT_RIFF_WAVE]	= ".wav",
	[CONTAINER_FORMAT_AU]		= ".au",
	[CONTAINER_FORMAT_VOC]		= ".voc",
	[CONTAINER_FORMAT_RAW]		= "",
};

const char *const container_suffix_from_format(enum container_format format)
{
	return suffixes[format];
}

int container_recursive_read(struct container_context *cntr, void *buf,
			     unsigned int byte_count)
{
	char *dst = buf;
	ssize_t result;
	size_t consumed = 0;

	while (consumed < byte_count && !cntr->interrupted) {
		result = read(cntr->fd, dst + consumed, byte_count - consumed);
		if (result < 0) {
			// This descriptor was configured with non-blocking
			// mode. EINTR is not cought when get any interrupts.
			if (cntr->interrupted)
				return -EINTR;
			if (errno == EAGAIN)
				continue;
			return -errno;
		}
		// Reach EOF.
		if (result == 0) {
			cntr->eof = true;
			return 0;
		}

		consumed += result;
	}

	return 0;
}

int container_recursive_write(struct container_context *cntr, void *buf,
			      unsigned int byte_count)
{
	char *src = buf;
	ssize_t result;
	size_t consumed = 0;

	while (consumed < byte_count && !cntr->interrupted) {
		result = write(cntr->fd, src + consumed, byte_count - consumed);
		if (result < 0) {
			// This descriptor was configured with non-blocking
			// mode. EINTR is not cought when get any interrupts.
			if (cntr->interrupted)
				return -EINTR;
			if (errno == EAGAIN)
				continue;
			return -errno;
		}

		consumed += result;
	}

	return 0;
}

enum container_format container_format_from_path(const char *path)
{
	const char *suffix;
	const char *pos;
	int i;

	for (i = 0; i < ARRAY_SIZE(suffixes); ++i) {
		suffix = suffixes[i];

		// Check last part of the string.
		pos = path + strlen(path) - strlen(suffix);
		if (!strcmp(pos, suffix))
			return i;
	}

	// Unsupported.
	return CONTAINER_FORMAT_RAW;
}

int container_seek_offset(struct container_context *cntr, off_t offset)
{
	off_t pos;

	pos = lseek(cntr->fd, offset, SEEK_SET);
	if (pos < 0)
		return -errno;
	if (pos != offset)
		return -EIO;

	return 0;
}

// To avoid blocking execution at system call iteration after receiving UNIX
// signals.
static int set_nonblock_flag(int fd)
{
	int flags;

	flags = fcntl(fd, F_GETFL);
	if (flags < 0)
		return -errno;

	flags |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, flags) < 0)
		return -errno;

	return 0;
}

int container_parser_init(struct container_context *cntr, int fd,
			  unsigned int verbose)
{
	const struct container_parser *parsers[] = {
		[CONTAINER_FORMAT_RIFF_WAVE] = &container_parser_riff_wave,
		[CONTAINER_FORMAT_AU] = &container_parser_au,
		[CONTAINER_FORMAT_VOC] = &container_parser_voc,
	};
	const struct container_parser *parser;
	unsigned int size;
	int i;
	int err;

	assert(cntr);
	assert(fd >= 0);

	// Detect forgotten to destruct.
	assert(cntr->fd == 0);
	assert(cntr->private_data == NULL);

	memset(cntr, 0, sizeof(*cntr));

	cntr->fd = fd;

	cntr->stdio = (cntr->fd == fileno(stdin));
	if (cntr->stdio) {
		if (isatty(cntr->fd)) {
			fprintf(stderr,
				"A terminal is referred for standard input. "
				"Output from any process or shell redirection "
				"should be referred instead.\n");
			return -EIO;
		}
	}

	err = set_nonblock_flag(cntr->fd);
	if (err < 0)
		return err;

	// 4 bytes are enough to detect supported containers.
	err = container_recursive_read(cntr, cntr->magic, sizeof(cntr->magic));
	if (err < 0)
		return err;
	for (i = 0; i < ARRAY_SIZE(parsers); ++i) {
		parser = parsers[i];
		size = strlen(parser->magic);
		if (size > 4)
			size = 4;
		if (!strncmp(cntr->magic, parser->magic, size))
			break;
	}

	// Don't forget that the first 4 bytes were already read for magic
	// bytes.
	cntr->magic_handled = false;

	// Unless detected, use raw container.
	if (i == ARRAY_SIZE(parsers))
		parser = &container_parser_raw;

	// Allocate private data for the parser.
	if (parser->private_size > 0) {
		cntr->private_data = malloc(parser->private_size);
		if (cntr->private_data == NULL)
			return -ENOMEM;
		memset(cntr->private_data, 0, parser->private_size);
	}

	cntr->type = CONTAINER_TYPE_PARSER;
	cntr->process_bytes = container_recursive_read;
	cntr->format = parser->format;
	cntr->ops = &parser->ops;
	cntr->max_size = parser->max_size;
	cntr->verbose = verbose;

	return 0;
}

int container_builder_init(struct container_context *cntr, int fd,
			   enum container_format format, unsigned int verbose)
{
	const struct container_builder *builders[] = {
		[CONTAINER_FORMAT_RIFF_WAVE] = &container_builder_riff_wave,
		[CONTAINER_FORMAT_AU] = &container_builder_au,
		[CONTAINER_FORMAT_VOC] = &container_builder_voc,
		[CONTAINER_FORMAT_RAW] = &container_builder_raw,
	};
	const struct container_builder *builder;
	int err;

	assert(cntr);
	assert(fd >= 0);

	// Detect forgotten to destruct.
	assert(cntr->fd == 0);
	assert(cntr->private_data == NULL);

	memset(cntr, 0, sizeof(*cntr));

	cntr->fd = fd;

	cntr->stdio = (cntr->fd == fileno(stdout));
	if (cntr->stdio) {
		if (isatty(cntr->fd)) {
			fprintf(stderr,
				"A terminal is referred for standard output. "
				"Input to any process or shell redirection "
				"should be referred instead.\n");
			return -EIO;
		}
	}

	err = set_nonblock_flag(cntr->fd);
	if (err < 0)
		return err;

	builder = builders[format];

	// Allocate private data for the builder.
	if (builder->private_size > 0) {
		cntr->private_data = malloc(builder->private_size);
		if (cntr->private_data == NULL)
			return -ENOMEM;
		memset(cntr->private_data, 0, builder->private_size);
	}

	cntr->type = CONTAINER_TYPE_BUILDER;
	cntr->process_bytes = container_recursive_write;
	cntr->format = builder->format;
	cntr->ops = &builder->ops;
	cntr->max_size = builder->max_size;
	cntr->verbose = verbose;

	return 0;
}

int container_context_pre_process(struct container_context *cntr,
				  snd_pcm_format_t *format,
				  unsigned int *samples_per_frame,
				  unsigned int *frames_per_second,
				  uint64_t *frame_count)
{
	uint64_t byte_count = 0;
	unsigned int bytes_per_frame;
	int err;

	assert(cntr);
	assert(format);
	assert(samples_per_frame);
	assert(frames_per_second);
	assert(frame_count);

	if (cntr->type == CONTAINER_TYPE_BUILDER)
		byte_count = cntr->max_size;

	if (cntr->ops->pre_process) {
		err = cntr->ops->pre_process(cntr, format, samples_per_frame,
					     frames_per_second, &byte_count);
		if (err < 0)
			return err;
		if (cntr->eof)
			return 0;
	}

	if (cntr->format == CONTAINER_FORMAT_RAW) {
		if (*format == SND_PCM_FORMAT_UNKNOWN ||
		    *samples_per_frame == 0 || *frames_per_second == 0) {
			fprintf(stderr,
				"Any file format is not detected. Need to "
				"indicate all of sample format, channels and "
				"rate explicitly.\n");
			return -EINVAL;
		}
	}
	assert(*format >= SND_PCM_FORMAT_S8);
	assert(*format <= SND_PCM_FORMAT_LAST);
	assert(*samples_per_frame > 0);
	assert(*frames_per_second > 0);
	assert(byte_count > 0);

	cntr->bytes_per_sample = snd_pcm_format_physical_width(*format) / 8;
	cntr->samples_per_frame = *samples_per_frame;
	cntr->frames_per_second = *frames_per_second;

	bytes_per_frame = cntr->bytes_per_sample * *samples_per_frame;
	*frame_count = byte_count / bytes_per_frame;
	cntr->max_size -= cntr->max_size / bytes_per_frame;

	if (cntr->verbose > 0) {
		fprintf(stderr, "Container: %s\n",
			cntr_type_labels[cntr->type]);
		fprintf(stderr, "  format: %s\n",
			cntr_format_labels[cntr->format]);
		fprintf(stderr, "  sample format: %s\n",
			snd_pcm_format_name(*format));
		fprintf(stderr, "  bytes/sample: %u\n",
			cntr->bytes_per_sample);
		fprintf(stderr, "  samples/frame: %u\n",
			cntr->samples_per_frame);
		fprintf(stderr, "  frames/second: %u\n",
			cntr->frames_per_second);
		if (cntr->type == CONTAINER_TYPE_PARSER) {
			fprintf(stderr, "  frames: %" PRIu64 "\n",
				*frame_count);
		} else {
			fprintf(stderr, "  max frames: %" PRIu64 "\n",
				*frame_count);
		}
	}

	return 0;
}

int container_context_process_frames(struct container_context *cntr,
				     void *frame_buffer,
				     unsigned int *frame_count)
{
	char *buf = frame_buffer;
	unsigned int bytes_per_frame;
	unsigned int byte_count;
	unsigned int target_byte_count;
	int err;

	assert(cntr);
	assert(!cntr->eof);
	assert(frame_buffer);
	assert(frame_count);

	bytes_per_frame = cntr->bytes_per_sample * cntr->samples_per_frame;
	target_byte_count = *frame_count * bytes_per_frame;

	// A parser of cotainers already read first 4 bytes to detect format
	// of container, however they includes PCM frames when any format was
	// undetected. Surely to write out them.
	byte_count = target_byte_count;
	if (cntr->format == CONTAINER_FORMAT_RAW &&
	    cntr->type == CONTAINER_TYPE_PARSER && !cntr->magic_handled) {
		memcpy(buf, cntr->magic, sizeof(cntr->magic));
		buf += sizeof(cntr->magic);
		byte_count -= sizeof(cntr->magic);
		cntr->magic_handled = true;
	}

	// Each container has limitation for its volume for sample data.
	if (cntr->handled_byte_count > cntr->max_size - byte_count)
		byte_count = cntr->max_size - cntr->handled_byte_count;

	// All of supported containers include interleaved PCM frames.
	// TODO: process frames for truncate case.
	err = cntr->process_bytes(cntr, buf, byte_count);
	if (err < 0) {
		*frame_count = 0;
		return err;
	}

	cntr->handled_byte_count += target_byte_count;
	if (cntr->handled_byte_count == cntr->max_size)
		cntr->eof = true;

	*frame_count = target_byte_count / bytes_per_frame;

	return 0;
}

int container_context_post_process(struct container_context *cntr,
				   uint64_t *frame_count)
{
	int err = 0;

	assert(cntr);
	assert(frame_count);

	if (cntr->verbose && cntr->handled_byte_count > 0) {
		fprintf(stderr, "  Handled bytes: %" PRIu64 "\n",
			cntr->handled_byte_count);
	}

	// NOTE* we cannot seek when using standard input/output.
	if (!cntr->stdio && cntr->ops && cntr->ops->post_process) {
		// Usually, need to write out processed bytes in container
		// header even it this program is interrupted.
		cntr->interrupted = false;

		err = cntr->ops->post_process(cntr, cntr->handled_byte_count);
	}

	// Ensure to perform write-back from disk cache.
	if (cntr->type == CONTAINER_TYPE_BUILDER)
		fsync(cntr->fd);

	if (err < 0)
		return err;

	if (cntr->bytes_per_sample == 0 || cntr->samples_per_frame == 0) {
		*frame_count = 0;
	} else {
		*frame_count = cntr->handled_byte_count /
			       cntr->bytes_per_sample /
			       cntr->samples_per_frame;
	}

	return 0;
}

void container_context_destroy(struct container_context *cntr)
{
	assert(cntr);

	if (cntr->private_data)
		free(cntr->private_data);

	cntr->fd = 0;
	cntr->private_data = NULL;
}
