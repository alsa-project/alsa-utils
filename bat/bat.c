/*
 * Copyright (C) 2013-2015 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <getopt.h>
#include <math.h>
#include <limits.h>

#include "aconfig.h"
#include "gettext.h"
#include "version.h"

#include "common.h"

#include "alsa.h"
#include "convert.h"
#include "analyze.h"

static int get_duration(struct bat *bat)
{
	float duration_f;
	long duration_i;
	char *ptrf, *ptri;

	duration_f = strtof(bat->narg, &ptrf);
	if (duration_f == HUGE_VALF || duration_f == -HUGE_VALF) {
		fprintf(bat->err, _("duration float overflow: %f %d\n"),
				duration_f, -errno);
		return -errno;
	} else if (duration_f == 0.0 && errno != 0) {
		fprintf(bat->err, _("duration float underflow: %f %d\n"),
				duration_f, -errno);
		return -errno;
	}

	duration_i = strtol(bat->narg, &ptri, 10);
	if (duration_i == LONG_MAX) {
		fprintf(bat->err, _("duration long overflow: %ld %d\n"),
				duration_i, -errno);
		return -errno;
	} else if (duration_i == LONG_MIN) {
		fprintf(bat->err, _("duration long underflow: %ld %d\n"),
				duration_i, -errno);
		return -errno;
	}

	if (*ptrf == 's') {
		bat->frames = duration_f * bat->rate;
	} else if (*ptri == 0) {
		bat->frames = duration_i;
	} else {
		fprintf(bat->err, _("invalid duration: %s\n"), bat->narg);
		return -EINVAL;
	}

	if (bat->frames <= 0 || bat->frames > MAX_FRAMES) {
		fprintf(bat->err, _("duration out of range: (0, %d(%ds))\n"),
				MAX_FRAMES, (bat->frames / bat->rate));
		return -EINVAL;
	}

	return 0;
}

static void get_sine_frequencies(struct bat *bat, char *freq)
{
	char *tmp1;

	tmp1 = strchr(freq, ',');
	if (tmp1 == NULL) {
		bat->target_freq[1] = bat->target_freq[0] = atof(optarg);
	} else {
		*tmp1 = '\0';
		bat->target_freq[0] = atof(optarg);
		bat->target_freq[1] = atof(tmp1 + 1);
	}
}

static void get_format(struct bat *bat, char *optarg)
{
	if (strcasecmp(optarg, "cd") == 0) {
		bat->format = SND_PCM_FORMAT_S16_LE;
		bat->rate = 44100;
		bat->channels = 2;
	} else if (strcasecmp(optarg, "dat") == 0) {
		bat->format = SND_PCM_FORMAT_S16_LE;
		bat->rate = 48000;
		bat->channels = 2;
	} else {
		bat->format = snd_pcm_format_value(optarg);
		if (bat->format == SND_PCM_FORMAT_UNKNOWN) {
			fprintf(bat->err, _("wrong extended format '%s'\n"),
					optarg);
			exit(EXIT_FAILURE);
		}
	}

	switch (bat->format) {
	case SND_PCM_FORMAT_U8:
		bat->sample_size = 1;
		break;
	case SND_PCM_FORMAT_S16_LE:
		bat->sample_size = 2;
		break;
	case SND_PCM_FORMAT_S24_3LE:
		bat->sample_size = 3;
		break;
	case SND_PCM_FORMAT_S32_LE:
		bat->sample_size = 4;
		break;
	default:
		fprintf(bat->err, _("unsupported format: %d\n"), bat->format);
		exit(EXIT_FAILURE);
	}
}

static inline int thread_wait_completion(struct bat *bat,
		pthread_t id, int **val)
{
	int err;

	err = pthread_join(id, (void **) val);
	if (err)
		pthread_cancel(id);

	return err;
}

/* loopback test where we play sine wave and capture the same sine wave */
static void test_loopback(struct bat *bat)
{
	pthread_t capture_id, playback_id;
	int err;
	int *thread_result_capture, *thread_result_playback;

	/* start playback */
	err = pthread_create(&playback_id, NULL,
			(void *) bat->playback.fct, bat);
	if (err != 0) {
		fprintf(bat->err, _("Cannot create playback thread: %d\n"),
				err);
		exit(EXIT_FAILURE);
	}

	/* TODO: use a pipe to signal stream start etc - i.e. to sync threads */
	/* Let some time for playing something before capturing */
	usleep(CAPTURE_DELAY * 1000);

	/* start capture */
	err = pthread_create(&capture_id, NULL, (void *) bat->capture.fct, bat);
	if (err != 0) {
		fprintf(bat->err, _("Cannot create capture thread: %d\n"), err);
		pthread_cancel(playback_id);
		exit(EXIT_FAILURE);
	}

	/* wait for playback to complete */
	err = thread_wait_completion(bat, playback_id, &thread_result_playback);
	if (err != 0) {
		fprintf(bat->err, _("Cannot join playback thread: %d\n"), err);
		free(thread_result_playback);
		pthread_cancel(capture_id);
		exit(EXIT_FAILURE);
	}

	/* check playback status */
	if (*thread_result_playback != 0) {
		fprintf(bat->err, _("Exit playback thread fail: %d\n"),
				*thread_result_playback);
		pthread_cancel(capture_id);
		exit(EXIT_FAILURE);
	} else {
		fprintf(bat->log, _("Playback completed.\n"));
	}

	/* now stop and wait for capture to finish */
	pthread_cancel(capture_id);
	err = thread_wait_completion(bat, capture_id, &thread_result_capture);
	if (err != 0) {
		fprintf(bat->err, _("Cannot join capture thread: %d\n"), err);
		free(thread_result_capture);
		exit(EXIT_FAILURE);
	}

	/* check capture status */
	if (*thread_result_capture != 0) {
		fprintf(bat->err, _("Exit capture thread fail: %d\n"),
				*thread_result_capture);
		exit(EXIT_FAILURE);
	} else {
		fprintf(bat->log, _("Capture completed.\n"));
	}
}

