// SPDX-License-Identifier: GPL-2.0
//
// xfer-options.c - a parser of commandline options for xfer.
//
// Copyright (c) 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp>
//
// Licensed under the terms of the GNU General Public License, version 2.

#include "xfer.h"
#include "misc.h"

#include <getopt.h>
#include <math.h>
#include <limits.h>

enum no_short_opts {
	// 128 or later belong to non us-ascii character set.
	OPT_XFER_TYPE = 128,
	OPT_DUMP_HW_PARAMS,
	OPT_PERIOD_SIZE,
	OPT_BUFFER_SIZE,
	// Obsoleted.
	OPT_MAX_FILE_TIME,
	OPT_USE_STRFTIME,
	OPT_PROCESS_ID_FILE,
};

static void print_help()
{
	printf(
"Usage:\n"
"  axfer transfer DIRECTION [ COMMON-OPTIONS ] [ BACKEND-OPTIONS ]\n"
"\n"
"  where:\n"
"    DIRECTION = capture | playback\n"
"    COMMON-OPTIONS =\n"
"      -h, --help              help\n"
"      -v, --verbose           verbose\n"
"      -q, --quiet             quiet mode\n"
"      -d, --duration=#        interrupt after # seconds\n"
"      -s, --samples=#         interrupt after # frames\n"
"      -f, --format=FORMAT     sample format (case-insensitive)\n"
"      -c, --channels=#        channels\n"
"      -r, --rate=#            numeric sample rate in unit of Hz or kHz\n"
"      -t, --file-type=TYPE    file type (wav, au, sparc, voc or raw, case-insentive)\n"
"      -I, --separate-channels one file for each channel\n"
"      --dump-hw-params        dump hw_params of the device\n"
"      --xfer-type=BACKEND     backend type (libasound, libffado)\n"
	);
}

static int allocate_paths(struct xfer_context *xfer, char *const *paths,
			   unsigned int count)
{
	bool stdio = false;
	int i;

	if (count == 0) {
		stdio = true;
		count = 1;
	}

	xfer->paths = calloc(count, sizeof(xfer->paths[0]));
	if (xfer->paths == NULL)
		return -ENOMEM;
	xfer->path_count = count;

	if (stdio) {
		xfer->paths[0] = strndup("-", PATH_MAX);
		if (xfer->paths[0] == NULL)
			return -ENOMEM;
	} else {
		for (i = 0; i < count; ++i) {
			xfer->paths[i] = strndup(paths[i], PATH_MAX);
			if (xfer->paths[i] == NULL)
				return -ENOMEM;
		}
	}

	return 0;
}

static int verify_cntr_format(struct xfer_context *xfer)
{
	static const struct {
		const char *const literal;
		enum container_format cntr_format;
	} *entry, entries[] = {
		{"raw",		CONTAINER_FORMAT_RAW},
		{"voc",		CONTAINER_FORMAT_VOC},
		{"wav",		CONTAINER_FORMAT_RIFF_WAVE},
		{"au",		CONTAINER_FORMAT_AU},
		{"sparc",	CONTAINER_FORMAT_AU},
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(entries); ++i) {
		entry = &entries[i];
		if (strcasecmp(xfer->cntr_format_literal, entry->literal))
			continue;

		xfer->cntr_format = entry->cntr_format;
		return 0;
	}

	fprintf(stderr, "unrecognized file format '%s'\n",
		xfer->cntr_format_literal);

	return -EINVAL;
}

