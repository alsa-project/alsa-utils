/*
  Copyright(c) 2019 Red Hat Inc.
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
"-u, --dump=FILE         dump (reparse) file\n"
"-v, --verbose=LEVEL     set verbosity level (0...1)\n"
"-o, --output=FILE       set output file\n"
"-s, --sort              sort the identifiers in the normalized output\n"
"-g, --group             save configuration by group indexes\n"
"-x, --nocheck           save configuration without additional integrity checks\n"
), name);
}

static int load(snd_tplg_t **tplg, const char *source_file, int cflags)
{
	int fd, err;
	char *buf, *buf2;
	size_t size, pos;
	ssize_t r;

	if (strcmp(source_file, "-") == 0) {
		fd = fileno(stdin);
	} else {
		fd = open(source_file, O_RDONLY);
		if (fd < 0) {
			fprintf(stderr, _("Unable to open input file '%s': %s\n"),
				source_file, strerror(-errno));
			return 1;
		}
	}

	size = 16*1024;
	pos = 0;
	buf = malloc(size);
	if (buf == NULL)
		goto _nomem;
	while (1) {
		r = read(fd, buf + pos, size - pos);
		if (r < 0 && (errno == EAGAIN || errno == EINTR))
			continue;
		if (r <= 0)
			break;
		pos += r;
		size += 8*1024;
		buf2 = realloc(buf, size);
		if (buf2 == NULL) {
			free(buf);
			goto _nomem;
		}
		buf = buf2;
	}
	if (fd != fileno(stdin))
		close(fd);
	if (r < 0) {
		fprintf(stderr, _("Read error: %s\n"), strerror(-errno));
		free(buf);
		goto _err;
	}

	*tplg = snd_tplg_create(cflags);
	if (*tplg == NULL) {
		fprintf(stderr, _("failed to create new topology context\n"));
		free(buf);
		return 1;
	}

	err = snd_tplg_load(*tplg, buf, pos);
	free(buf);
	if (err < 0) {
		fprintf(stderr, _("Unable to load configuration: %s\n"),
			snd_strerror(-err));
		snd_tplg_free(*tplg);
		return 1;
	}

	return 0;

_nomem:
	fprintf(stderr, _("No enough memory\n"));
_err:
	if (fd != fileno(stdin))
		close(fd);
	free(buf);
	return 1;
}

static int save(const char *output_file, void *buf, size_t size)
{
	char *fname = NULL;
	int fd;
	ssize_t r;

	if (strcmp(output_file, "-") == 0) {
		fd = fileno(stdout);
	} else {
		fname = alloca(strlen(output_file) + 5);
		strcpy(fname, output_file);
		strcat(fname, ".new");
		fd = open(fname, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
		if (fd < 0) {
			fprintf(stderr, _("Unable to open output file '%s': %s\n"),
				fname, strerror(-errno));
			return 1;
		}
	}

	r = 0;
	while (size > 0) {
		r = write(fd, buf, size);
		if (r < 0 && (errno == EAGAIN || errno == EINTR))
			continue;
		if (r < 0)
			break;
		size -= r;
		buf += r;
	}

	if (r < 0) {
		fprintf(stderr, _("Write error: %s\n"), strerror(-errno));
		if (fd != fileno(stdout)) {
			remove(fname);
			close(fd);
		}
		return 1;
	}

	if (fd != fileno(stdout))
		close(fd);

	if (fname && rename(fname, output_file)) {
		fprintf(stderr, _("Unable to rename file '%s' to '%s': %s\n"),
			fname, output_file, strerror(-errno));
		return 1;
	}

	return 0;
}

static int dump(const char *source_file, const char *output_file, int cflags, int sflags)
{
	snd_tplg_t *tplg;
	char *text;
	int err;

	err = load(&tplg, source_file, cflags);
	if (err)
		return err;
	err = snd_tplg_save(tplg, &text, sflags);
	snd_tplg_free(tplg);
	if (err < 0) {
		fprintf(stderr, _("Unable to save parsed configuration: %s\n"),
			snd_strerror(-err));
		return 1;
	}
	err = save(output_file, text, strlen(text));
	free(text);
	return err;
}

static int compile(const char *source_file, const char *output_file, int cflags)
{
	snd_tplg_t *tplg;
	void *bin;
	size_t size;
	int err;

	err = load(&tplg, source_file, cflags);
	if (err)
		return err;
	err = snd_tplg_build_bin(tplg, &bin, &size);
	snd_tplg_free(tplg);
	if (err < 0 || size == 0) {
		fprintf(stderr, _("failed to compile context %s\n"), source_file);
		return 1;
	}
	err = save(output_file, bin, size);
	free(bin);
	return err;
}

#define OP_COMPILE	1
#define OP_NORMALIZE	2
#define OP_DUMP		3

int main(int argc, char *argv[])
{
	static const char short_options[] = "hc:n:u:v:o:sgxz";
	static const struct option long_options[] = {
		{"help", 0, NULL, 'h'},
		{"verbose", 1, NULL, 'v'},
		{"compile", 1, NULL, 'c'},
		{"normalize", 1, NULL, 'n'},
		{"dump", 1, NULL, 'u'},
		{"output", 1, NULL, 'o'},
		{"sort", 0, NULL, 's'},
		{"group", 0, NULL, 'g'},
		{"nocheck", 0, NULL, 'x'},
		{"dapm-nosort", 0, NULL, 'z'},
		{0, 0, 0, 0},
	};
	char *source_file = NULL;
	char *output_file = NULL;
	int c, err, op = 'c', cflags = 0, sflags = 0, option_index;

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
			cflags |= SND_TPLG_CREATE_VERBOSE;
			break;
		case 'z':
			cflags |= SND_TPLG_CREATE_DAPM_NOSORT;
			break;
		case 'c':
		case 'n':
		case 'u':
			if (source_file) {
				fprintf(stderr, _("Cannot combine operations (compile, normalize, dump)\n"));
				return 1;
			}
			source_file = optarg;
			op = c;
			break;
		case 'o':
			output_file = optarg;
			break;
		case 's':
			sflags |= SND_TPLG_SAVE_SORT;
			break;
		case 'g':
			sflags |= SND_TPLG_SAVE_GROUPS;
			break;
		case 'x':
			sflags |= SND_TPLG_SAVE_NOCHECK;
			break;
		default:
			fprintf(stderr, _("Try `%s --help' for more information.\n"), argv[0]);
			return 1;
		}
	}

	if (source_file == NULL || output_file == NULL) {
		usage(argv[0]);
		return 1;
	}

	if (op == 'n') {
		if (sflags != 0 && sflags != SND_TPLG_SAVE_SORT) {
			fprintf(stderr, _("Wrong parameters for the normalize operation!\n"));
			return 1;
		}
		/* normalize has predefined output */
		sflags = SND_TPLG_SAVE_SORT;
	}

	switch (op) {
	case 'c':
		err = compile(source_file, output_file, cflags);
		break;
	default:
		err = dump(source_file, output_file, cflags, sflags);
		break;
	}

	snd_output_close(log);
	return err ? 1 : 0;
}