/* single ended playback only test */
static void test_playback(struct bat *bat)
{
	pthread_t playback_id;
	int err;
	int *thread_result;

	/* start playback */
	err = pthread_create(&playback_id, NULL,
			(void *) bat->playback.fct, bat);
	if (err != 0) {
		fprintf(bat->err, _("Cannot create playback thread: %d\n"),
				err);
		exit(EXIT_FAILURE);
	}

	/* wait for playback to complete */
	err = thread_wait_completion(bat, playback_id, &thread_result);
	if (err != 0) {
		fprintf(bat->err, _("Cannot join playback thread: %d\n"), err);
		free(thread_result);
		exit(EXIT_FAILURE);
	}

	/* check playback status */
	if (*thread_result != 0) {
		fprintf(bat->err, _("Exit playback thread fail: %d\n"),
				*thread_result);
		exit(EXIT_FAILURE);
	} else {
		fprintf(bat->log, _("Playback completed.\n"));
	}
}

/* single ended capture only test */
static void test_capture(struct bat *bat)
{
	pthread_t capture_id;
	int err;
	int *thread_result;

	/* start capture */
	err = pthread_create(&capture_id, NULL, (void *) bat->capture.fct, bat);
	if (err != 0) {
		fprintf(bat->err, _("Cannot create capture thread: %d\n"), err);
		exit(EXIT_FAILURE);
	}

	/* TODO: stop capture */

	/* wait for capture to complete */
	err = thread_wait_completion(bat, capture_id, &thread_result);
	if (err != 0) {
		fprintf(bat->err, _("Cannot join capture thread: %d\n"), err);
		free(thread_result);
		exit(EXIT_FAILURE);
	}

	/* check playback status */
	if (*thread_result != 0) {
		fprintf(bat->err, _("Exit capture thread fail: %d\n"),
				*thread_result);
		exit(EXIT_FAILURE);
	} else {
		fprintf(bat->log, _("Capture completed.\n"));
	}
}

static void usage(struct bat *bat, char *argv[])
{
	fprintf(bat->log,
_("Usage:%s [Option]...\n"
"\n"
"-h, --help             help\n"
"-D                     sound card\n"
"-P                     playback pcm\n"
"-C                     capture pcm\n"
"-f                     sample size\n"
"-c                     number of channels\n"
"-r                     sampling rate\n"
"-n                     frames to capture\n"
"-k                     sigma k\n"
"-F                     target frequency\n"
"-p                     total number of periods to play/capture\n"
"    --log=#            path of log file. if not set, logs be put to stdout,\n"
"                       and errors be put to stderr.\n"
"    --file=#           input file\n"
"    --saveplay=#       save playback content to target file, for debug\n"
"    --local            internal loop, bypass hardware\n"
), argv[0]);
	fprintf(bat->log, _("Recognized sample formats are: %s %s %s %s\n"),
			snd_pcm_format_name(SND_PCM_FORMAT_U8),
			snd_pcm_format_name(SND_PCM_FORMAT_S16_LE),
			snd_pcm_format_name(SND_PCM_FORMAT_S24_3LE),
			snd_pcm_format_name(SND_PCM_FORMAT_S32_LE));
	fprintf(bat->log, _("The available format shotcuts are:\n"));
	fprintf(bat->log, _("-f cd (16 bit little endian, 44100, stereo)\n"));
	fprintf(bat->log, _("-f dat (16 bit little endian, 48000, stereo)\n"));
}