// This should be called after 'verify_cntr_format()'.
static int verify_sample_format(struct xfer_context *xfer)
{
	static const struct {
		const char *const literal;
		unsigned int frames_per_second;
		unsigned int samples_per_frame;
		snd_pcm_format_t le_format;
		snd_pcm_format_t be_format;
	} *entry, entries[] = {
		{"cd",	44100, 2, SND_PCM_FORMAT_S16_LE, SND_PCM_FORMAT_S16_BE},
		{"cdr",	44100, 2, SND_PCM_FORMAT_S16_LE, SND_PCM_FORMAT_S16_BE},
		{"dat",	48000, 2, SND_PCM_FORMAT_S16_LE, SND_PCM_FORMAT_S16_BE},
	};
	int i;

	xfer->sample_format = snd_pcm_format_value(xfer->sample_format_literal);
	if (xfer->sample_format != SND_PCM_FORMAT_UNKNOWN)
		return 0;

	for (i = 0; i < ARRAY_SIZE(entries); ++i) {
		entry = &entries[i];
		if (strcmp(entry->literal, xfer->sample_format_literal))
			continue;

		if (xfer->frames_per_second > 0 &&
		    xfer->frames_per_second != entry->frames_per_second) {
			fprintf(stderr,
				"'%s' format can't be used with rate except "
				"for %u.\n",
				entry->literal, entry->frames_per_second);
			return -EINVAL;
		}

		if (xfer->samples_per_frame > 0 &&
		    xfer->samples_per_frame != entry->samples_per_frame) {
			fprintf(stderr,
				"'%s' format can't be used with channel except "
				"for %u.\n",
				entry->literal, entry->samples_per_frame);
			return -EINVAL;
		}

		xfer->frames_per_second = entry->frames_per_second;
		xfer->samples_per_frame = entry->samples_per_frame;
		if (xfer->cntr_format == CONTAINER_FORMAT_AU)
			xfer->sample_format = entry->be_format;
		else
			xfer->sample_format = entry->le_format;

		return 0;
	}

	fprintf(stderr, "wrong extended format '%s'\n",
		xfer->sample_format_literal);

	return -EINVAL;
}

static int validate_options(struct xfer_context *xfer)
{
	unsigned int val;
	int err = 0;

	if (xfer->cntr_format_literal == NULL) {
		if (xfer->direction == SND_PCM_STREAM_CAPTURE) {
			// To stdout.
			if (xfer->path_count == 1 &&
			    !strcmp(xfer->paths[0], "-")) {
				xfer->cntr_format = CONTAINER_FORMAT_RAW;
			} else {
				// Use first path as a representative.
				xfer->cntr_format = container_format_from_path(
								xfer->paths[0]);
			}
		}
		// For playback, perform auto-detection.
	} else {
		err = verify_cntr_format(xfer);
	}
	if (err < 0)
		return err;

	if (xfer->multiple_cntrs) {
		if (!strcmp(xfer->paths[0], "-")) {
			fprintf(stderr,
				"An option for separated channels is not "
				"available with stdin/stdout.\n");
			return -EINVAL;
		}

		// For captured PCM frames, even if one path is given for
		// container files, it can be used to generate several paths.
		// For this purpose, please see
		// 'xfer_options_fixup_paths()'.
		if (xfer->direction == SND_PCM_STREAM_PLAYBACK) {
			// Require several paths for containers.
			if (xfer->path_count == 1) {
				fprintf(stderr,
					"An option for separated channels "
					"requires several files to playback "
					"PCM frames.\n");
				return -EINVAL;
			}
		}
	} else {
		// A single path is available only.
		if (xfer->path_count > 1) {
			fprintf(stderr,
				"When using several files, an option for "
				"sepatated channels is used with.\n");
			return -EINVAL;
		}
	}

	xfer->sample_format = SND_PCM_FORMAT_UNKNOWN;
	if (xfer->sample_format_literal) {
		err = verify_sample_format(xfer);
		if (err < 0)
			return err;
	}

	val = xfer->frames_per_second;
	if (xfer->frames_per_second == 0)
		xfer->frames_per_second = 8000;
	if (xfer->frames_per_second < 1000)
		xfer->frames_per_second *= 1000;
	if (xfer->frames_per_second < 2000 ||
	    xfer->frames_per_second > 192000) {
		fprintf(stderr, "bad speed value '%u'\n", val);
		return -EINVAL;
	}

	if (xfer->samples_per_frame > 0) {
		if (xfer->samples_per_frame < 1 ||
		    xfer->samples_per_frame > 256) {
			fprintf(stderr, "invalid channels argument '%u'\n",
				xfer->samples_per_frame);
			return -EINVAL;
		}
	}

	return err;
}

