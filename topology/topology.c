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

#include <stdbool.h>
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
#include "version.h"
#include "topology.h"

bool pre_process_config = false;

static snd_output_t *log;

static void usage(const char *name)
{
	printf(
_("Usage: %s [OPTIONS]...\n"
"\n"
"-h, --help              help\n"
"-c, --compile=FILE      compile configuration file\n"
"-p, --pre-process       pre-process Topology2.0 configuration file before compilation\n"
"-P, --pre-process=FILE  pre-process Topology2.0 configuration file\n"
"-d, --decode=FILE       decode binary topology file\n"
"-n, --normalize=FILE    normalize configuration file\n"
"-u, --dump=FILE         dump (reparse) configuration file\n"
"-v, --verbose=LEVEL     set verbosity level (0...1)\n"
"-o, --output=FILE       set output file\n"
"-s, --sort              sort the identifiers in the normalized output\n"
"-g, --group             save configuration by group indexes\n"
"-x, --nocheck           save configuration without additional integrity checks\n"
"-z, --dapm-nosort       do not sort the DAPM widgets\n"
"-V, --version           print version\n"
), name);
}

static void version(const char *name)
{
	printf(
_("%s version %s\n"
"libasound version %s\n"
"libatopology version %s\n"
), name, SND_UTIL_VERSION_STR,
   snd_asoundlib_version(), snd_tplg_version());
}

static int load(const char *source_file, void **dst, size_t *dst_size)
{
	int fd;
	void *buf, *buf2;
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
		if (buf2 == NULL)
			goto _nomem;
		buf = buf2;
	}
	if (r < 0) {
		fprintf(stderr, _("Read error: %s\n"), strerror(-errno));
		goto _err;
	}

	if (fd != fileno(stdin))
		close(fd);

	*dst = buf;
	*dst_size = pos;
	return 0;

_nomem:
	fprintf(stderr, _("No enough memory\n"));
_err:
	if (fd != fileno(stdin))
		close(fd);
	free(buf);
	return 1;
}

