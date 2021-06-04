// SPDX-License-Identifier: GPL-2.0
//
// subcmd-transfer.c - operations for transfer sub command.
//
// Copyright (c) 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp>
//
// Licensed under the terms of the GNU General Public License, version 2.

#include "xfer.h"
#include "subcmd.h"
#include "misc.h"

#include <signal.h>
#include <inttypes.h>

struct context {
	struct xfer_context xfer;
	struct mapper_context mapper;
	struct container_context *cntrs;
	unsigned int cntr_count;

	int *cntr_fds;

	// NOTE: To handling Unix signal.
	bool interrupted;
	int signal;
};

// NOTE: To handling Unix signal.
static struct context *ctx_ptr;

static void handle_unix_signal_for_finish(int sig)
{
	int i;

	for (i = 0; i < ctx_ptr->cntr_count; ++i)
		ctx_ptr->cntrs[i].interrupted = true;

	ctx_ptr->signal = sig;
	ctx_ptr->interrupted = true;
}

static void handle_unix_signal_for_suspend(int sig)
{
	sigset_t curr, prev;
	struct sigaction sa = {0};

	// 1. suspend substream.
	xfer_context_pause(&ctx_ptr->xfer, true);

	// 2. Prepare for default handler(SIG_DFL) of SIGTSTP to stop this
	// process.
	if (sigaction(SIGTSTP, NULL, &sa) < 0) {
		fprintf(stderr, "sigaction(2)\n");
		exit(EXIT_FAILURE);
	}
	if (sa.sa_handler == SIG_ERR)
		exit(EXIT_FAILURE);
	if (sa.sa_handler == handle_unix_signal_for_suspend)
		sa.sa_handler = SIG_DFL;
	if (sigaction(SIGTSTP, &sa, NULL) < 0) {
		fprintf(stderr, "sigaction(2)\n");
		exit(EXIT_FAILURE);
	}

	// Queue SIGTSTP.
	raise(SIGTSTP);

	// Release the queued signal from being blocked. This causes an
	// additional interrupt for the default handler.
	sigemptyset(&curr);
	sigaddset(&curr, SIGTSTP);
	if (sigprocmask(SIG_UNBLOCK, &curr, &prev) < 0) {
		fprintf(stderr, "sigprocmask(2)\n");
		exit(EXIT_FAILURE);
	}

	// 3. SIGCONT is cought and rescheduled. Recover blocking status of
	// UNIX signals.
	if (sigprocmask(SIG_SETMASK, &prev, NULL) < 0) {
		fprintf(stderr, "sigprocmask(2)\n");
		exit(EXIT_FAILURE);
	}

	// Reconfigure this handler for SIGTSTP, instead of default one.
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = handle_unix_signal_for_suspend;
	if (sigaction(SIGTSTP, &sa, NULL) < 0) {
		fprintf(stderr, "sigaction(2)\n");
		exit(EXIT_FAILURE);
	}

	// 4. Continue the PCM substream.
	xfer_context_pause(&ctx_ptr->xfer, false);
}

static int prepare_signal_handler(struct context *ctx)
{
	struct sigaction sa = {0};

	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = handle_unix_signal_for_finish;

	if (sigaction(SIGINT, &sa, NULL) < 0)
		return -errno;
	if (sigaction(SIGTERM, &sa, NULL) < 0)
		return -errno;

	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = handle_unix_signal_for_suspend;
	if (sigaction(SIGTSTP, &sa, NULL) < 0)
		return -errno;

	ctx_ptr = ctx;

	return 0;
}

static int context_init(struct context *ctx, snd_pcm_stream_t direction,
			int argc, char *const *argv)
{
	const char *xfer_type_literal;
	enum xfer_type xfer_type;
	int i;

	// Decide transfer backend before option parser runs.
	xfer_type_literal = NULL;
	for (i = 0; i < argc; ++i) {
		if (strstr(argv[i], "--xfer-type") != argv[i])
			continue;
		xfer_type_literal = argv[i] + 12;
	}
	if (xfer_type_literal == NULL) {
		xfer_type = XFER_TYPE_LIBASOUND;
	} else {
		xfer_type = xfer_type_from_label(xfer_type_literal);
		if (xfer_type == XFER_TYPE_UNSUPPORTED) {
			fprintf(stderr, "The '%s' xfer type is not supported\n",
				xfer_type_literal);
			return -EINVAL;
		}
	}

	// Initialize transfer.
	return xfer_context_init(&ctx->xfer, xfer_type, direction, argc, argv);
}

static int allocate_containers(struct context *ctx, unsigned int count)
{
	ctx->cntrs = calloc(count, sizeof(*ctx->cntrs));
	if (ctx->cntrs == NULL)
		return -ENOMEM;
	ctx->cntr_count = count;

	ctx->cntr_fds = calloc(count, sizeof(*ctx->cntr_fds));
	if (ctx->cntr_fds == NULL)
		return -ENOMEM;

	return 0;
}

