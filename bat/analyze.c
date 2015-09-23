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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <math.h>
#include <fftw3.h>

#include "aconfig.h"
#include "gettext.h"

#include "common.h"

static void check_amplitude(struct bat *bat, double *buf)
{
	double sum, average, amplitude;
	int i, percent;

	/* calculate average value */
	for (i = 0, sum = 0.0; i < bat->frames; i++)
		sum += buf[i];
	average = sum / bat->frames;

	/* calculate peak-to-average amplitude */
	for (i = 0, sum = 0.0; i < bat->frames; i++)
		sum += abs(buf[i] - average);
	amplitude = sum / bat->frames * M_PI / 2.0;

	/* calculate amplitude percentage against full range */
	percent = amplitude * 100 / ((1 << ((bat->sample_size << 3) - 1)) - 1);

	fprintf(bat->log, _("Amplitude: %.1f; Percentage: [%d]\n"),
			amplitude, percent);
	if (percent < 0)
		fprintf(bat->err, _("ERROR: Amplitude can't be negative!\n"));
	else if (percent < 1)
		fprintf(bat->err, _("WARNING: Signal too weak!\n"));
	else if (percent > 100)
		fprintf(bat->err, _("WARNING: Signal overflow!\n"));
}

/**
 *
 * @return 0 if peak detected at right frequency,
 *         1 if peak detected somewhere else
 *         2 if DC detected
 */
int check_peak(struct bat *bat, struct analyze *a, int end, int peak, float hz,
		float mean, float p, int channel, int start)
{
	int err;
	float hz_peak = (float) (peak) * hz;
	float delta_rate = DELTA_RATE * bat->target_freq[channel];
	float delta_HZ = DELTA_HZ;
	float tolerance = (delta_rate > delta_HZ) ? delta_rate : delta_HZ;

	fprintf(bat->log, _("Detected peak at %2.2f Hz of %2.2f dB\n"), hz_peak,
			10.0 * log10(a->mag[peak] / mean));
	fprintf(bat->log, _(" Total %3.1f dB from %2.2f to %2.2f Hz\n"),
			10.0 * log10(p / mean), start * hz, end * hz);

	if (hz_peak < DC_THRESHOLD) {
		fprintf(bat->err, _(" WARNING: Found low peak %2.2f Hz,"),
				hz_peak);
		fprintf(bat->err, _(" very close to DC\n"));
		err = FOUND_DC;
	} else if (hz_peak < bat->target_freq[channel] - tolerance) {
		fprintf(bat->err, _(" FAIL: Peak freq too low %2.2f Hz\n"),
				hz_peak);
		err = FOUND_WRONG_PEAK;
	} else if (hz_peak > bat->target_freq[channel] + tolerance) {
		fprintf(bat->err, _(" FAIL: Peak freq too high %2.2f Hz\n"),
				hz_peak);
		err = FOUND_WRONG_PEAK;
	} else {
		fprintf(bat->log, _(" PASS: Peak detected"));
		fprintf(bat->log, _(" at target frequency\n"));
		err = 0;
	}

	return err;
}

/**
 * Search for main frequencies in fft results and compare it to target
 */
static int check(struct bat *bat, struct analyze *a, int channel)
{
	float hz = 1.0 / ((float) bat->frames / (float) bat->rate);
	float mean = 0.0, t, sigma = 0.0, p = 0.0;
	int i, start = -1, end = -1, peak = 0, signals = 0;
	int err = 0, N = bat->frames / 2;

	/* calculate mean */
	for (i = 0; i < N; i++)
		mean += a->mag[i];
	mean /= (float) N;

	/* calculate standard deviation */
	for (i = 0; i < N; i++) {
		t = a->mag[i] - mean;
		t *= t;
		sigma += t;
	}
	sigma /= (float) N;
	sigma = sqrtf(sigma);

	/* clip any data less than k sigma + mean */
	for (i = 0; i < N; i++) {
		if (a->mag[i] > mean + bat->sigma_k * sigma) {

			/* find peak start points */
			if (start == -1) {
				start = peak = end = i;
				signals++;
			} else {
				if (a->mag[i] > a->mag[peak])
					peak = i;
				end = i;
			}
			p += a->mag[i];
		} else if (start != -1) {
			/* Check if peak is as expected */
			err |= check_peak(bat, a, end, peak, hz, mean,
					p, channel, start);
			end = start = -1;
			if (signals == MAX_PEAKS)
				break;
		}
	}
	if (signals == 0)
		err = -ENOPEAK; /* No peak detected */
	else if ((err == FOUND_DC) && (signals == 1))
		err = -EONLYDC; /* Only DC detected */
	else if ((err & FOUND_WRONG_PEAK) == FOUND_WRONG_PEAK)
		err = -EBADPEAK; /* Bad peak detected */
	else
		err = 0; /* Correct peak detected */

	fprintf(bat->log, _("Detected at least %d signal(s) in total\n"),
			signals);

	return err;
}

