// SPDX-License-Identifier: GPL-2.0
//
// container-riff-wave.c - a parser/builder for a container of RIFF/Wave File.
//
// Copyright (c) 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp>
//
// Licensed under the terms of the GNU General Public License, version 2.

#include "container.h"
#include "misc.h"

// Not portable to all of UNIX platforms.
#include <endian.h>

// References:
// - 'Resource Interchange File Format (RIFF)' at msdn.microsoft.com
// - 'Multiple channel audio data and WAVE files' at msdn.microsoft.com
// - RFC 2361 'WAVE and AVI Codec Registries' at ietf.org
// - 'mmreg.h' in Wine project
// - 'mmreg.h' in ReactOS project

#define RIFF_MAGIC		"RIF"	// A common part.

#define RIFF_CHUNK_ID_LE	"RIFF"
#define RIFF_CHUNK_ID_BE	"RIFX"
#define RIFF_FORM_WAVE		"WAVE"
#define FMT_SUBCHUNK_ID		"fmt "
#define DATA_SUBCHUNK_ID	"data"

// See 'WAVE and AVI Codec Registries (Historic Registry)' in 'iana.org'.
// https://www.iana.org/assignments/wave-avi-codec-registry/
enum wave_format {
	WAVE_FORMAT_PCM			= 0x0001,
	WAVE_FORMAT_ADPCM		= 0x0002,
	WAVE_FORMAT_IEEE_FLOAT		= 0x0003,
	WAVE_FORMAT_ALAW		= 0x0006,
	WAVE_FORMAT_MULAW		= 0x0007,
	WAVE_FORMAT_G723_ADPCM		= 0x0014,
	// The others are not supported.
};

struct format_map {
	enum wave_format wformat;
	snd_pcm_format_t format;
};

static const struct format_map format_maps[] = {
	{WAVE_FORMAT_PCM,	SND_PCM_FORMAT_U8},
	{WAVE_FORMAT_PCM,	SND_PCM_FORMAT_S16_LE},
	{WAVE_FORMAT_PCM,	SND_PCM_FORMAT_S16_BE},
	{WAVE_FORMAT_PCM,	SND_PCM_FORMAT_S24_LE},
	{WAVE_FORMAT_PCM,	SND_PCM_FORMAT_S24_BE},
	{WAVE_FORMAT_PCM,	SND_PCM_FORMAT_S32_LE},
	{WAVE_FORMAT_PCM,	SND_PCM_FORMAT_S32_BE},
	{WAVE_FORMAT_PCM,	SND_PCM_FORMAT_S24_3LE},
	{WAVE_FORMAT_PCM,	SND_PCM_FORMAT_S24_3BE},
	{WAVE_FORMAT_PCM,	SND_PCM_FORMAT_S20_3LE},
	{WAVE_FORMAT_PCM,	SND_PCM_FORMAT_S20_3BE},
	{WAVE_FORMAT_PCM,	SND_PCM_FORMAT_S18_3LE},
	{WAVE_FORMAT_PCM,	SND_PCM_FORMAT_S18_3BE},
	{WAVE_FORMAT_IEEE_FLOAT, SND_PCM_FORMAT_FLOAT_LE},
	{WAVE_FORMAT_IEEE_FLOAT, SND_PCM_FORMAT_FLOAT_BE},
	{WAVE_FORMAT_IEEE_FLOAT, SND_PCM_FORMAT_FLOAT64_LE},
	{WAVE_FORMAT_IEEE_FLOAT, SND_PCM_FORMAT_FLOAT64_BE},
	{WAVE_FORMAT_ALAW,	SND_PCM_FORMAT_A_LAW},
	{WAVE_FORMAT_MULAW,	SND_PCM_FORMAT_MU_LAW},
	// Below sample formats are not currently supported, due to width of
	// its sample.
	//  - WAVE_FORMAT_ADPCM
	//  - WAVE_FORMAT_G723_ADPCM
	//  - WAVE_FORMAT_G723_ADPCM
	//  - WAVE_FORMAT_G723_ADPCM
	//  - WAVE_FORMAT_G723_ADPCM
};

