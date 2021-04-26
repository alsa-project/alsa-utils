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
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <alsa/input.h>
#include <alsa/output.h>
#include <alsa/conf.h>
#include <alsa/error.h>
#include "gettext.h"
#include "topology.h"
#include "pre-processor.h"

/* set the attribute value by type */
static int tplg_set_attribute_value(snd_config_t *attr, const char *value)
{
	int err;
	snd_config_type_t type = snd_config_get_type(attr);

	switch (type) {
	case SND_CONFIG_TYPE_INTEGER:
	{
		long v;

		v = strtol(value, NULL, 10);
		err = snd_config_set_integer(attr, v);
		assert(err >= 0);
		break;
	}
	case SND_CONFIG_TYPE_INTEGER64:
	{
		long long v;

		v = strtoll(value, NULL, 10);
		err = snd_config_set_integer64(attr, v);
		assert(err >= 0);
		break;
	}
	case SND_CONFIG_TYPE_STRING:
	{
		err = snd_config_set_string(attr, value);
		assert(err >= 0);
		break;
	}
	default:
		return -EINVAL;
	}

	return 0;
}

/*
 * Find the unique attribute in the class definition and set its value and type.
 * Only string or integer types are allowed for unique values.
 */
static int tplg_object_set_unique_attribute(struct tplg_pre_processor *tplg_pp,
					    snd_config_t *obj, snd_config_t *class_cfg,
					    const char *id)
{
	snd_config_t *unique_attr, *new;
	const char *unique_name, *class_id;
	int ret;

	if (snd_config_get_id(class_cfg, &class_id) < 0)
		return 0;

	/* find config for class unique attribute */
	unique_name = tplg_class_get_unique_attribute_name(tplg_pp, class_cfg);
	if (!unique_name)
		return -ENOENT;

	/* find the unique attribute definition in the class */
	unique_attr = tplg_class_find_attribute_by_name(tplg_pp, class_cfg, unique_name);
	if (!unique_attr)
		return -ENOENT;

	/* override value if unique attribute is set in the object instance */
	ret = snd_config_search(obj, unique_name, &new);
	if (ret < 0) {
		ret = snd_config_make(&new, unique_name,
				      tplg_class_get_attribute_type(tplg_pp, unique_attr));
		if (ret < 0) {
			SNDERR("error creating new attribute cfg for object %s\n", id);
			return ret;
		}
		ret = snd_config_add(obj, new);
		if (ret < 0) {
			SNDERR("error adding new attribute cfg for object %s\n", id);
			return ret;
		}
	}

	ret = tplg_set_attribute_value(new, id);
	if (ret < 0) {
		SNDERR("error setting unique attribute cfg for object %s\n", id);
		return ret;
	}

	return ret;
}

/*
 * Helper function to get object instance config which is 2 nodes down from class_type config.
 * ex: Get the pointer to the config node with ID "0" from the input config Widget.pga.0 {}
 */
snd_config_t *tplg_object_get_instance_config(struct tplg_pre_processor *tplg_pp,
					snd_config_t *class_type)
{
	snd_config_iterator_t first;
	snd_config_t *cfg;

	first = snd_config_iterator_first(class_type);
	cfg = snd_config_iterator_entry(first);
	first = snd_config_iterator_first(cfg);
	return snd_config_iterator_entry(first);
}

/* build object config */
static int tplg_build_object(struct tplg_pre_processor *tplg_pp, snd_config_t *new_obj,
			      snd_config_t *parent)
{
	snd_config_t *obj_local, *class_cfg;
	const char *id, *class_id;
	int ret;

	obj_local = tplg_object_get_instance_config(tplg_pp, new_obj);
	if (!obj_local)
		return -EINVAL;

	class_cfg = tplg_class_lookup(tplg_pp, new_obj);
	if (!class_cfg)
		return -EINVAL;

	if (snd_config_get_id(obj_local, &id) < 0)
		return 0;

	if (snd_config_get_id(class_cfg, &class_id) < 0)
		return 0;

	/* set unique attribute value */
	ret = tplg_object_set_unique_attribute(tplg_pp, obj_local, class_cfg, id);
	if (ret < 0)
		SNDERR("error setting unique attribute value for '%s.%s'\n", class_id, id);

	return ret;
}

/* create top-level topology objects */
int tplg_pre_process_objects(struct tplg_pre_processor *tplg_pp, snd_config_t *cfg,
			     snd_config_t *parent)
{
	snd_config_iterator_t i, next, i2, next2;
	snd_config_t *n, *n2, *_obj_type, *_obj_class, *_obj;
	const char *id, *class_type, *class_name;
	int ret;

	if (snd_config_get_id(cfg, &class_type) < 0)
		return 0;

	/* create all objects of the same type and class */
	snd_config_for_each(i, next, cfg) {
		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &class_name) < 0)
			continue;
		snd_config_for_each(i2, next2, n) {
			n2 = snd_config_iterator_entry(i2);
			if (snd_config_get_id(n2, &id) < 0) {
				SNDERR("Invalid id for object\n");
				return -EINVAL;
			}

			/* create a temp config for object with class type as the root node */
			ret = snd_config_make(&_obj_type, class_type, SND_CONFIG_TYPE_COMPOUND);
			if (ret < 0)
				return ret;

			ret = snd_config_make(&_obj_class, class_name, SND_CONFIG_TYPE_COMPOUND);
			if (ret < 0)
				goto err;

			ret = snd_config_add(_obj_type, _obj_class);
			if (ret < 0) {
				snd_config_delete(_obj_class);
				goto err;
			}

			ret = snd_config_copy(&_obj, n2);
			if (ret < 0)
				goto err;

			ret = snd_config_add(_obj_class, _obj);
			if (ret < 0) {
				snd_config_delete(_obj);
				goto err;
			}

			/* Build the object now */
			ret = tplg_build_object(tplg_pp, _obj_type, parent);
			if (ret < 0)
				SNDERR("Error building object %s.%s.%s\n",
				       class_type, class_name, id);
err:
			snd_config_delete(_obj_type);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}
