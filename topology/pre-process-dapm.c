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
#include <errno.h>
#include <stdio.h>
#include <alsa/asoundlib.h>
#include "topology.h"
#include "pre-processor.h"

int tplg_build_base_object(struct tplg_pre_processor *tplg_pp, snd_config_t *obj_cfg,
			   snd_config_t *parent, bool skip_name)
{
	snd_config_t *top, *parent_obj, *cfg, *dest;
	const char *parent_name;

	/* find parent section config */
	top = tplg_object_get_section(tplg_pp, parent);
	if (!top)
		return -EINVAL;

	parent_obj = tplg_object_get_instance_config(tplg_pp, parent);

	/* get parent name */
	parent_name = tplg_object_get_name(tplg_pp, parent_obj);
	if (!parent_name)
		return 0;

	/* find parent config with name */
	dest = tplg_find_config(top, parent_name);
	if (!dest) {
		SNDERR("Cannot find parent config %s\n", parent_name);
		return -EINVAL;
	}

	/* build config from template and add to parent */
	return tplg_build_object_from_template(tplg_pp, obj_cfg, &cfg, dest, skip_name);
}

int tplg_build_scale_object(struct tplg_pre_processor *tplg_pp, snd_config_t *obj_cfg,
			      snd_config_t *parent)
{
	return tplg_build_base_object(tplg_pp, obj_cfg, parent, true);
}

int tplg_build_ops_object(struct tplg_pre_processor *tplg_pp, snd_config_t *obj_cfg,
			      snd_config_t *parent)
{
	return tplg_build_base_object(tplg_pp, obj_cfg, parent, false);
}

int tplg_build_channel_object(struct tplg_pre_processor *tplg_pp, snd_config_t *obj_cfg,
			      snd_config_t *parent)
{
	return tplg_build_base_object(tplg_pp, obj_cfg, parent, false);
}

int tplg_build_tlv_object(struct tplg_pre_processor *tplg_pp, snd_config_t *obj_cfg,
			      snd_config_t *parent)
{
	snd_config_t *cfg;
	const char *name;
	int ret;

	cfg = tplg_object_get_instance_config(tplg_pp, obj_cfg);

	name = tplg_object_get_name(tplg_pp, cfg);
	if (!name)
		return -EINVAL;

	ret = tplg_build_object_from_template(tplg_pp, obj_cfg, &cfg, NULL, false);
	if (ret < 0)
		return ret;

	return tplg_parent_update(tplg_pp, parent, "tlv", name);
}

static int tplg_build_control(struct tplg_pre_processor *tplg_pp, snd_config_t *obj_cfg,
			      snd_config_t *parent, char *type)
{
	snd_config_t *cfg, *obj;
	const char *name;
	int ret;

	obj = tplg_object_get_instance_config(tplg_pp, obj_cfg);

	/* get control name */
	ret = snd_config_search(obj, "name", &cfg);
	if (ret < 0)
		return 0;

	ret = snd_config_get_string(cfg, &name);
	if (ret < 0)
		return ret;

	ret = tplg_build_object_from_template(tplg_pp, obj_cfg, &cfg, NULL, false);
	if (ret < 0)
		return ret;

	ret = tplg_add_object_data(tplg_pp, obj_cfg, cfg, NULL);
	if (ret < 0)
		SNDERR("Failed to add data section for %s\n", name);

	return tplg_parent_update(tplg_pp, parent, type, name);
}

int tplg_build_mixer_control(struct tplg_pre_processor *tplg_pp, snd_config_t *obj_cfg,
			      snd_config_t *parent)
{
	return tplg_build_control(tplg_pp, obj_cfg, parent, "mixer");
}

int tplg_build_bytes_control(struct tplg_pre_processor *tplg_pp, snd_config_t *obj_cfg,
			      snd_config_t *parent)
{
	return tplg_build_control(tplg_pp, obj_cfg, parent, "bytes");
}

/*
 * Widget names for pipeline endpoints can be of the following type:
 * "class.<constructor args separated by .> ex: pga.0.1, buffer.1.1 etc
 * Optionally, the index argument for a widget can be omitted and will be substituted with
 * the index from the route: ex: pga..0, host..playback etc
 */