int xfer_options_parse_args(struct xfer_context *xfer,
			    const struct xfer_data *data, int argc,
			    char *const *argv)
{
	static const char *short_opts = "CPhvqd:s:f:c:r:t:IV:i";
	static const struct option long_opts[] = {
		// For generic purposes.
		{"capture",		0, 0, 'C'},
		{"playback",		0, 0, 'P'},
		{"xfer-type",		1, 0, OPT_XFER_TYPE},
		{"help",		0, 0, 'h'},
		{"verbose",		0, 0, 'v'},
		{"quiet",		0, 0, 'q'},
		{"duration",		1, 0, 'd'},
		{"samples",		1, 0, 's'},
		// For transfer backend.
		{"format",		1, 0, 'f'},
		{"channels",		1, 0, 'c'},
		{"rate",		1, 0, 'r'},
		// For containers.
		{"file-type",		1, 0, 't'},
		// For mapper.
		{"separate-channels",	0, 0, 'I'},
		// For debugging.
		{"dump-hw-params",	0, 0, OPT_DUMP_HW_PARAMS},
		// Obsoleted.
		{"max-file-time",	1, 0, OPT_MAX_FILE_TIME},
		{"use-strftime",	0, 0, OPT_USE_STRFTIME},
		{"process-id-file",	1, 0, OPT_PROCESS_ID_FILE},
		{"vumeter",		1, 0, 'V'},
		{"interactive",		0, 0, 'i'},
	};
	char *s_opts;
	struct option *l_opts;
	int l_index;
	int key;
	int err = 0;

	// Concatenate short options.
	s_opts = malloc(strlen(data->s_opts) + strlen(short_opts) + 1);
	if (s_opts == NULL)
		return -ENOMEM;
	strcpy(s_opts, data->s_opts);
	strcpy(s_opts + strlen(s_opts), short_opts);
	s_opts[strlen(data->s_opts) + strlen(short_opts)] = '\0';

	// Concatenate long options, including a sentinel.
	l_opts = calloc(ARRAY_SIZE(long_opts) * data->l_opts_count + 1,
			sizeof(*l_opts));
	if (l_opts == NULL) {
		free(s_opts);
		return -ENOMEM;
	}
	memcpy(l_opts, long_opts, ARRAY_SIZE(long_opts) * sizeof(*l_opts));
	memcpy(&l_opts[ARRAY_SIZE(long_opts)], data->l_opts,
	       data->l_opts_count * sizeof(*l_opts));

	// Parse options.
	l_index = 0;
	optarg = NULL;
	optind = 1;
	opterr = 1;	// use error output.
	optopt = 0;
	while (1) {
		key = getopt_long(argc, argv, s_opts, l_opts, &l_index);
		if (key < 0)
			break;
		else if (key == 'C')
			;	// already parsed.
		else if (key == 'P')
			;	// already parsed.
		else if (key == OPT_XFER_TYPE)
			;	// already parsed.
		else if (key == 'h')
			xfer->help = true;
		else if (key == 'v')
			++xfer->verbose;
		else if (key == 'q')
			xfer->quiet = true;
		else if (key == 'd')
			xfer->duration_seconds = arg_parse_decimal_num(optarg, &err);
		else if (key == 's')
			xfer->duration_frames = arg_parse_decimal_num(optarg, &err);
		else if (key == 'f')
			xfer->sample_format_literal = arg_duplicate_string(optarg, &err);
		else if (key == 'c')
			xfer->samples_per_frame = arg_parse_decimal_num(optarg, &err);
		else if (key == 'r')
			xfer->frames_per_second = arg_parse_decimal_num(optarg, &err);
		else if (key == 't')
			xfer->cntr_format_literal = arg_duplicate_string(optarg, &err);
		else if (key == 'I')
			xfer->multiple_cntrs = true;
		else if (key == OPT_DUMP_HW_PARAMS)
			xfer->dump_hw_params = true;
		else if (key == '?') {
			free(l_opts);
			free(s_opts);
			return -EINVAL;
		}
		else if (key == OPT_MAX_FILE_TIME ||
			 key == OPT_USE_STRFTIME ||
			 key == OPT_PROCESS_ID_FILE ||
			 key == 'V' ||
			 key == 'i') {
			fprintf(stderr,
				"An option '--%s' is obsoleted and has no "
				"effect.\n",
				l_opts[l_index].name);
			err = -EINVAL;
		} else {
			err = xfer->ops->parse_opt(xfer, key, optarg);
			if (err < 0 && err != -ENXIO)
				break;
		}
	}

	free(l_opts);
	free(s_opts);

	if (xfer->help) {
		print_help();
		if (xfer->ops->help) {
			printf("\n");
			printf("    BACKEND-OPTIONS (%s) =\n",
			       xfer_label_from_type(xfer->type));
			xfer->ops->help(xfer);
		}
		return 0;
	}

	err = allocate_paths(xfer, argv + optind, argc - optind);
	if (err < 0)
		return err;

	return validate_options(xfer);
}