static void set_defaults(struct bat *bat)
{
	memset(bat, 0, sizeof(struct bat));

	/* Set default values */
	bat->rate = 44100;
	bat->channels = 1;
	bat->frame_size = 2;
	bat->sample_size = 2;
	bat->format = SND_PCM_FORMAT_S16_LE;
	bat->convert_float_to_sample = convert_float_to_int16;
	bat->convert_sample_to_double = convert_int16_to_double;
	bat->frames = bat->rate * 2;
	bat->target_freq[0] = 997.0;
	bat->target_freq[1] = 997.0;
	bat->sigma_k = 3.0;
	bat->playback.device = NULL;
	bat->capture.device = NULL;
	bat->buf = NULL;
	bat->local = false;
	bat->playback.fct = &playback_alsa;
	bat->capture.fct = &record_alsa;
	bat->playback.mode = MODE_LOOPBACK;
	bat->capture.mode = MODE_LOOPBACK;
	bat->period_is_limited = false;
	bat->log = stdout;
	bat->err = stderr;
}

static void parse_arguments(struct bat *bat, int argc, char *argv[])
{
	int c, option_index;
	static const char short_options[] = "D:P:C:f:n:F:c:r:s:k:p:lth";
	static const struct option long_options[] = {
		{"help",     0, 0, 'h'},
		{"log",      1, 0, OPT_LOG},
		{"file",     1, 0, OPT_READFILE},
		{"saveplay", 1, 0, OPT_SAVEPLAY},
		{"local",    0, 0, OPT_LOCAL},
		{0, 0, 0, 0}
	};

	while ((c = getopt_long(argc, argv, short_options, long_options,
					&option_index)) != -1) {
		switch (c) {
		case OPT_LOG:
			bat->logarg = optarg;
			break;
		case OPT_READFILE:
			bat->playback.file = optarg;
			break;
		case OPT_SAVEPLAY:
			bat->debugplay = optarg;
			break;
		case OPT_LOCAL:
			bat->local = true;
			break;
		case 'D':
			if (bat->playback.device == NULL)
				bat->playback.device = optarg;
			if (bat->capture.device == NULL)
				bat->capture.device = optarg;
			break;
		case 'P':
			if (bat->capture.mode == MODE_SINGLE)
				bat->capture.mode = MODE_LOOPBACK;
			else
				bat->playback.mode = MODE_SINGLE;
			bat->playback.device = optarg;
			break;
		case 'C':
			if (bat->playback.mode == MODE_SINGLE)
				bat->playback.mode = MODE_LOOPBACK;
			else
				bat->capture.mode = MODE_SINGLE;
			bat->capture.device = optarg;
			break;
		case 'n':
			bat->narg = optarg;
			break;
		case 'F':
			get_sine_frequencies(bat, optarg);
			break;
		case 'c':
			bat->channels = atoi(optarg);
			break;
		case 'r':
			bat->rate = atoi(optarg);
			break;
		case 'f':
			get_format(bat, optarg);
			break;
		case 'k':
			bat->sigma_k = atof(optarg);
			break;
		case 'p':
			bat->periods_total = atoi(optarg);
			bat->period_is_limited = true;
			break;
		case 'h':
		default:
			usage(bat, argv);
			exit(EXIT_SUCCESS);
		}
	}
}

static int validate_options(struct bat *bat)
{
	int c;
	float freq_low, freq_high;

	/* check if we have an input file for local mode */
	if ((bat->local == true) && (bat->capture.file == NULL)) {
		fprintf(bat->err, _("no input file for local testing\n"));
		return -EINVAL;
	}

	/* check supported channels */
	if (bat->channels > MAX_CHANNELS || bat->channels < MIN_CHANNELS) {
		fprintf(bat->err, _("%d channels not supported\n"),
				bat->channels);
		return -EINVAL;
	}

	/* check single ended is in either playback or capture - not both */
	if ((bat->playback.mode == MODE_SINGLE)
			&& (bat->capture.mode == MODE_SINGLE)) {
		fprintf(bat->err, _("single ended mode is simplex\n"));
		return -EINVAL;
	}

	/* check sine wave frequency range */
	freq_low = DC_THRESHOLD;
	freq_high = bat->rate * RATE_FACTOR;
	for (c = 0; c < bat->channels; c++) {
		if (bat->target_freq[c] < freq_low
				|| bat->target_freq[c] > freq_high) {
			fprintf(bat->err, _("sine wave frequency out of"));
			fprintf(bat->err, _(" range: (%.1f, %.1f)\n"),
				freq_low, freq_high);
			return -EINVAL;
		}
	}

	return 0;
}