static int tplg_pp_get_widget_name(struct tplg_pre_processor *tplg_pp,
				      const char *string, long index, char **widget)
{
	snd_config_iterator_t i, next;
	snd_config_t *temp_cfg, *child, *class_cfg, *n;
	char *class_name, *args, *widget_name;
	int ret;

	/* get class name */
	args = strchr(string, '.');
	if (!args) {
		SNDERR("Error getting class name for %s\n", string);
		return -EINVAL;
	}

	class_name = calloc(1, strlen(string) - strlen(args) + 1);
	if (!class_name)
		return -ENOMEM;

	snprintf(class_name, strlen(string) - strlen(args) + 1, "%s", string);

	/* create config with Widget class type */
	ret = snd_config_make(&temp_cfg, "Widget", SND_CONFIG_TYPE_COMPOUND);
	if (ret < 0) {
		free(class_name);
		return ret;
	}

	/* create config with class name and add it to the Widget config */
	ret = tplg_config_make_add(&child, class_name, SND_CONFIG_TYPE_COMPOUND, temp_cfg);
	if (ret < 0) {
		free(class_name);
		return ret;
	}

	/* get class definition for widget */
	class_cfg = tplg_class_lookup(tplg_pp, temp_cfg);
	snd_config_delete(temp_cfg);
	if (!class_cfg) {
		free(class_name);
		return -EINVAL;
	}

	/* get constructor for class */
	ret = snd_config_search(class_cfg, "attributes.constructor", &temp_cfg);
	if (ret < 0) {
		SNDERR("No arguments in class for widget %s\n", string);
		free(class_name);
		return ret;
	}

	widget_name = strdup(class_name);
	free(class_name);
	if (!widget_name)
		return -ENOMEM;

	/* construct widget name using the constructor argument values */
	snd_config_for_each(i, next, temp_cfg) {
		const char *id;
		char *arg, *remaining, *temp;

		n = snd_config_iterator_entry(i);
		if (snd_config_get_string(n, &id) < 0)
			continue;

		if (!args) {
			SNDERR("insufficient arugments for widget %s\n", string);
			ret = -EINVAL;
			goto err;
		}

		remaining = strchr(args + 1, '.');
		if (remaining) {
			arg = calloc(1, strlen(args + 1) - strlen(remaining) + 1);
			if (!arg) {
				ret = -ENOMEM;
				goto err;
			}
			snprintf(arg, strlen(args + 1) - strlen(remaining) + 1, "%s", args + 1);
		} else {
			arg = calloc(1, strlen(args + 1) + 1);
			if (!arg) {
				ret = -ENOMEM;
				goto err;
			}

			snprintf(arg, strlen(args + 1) + 1, "%s", args + 1);
		}

		/* if no index provided, substitue with route index */
		if (!strcmp(arg, "") && !strcmp(id, "index")) {
			free(arg);
			arg = tplg_snprintf("%ld", index);
			if (!arg) {
				ret = -ENOMEM;
				free(arg);
				goto err;
			}
		}

		temp = tplg_snprintf("%s.%s", widget_name, arg);
		if (!temp) {
			ret = -ENOMEM;
			free(arg);
			goto err;
		}

		free(widget_name);
		widget_name = temp;
		free(arg);
		if (remaining)
			args = remaining;
		else
			args = NULL;
	}

	*widget = widget_name;
	return 0;

err:
	free(widget_name);
	return ret;
}