static void calc_magnitude(struct bat *bat, struct analyze *a, int N)
{
	double r2, i2;
	int i;

	for (i = 1; i < N / 2; i++) {
		r2 = a->out[i] * a->out[i];
		i2 = a->out[N - i] * a->out[N - i];

		a->mag[i] = sqrtf(r2 + i2);
	}
	a->mag[0] = 0.0;
}

static int find_and_check_harmonics(struct bat *bat, struct analyze *a,
		int channel)
{
	fftw_plan p;
	int err = -ENOMEM, N = bat->frames;

	/* Allocate FFT buffers */
	a->in = (double *) fftw_malloc(sizeof(double) * bat->frames);
	if (a->in == NULL)
		goto out1;

	a->out = (double *) fftw_malloc(sizeof(double) * bat->frames);
	if (a->out == NULL)
		goto out2;

	a->mag = (double *) fftw_malloc(sizeof(double) * bat->frames);
	if (a->mag == NULL)
		goto out3;

	/* create FFT plan */
	p = fftw_plan_r2r_1d(N, a->in, a->out, FFTW_R2HC,
			FFTW_MEASURE | FFTW_PRESERVE_INPUT);
	if (p == NULL)
		goto out4;

	/* convert source PCM to doubles */
	bat->convert_sample_to_double(a->buf, a->in, bat->frames);

	/* check amplitude */
	check_amplitude(bat, a->in);

	/* run FFT */
	fftw_execute(p);

	/* FFT out is real and imaginary numbers - calc magnitude for each */
	calc_magnitude(bat, a, N);

	/* check data */
	err = check(bat, a, channel);

	fftw_destroy_plan(p);

out4:
	fftw_free(a->mag);
out3:
	fftw_free(a->out);
out2:
	fftw_free(a->in);
out1:
	return err;
}

/**
 * Convert interleaved samples from channels in samples from a single channel
 */
static int reorder_data(struct bat *bat)
{
	char *p, *new_bat_buf;
	int ch, i, j;

	if (bat->channels == 1)
		return 0; /* No need for reordering */

	p = malloc(bat->frames * bat->frame_size);
	new_bat_buf = p;
	if (p == NULL)
		return -ENOMEM;

	for (ch = 0; ch < bat->channels; ch++) {
		for (j = 0; j < bat->frames; j++) {
			for (i = 0; i < bat->sample_size; i++) {
				*p++ = ((char *) (bat->buf))[j * bat->frame_size
						+ ch * bat->sample_size + i];
			}
		}
	}

	free(bat->buf);
	bat->buf = new_bat_buf;

	return 0;
}

int analyze_capture(struct bat *bat)
{
	int err = 0;
	size_t items;
	int c;
	struct analyze a;

	fprintf(bat->log, _("\nBAT analysis: signal has %d frames at %d Hz,"),
			bat->frames, bat->rate);
	fprintf(bat->log, _(" %d channels, %d bytes per sample.\n"),
			bat->channels, bat->sample_size);

	bat->buf = malloc(bat->frames * bat->frame_size);
	if (bat->buf == NULL)
		return -ENOMEM;

	bat->fp = fopen(bat->capture.file, "rb");
	if (bat->fp == NULL) {
		fprintf(bat->err, _("Cannot open file for capture: %s %d\n"),
				bat->capture.file, -errno);
		err = -errno;
		goto exit1;
	}

	/* Skip header */
	err = read_wav_header(bat, bat->capture.file, bat->fp, true);
	if (err != 0)
		goto exit2;

	items = fread(bat->buf, bat->frame_size, bat->frames, bat->fp);
	if (items != bat->frames) {
		err = -EIO;
		goto exit2;
	}

	err = reorder_data(bat);
	if (err != 0)
		goto exit2;

	for (c = 0; c < bat->channels; c++) {
		fprintf(bat->log, _("\nChannel %i - "), c + 1);
		fprintf(bat->log, _("Checking for target frequency %2.2f Hz\n"),
				bat->target_freq[c]);
		a.buf = bat->buf +
				c * bat->frames * bat->frame_size
				/ bat->channels;
		err = find_and_check_harmonics(bat, &a, c);
	}

exit2:
	fclose(bat->fp);
exit1:
	free(bat->buf);

	return err;
}
