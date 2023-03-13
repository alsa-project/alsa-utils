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
#include <dlfcn.h>

#include <alsa/asoundlib.h>
#include "gettext.h"
#include "topology.h"
#include "pre-processor.h"
#include "pre-process-external.h"

#define SND_TOPOLOGY_MAX_PLUGINS 32

static int get_plugin_string(struct tplg_pre_processor *tplg_pp, char **plugin_string)
{
	const char *lib_names_t = NULL;
	snd_config_t *defines;
	int ret;

	ret = snd_config_search(tplg_pp->input_cfg, "Define.PREPROCESS_PLUGINS", &defines);
	if (ret < 0)
		return ret;

	ret = snd_config_get_string(defines, &lib_names_t);
	if (ret < 0)
		return ret;

	*plugin_string = strdup(lib_names_t);

	if (!*plugin_string)
		return -ENOMEM;

	return 0;
}

static int run_plugin(struct tplg_pre_processor *tplg_pp, char *plugin)
{
	plugin_pre_process process;
	char *xlib, *xfunc, *path;
	void *h = NULL;
	int ret = 0;

	/* compose the plugin path, if not from environment, then from default plugins dir */
	path = getenv("ALSA_TOPOLOGY_PLUGIN_DIR");
	if (!path)
		path = ALSA_TOPOLOGY_PLUGIN_DIR;

	xlib = tplg_snprintf("%s/%s%s%s", path, PROCESS_LIB_PREFIX, plugin,
			     PROCESS_LIB_POSTFIX);
	xfunc = tplg_snprintf("%s%s%s", PROCESS_FUNC_PREFIX, plugin,
			      PROCESS_FUNC_POSTFIX);

	if (!xlib || !xfunc) {
		fprintf(stderr, "can't reserve memory for plugin paths and func names\n");
		ret = -ENOMEM;
		goto err;
	}

	/* open plugin */
	h = dlopen(xlib, RTLD_NOW);
	if (!h) {
		fprintf(stderr, "unable to open library '%s'\n", xlib);
		ret = -EINVAL;
		goto err;
	}

	/* find function */
	process = dlsym(h, xfunc);

	if (!process) {
		fprintf(stderr, "symbol 'topology_process' was not found in %s\n", xlib);
		ret = -EINVAL;
		goto err;
	}

	/* process plugin */
	ret = process(tplg_pp->input_cfg, tplg_pp->output_cfg);

err:
	if (h)
		dlclose(h);
	if (xlib)
		free(xlib);
	if (xfunc)
		free(xfunc);

	return ret;
}

static int pre_process_plugins(struct tplg_pre_processor *tplg_pp)
{
	char *plugins[SND_TOPOLOGY_MAX_PLUGINS];
	char *plugin_string;
	int count;
	int ret;
	int i;

	/* parse plugin names */
	ret = get_plugin_string(tplg_pp, &plugin_string);

	/* no plugins defined, so just return */
	if (ret < 0)
		return 0;

	count = 0;
	plugins[count] = strtok(plugin_string, ":");
	while ((count < SND_TOPOLOGY_MAX_PLUGINS - 1) && plugins[count]) {
		count++;
		plugins[count] = strtok(NULL, ":");
	}

	/* run all plugins */
	for (i = 0; i < count; i++) {
		ret = run_plugin(tplg_pp, plugins[i]);
		if (ret < 0)
			return ret;
	}

	free(plugin_string);

	return 0;
}

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