int tplg_build_dapm_route_object(struct tplg_pre_processor *tplg_pp, snd_config_t *obj_cfg,
			      snd_config_t *parent)
{
	snd_config_t *top, *obj, *cfg, *route, *child, *parent_obj;
	const char *name, *wname;
	const char *parent_name = "Endpoint";
	char *src_widget_name, *sink_widget_name, *line_str, *route_name;
	const char *control = "";
	long index = 0;
	int ret;

	obj = tplg_object_get_instance_config(tplg_pp, obj_cfg);

	ret = snd_config_get_id(obj, &name);
	if (ret < 0)
		return -EINVAL;

	/* endpoint connections at the top-level conf have no parent */
	if (parent) {
		parent_obj = tplg_object_get_instance_config(tplg_pp, parent);

		ret = snd_config_get_id(parent_obj, &parent_name);
		if (ret < 0)
			return -EINVAL;
	}

	tplg_pp_debug("Building DAPM route object: '%s' ...", name);

	ret = snd_config_search(tplg_pp->output_cfg, "SectionGraph", &top);
	if (ret < 0) {
		ret = tplg_config_make_add(&top, "SectionGraph",
					  SND_CONFIG_TYPE_COMPOUND, tplg_pp->output_cfg);
		if (ret < 0) {
			SNDERR("Error creating 'SectionGraph' config\n");
			return ret;
		}
	}

	/* get route index */
	ret = snd_config_search(obj, "index", &cfg);
	if (ret >= 0) {
		ret = snd_config_get_integer(cfg, &index);
		if (ret < 0) {
			SNDERR("Invalid index route %s\n", name);
			return ret;
		}
	}

	/* get source widget name */
	ret = snd_config_search(obj, "source", &cfg);
	if (ret < 0) {
		SNDERR("No source for route %s\n", name);
		return ret;
	}

	ret = snd_config_get_string(cfg, &wname);
	if (ret < 0) {
		SNDERR("Invalid name for source in route %s\n", name);
		return ret;
	}

	ret = tplg_pp_get_widget_name(tplg_pp, wname, index, &src_widget_name);
	if (ret < 0) {
		SNDERR("error getting widget name for %s\n", wname);
		return ret;
	}

	/* get sink widget name */
	ret = snd_config_search(obj, "sink", &cfg);
	if (ret < 0) {
		SNDERR("No sink for route %s\n", name);
		free(src_widget_name);
		return ret;
	}

	ret = snd_config_get_string(cfg, &wname);
	if (ret < 0) {
		SNDERR("Invalid name for sink in route %s\n", name);
		free(src_widget_name);
		return ret;
	}

	ret = tplg_pp_get_widget_name(tplg_pp, wname, index, &sink_widget_name);
	if (ret < 0) {
		SNDERR("error getting widget name for %s\n", wname);
		free(src_widget_name);
		return ret;
	}

	/* get control name */
	ret = snd_config_search(obj, "control", &cfg);
	if (ret >= 0) {
		ret = snd_config_get_string(cfg, &control);
		if (ret < 0) {
			SNDERR("Invalid control name for route %s\n", name);
			goto err;
		}
	}

	/* add route */
	route_name = tplg_snprintf("%s.%s", parent_name, name);
	if (!route_name) {
		ret = -ENOMEM;
		goto err;
	}

	ret = snd_config_make(&route, route_name, SND_CONFIG_TYPE_COMPOUND);
	free(route_name);
	if (ret < 0) {
		SNDERR("Error creating route config for %s %d\n", name, ret);
		goto err;
	}

	ret = snd_config_add(top, route);
	if (ret < 0) {
		SNDERR("Error adding route config for %s %d\n", name, ret);
		goto err;
	}

	/* add index */
	ret = tplg_config_make_add(&child, "index", SND_CONFIG_TYPE_INTEGER, route);
	if (ret < 0) {
		SNDERR("Error creating index config for %s\n", name);
		goto err;
	}

	ret = snd_config_set_integer(child, index);
	if (ret < 0) {
		SNDERR("Error setting index config for %s\n", name);
		goto err;
	}

	/* add lines */
	ret = tplg_config_make_add(&cfg, "lines", SND_CONFIG_TYPE_COMPOUND, route);
	if (ret < 0) {
		SNDERR("Error creating lines config for %s\n", name);
		goto err;
	}

	/* add route string */
	ret = tplg_config_make_add(&child, "0", SND_CONFIG_TYPE_STRING, cfg);
	if (ret < 0) {
		SNDERR("Error creating lines config for %s\n", name);
		goto err;
	}

	line_str = tplg_snprintf("%s, %s, %s", sink_widget_name, control, src_widget_name);
	if (!line_str) {
		ret = -ENOMEM;
		goto err;
	}

	/* set the line string */
	ret = snd_config_set_string(child, line_str);
	free(line_str);
	if (ret < 0)
		SNDERR("Error creating lines config for %s\n", name);
err:
	free(src_widget_name);
	free(sink_widget_name);
	return ret;
}
