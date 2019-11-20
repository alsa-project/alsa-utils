// SPDX-License-Identifier: GPL-2.0
//
// xfer-libffado.c - receive/transmit frames by libffado.
//
// Copyright (c) 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp>
//
// Licensed under the terms of the GNU General Public License, version 2.

#include "xfer.h"
#include "misc.h"

#include "frame-cache.h"

#include <stdio.h>
#include <sched.h>

#include <libffado/ffado.h>

struct libffado_state {
	ffado_device_t *handle;
	enum ffado_direction direction;

	char *port_literal;
	char *node_literal;
	char *guid_literal;
	unsigned int frames_per_period;
	unsigned int periods_per_buffer;
	unsigned int sched_priority;
	bool slave_mode:1;
	bool snoop_mode:1;

	unsigned int data_ch_count;
	ffado_streaming_stream_type *data_ch_map;

	int (*process_frames)(struct xfer_context *xfer,
			      unsigned int *frame_count,
			      struct mapper_context *mapper,
			      struct container_context *cntrs);

	struct frame_cache cache;
};

enum no_short_opts {
	OPT_FRAMES_PER_PERIOD = 200,
	OPT_PERIODS_PER_BUFFER,
	OPT_SLAVE_MODE,
	OPT_SNOOP_MODE,
	OPT_SCHED_PRIORITY,
};

#define S_OPTS	"p:n:g:"
static const struct option l_opts[] = {
	{"port",		1, 0, 'p'},
	{"node",		1, 0, 'n'},
	{"guid",		1, 0, 'g'},
	{"frames-per-period",	1, 0, OPT_FRAMES_PER_PERIOD},
	{"periods-per-buffer",	1, 0, OPT_PERIODS_PER_BUFFER},
	{"slave",		0, 0, OPT_SLAVE_MODE},
	{"snoop",		0, 0, OPT_SNOOP_MODE},
	{"sched-priority",	1, 0, OPT_SCHED_PRIORITY}, // to SCHED_FIFO
};

static int xfer_libffado_init(struct xfer_context *xfer,
			       snd_pcm_stream_t direction)
{
	struct libffado_state *state = xfer->private_data;

	if (direction == SND_PCM_STREAM_CAPTURE)
		state->direction = FFADO_CAPTURE;
	else if (direction == SND_PCM_STREAM_PLAYBACK)
		state->direction = FFADO_PLAYBACK;
	else
		return -EINVAL;

	return 0;
}

static int xfer_libffado_parse_opt(struct xfer_context *xfer, int key,
				    const char *optarg)
{
	struct libffado_state *state = xfer->private_data;
	int err;

	if (key == 'p')
		state->port_literal = arg_duplicate_string(optarg, &err);
	else if (key == 'n')
		state->node_literal = arg_duplicate_string(optarg, &err);
	else if (key == 'g')
		state->guid_literal = arg_duplicate_string(optarg, &err);
	else if (key == OPT_FRAMES_PER_PERIOD)
		state->frames_per_period = arg_parse_decimal_num(optarg, &err);
	else if (key == OPT_PERIODS_PER_BUFFER)
		state->periods_per_buffer = arg_parse_decimal_num(optarg, &err);
	else if (key == OPT_SLAVE_MODE)
		state->slave_mode = true;
	else if (key == OPT_SNOOP_MODE)
		state->snoop_mode = true;
	else if (key == OPT_SCHED_PRIORITY)
		state->sched_priority = arg_parse_decimal_num(optarg, &err);
	else
		err = -ENXIO;

	return err;
}

static int validate_sched_priority(struct libffado_state *state)
{
	int val;

	val = sched_get_priority_max(SCHED_FIFO);
	if (val < 0)
		return -errno;
	if (state->sched_priority > val)
		return -EINVAL;

	val = sched_get_priority_min(SCHED_FIFO);
	if (val < 0)
		return -errno;
	if (state->sched_priority < val)
		return -EINVAL;

	return 0;
}