static int bat_init(struct bat *bat)
{
	int err = 0;

	/* Determine logging to a file or stdout and stderr */
	if (bat->logarg) {
		bat->log = NULL;
		bat->log = fopen(bat->logarg, "wb");
		if (bat->log == NULL) {
			fprintf(bat->err, _("Cannot open file for capture:"));
			fprintf(bat->err, _(" %s %d\n"),
					bat->logarg, -errno);
			return -errno;
		}
		bat->err = bat->log;
	}

	/* Determine duration of playback and/or capture */
	if (bat->narg) {
		err = get_duration(bat);
		if (err < 0)
			return err;
	}

	/* Determine capture file */
	if (bat->local)
		bat->capture.file = bat->playback.file;
	else
		bat->capture.file = TEMP_RECORD_FILE_NAME;

	/* Initial for playback */
	if (bat->playback.file == NULL) {
		/* No input file so we will generate our own sine wave */
		if (bat->frames) {
			if (bat->playback.mode == MODE_SINGLE) {
				/* Play nb of frames given by -n argument */
				bat->sinus_duration = bat->frames;
			} else {
				/* Play CAPTURE_DELAY msec +
				 * 150% of the nb of frames to be analyzed */
				bat->sinus_duration = bat->rate *
						CAPTURE_DELAY / 1000;
				bat->sinus_duration +=
						(bat->frames + bat->frames / 2);
			}
		} else {
			/* Special case where we want to generate a sine wave
			 * endlessly without capturing */
			bat->sinus_duration = 0;
			bat->playback.mode = MODE_SINGLE;
		}
	} else {
		bat->fp = fopen(bat->playback.file, "rb");
		if (bat->fp == NULL) {
			fprintf(bat->err, _("Cannot open file for playback:"));
			fprintf(bat->err, _(" %s %d\n"),
					bat->playback.file, -errno);
			return -errno;
		}
		err = read_wav_header(bat, bat->playback.file, bat->fp, false);
		fclose(bat->fp);
		if (err != 0)
			return err;
	}

	bat->frame_size = bat->sample_size * bat->channels;

	/* Set conversion functions */
	switch (bat->sample_size) {
	case 1:
		bat->convert_float_to_sample = convert_float_to_uint8;
		bat->convert_sample_to_double = convert_uint8_to_double;
		break;
	case 2:
		bat->convert_float_to_sample = convert_float_to_int16;
		bat->convert_sample_to_double = convert_int16_to_double;
		break;
	case 3:
		bat->convert_float_to_sample = convert_float_to_int24;
		bat->convert_sample_to_double = convert_int24_to_double;
		break;
	case 4:
		bat->convert_float_to_sample = convert_float_to_int32;
		bat->convert_sample_to_double = convert_int32_to_double;
		break;
	default:
		fprintf(bat->err, _("Invalid PCM format: size=%d\n"),
				bat->sample_size);
		return -EINVAL;
	}

	return err;
}

int main(int argc, char *argv[])
{
	struct bat bat;
	int err = 0;

	set_defaults(&bat);

#ifdef ENABLE_NLS
	setlocale(LC_ALL, "");
	textdomain(PACKAGE);
#endif

	fprintf(bat.log, _("%s version %s\n\n"), PACKAGE_NAME, PACKAGE_VERSION);

	parse_arguments(&bat, argc, argv);

	err = bat_init(&bat);
	if (err < 0)
		goto out;

	err = validate_options(&bat);
	if (err < 0)
		goto out;

	/* single line playback thread: playback only, no capture */
	if (bat.playback.mode == MODE_SINGLE) {
		test_playback(&bat);
		goto out;
	}

	/* single line capture thread: capture only, no playback */
	if (bat.capture.mode == MODE_SINGLE) {
		test_capture(&bat);
		goto analyze;
	}

	/* loopback thread: playback and capture in a loop */
	if (bat.local == false)
		test_loopback(&bat);

analyze:
	err = analyze_capture(&bat);
out:
	fprintf(bat.log, _("\nReturn value is %d\n"), err);
	if (bat.logarg)
		fclose(bat.log);

	return err;
}
