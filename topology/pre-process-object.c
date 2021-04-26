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

static void tplg_attribute_print_valid_values(snd_config_t *valid_values, const char *name)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;

	SNDERR("valid values for attribute %s are:\n", name);

	snd_config_for_each(i, next, valid_values) {
		const char *s, *id;

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (snd_config_get_string(n, &s) < 0)
			continue;

		SNDERR("%s", s);
	}
}

/* check is attribute value belongs in the set of valid values */
static bool tplg_is_attribute_valid_value(snd_config_t *valid_values, const char *value)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;

	snd_config_for_each(i, next, valid_values) {
		const char *s, *id;

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (snd_config_get_string(n, &s) < 0)
			continue;

		if (!strcmp(value, s))
			return true;
	}

	return false;
}

/* check if attribute value passes the min/max value constraints */
static bool tplg_object_is_attribute_min_max_valid(snd_config_t *attr, snd_config_t *obj_attr,
						   bool min_check)
{
	snd_config_type_t type = snd_config_get_type(obj_attr);
	snd_config_t *valid;
	const char *attr_name;
	int ret;

	if (snd_config_get_id(attr, &attr_name) < 0)
		return false;

	if (min_check) {
		ret = snd_config_search(attr, "constraints.min", &valid);
		if (ret < 0)
			return true;
	} else {
		ret = snd_config_search(attr, "constraints.max", &valid);
		if (ret < 0)
			return true;
	}

	switch(type) {
	case SND_CONFIG_TYPE_INTEGER:
	{
		long v, m;

		if (snd_config_get_integer(valid, &m) < 0)
			return true;

		if (snd_config_get_integer(obj_attr, &v) < 0)
			return false;

		if (min_check) {
			if (v < m) {
				SNDERR("attribute '%s' value: %ld is less than min value: %d\n",
				       attr_name, v, m);
				return false;
			}
		} else {
			if (v > m) {
				SNDERR("attribute '%s' value: %ld is greater than max value: %d\n",
				       attr_name, v, m);
				return false;
			}
		}

		return true;
	}
	case SND_CONFIG_TYPE_INTEGER64:
	{
		long long v;
		long m;

		if (snd_config_get_integer(valid, &m) < 0)
			return true;

		if (snd_config_get_integer64(obj_attr, &v) < 0)
			return false;

		if (min_check) {
			if (v < m) {
				SNDERR("attribute '%s' value: %ld is less than min value: %d\n",
				       attr_name, v, m);
				return false;
			}
		} else {
			if (v > m) {
				SNDERR("attribute '%s' value: %ld is greater than max value: %d\n",
				       attr_name, v, m);
				return false;
			}
		}
		
		return true;
	}
	default:
		break;
	}

	return false;
}

/* check for min/max and valid value constraints */
static bool tplg_object_is_attribute_valid(struct tplg_pre_processor *tplg_pp,
					   snd_config_t *attr, snd_config_t *object)
{
	snd_config_iterator_t i, next;
	snd_config_t *valid, *obj_attr, *n;
	snd_config_type_t type;
	const char *attr_name, *obj_value;
	int ret;

	if (snd_config_get_id(attr, &attr_name) < 0)
		return false;

	ret = snd_config_search(object, attr_name, &obj_attr);
	if (ret < 0) {
		SNDERR("attr %s not found \n", attr_name);
		return false;
	}
	type = snd_config_get_type(obj_attr);

	/* check if attribute has valid values */
	ret = snd_config_search(attr, "constraints.valid_values", &valid);
	if (ret < 0)
		goto min_max_check;

	switch(type) {
	case SND_CONFIG_TYPE_STRING:
		if (snd_config_get_string(obj_attr, &obj_value) < 0)
			return false;
		if (!tplg_is_attribute_valid_value(valid, obj_value)) {
			tplg_attribute_print_valid_values(valid, attr_name);
			return false;
		}
		return true;
	case SND_CONFIG_TYPE_COMPOUND:
		snd_config_for_each(i, next, obj_attr) {
			const char *s, *id;

			n = snd_config_iterator_entry(i);
			if (snd_config_get_id(n, &id) < 0)
				continue;

			if (snd_config_get_string(n, &s) < 0)
				continue;

			if (!tplg_is_attribute_valid_value(valid, s)) {
				tplg_attribute_print_valid_values(valid, attr_name);
				return false;
			}
		}
		return true;
	default:
		break;
	}

	return false;

min_max_check:
	if (!tplg_object_is_attribute_min_max_valid(attr, obj_attr, true))
		return false;

	return tplg_object_is_attribute_min_max_valid(attr, obj_attr, false);
}

/* look up the instance of object in a config */
static snd_config_t *tplg_object_lookup_in_config(struct tplg_pre_processor *tplg_pp,
						  snd_config_t *class, const char *type,
						  const char *class_name, const char *id)
{
	snd_config_t *obj_cfg = NULL;
	char *config_id;

	config_id = tplg_snprintf("Object.%s.%s.%s", type, class_name, id);
	if (!config_id)
		return NULL;

	snd_config_search(class, config_id, &obj_cfg);
	free(config_id);
	return obj_cfg;
}