static int xfer_libffado_validate_opts(struct xfer_context *xfer)
{
	struct libffado_state *state = xfer->private_data;
	int err;

	if (state->node_literal != NULL) {
		if (state->port_literal == NULL) {
			fprintf(stderr,
				"'n' option should correspond 'p' option.\n");
			return -EINVAL;
		}
	}

	if (state->port_literal != NULL && state->guid_literal != NULL) {
		fprintf(stderr,
			"Neither 'p' option nor 'g' option is available at the "
			"same time.\n");
		return -EINVAL;
	}

	if (state->guid_literal != NULL) {
		if (strstr(state->guid_literal, "0x") != state->guid_literal) {
			fprintf(stderr,
				"A value of 'g' option should have '0x' as its "
				"prefix for hexadecimal number.\n");
			return -EINVAL;
		}
	}

	if (state->slave_mode && state->snoop_mode) {
		fprintf(stderr, "Neither slave mode nor snoop mode is available"
				"at the same time.\n");
		return -EINVAL;
	}

	if (state->sched_priority > 0) {
		err = validate_sched_priority(state);
		if (err < 0)
			return err;
	}

	if (state->frames_per_period == 0)
		state->frames_per_period = 512;
	if (state->periods_per_buffer == 0)
		state->periods_per_buffer = 2;

	return 0;
}

static int r_process_frames(struct xfer_context *xfer,
			    unsigned int *frame_count,
			    struct mapper_context *mapper,
			    struct container_context *cntrs)
{
	struct libffado_state *state = xfer->private_data;
	unsigned int avail_count;
	unsigned int bytes_per_frame;
	unsigned int consumed_count;
	int err;

	// Trim up to expected frame count.
	avail_count = state->frames_per_period;
	if (*frame_count < avail_count)
		avail_count = *frame_count;

	// Cache required amount of frames.
	if (avail_count > frame_cache_get_count(&state->cache)) {
		int ch;
		int pos;

		// Register buffers.
		pos = 0;
		bytes_per_frame = state->cache.bytes_per_sample *
				  state->cache.samples_per_frame;
		for (ch = 0; ch < state->data_ch_count; ++ch) {
			char *buf;

			if (state->data_ch_map[ch] != ffado_stream_type_audio)
				continue;

			buf = state->cache.buf_ptr;
			buf += ch * bytes_per_frame;
			if (ffado_streaming_set_capture_stream_buffer(state->handle,
								      ch, buf))
				return -EIO;
			++pos;
		}

		assert(pos == xfer->samples_per_frame);

		// Move data to the buffer from intermediate buffer.
		if (!ffado_streaming_transfer_buffers(state->handle))
			return -EIO;

		frame_cache_increase_count(&state->cache,
					   state->frames_per_period);
	}

	// Write out to file descriptors.
	consumed_count = frame_cache_get_count(&state->cache);
	err = mapper_context_process_frames(mapper, state->cache.buf,
					    &consumed_count, cntrs);
	if (err < 0)
		return err;

	frame_cache_reduce(&state->cache, consumed_count);

	*frame_count = consumed_count;

	return 0;
}

static int w_process_frames(struct xfer_context *xfer,
			    unsigned int *frame_count,
			    struct mapper_context *mapper,
			    struct container_context *cntrs)
{
	struct libffado_state *state = xfer->private_data;
	unsigned int avail_count;
	int pos;
	int ch;
	unsigned int bytes_per_frame;
	unsigned int consumed_count;
	int err;

	// Trim up to expected frame_count.
	avail_count = state->frames_per_period;
	if (*frame_count < avail_count)
		avail_count = *frame_count;

	// Cache required amount of frames.
	if (avail_count > frame_cache_get_count(&state->cache)) {
		avail_count -= frame_cache_get_count(&state->cache);

		err = mapper_context_process_frames(mapper, state->cache.buf_ptr,
						    &avail_count, cntrs);
		if (err < 0)
			return err;
		frame_cache_increase_count(&state->cache, avail_count);
		avail_count = state->cache.remained_count;
	}

	// Register buffers.
	pos = 0;
	bytes_per_frame = state->cache.bytes_per_sample *
			  state->cache.samples_per_frame;
	for (ch = 0; ch < state->data_ch_count; ++ch) {
		char *buf;

		if (state->data_ch_map[ch] != ffado_stream_type_audio)
			continue;

		buf = state->cache.buf;
		buf += bytes_per_frame;
		if (ffado_streaming_set_playback_stream_buffer(state->handle,
								ch, buf))
			return -EIO;
		++pos;
	}

	assert(pos == xfer->samples_per_frame);

	// Move data on the buffer for transmission.
	if (!ffado_streaming_transfer_buffers(state->handle))
		return -EIO;
	consumed_count = state->frames_per_period;

	frame_cache_reduce(&state->cache, consumed_count);

	*frame_count = consumed_count;

	return 0;
}