struct riff_chunk {
	uint8_t id[4];
	uint32_t size;

	uint8_t data[0];
};

struct riff_chunk_data {
	uint8_t id[4];

	uint8_t subchunks[0];
};

struct riff_subchunk {
	uint8_t id[4];
	uint32_t size;

	uint8_t data[0];
};

struct wave_fmt_subchunk {
	uint8_t id[4];
	uint32_t size;

	uint16_t format;
	uint16_t samples_per_frame;
	uint32_t frames_per_second;
	uint32_t average_bytes_per_second;
	uint16_t bytes_per_frame;
	uint16_t bits_per_sample;
	uint8_t extension[0];
};

struct wave_data_subchunk {
	uint8_t id[4];
	uint32_t size;

	uint8_t frames[0];
};

struct parser_state {
	bool be;
	enum wave_format format;
	unsigned int samples_per_frame;
	unsigned int frames_per_second;
	unsigned int average_bytes_per_second;
	unsigned int bytes_per_frame;
	unsigned int bytes_per_sample;
	unsigned int avail_bits_in_sample;
	unsigned int byte_count;
};

static int parse_riff_chunk_header(struct parser_state *state,
				   struct riff_chunk *chunk,
				   uint64_t *byte_count)
{
	if (!memcmp(chunk->id, RIFF_CHUNK_ID_BE, sizeof(chunk->id)))
		state->be = true;
	else if (!memcmp(chunk->id, RIFF_CHUNK_ID_LE, sizeof(chunk->id)))
		state->be = false;
	else
		return -EINVAL;

	if (state->be)
		*byte_count = be32toh(chunk->size);
	else
		*byte_count = le32toh(chunk->size);

	return 0;
}

static int parse_riff_chunk(struct container_context *cntr,
			    uint64_t *byte_count)
{
	struct parser_state *state = cntr->private_data;
	union {
		struct riff_chunk chunk;
		struct riff_chunk_data chunk_data;
	} buf = {0};
	int err;

	// Chunk header. 4 bytes were alread read to detect container type.
	memcpy(buf.chunk.id, cntr->magic, sizeof(cntr->magic));
	err = container_recursive_read(cntr,
				       (char *)&buf.chunk + sizeof(cntr->magic),
				       sizeof(buf.chunk) - sizeof(cntr->magic));
	if (err < 0)
		return err;
	if (cntr->eof)
		return 0;

	err = parse_riff_chunk_header(state, &buf.chunk, byte_count);
	if (err < 0)
		return err;

	// Chunk data header.
	err = container_recursive_read(cntr, &buf, sizeof(buf.chunk_data));
	if (err < 0)
		return err;
	if (cntr->eof)
		return 0;

	if (memcmp(buf.chunk_data.id, RIFF_FORM_WAVE,
		   sizeof(buf.chunk_data.id)))
		return -EINVAL;

	return 0;
}

static int parse_wave_fmt_subchunk(struct parser_state *state,
				   struct wave_fmt_subchunk *subchunk)
{
	if (state->be) {
		state->format = be16toh(subchunk->format);
		state->samples_per_frame = be16toh(subchunk->samples_per_frame);
		state->frames_per_second = be32toh(subchunk->frames_per_second);
		state->average_bytes_per_second =
				be32toh(subchunk->average_bytes_per_second);
		state->bytes_per_frame = be16toh(subchunk->bytes_per_frame);
		state->avail_bits_in_sample =
					be16toh(subchunk->bits_per_sample);
	} else {
		state->format = le16toh(subchunk->format);
		state->samples_per_frame = le16toh(subchunk->samples_per_frame);
		state->frames_per_second = le32toh(subchunk->frames_per_second);
		state->average_bytes_per_second =
				le32toh(subchunk->average_bytes_per_second);
		state->bytes_per_frame = le16toh(subchunk->bytes_per_frame);
		state->avail_bits_in_sample =
					le16toh(subchunk->bits_per_sample);
	}

