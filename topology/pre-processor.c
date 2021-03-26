/*
  Copyright(c) 2021 Intel Corporation
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
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>

#include <alsa/input.h>
#include <alsa/output.h>
#include <alsa/conf.h>
#include "gettext.h"
#include "topology.h"

static int pre_process_config(struct tplg_pre_processor *tplg_pp, snd_config_t *cfg)
{
	snd_config_iterator_t i, next, i2, next2;
	snd_config_t *n, *n2;
	const char *id;

	if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
		fprintf(stderr, "compound type expected at top level");
		return -EINVAL;
	}

	/* parse topology objects */
	snd_config_for_each(i, next, cfg) {
		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (strcmp(id, "Object"))
			continue;

		if (snd_config_get_type(n) != SND_CONFIG_TYPE_COMPOUND) {
			fprintf(stderr, "compound type expected for %s", id);
			return -EINVAL;
		}

		snd_config_for_each(i2, next2, n) {
			n2 = snd_config_iterator_entry(i2);

			if (snd_config_get_id(n, &id) < 0)
				continue;

			if (snd_config_get_type(n2) != SND_CONFIG_TYPE_COMPOUND) {
				fprintf(stderr, "compound type expected for %s", id);
				return -EINVAL;
			}

			/* TODO: Add support for parsing objects */
		}
	}

	return 0;
}

void free_pre_preprocessor(struct tplg_pre_processor *tplg_pp)
{
	snd_output_close(tplg_pp->output);
	snd_output_close(tplg_pp->dbg_output);
	snd_config_delete(tplg_pp->output_cfg);
	free(tplg_pp);
}

int init_pre_precessor(struct tplg_pre_processor **tplg_pp, snd_output_type_t type,
		       const char *output_file)
{
	struct tplg_pre_processor *_tplg_pp;
	int ret;

	_tplg_pp = calloc(1, sizeof(struct tplg_pre_processor));
	if (!_tplg_pp)
		ret = -ENOMEM;

	*tplg_pp = _tplg_pp;

	/* create output top-level config node */
	ret = snd_config_top(&_tplg_pp->output_cfg);
	if (ret < 0)
		goto err;

	/* open output based on type */
	if (type == SND_OUTPUT_STDIO) {
		ret = snd_output_stdio_open(&_tplg_pp->output, output_file, "w");
		if (ret < 0) {
			fprintf(stderr, "failed to open file output\n");
			goto open_err;
		}
	} else {
		ret = snd_output_buffer_open(&_tplg_pp->output);
		if (ret < 0) {
			fprintf(stderr, "failed to open buffer output\n");
			goto open_err;
		}
	}

	/* debug output */
	ret = snd_output_stdio_attach(&_tplg_pp->dbg_output, stdout, 0);
	if (ret < 0) {
		fprintf(stderr, "failed to open stdout output\n");
		goto out_close;
	}

	return 0;
out_close:
	snd_output_close(_tplg_pp->output);
open_err:
	snd_config_delete(_tplg_pp->output_cfg);
err:
	free(_tplg_pp);
	return ret;
}

int pre_process(struct tplg_pre_processor *tplg_pp, char *config, size_t config_size)
{
	snd_input_t *in;
	snd_config_t *top;
	int err;

	/* create input buffer */
	err = snd_input_buffer_open(&in, config, config_size);
	if (err < 0) {
		fprintf(stderr, "Unable to open input buffer\n");
		return err;
	}

	/* create top-level config node */
	err = snd_config_top(&top);
	if (err < 0)
		goto input_close;

	/* load config */
	err = snd_config_load(top, in);
	if (err < 0) {
		fprintf(stderr, "Unable not load configuration\n");
		goto err;
	}

	tplg_pp->input_cfg = top;

	err = pre_process_config(tplg_pp, top);
	if (err < 0) {
		fprintf(stderr, "Unable to pre-process configuration\n");
		goto err;
	}

	/* save config to output */
	err = snd_config_save(tplg_pp->output_cfg, tplg_pp->output);
	if (err < 0)
		fprintf(stderr, "failed to save pre-processed output file\n");

err:
	snd_config_delete(top);
input_close:
	snd_input_close(in);

	return err;
}
