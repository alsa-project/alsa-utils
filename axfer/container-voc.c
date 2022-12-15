// SPDX-License-Identifier: GPL-2.0
//
// container-voc.c - a parser/builder for a container of Creative Voice File.
//
// Copyright (c) 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp>
//
// Licensed under the terms of the GNU General Public License, version 2.

#include "container.h"
#include "misc.h"

// Not portable to all of UNIX platforms.
#include <endian.h>

// References:
//  - http://sox.sourceforge.net/

#define VOC_MAGIC		"Creative Voice File\x1a"
#define VOC_VERSION_1_10	0x010a
#define VOC_VERSION_1_20	0x0114

enum block_type {
	BLOCK_TYPE_TERMINATOR		= 0x00,
	BLOCK_TYPE_V110_DATA		= 0x01,
	BLOCK_TYPE_CONTINUOUS_DATA	= 0x02,
	BLOCK_TYPE_SILENCE		= 0x03,
	BLOCK_TYPE_MARKER		= 0x04,
	BLOCK_TYPE_STRING		= 0x05,
	BLOCK_TYPE_REPEAT_START		= 0x06,
	BLOCK_TYPE_REPEAT_END		= 0x07,
	BLOCK_TYPE_EXTENDED_V110_FORMAT	= 0x08,
	BLOCK_TYPE_V120_DATA		= 0x09,
};

enum code_id {
	// Version 1.10.
	CODE_ID_GENERIC_MBLA_U8			= 0x00,
	CODE_ID_CREATIVE_ADPCM_8BIT_TO_4BIT_LE	= 0x01,
	CODE_ID_CREATIVE_ADPCM_8BIT_TO_3BIT_LE	= 0x02,
	CODE_ID_CREATIVE_ADPCM_8BIT_TO_2BIT_LE	= 0x03,
	// Version 1.20.
	CODE_ID_GENERIC_MBLA_S16_LE		= 0x04,
	CODE_ID_CCIT_A_LAW_LE			= 0x06,
	CODE_ID_CCIT_MU_LAW_LE			= 0x07,
	CODE_ID_CREATIVE_ADPCM_16BIT_TO_4BIT_LE	= 0x2000,
};

struct format_map {
	unsigned int minimal_version;
	enum code_id code_id;
	snd_pcm_format_t format;
};

static const struct format_map format_maps[] = {
	{VOC_VERSION_1_10, CODE_ID_GENERIC_MBLA_U8,	SND_PCM_FORMAT_U8},
	{VOC_VERSION_1_20, CODE_ID_GENERIC_MBLA_S16_LE,	SND_PCM_FORMAT_S16_LE},
	{VOC_VERSION_1_20, CODE_ID_CCIT_A_LAW_LE,	SND_PCM_FORMAT_A_LAW},
	{VOC_VERSION_1_20, CODE_ID_CCIT_MU_LAW_LE,	SND_PCM_FORMAT_MU_LAW},
	// The other formats are not supported by ALSA.
};

struct container_header {
	uint8_t magic[20];
	uint16_t hdr_size;
	uint16_t version;
	uint16_t version_compr;
};

// A format for data blocks except for terminator type.
struct block_header {
	uint8_t type;
	uint8_t size[3];

	uint8_t data[0];
};

// Data block for terminator type has an exceptional format.
struct block_terminator {
	uint8_t type;
};

struct time_const {
	unsigned int frames_per_second;
	uint16_t code;
};

static const struct time_const v110_time_consts[] = {
	{5512, 74},
	{8000, 130},
	{11025, 165},
	{16000, 193},
	{22050, 210},
	{32000, 224},
	{44100, 233},
	{48000, 235},
	{64000, 240},
	// Time constant for the upper sampling rate is not identical.
};