void xfer_options_calculate_duration(struct xfer_context *xfer,
				     uint64_t *total_frame_count)
{
	uint64_t frame_count;

	if (xfer->duration_seconds > 0) {
		frame_count = (uint64_t)xfer->duration_seconds * (uint64_t)xfer->frames_per_second;
		if (frame_count < *total_frame_count)
			*total_frame_count = frame_count;
	}

	if (xfer->duration_frames > 0) {
		frame_count = xfer->duration_frames;
		if (frame_count < *total_frame_count)
			*total_frame_count = frame_count;
	}
}

static const char *const allowed_duplication[] = {
	"/dev/null",
	"/dev/zero",
	"/dev/full",
	"/dev/random",
	"/dev/urandom",
};

static int generate_path_with_suffix(struct xfer_context *xfer,
				     const char *template, unsigned int index,
				     const char *suffix)
{
	static const char *const single_format = "%s%s";
	static const char *const multiple_format = "%s-%i%s";
	unsigned int len;

	len = strlen(template) + strlen(suffix) + 1;
	if (xfer->path_count > 1)
		len += (unsigned int)log10(xfer->path_count) + 2;

	xfer->paths[index] = malloc(len);
	if (xfer->paths[index] == NULL)
		return -ENOMEM;

	if (xfer->path_count == 1) {
		snprintf(xfer->paths[index], len, single_format, template,
			 suffix);
	} else {
		snprintf(xfer->paths[index], len, multiple_format, template,
			 index, suffix);
	}

	return 0;
}

static int generate_path_without_suffix(struct xfer_context *xfer,
				        const char *template,
					unsigned int index, const char *suffix)
{
	static const char *const single_format = "%s";
	static const char *const multiple_format = "%s-%i";
	unsigned int len;

	len = strlen(template) + 1;
	if (xfer->path_count > 1)
		len += (unsigned int)log10(xfer->path_count) + 2;

	xfer->paths[index] = malloc(len);
	if (xfer->paths[index] == NULL)
		return -ENOMEM;

	if (xfer->path_count == 1) {
		snprintf(xfer->paths[index], len, single_format, template);
	} else {
		snprintf(xfer->paths[index], len, multiple_format, template,
			index);
	}

	return 0;
}

static int generate_path(struct xfer_context *xfer, char *template,
			 unsigned int index, const char *suffix)
{
	int (*generator)(struct xfer_context *xfer, const char *template,
			 unsigned int index, const char *suffix);
	char *pos;

	if (strlen(suffix) > 0) {
		pos = template + strlen(template) - strlen(suffix);
		// Separate filename and suffix.
		if (!strcmp(pos, suffix))
			*pos = '\0';
	}

	// Select handlers.
	if (strlen(suffix) > 0)
		generator = generate_path_with_suffix;
	else
		generator = generate_path_without_suffix;

	return generator(xfer, template, index, suffix);
}