static int capture_pre_process(struct context *ctx, snd_pcm_access_t *access,
			       snd_pcm_uframes_t *frames_per_buffer,
			       uint64_t *total_frame_count)
{
	snd_pcm_format_t sample_format = SND_PCM_FORMAT_UNKNOWN;
	unsigned int samples_per_frame = 0;
	unsigned int frames_per_second = 0;
	unsigned int channels;
	int i;
	int err;

	err = xfer_context_pre_process(&ctx->xfer, &sample_format,
				       &samples_per_frame, &frames_per_second,
				       access, frames_per_buffer);
	if (err < 0)
		return err;

	// Prepare for containers.
	err = allocate_containers(ctx, ctx->xfer.path_count);
	if (err < 0)
		return err;

	if (ctx->cntr_count > 1)
		channels = 1;
	else
		channels = samples_per_frame;

	*total_frame_count = 0;
	for (i = 0; i < ctx->cntr_count; ++i) {
		const char *path = ctx->xfer.paths[i];
		int fd;
		uint64_t frame_count;

		if (!strcmp(path, "-")) {
			fd = fileno(stdout);
		} else {
			fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
			if (fd < 0)
				return -errno;
		}
		ctx->cntr_fds[i] = fd;

		err = container_builder_init(ctx->cntrs + i, ctx->cntr_fds[i],
					     ctx->xfer.cntr_format,
					     ctx->xfer.verbose > 1);
		if (err < 0)
			return err;

		err = container_context_pre_process(ctx->cntrs + i,
						    &sample_format, &channels,
						    &frames_per_second,
						    &frame_count);
		if (err < 0)
			return err;

		if (*total_frame_count == 0)
			*total_frame_count = frame_count;
		if (frame_count < *total_frame_count)
			*total_frame_count = frame_count;
	}

	return 0;
}

static int playback_pre_process(struct context *ctx, snd_pcm_access_t *access,
				snd_pcm_uframes_t *frames_per_buffer,
				uint64_t *total_frame_count)
{
	snd_pcm_format_t sample_format = SND_PCM_FORMAT_UNKNOWN;
	unsigned int samples_per_frame = 0;
	unsigned int frames_per_second = 0;
	int i;
	int err;

	// Prepare for containers.
	err = allocate_containers(ctx, ctx->xfer.path_count);
	if (err < 0)
		return err;

	for (i = 0; i < ctx->cntr_count; ++i) {
		const char *path = ctx->xfer.paths[i];
		int fd;
		snd_pcm_format_t format;
		unsigned int channels;
		unsigned int rate;
		uint64_t frame_count;

		if (!strcmp(path, "-")) {
			fd = fileno(stdin);
		} else {
			fd = open(path, O_RDONLY);
			if (fd < 0)
				return -errno;
		}
		ctx->cntr_fds[i] = fd;

		err = container_parser_init(ctx->cntrs + i, ctx->cntr_fds[i],
					    ctx->xfer.verbose > 1);
		if (err < 0)
			return err;

		if (i == 0) {
			// For a raw container.
			format = ctx->xfer.sample_format;
			channels = ctx->xfer.samples_per_frame;
			rate = ctx->xfer.frames_per_second;
		} else {
			format = sample_format;
			channels = samples_per_frame;
			rate = frames_per_second;
		}

		err = container_context_pre_process(ctx->cntrs + i, &format,
						    &channels, &rate,
						    &frame_count);
		if (err < 0)
			return err;

		if (format == SND_PCM_FORMAT_UNKNOWN || channels == 0 ||
		    rate == 0) {
			fprintf(stderr,
				"Sample format, channels and rate should be "
				"indicated for given files.\n");
			return -EINVAL;
		}

		if (i == 0) {
			sample_format = format;
			samples_per_frame = channels;
			frames_per_second = rate;
			*total_frame_count = frame_count;
		} else {
			if (format != sample_format) {
				fprintf(stderr,
					"When using several files, they "
					"should include the same sample "
					"format.\n");
				return -EINVAL;
			}

			// No need to check channels to handle multiple
			// containers.
			if (rate != frames_per_second) {
				fprintf(stderr,
					"When using several files, they "
					"should include samples at the same "
					"sampling rate.\n");
				return -EINVAL;
			}
			if (frame_count < *total_frame_count)
				*total_frame_count = frame_count;
		}
	}

	if (ctx->cntr_count > 1)
		samples_per_frame = ctx->cntr_count;

	// Configure hardware with these parameters.
	return xfer_context_pre_process(&ctx->xfer, &sample_format,
					&samples_per_frame, &frames_per_second,
					access, frames_per_buffer);
}