static const struct time_const ex_v110_time_consts[] = {
	{5512, 19092},
	{8000, 33536},
	{11025, 42317},
	{16000, 49536},
	{22050, 53927},
	{32000, 57536},
	{44100, 59732},
	{48000, 60203},
	{64000, 61536},
	{88200, 62634},
	{96000, 62870},
	{176400, 64085},
	{192000, 64203},
	// This support up to 192.0 kHz. The rest is for cases with 2ch.
	{352800, 64811},
	{384000, 64870},
};

// v1.10 format:
// - monaural.
// - frames_per_second = 1,000,000 / (256 - time_const)
struct block_v110_data {
	uint8_t type;
	uint8_t size[3];	// Equals to (2 + the size of frames).

	uint8_t time_const;
	uint8_t code_id;
	uint8_t frames[0];	// Aligned to little-endian.
};

struct block_continuous_data {
	uint8_t type;
	uint8_t size[3];	// Equals to the size of frames.

	uint8_t frames[0];	// Aligned to little-endian.
};

// v1.10 format:
// - monaural.
// - frames_per_second = 1,000,000 / (256 - time_const).
struct block_silence {
	uint8_t type;
	uint8_t size[3];	// Equals to 3.

	uint16_t frame_count;
	uint8_t time_const;
};

struct block_marker {
	uint8_t type;
	uint8_t size[3];	// Equals to 2.

	uint16_t mark;
};

struct block_string {
	uint8_t type;
	uint8_t size[3];	// Equals to the length of string with 0x00.

	uint8_t chars[0];
};

struct block_repeat_start {
	uint8_t type;
	uint8_t size[3];	// Equals to 2.

	uint16_t count;
};

struct block_repeat_end {
	uint8_t type;
	uint8_t size[3];	// Equals to 0.
};

// Extended v1.10 format:
// - manaural/stereo.
// - frames_per_second =
//		256,000,000 / (samples_per_frame * (65536 - time_const)).
// - Appear just before v110_data block.
struct block_extended_v110_format {
	uint8_t type;
	uint8_t size[3];	// Equals to 4.

	uint16_t time_const;
	uint8_t code_id;
	uint8_t ch_mode;	// 0 is monaural, 1 is stereo.
};

// v1.20 format:
// - monaural/stereo.
// - 8/16 bits_per_sample.
// - time_const is not used.
// - code_id is extended.
struct block_v120_format {
	uint8_t type;
	uint8_t size[3];	// Equals to (12 + ).

	uint32_t frames_per_second;
	uint8_t bits_per_sample;
	uint8_t samples_per_frame;
	uint16_t code_id;
	uint8_t reserved[4];

	uint8_t frames[0];	// Aligned to little-endian.
};

// Aligned to little endian order but 24 bits field.
static uint32_t parse_block_data_size(uint8_t fields[3])
{
	return (fields[2] << 16) | (fields[1] << 8) | fields[0];
}

static void build_block_data_size(uint8_t fields[3], unsigned int size)
{
	fields[0] = (size & 0x0000ff);
	fields[1] = (size & 0x00ff00) >> 8;
	fields[2] = (size & 0xff0000) >> 16;
}

static int build_time_constant(unsigned int frames_per_second,
			       unsigned int samples_per_frame, uint16_t *code,
			       bool extended)
{
	int i;

	// 16 bits are available for this purpose.
	if (extended) {
		if (samples_per_frame > 2)
			return -EINVAL;
		frames_per_second *= samples_per_frame;

		for (i = 0; i < ARRAY_SIZE(ex_v110_time_consts); ++i) {
			if (ex_v110_time_consts[i].frames_per_second ==
					frames_per_second)
				break;
		}
		if (i < ARRAY_SIZE(ex_v110_time_consts) &&
		    frames_per_second <= 192000) {
			*code = ex_v110_time_consts[i].code;
		} else {
			*code = 65536 - 256000000 / frames_per_second;
		}
	} else {
		if (samples_per_frame != 1)
			return -EINVAL;

		for (i = 0; i < ARRAY_SIZE(v110_time_consts); ++i) {
			if (v110_time_consts[i].frames_per_second ==
					frames_per_second)
			break;
		}
		// Should be within 8 bit.
		if (i < ARRAY_SIZE(v110_time_consts))
			*code = (uint8_t)v110_time_consts[i].code;
		else
			*code = 256 - 1000000 / frames_per_second;
	}

	return 0;
}

