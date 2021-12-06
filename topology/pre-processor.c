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

#include <stdarg.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <regex.h>

#include <alsa/asoundlib.h>
#include "gettext.h"
#include "topology.h"
#include "pre-processor.h"

/*
 * Helper function to find config by id.
 * Topology2.0 object names are constructed with attribute values separated by '.'.
 * So snd_config_search() cannot be used as it interprets the '.' as the node separator.
 */
snd_config_t *tplg_find_config(snd_config_t *config, const char *name)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id;

	snd_config_for_each(i, next, config) {
		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (!strcmp(id, name))
			return n;
	}

	return NULL;
}

/* make a new config and add it to parent */
int tplg_config_make_add(snd_config_t **config, const char *id, snd_config_type_t type,
			 snd_config_t *parent)
{
	int ret;

	ret = snd_config_make(config, id, type);
	if (ret < 0)
		return ret;

	ret = snd_config_add(parent, *config);
	if (ret < 0)
		snd_config_delete(*config);

	return ret;
}

/*
 * The pre-processor will need to concat multiple strings separate by '.' to construct the object
 * name and search for configs with ID's separated by '.'.
 * This function helps concat input strings in the specified input format
 */
char *tplg_snprintf(char *fmt, ...)
{
	char *string;
	int len = 1;

	va_list va;

	va_start(va, fmt);
	len += vsnprintf(NULL, 0, fmt, va);
	va_end(va);

	string = calloc(1, len);
	if (!string)
		return NULL;

	va_start(va, fmt);
	vsnprintf(string, len, fmt, va);
	va_end(va);

	return string;
}

#ifdef TPLG_DEBUG
void tplg_pp_debug(char *fmt, ...)
{
	char msg[DEBUG_MAX_LENGTH];
	va_list va;

	va_start(va, fmt);
	vsnprintf(msg, DEBUG_MAX_LENGTH, fmt, va);
	va_end(va);

	fprintf(stdout, "%s\n", msg);
}

void tplg_pp_config_debug(struct tplg_pre_processor *tplg_pp, snd_config_t *cfg)
{
	snd_config_save(cfg, tplg_pp->dbg_output);
}
#else
void tplg_pp_debug(char *fmt, ...) {}
void tplg_pp_config_debug(struct tplg_pre_processor *tplg_pp, snd_config_t *cfg){}
#endif