/* return 1 if attribute not found in search_config, 0 on success and negative value on error */
static int tplg_object_copy_and_add_param(struct tplg_pre_processor *tplg_pp,
					  snd_config_t *obj,
					  snd_config_t *attr_cfg,
					  snd_config_t *search_config)
{
	snd_config_t *attr, *new;
	const char *id, *search_id;
	int ret;

	if (snd_config_get_id(attr_cfg, &id) < 0)
		return 0;

	if (snd_config_get_id(search_config, &search_id) < 0)
		return 0;

	/* copy object value */
	ret = snd_config_search(search_config, id, &attr);
	if (ret < 0)
		return 1;

	ret = snd_config_copy(&new, attr);
	if (ret < 0) {
		SNDERR("error copying attribute '%s' value from %s\n", id, search_id);
		return ret;
	}

	ret = snd_config_add(obj, new);
	if (ret < 0) {
		snd_config_delete(new);
		SNDERR("error adding attribute '%s' value to %s\n", id, search_id);
	}

	return ret;
}

/*
 * Attribute values for an object can be set in one of the following in order of
 * precedence:
 * 1. Value set in object instance
 * 2. Default value set in the object's class definition
 * 3. Inherited value from the parent object
 * 4. Value set in the object instance embedded in the parent object
 * 5. Value set in the object instance embedded in the parent class definition
 */
static int tplg_object_update(struct tplg_pre_processor *tplg_pp, snd_config_t *obj,
			      snd_config_t *parent)
{
	snd_config_iterator_t i, next;
	snd_config_t *n, *cfg, *args;
	snd_config_t *obj_cfg, *class_cfg, *parent_obj;
	const char *obj_id, *class_name, *class_type;
	int ret;

	class_cfg = tplg_class_lookup(tplg_pp, obj);
	if (!class_cfg)
		return -EINVAL;

	/* find config for class attributes */
	ret = snd_config_search(class_cfg, "DefineAttribute", &args);
	if (ret < 0)
		return 0;

	if (snd_config_get_id(obj, &class_type) < 0)
		return 0;

	if (snd_config_get_id(class_cfg, &class_name) < 0)
		return 0;

	/* get obj cfg */
	obj_cfg = tplg_object_get_instance_config(tplg_pp, obj);
	if (snd_config_get_id(obj_cfg, &obj_id) < 0)
		return 0;

	/* copy and add attributes */
	snd_config_for_each(i, next, args) {
		snd_config_t *attr;
		const char *id;

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (tplg_class_is_attribute_unique(id, class_cfg))
			continue;

		if (tplg_class_is_attribute_immutable(id, class_cfg))
			goto class;

		/* check if attribute value is set in the object */
		ret = snd_config_search(obj_cfg, id, &attr);
		if (ret < 0)
			goto class;
		goto validate;
class:
		/* search for attributes value in class */
		ret = tplg_object_copy_and_add_param(tplg_pp, obj_cfg, n, class_cfg);
		if (ret == 1) {
			if (tplg_class_is_attribute_immutable(id, class_cfg)) {
				SNDERR("Immutable attribute %s not set in class %s\n",
				       id, class_name);
				return -EINVAL;
			}
			goto parent;
		}
		else if (ret < 0)
			return ret;
		goto validate;
parent:
		/* search for attribute value in parent */
		if (!parent)
			goto parent_object;

		/* get parent obj cfg */
		parent_obj = tplg_object_get_instance_config(tplg_pp, parent);
		if (!parent_obj)
			goto parent_object;

		ret = tplg_object_copy_and_add_param(tplg_pp, obj_cfg, n, parent_obj);
		if (ret == 1)
			goto parent_object;
		else if (ret < 0)
			return ret;
		goto validate;
parent_object:
		if (!parent)
			goto parent_class;

		cfg = tplg_object_lookup_in_config(tplg_pp, parent_obj, class_type,
						   class_name, obj_id);
		if (!cfg)
			goto parent_class;

		ret = tplg_object_copy_and_add_param(tplg_pp, obj_cfg, n, cfg);
		if (ret == 1)
			goto parent_class;
		else if (ret < 0)
			return ret;
		goto validate;
parent_class:
		if (!parent)
			goto check;

		cfg = tplg_class_lookup(tplg_pp, parent);
		if (!cfg)
			return -EINVAL;

		cfg = tplg_object_lookup_in_config(tplg_pp, cfg, class_type,
						   class_name, obj_id);
		if (!cfg)
			goto check;

		ret = tplg_object_copy_and_add_param(tplg_pp, obj_cfg, n, cfg);
		if (ret == 1)
			goto check;
		else if (ret < 0)
			return ret;
		goto validate;
check:
		if (tplg_class_is_attribute_mandatory(id, class_cfg)) {
			SNDERR("Mandatory attribute %s not set for class %s\n", id, class_name);
			return -EINVAL;
		}
		continue;
validate:
		if (!tplg_object_is_attribute_valid(tplg_pp, n, obj_cfg))
			return -EINVAL;
	}

	return 0;
}

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
	if (ret < 0) {
		SNDERR("error setting unique attribute value for '%s.%s'\n", class_id, id);
		return ret;
	}

	/* update object attributes and validate them */
	ret = tplg_object_update(tplg_pp, new_obj, parent);
	if (ret < 0)
		SNDERR("Failed to update attributes for object '%s.%s'\n", class_id, id);

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