static unsigned int parse_time_constant(uint16_t code,
					unsigned int samples_per_frame,
					unsigned int *frames_per_second,
					bool extended)
{
	int i;

	if (extended) {
		if (samples_per_frame > 2)
			return -EINVAL;

		for (i = 0; i < ARRAY_SIZE(ex_v110_time_consts); ++i) {
			if (ex_v110_time_consts[i].code == code ||
			    ex_v110_time_consts[i].code - 1 == code)
				break;
		}
		if (i < ARRAY_SIZE(ex_v110_time_consts)) {
			*frames_per_second =
				ex_v110_time_consts[i].frames_per_second /
				samples_per_frame;
		} else {
			*frames_per_second = 256000000 / samples_per_frame /
					     (65536 - code);
		}
	} else {
		if (samples_per_frame != 1)
			return -EINVAL;

		for (i = 0; i < ARRAY_SIZE(v110_time_consts); ++i) {
			if (v110_time_consts[i].code == code ||
			    v110_time_consts[i].code - 1 == code)
				break;
		}
		if (i < ARRAY_SIZE(v110_time_consts)) {
			*frames_per_second =
					v110_time_consts[i].frames_per_second;
		} else {
			*frames_per_second = 1000000 / (256 - code);
		}
	}

	return 0;
}

struct parser_state {
	unsigned int version;
	bool extended;

	unsigned int frames_per_second;
	unsigned int samples_per_frame;
	unsigned int bytes_per_sample;
	enum code_id code_id;
	uint32_t byte_count;
};

static int parse_container_header(struct parser_state *state,
				  struct container_header *header)
{
	uint16_t hdr_size;
	uint16_t version;
	uint16_t version_compr;

	hdr_size = le16toh(header->hdr_size);
	version = le16toh(header->version);
	version_compr = le16toh(header->version_compr);

	if (memcmp(header->magic, VOC_MAGIC, sizeof(header->magic)))
		return -EIO;

	if (hdr_size != sizeof(*header))
		return -EIO;

	if (version_compr != 0x1234 + ~version)
		return -EIO;

	if (version != VOC_VERSION_1_10 && version != VOC_VERSION_1_20)
		return -EIO;

	state->version = version;

	return 0;
}

static bool check_code_id(uint8_t code_id, unsigned int version)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(format_maps); ++i) {
		if (code_id != format_maps[i].code_id)
			continue;
		if (version >= format_maps[i].minimal_version)
			return true;
	}

	return false;
}

static int parse_v120_format_block(struct parser_state *state,
				   struct block_v120_format *block)
{
	state->frames_per_second = le32toh(block->frames_per_second);
	state->bytes_per_sample = block->bits_per_sample / 8;
	state->samples_per_frame = block->samples_per_frame;
	state->code_id = le16toh(block->code_id);
	state->byte_count = parse_block_data_size(block->size) - 12;

	if (!check_code_id(state->code_id, VOC_VERSION_1_20))
		return -EIO;

	return 0;
}

static int parse_extended_v110_format(struct parser_state *state,
				      struct block_extended_v110_format *block)
{
	unsigned int time_const;
	unsigned int frames_per_second;
	int err;

	state->code_id = block->code_id;
	if (!check_code_id(state->code_id, VOC_VERSION_1_10))
		return -EIO;

	if (block->ch_mode == 0)
		state->samples_per_frame = 1;
	else if (block->ch_mode == 1)
		state->samples_per_frame = 2;
	else
		return -EIO;

	time_const = le16toh(block->time_const);
	err = parse_time_constant(time_const, state->samples_per_frame,
				  &frames_per_second, true);
	if (err < 0)
		return err;
	state->frames_per_second = frames_per_second;

	state->extended = true;

	return 0;
}