	if (state->average_bytes_per_second !=
			state->bytes_per_frame * state->frames_per_second)
		return -EINVAL;

	return 0;
}

static int parse_wave_data_subchunk(struct parser_state *state,
				    struct wave_data_subchunk *subchunk)
{
	if (state->be)
		state->byte_count = be32toh(subchunk->size);
	else
		state->byte_count = le32toh(subchunk->size);

	return 0;
}

static int parse_wave_subchunk(struct container_context *cntr)
{
	union {
		struct riff_subchunk subchunk;
		struct wave_fmt_subchunk fmt_subchunk;
		struct wave_data_subchunk data_subchunk;
	} buf = {0};
	enum {
		SUBCHUNK_TYPE_UNKNOWN = -1,
		SUBCHUNK_TYPE_FMT,
		SUBCHUNK_TYPE_DATA,
	} subchunk_type;
	struct parser_state *state = cntr->private_data;
	unsigned int required_size;
	unsigned int subchunk_data_size;
	int err;

	while (1) {
		err = container_recursive_read(cntr, &buf,
					       sizeof(buf.subchunk));
		if (err < 0)
			return err;
		if (cntr->eof)
			return 0;

		// Calculate the size of subchunk data.
		if (state->be)
			subchunk_data_size = be32toh(buf.subchunk.size);
		else
			subchunk_data_size = le32toh(buf.subchunk.size);

		// Detect type of subchunk.
		if (!memcmp(buf.subchunk.id, FMT_SUBCHUNK_ID,
			    sizeof(buf.subchunk.id))) {
			subchunk_type = SUBCHUNK_TYPE_FMT;
		} else if (!memcmp(buf.subchunk.id, DATA_SUBCHUNK_ID,
				   sizeof(buf.subchunk.id))) {
			subchunk_type = SUBCHUNK_TYPE_DATA;
		} else {
			subchunk_type = SUBCHUNK_TYPE_UNKNOWN;
		}

		if (subchunk_type != SUBCHUNK_TYPE_UNKNOWN) {
			// Parse data of this subchunk.
			if (subchunk_type == SUBCHUNK_TYPE_FMT) {
				required_size =
					sizeof(struct wave_fmt_subchunk) -
					sizeof(struct riff_chunk);
			} else {
				required_size =
					sizeof(struct wave_data_subchunk)-
					sizeof(struct riff_chunk);
			}

			if (subchunk_data_size < required_size)
				return -EINVAL;

			err = container_recursive_read(cntr, &buf.subchunk.data,
						       required_size);
			if (err < 0)
				return err;
			if (cntr->eof)
				return 0;
			subchunk_data_size -= required_size;

			if (subchunk_type == SUBCHUNK_TYPE_FMT) {
				err = parse_wave_fmt_subchunk(state,
							&buf.fmt_subchunk);
			} else if (subchunk_type == SUBCHUNK_TYPE_DATA) {
				err = parse_wave_data_subchunk(state,
							 &buf.data_subchunk);
			}
			if (err < 0)
				return err;

			// Found frame data.
			if (subchunk_type == SUBCHUNK_TYPE_DATA)
				break;
		}

		// Go to next subchunk.
		while (subchunk_data_size > 0) {
			unsigned int consume;

			if (subchunk_data_size > sizeof(buf))
				consume = sizeof(buf);
			else
				consume = subchunk_data_size;

			err = container_recursive_read(cntr, &buf, consume);
			if (err < 0)
				return err;
			if (cntr->eof)
				return 0;
			subchunk_data_size -= consume;
		}
	}

	return 0;
}

static int parse_riff_wave_format(struct container_context *cntr)
{
	uint64_t byte_count;
	int err;

	err = parse_riff_chunk(cntr, &byte_count);
	if (err < 0)
		return err;

	err = parse_wave_subchunk(cntr);
	if (err < 0)
		return err;

	return 0;
}