static int open_handle(struct xfer_context *xfer,
		       unsigned int frames_per_second)
{
	struct libffado_state *state = xfer->private_data;
	ffado_options_t options = {0};
	ffado_device_info_t info = {0};

	char str[32] = {0};
	char *strings[1];

	// Set target unit if given.
	if (state->port_literal != NULL) {
		if (state->node_literal != NULL) {
			snprintf(str, sizeof(str), "hw:%s,%s",
				 state->port_literal, state->node_literal);
		} else {
			snprintf(str, sizeof(str), "hw:%s",
				 state->port_literal);
		}
	} else if (state->guid_literal != NULL) {
		snprintf(str, sizeof(str), "guid:%s", state->guid_literal);
	}
	if (str[0] != '\0') {
		info.nb_device_spec_strings = 1;
		strings[0] = str;
		info.device_spec_strings = strings;
	}

	// Set common options.
	options.sample_rate = frames_per_second;
	options.period_size = state->frames_per_period;
	options.nb_buffers = state->periods_per_buffer;
	options.realtime = !!(state->sched_priority > 0);
	options.packetizer_priority = state->sched_priority;
	options.slave_mode = state->slave_mode;
	options.snoop_mode = state->snoop_mode;
	options.verbose = xfer->verbose;

	state->handle = ffado_streaming_init(info, options);
	if (state->handle == NULL)
		return -EINVAL;

	return 0;
}

static int enable_mbla_data_ch(struct libffado_state *state,
			       unsigned int *samples_per_frame)
{
	int (*func_type)(ffado_device_t *handle, int pos);
	int (*func_onoff)(ffado_device_t *handle, int pos, int on);
	int count;
	int ch;

	if (state->direction == FFADO_CAPTURE) {
		func_type = ffado_streaming_get_capture_stream_type;
		func_onoff = ffado_streaming_capture_stream_onoff;
		count = ffado_streaming_get_nb_capture_streams(state->handle);
	} else {
		func_type = ffado_streaming_get_playback_stream_type;
		func_onoff = ffado_streaming_playback_stream_onoff;
		count = ffado_streaming_get_nb_playback_streams(state->handle);
	}
	if (count <= 0)
		return -EIO;

	state->data_ch_map = calloc(count, sizeof(*state->data_ch_map));
	if (state->data_ch_map == NULL)
		return -ENOMEM;
	state->data_ch_count = count;

	// When a data ch is off, data in the ch is truncated. This helps to
	// align PCM frames in interleaved order.
	*samples_per_frame = 0;
	for (ch = 0; ch < count; ++ch) {
		int on;

		state->data_ch_map[ch] = func_type(state->handle, ch);

		on = !!(state->data_ch_map[ch] == ffado_stream_type_audio);
		if (func_onoff(state->handle, ch, on))
			return -EIO;
		if (on)
			++(*samples_per_frame);
	}

	return 0;
}

static int xfer_libffado_pre_process(struct xfer_context *xfer,
				     snd_pcm_format_t *format,
				     unsigned int *samples_per_frame,
				     unsigned int *frames_per_second,
				     snd_pcm_access_t *access,
				     snd_pcm_uframes_t *frames_per_buffer)
{
	struct libffado_state *state = xfer->private_data;
	unsigned int channels;
	int err;

	// Supported format of sample is 24 bit multi bit linear audio in
	// AM824 format or the others.
	if (state->direction == FFADO_CAPTURE) {
		if (*format == SND_PCM_FORMAT_UNKNOWN)
			*format = SND_PCM_FORMAT_S24;
	}
	if (*format != SND_PCM_FORMAT_S24) {
		fprintf(stderr,
			"A libffado backend supports S24 only.\n");
		return -EINVAL;
	}

	// The backend requires the number of frames per second for its
	// initialization.
	if (state->direction == FFADO_CAPTURE) {
		if (*frames_per_second == 0)
			*frames_per_second = 48000;
	}
	if (*frames_per_second < 32000 || *frames_per_second > 192000) {
		fprintf(stderr,
			"A libffado backend supports sampling rate between "
			"32000 and 192000, discretely.\n");
		return -EINVAL;
	}

	err = open_handle(xfer, *frames_per_second);
	if (err < 0)
		return err;

	if (ffado_streaming_set_audio_datatype(state->handle,
					       ffado_audio_datatype_int24))
		return -EINVAL;

	// Decide buffer layout.
	err = enable_mbla_data_ch(state, &channels);
	if (err < 0)
		return err;

	// This backend doesn't support resampling.
	if (state->direction == FFADO_CAPTURE) {
		if (*samples_per_frame == 0)
			*samples_per_frame = channels;
	}
	if (*samples_per_frame != channels) {
		fprintf(stderr,
			"The number of samples per frame should be %u.\n",
			channels);
		return -EINVAL;
	}

	// A buffer has interleaved-aligned PCM frames.
	*access = SND_PCM_ACCESS_RW_INTERLEAVED;
	*frames_per_buffer =
			state->frames_per_period * state->periods_per_buffer;

	// Use cache for double number of frames per period.
	err = frame_cache_init(&state->cache, *access,
			       snd_pcm_format_physical_width(*format) / 8,
			       *samples_per_frame, state->frames_per_period * 2);
	if (err < 0)
		return err;

	if (state->direction == FFADO_CAPTURE)
		state->process_frames = r_process_frames;
	else
		state->process_frames = w_process_frames;

	if (ffado_streaming_prepare(state->handle))
		return -EIO;

	if (ffado_streaming_start(state->handle))
		return -EIO;

	return 0;
}

