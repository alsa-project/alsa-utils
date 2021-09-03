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
#include <limits.h>
#include <stdio.h>
#include <alsa/asoundlib.h>
#include "topology.h"
#include "pre-processor.h"

bool tplg_class_is_attribute_check(const char *attr, snd_config_t *class_cfg, char *category)
{
	snd_config_iterator_t i, next;
	snd_config_t *cfg, *n;
	int ret;

	ret = snd_config_search(class_cfg, category, &cfg);
	if (ret < 0)
		return false;

	snd_config_for_each(i, next, cfg) {
		const char *id, *s;

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (snd_config_get_string(n, &s) < 0)
			continue;

		if (!strcmp(attr, s))
			return true;
	}

	return false;
}

/* check if attribute is mandatory */
bool tplg_class_is_attribute_mandatory(const char *attr, snd_config_t *class_cfg)
{
	return tplg_class_is_attribute_check(attr, class_cfg, "attributes.mandatory");
}

/* check if attribute is immutable */
bool tplg_class_is_attribute_immutable(const char *attr, snd_config_t *class_cfg)
{
	return tplg_class_is_attribute_check(attr, class_cfg, "attributes.immutable");
}

/* check if attribute is unique */
bool tplg_class_is_attribute_unique(const char *attr, snd_config_t *class_cfg)
{
	snd_config_t *unique;
	const char *s;
	int ret;

	ret = snd_config_search(class_cfg, "attributes.unique", &unique);
	if (ret < 0)
		return false;

	if (snd_config_get_string(unique, &s) < 0)
		return false;

	if (!strcmp(attr, s))
		return true;

	return false;
}

/*
 * Helper function to look up class definition from the Object config.
 * ex: For an object declaration, Object.Widget.pga.0{}, return the config correspdonding to
 * Class.Widget.pga{}. Note that input config , "cfg" does not include the "Object" node.
 */
snd_config_t *tplg_class_lookup(struct tplg_pre_processor *tplg_pp, snd_config_t *cfg)
{
	snd_config_iterator_t first, end;
	snd_config_t *class, *class_cfg = NULL;
	const char *class_type, *class_name;
	char *class_config_id;
	int ret;

	if (snd_config_get_id(cfg, &class_type) < 0)
		return NULL;

	first = snd_config_iterator_first(cfg);
	end = snd_config_iterator_end(cfg);

	if (first == end) {
		SNDERR("No class name provided for object type: %s\n", class_type);
		return NULL;
	}

	class = snd_config_iterator_entry(first);

	if (snd_config_get_id(class, &class_name) < 0)
		return NULL;

	class_config_id = tplg_snprintf("Class.%s.%s", class_type, class_name);
	if (!class_config_id)
		return NULL;

	ret = snd_config_search(tplg_pp->input_cfg, class_config_id, &class_cfg);
	if (ret < 0)
		SNDERR("No Class definition found for %s\n", class_config_id);

	free(class_config_id);
	return class_cfg;
}

/* find the attribute config by name in the class definition */
snd_config_t *tplg_class_find_attribute_by_name(struct tplg_pre_processor *tplg_pp,
						snd_config_t *class, const char *name)
{
	snd_config_t *attr = NULL;
	const char *class_id;
	char *attr_str;
	int ret;

	if (snd_config_get_id(class, &class_id) < 0)
		return NULL;

	attr_str = tplg_snprintf("DefineAttribute.%s", name);
	if (!attr_str)
		return NULL;

	ret = snd_config_search(class, attr_str, &attr);
	if (ret < 0)
		SNDERR("No definition for attribute '%s' in class '%s'\n",
			name, class_id);

	free(attr_str);
	return attr;
}

