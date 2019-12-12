/*
  Copyright(c) 2014-2015 Intel Corporation
  Copyright(c) 2010-2011 Texas Instruments Incorporated,
  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution
  in the file called LICENSE.GPL.
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <getopt.h>
#include <assert.h>

#include <alsa/asoundlib.h>
#include <alsa/topology.h>
#include "gettext.h"

static snd_output_t *log;

static void usage(char *name)
{
	printf(
_("Usage: %s [OPTIONS]...\n"
"\n"
"-h, --help              help\n"
"-c, --compile=FILE      compile file\n"
"-n, --normalize=FILE    normalize file\n"
"-v, --verbose=LEVEL     set verbosity level (0...1)\n"
"-o, --output=FILE       set output file\n"
"-s, --sort              sort the identifiers in the normalized output\n"
), name);
}

static int _compar(const void *a, const void *b)
{
	const snd_config_t *c1 = *(snd_config_t **)a;
	const snd_config_t *c2 = *(snd_config_t **)b;
	const char *id1, *id2;
	if (snd_config_get_id(c1, &id1)) return 0;
	if (snd_config_get_id(c2, &id2)) return 0;
	return strcmp(id1, id2);
}

static snd_config_t *normalize_config(const char *id, snd_config_t *src, int sort)
{
	snd_config_t *dst, **a;
	snd_config_iterator_t i, next;
	int index, count;

	if (snd_config_get_type(src) != SND_CONFIG_TYPE_COMPOUND) {
		if (snd_config_copy(&dst, src) >= 0)
			return dst;
		return NULL;
	}
	count = 0;
	snd_config_for_each(i, next, src)
		count++;
	a = malloc(sizeof(dst) * count);
	if (a == NULL)
		return NULL;
	index = 0;
	snd_config_for_each(i, next, src) {
		snd_config_t *s = snd_config_iterator_entry(i);
		a[index++] = s;
	}
	if (sort)
		qsort(a, count, sizeof(a[0]), _compar);
	if (snd_config_make_compound(&dst, id, count == 1)) {
		free(a);
		return NULL;
	}
	for (index = 0; index < count; index++) {
		snd_config_t *s = a[index];
		const char *id2;
		if (snd_config_get_id(s, &id2)) {
			snd_config_delete(dst);
			free(a);
			return NULL;
		}
		s = normalize_config(id2, s, sort);
		if (s == NULL || snd_config_add(dst, s)) {
			snd_config_delete(dst);
			free(a);
			return NULL;
		}
	}
	free(a);
	return dst;
}

static int compile(const char *source_file, const char *output_file, int verbose)
{
	snd_tplg_t *snd_tplg;
	int err;

	snd_tplg = snd_tplg_new();
	if (snd_tplg == NULL) {
		fprintf(stderr, _("failed to create new topology context\n"));
		return 1;
	}

	snd_tplg_verbose(snd_tplg, verbose);

	err = snd_tplg_build_file(snd_tplg, source_file, output_file);
	if (err < 0) {
		fprintf(stderr, _("failed to compile context %s\n"), source_file);
		snd_tplg_free(snd_tplg);
		unlink(output_file);
		return 1;
	}

	snd_tplg_free(snd_tplg);
	return 1;
}

static int normalize(const char *source_file, const char *output_file, int sort)
{
	snd_input_t *input;
	snd_output_t *output;
	snd_config_t *top, *norm;
	int err;

	err = snd_input_stdio_open(&input, source_file, "r");
	if (err < 0) {
		fprintf(stderr, "Unable to open source file '%s': %s\n", source_file, snd_strerror(-err));
		return 0;
	}

	err = snd_config_top(&top);
	if (err < 0) {
		snd_input_close(input);
		return 1;
	}

	err = snd_config_load(top, input);
	snd_input_close(input);
	if (err < 0) {
	snd_config_delete(top);
		fprintf(stderr, "Unable to parse source file '%s': %s\n", source_file, snd_strerror(-err));
		snd_config_delete(top);
		return 1;
	}

	err = snd_output_stdio_open(&output, output_file, "w+");
	if (err < 0) {
		fprintf(stderr, "Unable to open output file '%s': %s\n", output_file, snd_strerror(-err));
		snd_config_delete(top);
		return 1;
	}

	norm = normalize_config(NULL, top, sort);
	if (norm == NULL) {
		fprintf(stderr, "Unable to normalize configuration (out of memory?)\n");
		snd_output_close(output);
		snd_config_delete(top);
		return 1;
	}

	err = snd_config_save(norm, output);
	snd_output_close(output);
	snd_config_delete(norm);
	snd_config_delete(top);
	if (err < 0) {
		fprintf(stderr, "Unable to save normalized contents: %s\n", snd_strerror(-err));
		return 1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	static const char short_options[] = "hc:n:v:o:s";
	static const struct option long_options[] = {
		{"help", 0, NULL, 'h'},
		{"verbose", 1, NULL, 'v'},
		{"compile", 1, NULL, 'c'},
		{"normalize", 1, NULL, 'n'},
		{"output", 1, NULL, 'o'},
		{"sort", 0, NULL, 's'},
		{0, 0, 0, 0},
	};
	char *source_file = NULL, *normalize_file = NULL, *output_file = NULL;
	int c, err, verbose = 0, sort = 0, option_index;

#ifdef ENABLE_NLS
	setlocale(LC_ALL, "");
	textdomain(PACKAGE);
#endif

	err = snd_output_stdio_attach(&log, stderr, 0);
	assert(err >= 0);

	while ((c = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
		switch (c) {
		case 'h':
			usage(argv[0]);
			return 0;
		case 'v':
			verbose = atoi(optarg);
			break;
		case 'c':
			source_file = optarg;
			break;
		case 'n':
			normalize_file = optarg;
			break;
		case 'o':
			output_file = optarg;
			break;
		case 's':
			sort = 1;
			break;
		default:
			fprintf(stderr, _("Try `%s --help' for more information.\n"), argv[0]);
			return 1;
		}
	}

	if (source_file && normalize_file) {
		fprintf(stderr, "Cannot normalize and compile at a time!\n");
		return 1;
	}

	if ((source_file == NULL && normalize_file == NULL) || output_file == NULL) {
		usage(argv[0]);
		return 1;
	}

	if (source_file)
		err = compile(source_file, output_file, verbose);
	else
		err = normalize(normalize_file, output_file, sort);

	snd_output_close(log);
	return 0;
}