static int wave_parser_pre_process(struct container_context *cntr,
				   snd_pcm_format_t *format,
				   unsigned int *samples_per_frame,
				   unsigned int *frames_per_second,
				   uint64_t *byte_count)
{
	struct parser_state *state = cntr->private_data;
	int phys_width;
	const struct format_map *map;
	int i;
	int err;

	err = parse_riff_wave_format(cntr);
	if (err < 0)
		return err;

	phys_width = 8 * state->average_bytes_per_second /
		     state->samples_per_frame / state->frames_per_second;

	for (i = 0; i < ARRAY_SIZE(format_maps); ++i) {
		map = &format_maps[i];
		if (state->format != map->wformat)
			continue;
		if (state->avail_bits_in_sample !=
					snd_pcm_format_width(map->format))
			continue;
		if (phys_width != snd_pcm_format_physical_width(map->format))
			continue;

		if (state->be && snd_pcm_format_big_endian(map->format) != 1)
			continue;

		break;
	}
	if (i == ARRAY_SIZE(format_maps))
		return -EINVAL;

	// Set parameters.
	*format = format_maps[i].format;
	*samples_per_frame = state->samples_per_frame;
	*frames_per_second = state->frames_per_second;
	*byte_count = state->byte_count;

	return 0;
}

struct builder_state {
	bool be;
	enum wave_format format;
	unsigned int avail_bits_in_sample;
	unsigned int bytes_per_sample;
	unsigned int samples_per_frame;
	unsigned int frames_per_second;
};

static void build_riff_chunk_header(struct riff_chunk *chunk,
				    uint64_t byte_count, bool be)
{
	uint64_t data_size = sizeof(struct riff_chunk_data) +
			     sizeof(struct wave_fmt_subchunk) +
			     sizeof(struct wave_data_subchunk) + byte_count;

	if (be) {
		memcpy(chunk->id, RIFF_CHUNK_ID_BE, sizeof(chunk->id));
		chunk->size = htobe32(data_size);
	} else {
		memcpy(chunk->id, RIFF_CHUNK_ID_LE, sizeof(chunk->id));
		chunk->size = htole32(data_size);
	}
}

static void build_subchunk_header(struct riff_subchunk *subchunk,
				  const char *const form, uint64_t size,
				  bool be)
{
	memcpy(subchunk->id, form, sizeof(subchunk->id));
	if (be)
		subchunk->size = htobe32(size);
	else
		subchunk->size = htole32(size);
}

static void build_wave_format_subchunk(struct wave_fmt_subchunk *subchunk,
				       struct builder_state *state)
{
	unsigned int bytes_per_frame =
			state->bytes_per_sample * state->samples_per_frame;
	unsigned int average_bytes_per_second = state->bytes_per_sample *
			state->samples_per_frame * state->frames_per_second;
	uint64_t size;

	// No extensions.
	size = sizeof(struct wave_fmt_subchunk) - sizeof(struct riff_subchunk);
	build_subchunk_header((struct riff_subchunk *)subchunk, FMT_SUBCHUNK_ID,
			      size, state->be);

	if (state->be) {
		subchunk->format = htobe16(state->format);
		subchunk->samples_per_frame = htobe16(state->samples_per_frame);
		subchunk->frames_per_second = htobe32(state->frames_per_second);
		subchunk->average_bytes_per_second =
					htobe32(average_bytes_per_second);
		subchunk->bytes_per_frame = htobe16(bytes_per_frame);
		subchunk->bits_per_sample =
					htobe16(state->avail_bits_in_sample);
	} else {
		subchunk->format = htole16(state->format);
		subchunk->samples_per_frame = htole16(state->samples_per_frame);
		subchunk->frames_per_second = htole32(state->frames_per_second);
		subchunk->average_bytes_per_second =
					htole32(average_bytes_per_second);
		subchunk->bytes_per_frame = htole16(bytes_per_frame);
		subchunk->bits_per_sample =
					htole16(state->avail_bits_in_sample);
	}
}

static void build_wave_data_subchunk(struct wave_data_subchunk *subchunk,
				     uint64_t byte_count, bool be)
{
	build_subchunk_header((struct riff_subchunk *)subchunk,
			      DATA_SUBCHUNK_ID, byte_count, be);
}

