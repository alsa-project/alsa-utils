// SPDX-License-Identifier: GPL-2.0
//
// container.h - an interface of parser/builder for formatted files.
//
// Copyright (c) 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp>
//
// Licensed under the terms of the GNU General Public License, version 2.

#ifndef __ALSA_UTILS_AXFER_CONTAINER__H_
#define __ALSA_UTILS_AXFER_CONTAINER__H_

#define _LARGEFILE64_SOURCE
#include <sys/types.h>

#include <stdbool.h>
#include <stdint.h>

#include <alsa/asoundlib.h>

#include "os_compat.h"

enum container_type {
	CONTAINER_TYPE_PARSER = 0,
	CONTAINER_TYPE_BUILDER,
	CONTAINER_TYPE_COUNT,
};

enum container_format {
	CONTAINER_FORMAT_RIFF_WAVE = 0,
	CONTAINER_FORMAT_AU,
	CONTAINER_FORMAT_VOC,
	CONTAINER_FORMAT_RAW,
	CONTAINER_FORMAT_COUNT,
};

struct container_ops;

struct container_context {
	enum container_type type;
	int fd;
	int (*process_bytes)(struct container_context *cntr,
			     void *buffer, unsigned int byte_count);
	bool magic_handled;
	bool eof;
	bool interrupted;
	bool stdio;

	enum container_format format;
	uint64_t max_size;
	char magic[4];
	const struct container_ops *ops;
	void *private_data;

	// Available after pre-process.
	unsigned int bytes_per_sample;
	unsigned int samples_per_frame;
	unsigned int frames_per_second;

	unsigned int verbose;
	uint64_t handled_byte_count;
};

const char *const container_suffix_from_format(enum container_format format);
enum container_format container_format_from_path(const char *path);
int container_parser_init(struct container_context *cntr, int fd,
			  unsigned int verbose);
int container_builder_init(struct container_context *cntr, int fd,
			   enum container_format format, unsigned int verbose);
void container_context_destroy(struct container_context *cntr);
int container_context_pre_process(struct container_context *cntr,
				  snd_pcm_format_t *format,
				  unsigned int *samples_per_frame,
				  unsigned int *frames_per_second,
				  uint64_t *frame_count);
int container_context_process_frames(struct container_context *cntr,
				     void *frame_buffer,
				     unsigned int *frame_count);
int container_context_post_process(struct container_context *cntr,
				   uint64_t *frame_count);

// For internal use in 'container' module.

struct container_ops {
	int (*pre_process)(struct container_context *cntr,
			   snd_pcm_format_t *format,
			   unsigned int *samples_per_frame,
			   unsigned int *frames_per_second,
			   uint64_t *byte_count);
	int (*post_process)(struct container_context *cntr,
			    uint64_t handled_byte_count);
};
struct container_parser {
	enum container_format format;
	const char *const magic;
	uint64_t max_size;
	struct container_ops ops;
	unsigned int private_size;
};

struct container_builder {
	enum container_format format;
	const char *const suffix;
	uint64_t max_size;
	struct container_ops ops;
	unsigned int private_size;
};

int container_recursive_read(struct container_context *cntr, void *buf,
			     unsigned int byte_count);
int container_recursive_write(struct container_context *cntr, void *buf,
			      unsigned int byte_count);
int container_seek_offset(struct container_context *cntr, off_t offset);

extern const struct container_parser container_parser_riff_wave;
extern const struct container_builder container_builder_riff_wave;

extern const struct container_parser container_parser_au;
extern const struct container_builder container_builder_au;

extern const struct container_parser container_parser_voc;
extern const struct container_builder container_builder_voc;

extern const struct container_parser container_parser_raw;
extern const struct container_builder container_builder_raw;

#endif