static int pre_process_config(struct tplg_pre_processor *tplg_pp, snd_config_t *cfg)
{
	snd_config_iterator_t i, next, i2, next2;
	snd_config_t *n, *n2;
	const char *id;
	int err;

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

			if (snd_config_get_id(n2, &id) < 0)
				continue;

			if (snd_config_get_type(n2) != SND_CONFIG_TYPE_COMPOUND) {
				fprintf(stderr, "compound type expected for %s", id);
				return -EINVAL;
			}

			/* pre-process Object instance. Top-level object have no parent */
			err = tplg_pre_process_objects(tplg_pp, n2, NULL);
			if (err < 0)
				return err;
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

int init_pre_processor(struct tplg_pre_processor **tplg_pp, snd_output_type_t type,
		       const char *output_file)
{
	struct tplg_pre_processor *_tplg_pp;
	int ret;

	_tplg_pp = calloc(1, sizeof(struct tplg_pre_processor));
	if (!_tplg_pp)
		return -ENOMEM;

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

#if SND_LIB_VER(1, 2, 5) < SND_LIB_VERSION
static int pre_process_defines(struct tplg_pre_processor *tplg_pp, const char *pre_processor_defs,
			       snd_config_t *top)
{
	snd_config_t *conf_defines, *defines;
	int ret;

	ret = snd_config_search(tplg_pp->input_cfg, "Define", &conf_defines);
	if (ret < 0)
		return 0;

	if (snd_config_get_type(conf_defines) != SND_CONFIG_TYPE_COMPOUND)
		return 0;

	/*
	 * load and merge the command line defines with the variables in the conf file to override
	 * default values
	 */
	if (pre_processor_defs != NULL) {
		ret = snd_config_load_string(&defines, pre_processor_defs, strlen(pre_processor_defs));
		if (ret < 0) {
			fprintf(stderr, "Failed to load pre-processor command line definitions\n");
			return ret;
		}

		ret = snd_config_merge(conf_defines, defines, true);
		if (ret < 0) {
			fprintf(stderr, "Failed to override variable definitions\n");
			return ret;
		}
	}

	return 0;
}

static int pre_process_variables_expand_fcn(snd_config_t **dst, const char *str,
					    void *private_data)
{
	struct tplg_pre_processor *tplg_pp = private_data;
	snd_config_iterator_t i, next;
	snd_config_t *conf_defines;
	int ret;

	ret = snd_config_search(tplg_pp->input_cfg, "Define", &conf_defines);
	if (ret < 0)
		return 0;

	/* find variable definition */
	snd_config_for_each(i, next, conf_defines) {
		snd_config_t *n;
		const char *id;

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (strcmp(id, str))
			continue;

		/* found definition. Match type and return appropriate config */
		if (snd_config_get_type(n) == SND_CONFIG_TYPE_STRING) {
			const char *s;

			if (snd_config_get_string(n, &s) < 0)
				continue;

			return snd_config_imake_string(dst, NULL, s);
		}

		if (snd_config_get_type(n) == SND_CONFIG_TYPE_INTEGER) {
			long v;

			if (snd_config_get_integer(n, &v) < 0)
				continue;

			ret = snd_config_imake_integer(dst, NULL, v);
			return ret;
		}

	}

	fprintf(stderr, "No definition for variable %s\n", str);

	return -EINVAL;
}

static int pre_process_includes(struct tplg_pre_processor *tplg_pp, snd_config_t *top,
				const char *pre_processor_defs, const char *inc_path);

static int pre_process_include_conf(struct tplg_pre_processor *tplg_pp, snd_config_t *config,
				    const char *pre_processor_defs, snd_config_t **new,
				    snd_config_t *variable, const char *inc_path)
{
	snd_config_iterator_t i, next;
	const char *variable_name;
	char *value;
	int ret;

	if (snd_config_get_id(variable, &variable_name) < 0)
		return 0;

	switch(snd_config_get_type(variable)) {
	case SND_CONFIG_TYPE_STRING:
	{
		const char *s;

		if (snd_config_get_string(variable, &s) < 0) {
			SNDERR("Invalid value for variable %s\n", variable_name);
			return -EINVAL;
		}
		value = strdup(s);
		if (!value)
			return -ENOMEM;
		break;
	}
	case SND_CONFIG_TYPE_INTEGER:
	{
		long v;

		ret = snd_config_get_integer(variable, &v);
		if (ret < 0) {
			SNDERR("Invalid value for variable %s\n", variable_name);
			return ret;
		}

		value = tplg_snprintf("%ld", v);
		if (!value)
			return -ENOMEM;
		break;
	}
	default:
		SNDERR("Invalid type for variable definition %s\n", variable_name);
		return -EINVAL;
	}

	/* create top-level config node */
	ret = snd_config_top(new);
	if (ret < 0) {
		SNDERR("failed to create top-level node for include conf %s\n", variable_name);
		goto err;
	}

	snd_config_for_each(i, next, config) {
		snd_input_t *in;
		snd_config_t *n;
		regex_t regex;
		const char *filename;
		const char *id;
		char *full_path;

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		ret = regcomp(&regex, id, 0);
		if (ret) {
			fprintf(stderr, "Could not compile regex\n");
			goto err;
		}

		/* Execute regular expression */
		ret = regexec(&regex, value, 0, NULL, REG_ICASE);
		if (ret)
			continue;

		/* regex matched. now include the conf file */
		ret = snd_config_get_string(n, &filename);
		if (ret < 0)
			goto err;

		if (filename && filename[0] != '/')
			full_path = tplg_snprintf("%s/%s", inc_path, filename);
		else
			full_path = tplg_snprintf("%s", filename);

		ret = snd_input_stdio_open(&in, full_path, "r");
		if (ret < 0) {
			fprintf(stderr, "Unable to open included conf file %s\n", full_path);
			free(full_path);
			goto err;
		}
		free(full_path);

		/* load config */
		ret = snd_config_load(*new, in);
		snd_input_close(in);
		if (ret < 0) {
			fprintf(stderr, "Unable to load included configuration\n");
			goto err;
		}

		/* process any args in the included file */
		ret = pre_process_defines(tplg_pp, pre_processor_defs, *new);
		if (ret < 0) {
			fprintf(stderr, "Failed to parse arguments in input config\n");
			goto err;
		}

		/* recursively process any nested includes */
		return pre_process_includes(tplg_pp, *new, pre_processor_defs, inc_path);
	}

err:
	free(value);
	return ret;
}

static int pre_process_includes(struct tplg_pre_processor *tplg_pp, snd_config_t *top,
				const char *pre_processor_defs, const char *inc_path)
{
	snd_config_iterator_t i, next;
	snd_config_t *includes, *conf_defines;
	const char *top_id;
	int ret;

	ret = snd_config_search(top, "IncludeByKey", &includes);
	if (ret < 0)
		return 0;

	snd_config_get_id(top, &top_id);

	ret = snd_config_search(tplg_pp->input_cfg, "Define", &conf_defines);
	if (ret < 0)
		return 0;

	snd_config_for_each(i, next, includes) {
		snd_config_t *n, *new, *define;
		const char *id;

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		/* find id from variable definitions */
		ret = snd_config_search(conf_defines, id, &define);
		if (ret < 0) {
			fprintf(stderr, "No variable defined for %s\n", id);
			return ret;
		}

		/* create conf node from included file */
		ret = pre_process_include_conf(tplg_pp, n, pre_processor_defs, &new, define, inc_path);
		if (ret < 0) {
			fprintf(stderr, "Unable to process include file \n");
			return ret;
		}

		/* merge the included conf file with the top-level conf */
		ret = snd_config_merge(top, new, 0);
		if (ret < 0) {
			fprintf(stderr, "Failed to add included conf\n");
			return ret;
		}
	}

	/* remove all includes from current top */
	snd_config_remove(includes);

	return 0;
}

static int pre_process_includes_all(struct tplg_pre_processor *tplg_pp, snd_config_t *top,
				const char *pre_processor_defs, const char *inc_path)
{
	snd_config_iterator_t i, next;
	int ret;

	if (snd_config_get_type(top) != SND_CONFIG_TYPE_COMPOUND)
		return 0;

	/* process includes at this node */
	ret = pre_process_includes(tplg_pp, top, pre_processor_defs, inc_path);
	if (ret < 0) {
		fprintf(stderr, "Failed to process includes\n");
		return ret;
	}

	/* process includes at all child nodes */
	snd_config_for_each(i, next, top) {
		snd_config_t *n;

		n = snd_config_iterator_entry(i);

		ret = pre_process_includes_all(tplg_pp, n, pre_processor_defs, inc_path);
		if (ret < 0)
			return ret;
	}

	return 0;
}
#endif /* version < 1.2.6 */

int pre_process(struct tplg_pre_processor *tplg_pp, char *config, size_t config_size,
		const char *pre_processor_defs, const char *inc_path)
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

#if SND_LIB_VER(1, 2, 5) < SND_LIB_VERSION
	/* parse command line definitions */
	err = pre_process_defines(tplg_pp, pre_processor_defs, tplg_pp->input_cfg);
	if (err < 0) {
		fprintf(stderr, "Failed to parse arguments in input config\n");
		goto err;
	}

	/* include conditional conf files */
	err = pre_process_includes_all(tplg_pp, tplg_pp->input_cfg, pre_processor_defs, inc_path);
	if (err < 0) {
		fprintf(stderr, "Failed to process conditional includes in input config\n");
		goto err;
	}

	/* expand pre-processor variables */
	err = snd_config_expand_custom(tplg_pp->input_cfg, tplg_pp->input_cfg, pre_process_variables_expand_fcn,
				       tplg_pp, &tplg_pp->input_cfg);
	if (err < 0) {
		fprintf(stderr, "Failed to expand pre-processor definitions in input config\n");
		goto err;
	}
#endif

	err = pre_process_config(tplg_pp, tplg_pp->input_cfg);
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
