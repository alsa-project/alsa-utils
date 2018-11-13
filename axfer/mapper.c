// SPDX-License-Identifier: GPL-2.0
//
// mapper.c - an interface of muxer/demuxer between buffer with data frames and
// 	      formatted files.
//
// Copyright (c) 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp>
//
// Licensed under the terms of the GNU General Public License, version 2.

#include "mapper.h"
#include "misc.h"

#include <stdio.h>

static const char *const mapper_type_labels[] = {
	[MAPPER_TYPE_MUXER] = "muxer",
	[MAPPER_TYPE_DEMUXER] = "demuxer",
};

static const char *const mapper_target_labels[] = {
	[MAPPER_TARGET_SINGLE] = "single",
	[MAPPER_TARGET_MULTIPLE] = "multiple",
};

int mapper_context_init(struct mapper_context *mapper,
			enum mapper_type type, unsigned int cntr_count,
			unsigned int verbose)
{
	const struct mapper_data *data = NULL;

	assert(mapper);
	assert(cntr_count > 0);

	// Detect forgotten to destruct.
	assert(mapper->private_data == NULL);

	memset(mapper, 0, sizeof(*mapper));

	if (type == MAPPER_TYPE_MUXER) {
		if (cntr_count == 1) {
			data = &mapper_muxer_single;
			mapper->target = MAPPER_TARGET_SINGLE;
		} else {
			data = &mapper_muxer_multiple;
			mapper->target = MAPPER_TARGET_MULTIPLE;
		}
	} else {
		if (cntr_count == 1) {
			data = &mapper_demuxer_single;
			mapper->target = MAPPER_TARGET_SINGLE;
		} else {
			data = &mapper_demuxer_multiple;
			mapper->target = MAPPER_TARGET_MULTIPLE;
		}
	}

	mapper->ops = &data->ops;
	mapper->type = type;

	mapper->private_data = malloc(data->private_size);
	if (mapper->private_data == NULL)
		return -ENOMEM;
	memset(mapper->private_data, 0, data->private_size);

	mapper->cntr_count = cntr_count;
	mapper->verbose = verbose;

	return 0;
}

int mapper_context_pre_process(struct mapper_context *mapper,
			       snd_pcm_access_t access,
			       unsigned int bytes_per_sample,
			       unsigned int samples_per_frame,
			       unsigned int frames_per_buffer,
			       struct container_context *cntrs)
{
	int err;

	assert(mapper);
	assert(access >= SND_PCM_ACCESS_MMAP_INTERLEAVED);
	assert(access <= SND_PCM_ACCESS_RW_NONINTERLEAVED);
	assert(bytes_per_sample > 0);
	assert(samples_per_frame > 0);
	assert(cntrs);

	// The purpose of multiple target is to mux/demux each channels to/from
	// containers.
	if (mapper->target == MAPPER_TARGET_MULTIPLE &&
	    samples_per_frame != mapper->cntr_count)
		return -EINVAL;

	mapper->access = access;
	mapper->bytes_per_sample = bytes_per_sample;
	mapper->samples_per_frame = samples_per_frame;
	mapper->frames_per_buffer = frames_per_buffer;

	err = mapper->ops->pre_process(mapper, cntrs, mapper->cntr_count);
	if (err < 0)
		return err;

	if (mapper->verbose > 0) {
		fprintf(stderr, "Mapper: %s\n",
		       mapper_type_labels[mapper->type]);
		fprintf(stderr, "  target: %s\n",
		       mapper_target_labels[mapper->target]);
		fprintf(stderr, "  access: %s\n",
		       snd_pcm_access_name(mapper->access));
		fprintf(stderr, "  bytes/sample: %u\n",
			mapper->bytes_per_sample);
		fprintf(stderr, "  samples/frame: %u\n",
			mapper->samples_per_frame);
		fprintf(stderr, "  frames/buffer: %lu\n",
			mapper->frames_per_buffer);
	}

	return 0;
}

int mapper_context_process_frames(struct mapper_context *mapper,
				  void *frame_buffer,
				  unsigned int *frame_count,
				  struct container_context *cntrs)
{
	assert(mapper);
	assert(frame_buffer);
	assert(frame_count);
	assert(*frame_count <= mapper->frames_per_buffer);
	assert(cntrs);

	return mapper->ops->process_frames(mapper, frame_buffer, frame_count,
					    cntrs, mapper->cntr_count);
}

void mapper_context_post_process(struct mapper_context *mapper)
{
	assert(mapper);

	if (mapper->ops && mapper->ops->post_process)
		mapper->ops->post_process(mapper);
}

void mapper_context_destroy(struct mapper_context *mapper)
{
	assert(mapper);

	if (mapper->private_data)
		free(mapper->private_data);
	mapper->private_data = NULL;
}