/* get the name of the attribute that must have a unique value in the object instance */
const char *tplg_class_get_unique_attribute_name(struct tplg_pre_processor *tplg_pp,
						 snd_config_t *class)
{
	snd_config_t *unique;
	const char *unique_name, *class_id;
	int ret;

	if (snd_config_get_id(class, &class_id) < 0)
		return NULL;

	ret = snd_config_search(class, "attributes.unique", &unique);
	if (ret < 0) {
		SNDERR("No unique attribute in class '%s'\n", class_id);
		return NULL;
	}

	if (snd_config_get_string(unique, &unique_name) < 0) {
		SNDERR("Invalid name for unique attribute in class '%s'\n", class_id);
		return NULL;
	}

	return unique_name;
}

/* get attribute type from the definition */
snd_config_type_t tplg_class_get_attribute_type(struct tplg_pre_processor *tplg_pp,
						snd_config_t *attr)
{
	snd_config_t *type;
	const char *s;
	int ret;

	/* default to integer if no type is given */
	ret = snd_config_search(attr, "type", &type);
	if (ret < 0)
		return SND_CONFIG_TYPE_INTEGER;

	ret = snd_config_get_string(type, &s);
	assert(ret >= 0);

	if (!strcmp(s, "string"))
		return SND_CONFIG_TYPE_STRING;

	if (!strcmp(s, "compound"))
		return SND_CONFIG_TYPE_COMPOUND;

	if (!strcmp(s, "real"))
		return SND_CONFIG_TYPE_REAL;

	if (!strcmp(s, "integer64"))
		return SND_CONFIG_TYPE_INTEGER64;

	return SND_CONFIG_TYPE_INTEGER;
}

/* get token_ref for attribute with name attr_name in the class */
const char *tplg_class_get_attribute_token_ref(struct tplg_pre_processor *tplg_pp,
					       snd_config_t *class, const char *attr_name)
{
	snd_config_t *attributes, *attr, *token_ref;
	const char *token;
	int ret;

	ret = snd_config_search(class, "DefineAttribute", &attributes);
	if (ret < 0)
		return NULL;

	ret = snd_config_search(attributes, attr_name, &attr);
	if (ret < 0)
		return NULL;

	ret = snd_config_search(attr, "token_ref", &token_ref);
	if (ret < 0)
		return NULL;

	ret = snd_config_get_string(token_ref, &token);
	if (ret < 0)
		return NULL;

	return token;
}

/* convert a valid attribute string value to the corresponding tuple value */
long tplg_class_attribute_valid_tuple_value(struct tplg_pre_processor *tplg_pp,
					    snd_config_t *class, snd_config_t *attr)
{

	snd_config_t *attributes, *cfg, *valid, *tuples, *n;
	snd_config_iterator_t i, next;
	const char *attr_name, *attr_value;
	int ret;

	ret = snd_config_get_id(attr, &attr_name);
	if (ret < 0)
		return -EINVAL;

	ret = snd_config_get_string(attr, &attr_value);
	if (ret < 0)
		return -EINVAL;

	/* find attribute definition in class */
	ret = snd_config_search(class, "DefineAttribute", &attributes);
	if (ret < 0)
		return -EINVAL;


	ret = snd_config_search(attributes, attr_name, &cfg);
	if (ret < 0)
		return -EINVAL;

	/* check if it has valid values */
	ret = snd_config_search(cfg, "constraints.valid_values", &valid);
	if (ret < 0)
		return -EINVAL;

	ret = snd_config_search(cfg, "constraints.tuple_values", &tuples);
	if (ret < 0)
		return -EINVAL;

	/* find and return the tuple value matching the attribute value id */
	snd_config_for_each(i, next, valid) {
		const char *s, *id;

		n = snd_config_iterator_entry(i);
		if (snd_config_get_string(n, &s) < 0)
			continue;
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (!strcmp(attr_value, s)) {
			snd_config_t *tuple;
			long tuple_value;

			ret = snd_config_search(tuples, id, &tuple);
			if (ret < 0)
				return -EINVAL;

			ret = snd_config_get_integer(tuple, &tuple_value);
			if (ret < 0)
				return ret;

			return tuple_value;
		}
	}

	return -EINVAL;
}