static int context_pre_process(struct context *ctx, snd_pcm_stream_t direction,
			       uint64_t *total_frame_count)
{
	snd_pcm_access_t access;
	snd_pcm_uframes_t frames_per_buffer = 0;
	unsigned int bytes_per_sample = 0;
	enum mapper_type mapper_type;
	int err;

	if (direction == SND_PCM_STREAM_CAPTURE) {
		mapper_type = MAPPER_TYPE_DEMUXER;
		err = capture_pre_process(ctx, &access, &frames_per_buffer,
					  total_frame_count);
	} else {
		mapper_type = MAPPER_TYPE_MUXER;
		err = playback_pre_process(ctx, &access, &frames_per_buffer,
					   total_frame_count);
	}
	if (err < 0)
		return err;

	// Prepare for mapper.
	err = mapper_context_init(&ctx->mapper, mapper_type, ctx->cntr_count,
				  ctx->xfer.verbose > 1);
	if (err < 0)
		return err;

	bytes_per_sample =
		snd_pcm_format_physical_width(ctx->xfer.sample_format) / 8;
	if (bytes_per_sample <= 0)
		return -ENXIO;
	err = mapper_context_pre_process(&ctx->mapper, access, bytes_per_sample,
					 ctx->xfer.samples_per_frame,
					 frames_per_buffer, ctx->cntrs);
	if (err < 0)
		return err;

	xfer_options_calculate_duration(&ctx->xfer, total_frame_count);

	return 0;
}

static int context_process_frames(struct context *ctx,
				  snd_pcm_stream_t direction,
				  uint64_t expected_frame_count,
				  uint64_t *actual_frame_count)
{
	bool verbose = ctx->xfer.verbose > 2;
	unsigned int frame_count;
	int i;
	int err = 0;

	if (!ctx->xfer.quiet) {
		fprintf(stderr,
			"%s: Format '%s', Rate %u Hz, Channels ",
			snd_pcm_stream_name(direction),
			snd_pcm_format_description(ctx->xfer.sample_format),
			ctx->xfer.frames_per_second);
		if (ctx->xfer.samples_per_frame == 1)
			fprintf(stderr, "'monaural'");
		else if (ctx->xfer.samples_per_frame == 2)
			fprintf(stderr, "'Stereo'");
		else
			fprintf(stderr, "%u", ctx->xfer.samples_per_frame);
		fprintf(stderr, "\n");
	}

	*actual_frame_count = 0;
	while (!ctx->interrupted) {
		struct container_context *cntr;

		// Tell remains to expected frame count.
		frame_count = expected_frame_count - *actual_frame_count;
		err = xfer_context_process_frames(&ctx->xfer, &ctx->mapper,
						  ctx->cntrs, &frame_count);
		if (err < 0) {
			if (err == -EAGAIN || err == -EINTR)
				continue;
			break;
		}
		if (verbose) {
			fprintf(stderr,
				"  handled: %u\n", frame_count);
		}
		for (i = 0; i < ctx->cntr_count; ++i) {
			cntr = &ctx->cntrs[i];
			if (cntr->eof)
				break;
		}
		if (i < ctx->cntr_count)
			break;

		*actual_frame_count += frame_count;
		if (*actual_frame_count >= expected_frame_count)
			break;
	}

	if (!ctx->xfer.quiet) {
		fprintf(stderr,
			"%s: Expected %" PRIu64 "frames, "
			"Actual %" PRIu64 "frames\n",
			snd_pcm_stream_name(direction), expected_frame_count,
			*actual_frame_count);
		if (ctx->interrupted) {
			fprintf(stderr, "Aborted by signal: %s\n",
			       strsignal(ctx->signal));
			return 0;
		}
	}

	return err;
}

static void context_post_process(struct context *ctx,
				 uint64_t accumulated_frame_count)
{
	uint64_t total_frame_count;
	int i;

	xfer_context_post_process(&ctx->xfer);

	if (ctx->cntrs) {
		for (i = 0; i < ctx->cntr_count; ++i) {
			container_context_post_process(ctx->cntrs + i,
						       &total_frame_count);
			container_context_destroy(ctx->cntrs + i);
		}
		free(ctx->cntrs);
	}

	if (ctx->cntr_fds) {
		for (i = 0; i < ctx->cntr_count; ++i)
			close(ctx->cntr_fds[i]);
		free(ctx->cntr_fds);
	}

	mapper_context_post_process(&ctx->mapper);
	mapper_context_destroy(&ctx->mapper);
}

static void context_destroy(struct context *ctx)
{
	xfer_context_destroy(&ctx->xfer);
}

int subcmd_transfer(int argc, char *const *argv, snd_pcm_stream_t direction)
{
	struct context ctx = {0};
	uint64_t expected_frame_count = 0;
	uint64_t actual_frame_count = 0;
	int err = 0;

	err = prepare_signal_handler(&ctx);
	if (err < 0)
		return err;

	err = context_init(&ctx, direction, argc, argv);
	if (err < 0)
		goto end;
	if (ctx.xfer.help || ctx.xfer.dump_hw_params)
		goto end;

	err = context_pre_process(&ctx, direction, &expected_frame_count);
	if (err < 0)
		goto end;

	err = context_process_frames(&ctx, direction, expected_frame_count,
				     &actual_frame_count);
end:
	context_post_process(&ctx, actual_frame_count);

	context_destroy(&ctx);

	return err;
}