static int parse_v110_data(struct parser_state *state,
			   struct block_v110_data *block)
{
	unsigned int time_const;
	unsigned int frames_per_second;
	int err;

	if (!state->extended) {
		state->code_id = block->code_id;
		if (!check_code_id(state->code_id, VOC_VERSION_1_10))
			return -EIO;

		time_const = block->time_const;
		err = parse_time_constant(time_const, 1, &frames_per_second,
					  false);
		if (err < 0)
			return err;
		state->frames_per_second = frames_per_second;
		state->samples_per_frame = 1;
	}

	state->bytes_per_sample = 1;
	state->byte_count = parse_block_data_size(block->size) - 2;

	return 0;
}

static int detect_container_version(struct container_context *cntr)
{
	struct parser_state *state = cntr->private_data;
	struct container_header header = {0};
	int err;

	// 4 bytes were alread read to detect container type.
	memcpy(&header.magic, cntr->magic, sizeof(cntr->magic));
	err = container_recursive_read(cntr,
				       (char *)&header + sizeof(cntr->magic),
				       sizeof(header) - sizeof(cntr->magic));
	if (err < 0)
		return err;
	if (cntr->eof)
		return 0;

	return parse_container_header(state, &header);
}

static int allocate_for_block_cache(struct container_context *cntr,
				    struct block_header *header, void **buf)
{
	uint32_t block_size;
	char *cache;
	int err;

	if (header->type == BLOCK_TYPE_V110_DATA)
		block_size = sizeof(struct block_v110_data);
	else if (header->type == BLOCK_TYPE_CONTINUOUS_DATA)
		block_size = sizeof(struct block_continuous_data);
	else if (header->type == BLOCK_TYPE_EXTENDED_V110_FORMAT)
		block_size = sizeof(struct block_extended_v110_format);
	else if (header->type == BLOCK_TYPE_V120_DATA)
		block_size = sizeof(struct block_v120_format);
	else
		block_size = parse_block_data_size(header->size);

	cache = malloc(block_size);
	if (cache == NULL)
		return -ENOMEM;
	memset(cache, 0, block_size);

	memcpy(cache, header, sizeof(*header));
	err = container_recursive_read(cntr, cache + sizeof(*header),
				       block_size - sizeof(*header));
	if (err < 0) {
		free(cache);
		return err;
	}
	if (cntr->eof) {
		free(cache);
		return 0;
	}

	*buf = cache;

	return 0;
}

static int cache_data_block(struct container_context *cntr,
			    struct block_header *header, void **buf)
{
	int err;

	// Check type of this block.
	err = container_recursive_read(cntr, &header->type,
				       sizeof(header->type));
	if (err < 0)
		return err;
	if (cntr->eof)
		return 0;

	if (header->type > BLOCK_TYPE_V120_DATA)
		return -EIO;
	if (header->type == BLOCK_TYPE_TERMINATOR)
		return 0;

	// Check size of this block. If the block includes a batch of data,
	err = container_recursive_read(cntr, &header->size,
				       sizeof(header->size));
	if (err < 0)
		return err;
	if (cntr->eof)
		return 0;

	return allocate_for_block_cache(cntr, header, buf);
}

static int detect_format_block(struct container_context *cntr)
{
	struct parser_state *state = cntr->private_data;
	struct block_header header;
	void *buf;
	int err;

again:
	buf = NULL;
	err = cache_data_block(cntr, &header, &buf);
	if (err < 0)
		return err;
	if (buf) {
		if (header.type == BLOCK_TYPE_EXTENDED_V110_FORMAT) {
			err = parse_extended_v110_format(state, buf);
		} else if (header.type == BLOCK_TYPE_V120_DATA) {
			err = parse_v120_format_block(state, buf);
		} else if (header.type == BLOCK_TYPE_V110_DATA) {
			err = parse_v110_data(state, buf);
		} else {
			free(buf);
			goto again;
		}

		free(buf);

		if (err < 0)
			return err;
	}

	// Expect to detect block_v110_data.
	if (header.type == BLOCK_TYPE_EXTENDED_V110_FORMAT)
		goto again;

	return 0;
}