static int load_topology(snd_tplg_t **tplg, char *config,
			 size_t config_size, int cflags)
{
	int err;

	*tplg = snd_tplg_create(cflags);
	if (*tplg == NULL) {
		fprintf(stderr, _("failed to create new topology context\n"));
		return 1;
	}

	err = snd_tplg_load(*tplg, config, config_size);
	if (err < 0) {
		fprintf(stderr, _("Unable to load configuration: %s\n"),
			snd_strerror(-err));
		snd_tplg_free(*tplg);
		return 1;
	}

	return 0;
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
			if (fname && remove(fname))
				fprintf(stderr, _("Unable to remove file %s: %s\n"),
						fname, strerror(-errno));
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
	char *config, *text;
	size_t size;
	int err;

	err = load(source_file, (void **)&config, &size);
	if (err)
		return err;
	err = load_topology(&tplg, config, size, cflags);
	free(config);
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

/* Convert Topology2.0 conf to the existing conf syntax */
static int pre_process_conf(const char *source_file, const char *output_file,
			    const char *pre_processor_defs)
{
	struct tplg_pre_processor *tplg_pp;
	size_t config_size;
	char *config;
	int err;

	err = load(source_file, (void **)&config, &config_size);
	if (err)
		return err;

	/* init pre-processor */
	err = init_pre_precessor(&tplg_pp, SND_OUTPUT_STDIO, output_file);
	if (err < 0) {
		fprintf(stderr, _("failed to init pre-processor for Topology2.0\n"));
		free(config);
		return err;
	}

	/* pre-process conf file */
	err = pre_process(tplg_pp, config, config_size, pre_processor_defs);

	/* free pre-processor */
	free_pre_preprocessor(tplg_pp);
	free(config);
	return err;
}

static int compile(const char *source_file, const char *output_file, int cflags,
		   const char *pre_processor_defs)
{
	struct tplg_pre_processor *tplg_pp = NULL;
	snd_tplg_t *tplg;
	char *config;
	void *bin;
	size_t config_size, size;
	int err;

	err = load(source_file, (void **)&config, &config_size);
	if (err)
		return err;

	/* pre-process before compiling */
	if (pre_process_config) {
		char *pconfig;
		size_t size;

		/* init pre-processor */
		init_pre_precessor(&tplg_pp, SND_OUTPUT_BUFFER, NULL);

		/* pre-process conf file */
		err = pre_process(tplg_pp, config, config_size, pre_processor_defs);
		if (err) {
			free_pre_preprocessor(tplg_pp);
			free(config);
			return err;
		}

		/* load topology */
		size = snd_output_buffer_string(tplg_pp->output, &pconfig);
		err = load_topology(&tplg, pconfig, size, cflags);

		/* free pre-processor */
		free_pre_preprocessor(tplg_pp);
	} else {
		err = load_topology(&tplg, config, config_size, cflags);
	}
	free(config);
	if (err)
		return err;
	err = snd_tplg_build_bin(tplg, &bin, &size);
	snd_tplg_free(tplg);
	if (err < 0 || size == 0) {
		fprintf(stderr, _("failed to compile context %s: %s\n"),
			source_file, snd_strerror(-err));
		return 1;
	}
	err = save(output_file, bin, size);
	free(bin);
	return err;
}

static int decode(const char *source_file, const char *output_file,
		  int cflags, int dflags, int sflags)
{
	snd_tplg_t *tplg;
	void *bin;
	char *text;
	size_t size;
	int err;

	if (load(source_file, &bin, &size))
		return 1;
	tplg = snd_tplg_create(cflags);
	if (tplg == NULL) {
		fprintf(stderr, _("failed to create new topology context\n"));
		return 1;
	}
	err = snd_tplg_decode(tplg, bin, size, dflags);
	free(bin);
	if (err < 0) {
		snd_tplg_free(tplg);
		fprintf(stderr, _("failed to decode context %s: %s\n"),
			source_file, snd_strerror(-err));
		return 1;
	}
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

int main(int argc, char *argv[])
{
	static const char short_options[] = "hc:d:n:u:v:o:pP:sgxzVD:";
	static const struct option long_options[] = {
		{"help", 0, NULL, 'h'},
		{"verbose", 1, NULL, 'v'},
		{"compile", 1, NULL, 'c'},
		{"pre-process", 1, NULL, 'p'},
		{"decode", 1, NULL, 'd'},
		{"normalize", 1, NULL, 'n'},
		{"dump", 1, NULL, 'u'},
		{"output", 1, NULL, 'o'},
		{"sort", 0, NULL, 's'},
		{"group", 0, NULL, 'g'},
		{"nocheck", 0, NULL, 'x'},
		{"dapm-nosort", 0, NULL, 'z'},
		{"version", 0, NULL, 'V'},
		{0, 0, 0, 0},
	};
	char *source_file = NULL;
	char *output_file = NULL;
	const char *pre_processor_defs = NULL;
	int c, err, op = 'c', cflags = 0, dflags = 0, sflags = 0, option_index;

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
		case 'd':
		case 'n':
		case 'u':
			if (source_file) {
				fprintf(stderr, _("Cannot combine operations (compile, normalize, pre-process, dump)\n"));
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
		case 'P':
			op = 'P';
			source_file = optarg;
			break;
		case 'p':
			pre_process_config = true;
			break;
		case 'g':
			sflags |= SND_TPLG_SAVE_GROUPS;
			break;
		case 'x':
			sflags |= SND_TPLG_SAVE_NOCHECK;
			break;
		case 'D':
			pre_processor_defs = optarg;
			break;
		case 'V':
			version(argv[0]);
			return 0;
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
		err = compile(source_file, output_file, cflags, pre_processor_defs);
		break;
	case 'd':
		err = decode(source_file, output_file, cflags, dflags, sflags);
		break;
	case 'P':
		err = pre_process_conf(source_file, output_file, pre_processor_defs);
		break;
	default:
		err = dump(source_file, output_file, cflags, sflags);
		break;
	}

	snd_output_close(log);
	return err ? 1 : 0;
}