static int write_riff_chunk_for_wave(struct container_context *cntr,
				     uint64_t byte_count)
{
	struct builder_state *state = cntr->private_data;
	union {
		struct riff_chunk chunk;
		struct riff_chunk_data chunk_data;
		struct wave_fmt_subchunk fmt_subchunk;
		struct wave_data_subchunk data_subchunk;
	} buf = {0};
	uint64_t total_byte_count;
	int err;

	// Chunk header.
	total_byte_count = sizeof(struct riff_chunk_data) +
			   sizeof(struct wave_fmt_subchunk) +
			   sizeof(struct wave_data_subchunk);
	if (byte_count > cntr->max_size - total_byte_count)
		total_byte_count = cntr->max_size;
	else
		total_byte_count += byte_count;
	build_riff_chunk_header(&buf.chunk, total_byte_count, state->be);
	err = container_recursive_write(cntr, &buf, sizeof(buf.chunk));
	if (err < 0)
		return err;

	// Chunk data header.
	memcpy(buf.chunk_data.id, RIFF_FORM_WAVE, sizeof(buf.chunk_data.id));
	err = container_recursive_write(cntr, &buf, sizeof(buf.chunk_data));
	if (err < 0)
		return err;

	// A subchunk in the chunk data for WAVE format.
	build_wave_format_subchunk(&buf.fmt_subchunk, state);
	err = container_recursive_write(cntr, &buf, sizeof(buf.fmt_subchunk));
	if (err < 0)
		return err;

	// A subchunk in the chunk data for WAVE data.
	build_wave_data_subchunk(&buf.data_subchunk, byte_count, state->be);
	return container_recursive_write(cntr, &buf, sizeof(buf.data_subchunk));
}

static int wave_builder_pre_process(struct container_context *cntr,
				    snd_pcm_format_t *format,
				    unsigned int *samples_per_frame,
				    unsigned int *frames_per_second,
				    uint64_t *byte_count)
{
	struct builder_state *state = cntr->private_data;
	int i;

	// Validate parameters.
	for (i = 0; i < ARRAY_SIZE(format_maps); ++i) {
		if (format_maps[i].format == *format)
			break;
	}
	if (i == ARRAY_SIZE(format_maps))
		return -EINVAL;

	state->format = format_maps[i].wformat;
	state->avail_bits_in_sample = snd_pcm_format_width(*format);
	state->bytes_per_sample = snd_pcm_format_physical_width(*format) / 8;
	state->samples_per_frame = *samples_per_frame;
	state->frames_per_second = *frames_per_second;

	state->be = (snd_pcm_format_big_endian(*format) == 1);

	return write_riff_chunk_for_wave(cntr, *byte_count);
}

static int wave_builder_post_process(struct container_context *cntr,
				     uint64_t handled_byte_count)
{
	int err;

	err = container_seek_offset(cntr, 0);
	if (err < 0)
		return err;

	return write_riff_chunk_for_wave(cntr, handled_byte_count);
}

const struct container_parser container_parser_riff_wave = {
	.format = CONTAINER_FORMAT_RIFF_WAVE,
	.magic =  RIFF_MAGIC,
	.max_size = UINT32_MAX -
		    sizeof(struct riff_chunk_data) -
		    sizeof(struct wave_fmt_subchunk) -
		    sizeof(struct wave_data_subchunk),
	.ops = {
		.pre_process	= wave_parser_pre_process,
	},
	.private_size = sizeof(struct parser_state),
};

const struct container_builder container_builder_riff_wave = {
	.format = CONTAINER_FORMAT_RIFF_WAVE,
	.max_size = UINT32_MAX -
		    sizeof(struct riff_chunk_data) -
		    sizeof(struct wave_fmt_subchunk) -
		    sizeof(struct wave_data_subchunk),
	.ops = {
		.pre_process	= wave_builder_pre_process,
		.post_process	= wave_builder_post_process,
	},
	.private_size = sizeof(struct builder_state),
};