static int voc_parser_pre_process(struct container_context *cntr,
				  snd_pcm_format_t *format,
				  unsigned int *samples_per_frame,
				  unsigned int *frames_per_second,
				  uint64_t *byte_count)
{
	struct parser_state *state = cntr->private_data;
	int i;
	int err;

	err = detect_container_version(cntr);
	if (err < 0)
		return err;

	err = detect_format_block(cntr);
	if (err < 0)
		return err;

	for (i = 0; i < ARRAY_SIZE(format_maps); ++i) {
		if (format_maps[i].code_id == state->code_id)
			break;
	}
	if (i == ARRAY_SIZE(format_maps))
		return -EINVAL;

	*format = format_maps[i].format;
	*samples_per_frame = state->samples_per_frame;
	*frames_per_second = state->frames_per_second;

	// This program handles PCM frames in this data block only.
	*byte_count = state->byte_count;

	return 0;
}

struct builder_state {
	unsigned int version;
	bool extended;
	enum code_id code_id;

	unsigned int samples_per_frame;
	unsigned int bytes_per_sample;
};

static int write_container_header(struct container_context *cntr,
				  struct container_header *header)
{
	struct builder_state *state = cntr->private_data;

	// Process container header.
	memcpy(header->magic, VOC_MAGIC, sizeof(header->magic));
	header->hdr_size = htole16(sizeof(*header));
	header->version = htole16(state->version);
	header->version_compr = htole16(0x1234 + ~state->version);

	return container_recursive_write(cntr, header, sizeof(*header));
}

static int write_v120_format_block(struct container_context *cntr,
				   struct block_v120_format *block,
				   unsigned int frames_per_second,
				   uint64_t byte_count)
{
	struct builder_state *state = cntr->private_data;

	block->type = BLOCK_TYPE_V120_DATA;
	build_block_data_size(block->size, 12 + byte_count);

	block->frames_per_second = htole32(frames_per_second);
	block->bits_per_sample = state->bytes_per_sample * 8;
	block->samples_per_frame = state->samples_per_frame;
	block->code_id = htole16(state->code_id);

	return container_recursive_write(cntr, block, sizeof(*block));
}

static int write_extended_v110_format_block(struct container_context *cntr,
				unsigned int frames_per_second,
				struct block_extended_v110_format *block)
{
	struct builder_state *state = cntr->private_data;
	uint16_t time_const;
	int err;

	block->type = BLOCK_TYPE_EXTENDED_V110_FORMAT;
	build_block_data_size(block->size, 4);

	// 16 bits are available for this purpose.
	err = build_time_constant(frames_per_second, state->samples_per_frame,
				  &time_const, true);
	if (err < 0)
		return err;
	block->time_const = htole16(time_const);
	block->code_id = htole16(state->code_id);

	if (state->samples_per_frame == 1)
		block->ch_mode = 0;
	else
		block->ch_mode = 1;

	return container_recursive_write(cntr, block, sizeof(*block));
}

static int write_v110_format_block(struct container_context *cntr,
				   struct block_v110_data *block,
				   unsigned int frames_per_second,
				   uint64_t byte_count)
{
	struct builder_state *state = cntr->private_data;
	uint16_t time_const;
	int err;

	block->type = BLOCK_TYPE_V110_DATA;
	build_block_data_size(block->size, 2 + byte_count);

	// These fields were obsoleted by extension.
	err = build_time_constant(frames_per_second, 1, &time_const, false);
	if (err < 0)
		return err;
	block->time_const = (uint8_t)time_const;
	block->code_id = state->code_id;
	return container_recursive_write(cntr, block, sizeof(*block));
}