static int xfer_libffado_process_frames(struct xfer_context *xfer,
					unsigned int *frame_count,
					struct mapper_context *mapper,
					struct container_context *cntrs)
{
	struct libffado_state *state = xfer->private_data;
	ffado_wait_response res;
	int err;

	res = ffado_streaming_wait(state->handle);
	if (res == ffado_wait_shutdown || res == ffado_wait_error) {
		err = -EIO;
	} else if (res == ffado_wait_xrun) {
		// No way to recover in this backend.
		err = -EPIPE;
	} else if (res == ffado_wait_ok) {
		err = state->process_frames(xfer, frame_count, mapper, cntrs);
	} else {
		err = -ENXIO;
	}

	if (err < 0)
		*frame_count = 0;

	return err;
}

static void xfer_libffado_pause(struct xfer_context *xfer, bool enable)
{
	struct libffado_state *state = xfer->private_data;

	// This is an emergency avoidance because this backend doesn't support
	// suspend/aresume operation.
	if (enable) {
		ffado_streaming_stop(state->handle);
		ffado_streaming_finish(state->handle);
		exit(EXIT_FAILURE);
	}
}

static void xfer_libffado_post_process(struct xfer_context *xfer)
{
	struct libffado_state *state = xfer->private_data;

	if (state->handle != NULL) {
		ffado_streaming_stop(state->handle);
		ffado_streaming_finish(state->handle);
	}

	frame_cache_destroy(&state->cache);
	free(state->data_ch_map);
	state->data_ch_map = NULL;
}

static void xfer_libffado_destroy(struct xfer_context *xfer)
{
	struct libffado_state *state = xfer->private_data;

	free(state->port_literal);
	free(state->node_literal);
	free(state->guid_literal);
	state->port_literal = NULL;
	state->node_literal = NULL;
	state->guid_literal = NULL;
}

static void xfer_libffado_help(struct xfer_context *xfer)
{
	printf(
"      -p, --port           decimal ID of port to decide 1394 OHCI controller for communication on IEEE 1394 bus\n"
"      -n, --node           decimal ID of node to decide unit on IEEE 1394 bus for transmission of audio data frame\n"
"      -g, --guid           hexadecimal ID for node on IEEE 1394 bus for transmission of audio data frame\n"
"      --frames-per-period  the number of audio data frame to handle one operation (frame unit)\n"
"      --periods-per-bufer  the number of periods in intermediate buffer between libffado (frame unit)\n"
"      --slave              receive frames from the other Linux system on IEEE 1394 bus running with libffado.\n"
"      --snoop              receive frames on packets of all isochronous channels.\n"
"      --sched-priority     set SCHED_FIFO with given priority. see RLIMIT_RTPRIO in getrlimit(2).\n"
	);
}

const struct xfer_data xfer_libffado = {
	.s_opts = S_OPTS,
	.l_opts = l_opts,
	.l_opts_count = ARRAY_SIZE(l_opts),
	.ops = {
		.init		= xfer_libffado_init,
		.parse_opt	= xfer_libffado_parse_opt,
		.validate_opts	= xfer_libffado_validate_opts,
		.pre_process	= xfer_libffado_pre_process,
		.process_frames	= xfer_libffado_process_frames,
		.pause		= xfer_libffado_pause,
		.post_process	= xfer_libffado_post_process,
		.destroy	= xfer_libffado_destroy,
		.help		= xfer_libffado_help,
	},
	.private_size = sizeof(struct libffado_state),
};