static int create_paths(struct xfer_context *xfer, unsigned int path_count)
{
	char *template;
	const char *suffix;
	int i, j;
	int err = 0;

	// Can cause memory leak.
	assert(xfer->path_count == 1);
	assert(xfer->paths);
	assert(xfer->paths[0]);
	assert(xfer->paths[0][0] != '\0');

	// Release at first.
	template = xfer->paths[0];
	free(xfer->paths);
	xfer->paths = NULL;

	// Allocate again.
	xfer->paths = calloc(path_count, sizeof(*xfer->paths));
	if (xfer->paths == NULL) {
		err = -ENOMEM;
		goto end;
	}
	xfer->path_count = path_count;

	suffix = container_suffix_from_format(xfer->cntr_format);

	for (i = 0; i < xfer->path_count; ++i) {
		// Some file names are allowed to be duplicated.
		for (j = 0; j < ARRAY_SIZE(allowed_duplication); ++j) {
			if (!strcmp(template, allowed_duplication[j]))
				break;
		}
		if (j < ARRAY_SIZE(allowed_duplication))
			continue;

		err = generate_path(xfer, template, i, suffix);
		if (err < 0)
			break;
	}
end:
	free(template);

	return err;
}

static int fixup_paths(struct xfer_context *xfer)
{
	const char *suffix;
	char *template;
	int i, j;
	int err = 0;

	suffix = container_suffix_from_format(xfer->cntr_format);

	for (i = 0; i < xfer->path_count; ++i) {
		// Some file names are allowed to be duplicated.
		for (j = 0; j < ARRAY_SIZE(allowed_duplication); ++j) {
			if (!strcmp(xfer->paths[i], allowed_duplication[j]))
				break;
		}
		if (j < ARRAY_SIZE(allowed_duplication))
			continue;

		template = xfer->paths[i];
		xfer->paths[i] = NULL;
		err = generate_path(xfer, template, i, suffix);
		free(template);
		if (err < 0)
			break;
	}

	return err;
}

int xfer_options_fixup_paths(struct xfer_context *xfer)
{
	int i, j;
	int err;

	if (xfer->path_count == 1) {
		// Nothing to do for sign of stdin/stdout.
		if (!strcmp(xfer->paths[0], "-"))
			return 0;
		if (!xfer->multiple_cntrs)
			err = fixup_paths(xfer);
		else
			err = create_paths(xfer, xfer->samples_per_frame);
	} else {
		if (!xfer->multiple_cntrs)
			return -EINVAL;
		if (xfer->path_count != xfer->samples_per_frame)
			return -EINVAL;
		else
			err = fixup_paths(xfer);
	}
	if (err < 0)
		return err;

	// Check duplication of the paths.
	for (i = 0; i < xfer->path_count - 1; ++i) {
		// Some file names are allowed to be duplicated.
		for (j = 0; j < ARRAY_SIZE(allowed_duplication); ++j) {
			if (!strcmp(xfer->paths[i], allowed_duplication[j]))
				break;
		}
		if (j < ARRAY_SIZE(allowed_duplication))
			continue;

		for (j = i + 1; j < xfer->path_count; ++j) {
			if (!strcmp(xfer->paths[i], xfer->paths[j])) {
				fprintf(stderr,
					"Detect duplicated file names:\n");
				err = -EINVAL;
				break;
			}
		}
		if (j < xfer->path_count)
			break;
	}

	if (xfer->verbose > 1)
		fprintf(stderr, "Handled file names:\n");
	if (err < 0 || xfer->verbose > 1) {
		for (i = 0; i < xfer->path_count; ++i)
			fprintf(stderr, "    %d: %s\n", i, xfer->paths[i]);
	}

	return err;
}