static int write_data_blocks(struct container_context *cntr,
			     unsigned int frames_per_second,
			     uint64_t byte_count)
{
	union {
		struct container_header header;
		struct block_v110_data v110_data;
		struct block_extended_v110_format extended_v110_format;
		struct block_v120_format v120_format;
	} buf = {0};
	struct builder_state *state = cntr->private_data;
	int err;

	err = write_container_header(cntr, &buf.header);
	if (err < 0)
		return err;

	if (state->version == VOC_VERSION_1_20) {
		err = write_v120_format_block(cntr, &buf.v120_format,
					      frames_per_second, byte_count);
	} else {
		if (state->extended) {
			err = write_extended_v110_format_block(cntr,
					frames_per_second,
					&buf.extended_v110_format);
			if (err < 0)
				return err;
		}
		err = write_v110_format_block(cntr, &buf.v110_data,
					      frames_per_second, byte_count);
	}

	return err;
}

static int voc_builder_pre_process(struct container_context *cntr,
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
	state->code_id = format_maps[i].code_id;

	// Decide container version.
	if (*samples_per_frame > 2)
		state->version = VOC_VERSION_1_20;
	else
		state->version = format_maps[i].minimal_version;
	if (state->version == VOC_VERSION_1_10) {
		if (*samples_per_frame == 2) {
			for (i = 0;
			     i < ARRAY_SIZE(ex_v110_time_consts); ++i) {
				if (ex_v110_time_consts[i].frames_per_second ==
						*frames_per_second)
					break;
			}
			if (i == ARRAY_SIZE(ex_v110_time_consts))
				state->version = VOC_VERSION_1_20;
			else
				state->extended = true;
		} else {
			for (i = 0; i < ARRAY_SIZE(v110_time_consts); ++i) {
				if (v110_time_consts[i].frames_per_second ==
							*frames_per_second)
					break;
			}
			if (i == ARRAY_SIZE(v110_time_consts))
				state->version = VOC_VERSION_1_20;
		}
	}

	state->bytes_per_sample = snd_pcm_format_physical_width(*format) / 8;
	state->samples_per_frame = *samples_per_frame;

	return write_data_blocks(cntr, *frames_per_second, *byte_count);
}

static int write_block_terminator(struct container_context *cntr)
{
	struct block_terminator block = {0};

	block.type = BLOCK_TYPE_TERMINATOR;
	return container_recursive_write(cntr, &block, sizeof(block));
}

static int write_data_size(struct container_context *cntr, uint64_t byte_count)
{
	struct builder_state *state = cntr->private_data;
	off_t offset;
	uint8_t size_field[3];
	int err;

	offset = sizeof(struct container_header) + sizeof(uint8_t);
	if (state->version == VOC_VERSION_1_10 && state->extended)
		offset += sizeof(struct block_extended_v110_format);
	err = container_seek_offset(cntr, offset);
	if (err < 0)
		return err;

	if (state->version == VOC_VERSION_1_10)
		offset = 2;
	else
		offset = 12;

	if (byte_count > cntr->max_size - offset)
		byte_count = cntr->max_size;
	else
		byte_count += offset;
	build_block_data_size(size_field, byte_count);

	return container_recursive_write(cntr, &size_field, sizeof(size_field));
}

static int voc_builder_post_process(struct container_context *cntr,
				    uint64_t handled_byte_count)
{
	int err;

	err = write_block_terminator(cntr);
	if (err < 0)
		return err;

	return write_data_size(cntr, handled_byte_count);
}

const struct container_parser container_parser_voc = {
	.format = CONTAINER_FORMAT_VOC,
	.magic = VOC_MAGIC,
	.max_size = 0xffffff -	// = UINT24_MAX.
		    sizeof(struct block_terminator),
	.ops = {
		.pre_process	= voc_parser_pre_process,
	},
	.private_size = sizeof(struct parser_state),
};

const struct container_builder container_builder_voc = {
	.format = CONTAINER_FORMAT_VOC,
	.max_size = 0xffffff -	// = UINT24_MAX.
		    sizeof(struct block_terminator),
	.ops = {
		.pre_process	= voc_builder_pre_process,
		.post_process	= voc_builder_post_process,
	},
	.private_size = sizeof(struct builder_state),
};