void free_pre_processor(struct tplg_pre_processor *tplg_pp)
{
	snd_output_close(tplg_pp->output);
	snd_output_close(tplg_pp->dbg_output);
	snd_config_delete(tplg_pp->output_cfg);
	if (tplg_pp->define_cfg)
		snd_config_delete(tplg_pp->define_cfg);
	free(tplg_pp->inc_path);
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
static int pre_process_set_defines(struct tplg_pre_processor *tplg_pp, const char *pre_processor_defs)
{
	int ret;

	/*
	 * load the command line defines to the configuration tree
	 */
	if (pre_processor_defs != NULL) {
		ret = snd_config_load_string(&tplg_pp->define_cfg, pre_processor_defs, 0);
		if (ret < 0) {
			fprintf(stderr, "Failed to load pre-processor command line definitions\n");
			return ret;
		}
	} else {
		tplg_pp->define_cfg = NULL;
	}

	return 0;
}

static int pre_process_add_defines(struct tplg_pre_processor *tplg_pp, snd_config_t *from)
{
	snd_config_t *conf_defines, *conf_tmp;
	int ret;

	ret = snd_config_search(from, "Define", &conf_defines);
	if (ret == -ENOENT) {
		if (tplg_pp->input_cfg == from && tplg_pp->define_cfg) {
			conf_defines = NULL;
			goto create;
		}
	}
	if (ret < 0)
		return ret;

	if (snd_config_get_type(conf_defines) != SND_CONFIG_TYPE_COMPOUND) {
		fprintf(stderr, "Define must be a compound!\n");
		return -EINVAL;
	}

	if (tplg_pp->input_cfg == from)
		tplg_pp->define_cfg_merged = conf_defines;

	if (tplg_pp->define_cfg_merged == NULL) {
create:
		ret = snd_config_make_compound(&tplg_pp->define_cfg_merged, "Define", 0);
		if (ret < 0)
			return ret;
		ret = snd_config_add(tplg_pp->input_cfg, tplg_pp->define_cfg_merged);
		if (ret < 0)
			return ret;
	}

	if (tplg_pp->define_cfg_merged != conf_defines) {
		/*
		 * merge back to the main configuration tree (Define subtree)
		 */
		ret = snd_config_merge(tplg_pp->define_cfg_merged, conf_defines, true);
		if (ret < 0) {
			fprintf(stderr, "Failed to override main variable definitions\n");
			return ret;
		}
	}

	/*
	 * merge the command line defines with the variables in the conf file to override
	 * default values; use a copy (merge deletes the source tree)
	 */
	if (tplg_pp->define_cfg) {
		ret = snd_config_copy(&conf_tmp, tplg_pp->define_cfg);
		if (ret < 0) {
			fprintf(stderr, "Failed to copy variable definitions\n");
			return ret;
		}
		ret = snd_config_merge(tplg_pp->define_cfg_merged, conf_tmp, true);
		if (ret < 0) {
			fprintf(stderr, "Failed to override variable definitions\n");
			snd_config_delete(conf_tmp);
			return ret;
		}
	}

	return 0;
}

static int pre_process_includes(struct tplg_pre_processor *tplg_pp, snd_config_t *top);

static int pre_process_include_conf(struct tplg_pre_processor *tplg_pp, snd_config_t *config,
				    snd_config_t **new, snd_config_t *variable)
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

		ret = regcomp(&regex, id, REG_EXTENDED | REG_ICASE);
		if (ret) {
			fprintf(stderr, "Could not compile regex\n");
			goto err;
		}

		/* Execute regular expression */
		ret = regexec(&regex, value, 0, NULL, 0);
		if (ret)
			continue;

		/* regex matched. now include or use the configuration */
		if (snd_config_get_type(n) == SND_CONFIG_TYPE_COMPOUND) {
			/* configuration block */
			ret = snd_config_merge(*new, n, 0);
			if (ret < 0) {
				fprintf(stderr, "Unable to merge key '%s'\n", value);
				goto err;
			}
		} else {
			ret = snd_config_get_string(n, &filename);
			if (ret < 0)
				goto err;

			if (filename && filename[0] != '/')
				full_path = tplg_snprintf("%s/%s", tplg_pp->inc_path, filename);
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
		}

		/* forcefully overwrite with defines from the command line */
		ret = pre_process_add_defines(tplg_pp, *new);
		if (ret < 0 && ret != -ENOENT) {
			fprintf(stderr, "Failed to parse arguments in input config\n");
			goto err;
		}

		/* recursively process any nested includes */
		ret = pre_process_includes(tplg_pp, *new);
		if (ret < 0)
			goto err;
	}

err:
	free(value);
	return ret;
}

static int pre_process_includes(struct tplg_pre_processor *tplg_pp, snd_config_t *top)
{
	snd_config_iterator_t i, next;
	snd_config_t *includes;
	const char *top_id;
	int ret;

	if (tplg_pp->define_cfg_merged == NULL)
		return 0;

	ret = snd_config_search(top, "IncludeByKey", &includes);
	if (ret < 0)
		return 0;

	snd_config_get_id(top, &top_id);

	snd_config_for_each(i, next, includes) {
		snd_config_t *n, *new, *define;
		const char *id;

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		/* find id from variable definitions */
		ret = snd_config_search(tplg_pp->define_cfg_merged, id, &define);
		if (ret < 0) {
			fprintf(stderr, "No variable defined for %s\n", id);
			return ret;
		}

		/* create conf node from included file */
		ret = pre_process_include_conf(tplg_pp, n, &new, define);
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

	/* delete all includes from current top */
	snd_config_delete(includes);

	return 0;
}

static int pre_process_includes_all(struct tplg_pre_processor *tplg_pp, snd_config_t *top)
{
	snd_config_iterator_t i, next;
	int ret;

	if (snd_config_get_type(top) != SND_CONFIG_TYPE_COMPOUND)
		return 0;

	/* process includes at this node */
	ret = pre_process_includes(tplg_pp, top);
	if (ret < 0) {
		fprintf(stderr, "Failed to process includes\n");
		return ret;
	}

	/* process includes at all child nodes */
	snd_config_for_each(i, next, top) {
		snd_config_t *n;

		n = snd_config_iterator_entry(i);

		ret = pre_process_includes_all(tplg_pp, n);
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
	tplg_pp->inc_path = inc_path ? strdup(inc_path) : NULL;

#if SND_LIB_VER(1, 2, 5) < SND_LIB_VERSION
	/* parse command line definitions */
	err = pre_process_set_defines(tplg_pp, pre_processor_defs);
	if (err < 0) {
		fprintf(stderr, "Failed to parse arguments in input config\n");
		goto err;
	}

	/* parse command line definitions */
	err = pre_process_add_defines(tplg_pp, top);
	if (err < 0) {
		fprintf(stderr, "Failed to parse arguments in input config\n");
		goto err;
	}

	/* include conditional conf files */
	err = pre_process_includes_all(tplg_pp, tplg_pp->input_cfg);
	if (err < 0) {
		fprintf(stderr, "Failed to process conditional includes in input config\n");
		goto err;
	}
#endif

	err = pre_process_config(tplg_pp, tplg_pp->input_cfg);
	if (err < 0) {
		fprintf(stderr, "Unable to pre-process configuration\n");
		goto err;
	}

	/* process topology plugins */
	err = pre_process_plugins(tplg_pp);
	if (err < 0) {
		fprintf(stderr, "Unable to run pre-process plugins or plugins return error\n");
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
