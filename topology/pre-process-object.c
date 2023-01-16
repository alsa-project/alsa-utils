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
#include <ctype.h>
#include <alsa/asoundlib.h>
#include "gettext.h"
#include "topology.h"
#include "pre-processor.h"

/* Parse VendorToken object, create the "SectionVendorToken" and save it */
int tplg_build_vendor_token_object(struct tplg_pre_processor *tplg_pp,
				   snd_config_t *obj_cfg, snd_config_t *parent)
{
	snd_config_iterator_t i, next;
	snd_config_t *vtop, *n, *obj;
	const char *name;
	int ret;

	ret = tplg_build_object_from_template(tplg_pp, obj_cfg, &vtop, NULL, false);
	if (ret < 0)
		return ret;

	ret = snd_config_get_id(vtop, &name);
	if (ret < 0)
		return ret;

	/* add the tuples */
	obj = tplg_object_get_instance_config(tplg_pp, obj_cfg);
	snd_config_for_each(i, next, obj) {
		snd_config_t *dst;
		const char *id;

		n = snd_config_iterator_entry(i);

		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (!strcmp(id, "name"))
			continue;

		ret = snd_config_copy(&dst, n);
		if (ret < 0) {
			SNDERR("Error copying config node %s for '%s'\n", id, name);
			return ret;
		}

		ret = snd_config_add(vtop, dst);
		if (ret < 0) {
			snd_config_delete(dst);
			SNDERR("Error adding vendortoken %s for %s\n", id, name);
			return ret;
		}
	}

	return ret;
}

int tplg_parent_update(struct tplg_pre_processor *tplg_pp, snd_config_t *parent,
			  const char *section_name, const char *item_name)
{
	snd_config_iterator_t i, next;
	snd_config_t *child, *cfg, *top, *item_config, *n;
	const char *parent_name;
	char *item_id;
	int ret, id = 0;

	/* Nothing to do if parent is NULL */
	if (!parent)
		return 0;

	child = tplg_object_get_instance_config(tplg_pp, parent);
	ret = snd_config_search(child, "name", &cfg);
	if (ret < 0) {
		ret = snd_config_get_id(child, &parent_name);
		if (ret < 0) {
			SNDERR("No name config for parent\n");
			return ret;
		}
	} else {
		ret = snd_config_get_string(cfg, &parent_name);
		if (ret < 0) {
			SNDERR("Invalid name for parent\n");
			return ret;
		}
	}

	top = tplg_object_get_section(tplg_pp, parent);
	if (!top)
		return -EINVAL;

	/* get config with name */
	cfg = tplg_find_config(top, parent_name);
	if (!cfg)
		return ret;

	/* get section config */
	if (!strcmp(section_name, "tlv")) {
		/* set tlv name if config exists already */
		ret = snd_config_search(cfg, section_name, &item_config);
			if (ret < 0) {
			ret = tplg_config_make_add(&item_config, section_name,
						  SND_CONFIG_TYPE_STRING, cfg);
			if (ret < 0) {
				SNDERR("Error creating section config widget %s for %s\n",
				       section_name, parent_name);
				return ret;
			}
		}

		return snd_config_set_string(item_config, item_name);
	}

	ret = snd_config_search(cfg, section_name, &item_config);
	if (ret < 0) {
		ret = tplg_config_make_add(&item_config, section_name,
					  SND_CONFIG_TYPE_COMPOUND, cfg);
		if (ret < 0) {
			SNDERR("Error creating section config widget %s for %s\n",
			       section_name, parent_name);
			return ret;
		}
	}

	snd_config_for_each(i, next, item_config) {
		const char *name;

		n = snd_config_iterator_entry(i);
		if (snd_config_get_string(n, &name) < 0)
			continue;

		/* item already exists */
		if (!strcmp(name, item_name))
			return 0;
		id++;
	}

	/* add new item */
	item_id = tplg_snprintf("%d", id);
	if (!item_id)
		return -ENOMEM;

	ret = snd_config_make(&cfg, item_id, SND_CONFIG_TYPE_STRING);
	free(item_id);
	if (ret < 0)
		return ret;

	ret = snd_config_set_string(cfg, item_name);
	if (ret < 0)
		return ret;

	ret = snd_config_add(item_config, cfg);
	if (ret < 0)
		snd_config_delete(cfg);

	return ret;
}

/* Parse data object, create the "SectionData" and save it. Only "bytes" data supported for now */
int tplg_build_data_object(struct tplg_pre_processor *tplg_pp, snd_config_t *obj_cfg,
			    snd_config_t *parent)
{
	snd_config_t *dtop;
	const char *name;
	int ret;

	ret = tplg_build_object_from_template(tplg_pp, obj_cfg, &dtop, NULL, false);
	if (ret < 0)
		return ret;

	ret = snd_config_get_id(dtop, &name);
	if (ret < 0)
		return ret;

	return tplg_parent_update(tplg_pp, parent, "data", name);
}

static int tplg_create_config_template(struct tplg_pre_processor *tplg_pp,
				       snd_config_t **template,
				       const struct config_template_items *items)
{
	snd_config_t *top, *child;
	int ret, i;

	ret = snd_config_make(&top, "template", SND_CONFIG_TYPE_COMPOUND);
	if (ret < 0)
		return ret;

	/* add integer configs */
	for (i = 0; i < MAX_CONFIGS_IN_TEMPLATE; i++)
		if (items->int_config_ids[i]) {
			ret = tplg_config_make_add(&child, items->int_config_ids[i],
						   SND_CONFIG_TYPE_INTEGER, top);
			if (ret < 0)
				goto err;
		}

	/* add string configs */
	for (i = 0; i < MAX_CONFIGS_IN_TEMPLATE; i++)
		if (items->string_config_ids[i]) {
			ret = tplg_config_make_add(&child, items->string_config_ids[i],
						   SND_CONFIG_TYPE_STRING, top);
			if (ret < 0)
				goto err;
		}

	/* add compound configs */
	for (i = 0; i < MAX_CONFIGS_IN_TEMPLATE; i++) {
		if (items->compound_config_ids[i]) {
			ret = tplg_config_make_add(&child, items->compound_config_ids[i],
						   SND_CONFIG_TYPE_COMPOUND, top);
			if (ret < 0)
				goto err;
		}
	}

err:
	if (ret < 0) {
		snd_config_delete(top);
		return ret;
	}

	*template = top;
	return ret;
}

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

#if SND_LIB_VER(1, 2, 5) < SND_LIB_VERSION
static int pre_process_find_variable(snd_config_t **dst, const char *str, snd_config_t *config);

/* check is attribute value belongs in the set of valid values after expanding the valid values */
static bool tplg_is_attribute_valid_expanded_value(struct tplg_pre_processor *tplg_pp,
						   snd_config_t *valid_values, int value)
{
	snd_config_iterator_t i, next;
	snd_config_t *n, *var, *conf_defines;
	int ret;

	ret = snd_config_search(tplg_pp->input_cfg, "Define", &conf_defines);
	if (ret < 0)
		return false;

	snd_config_for_each(i, next, valid_values) {
		const char *s, *id;
		long v;

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (snd_config_get_string(n, &s) < 0)
			continue;

		if (s[0] != '$')
			continue;

		/* find the variable definition */
		ret = pre_process_find_variable(&var, s + 1, conf_defines);
		if (ret < 0) {
			SNDERR("No definition for variable %s\n", s + 1);
			return false;
		}

		if (snd_config_get_integer(var, &v) < 0) {
			SNDERR("Invalid definition for %s\n", s);
			snd_config_delete(var);
			return false;
		}

		snd_config_delete(var);

		if (value == v)
			return true;
	}

	return false;
}
#endif

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
					   snd_config_t *attr, snd_config_t *obj_attr)
{
	snd_config_iterator_t i, next;
	snd_config_t *valid, *n;
	snd_config_type_t type;
	const char *attr_name, *obj_value;
	int ret;

	if (snd_config_get_id(attr, &attr_name) < 0)
		return false;

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
#if SND_LIB_VER(1, 2, 5) < SND_LIB_VERSION
	case SND_CONFIG_TYPE_INTEGER:
	{
		long v;

		if (snd_config_get_integer(obj_attr, &v) < 0)
			return false;

		if (!tplg_is_attribute_valid_expanded_value(tplg_pp, valid, v)) {
			tplg_attribute_print_valid_values(valid, attr_name);
			return false;
		}
		return true;
	}
#endif
	default:
		break;
	}

	return false;

min_max_check:
	if (!tplg_object_is_attribute_min_max_valid(attr, obj_attr, true))
		return false;

	return tplg_object_is_attribute_min_max_valid(attr, obj_attr, false);
}

/* get object's name attribute value */
const char *tplg_object_get_name(struct tplg_pre_processor *tplg_pp,
				 snd_config_t *object)
{
	snd_config_t *cfg;
	const char *name;
	int ret;

	ret = snd_config_search(object, "name", &cfg);
	if (ret < 0)
		return NULL;

	ret = snd_config_get_string(cfg, &name);
	if (ret < 0)
		return NULL;

	return name;
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

	if (snd_config_search(class, config_id, &obj_cfg) < 0)
		return NULL;
	free(config_id);
	return obj_cfg;
}

static int tplg_pp_add_object_tuple_section(struct tplg_pre_processor *tplg_pp,
					    snd_config_t *class_cfg,
					    snd_config_t *attr, char *data_name,
					    const char *token_ref, const char *array_name)
{
	snd_config_t *top, *tuple_cfg, *child, *cfg, *new;
	const char *id;
	char *token, *type, *str;
	long tuple_value;
	int ret;

	tplg_pp_debug("Building vendor tuples section: '%s' ...", data_name);

	ret = snd_config_search(tplg_pp->output_cfg, "SectionVendorTuples", &top);
	if (ret < 0) {
		ret = tplg_config_make_add(&top, "SectionVendorTuples",
					  SND_CONFIG_TYPE_COMPOUND, tplg_pp->output_cfg);
		if (ret < 0) {
			SNDERR("Error creating SectionVendorTuples config\n");
			return ret;
		}
	}

	type = strchr(token_ref, '.');
	if(!type) {
		SNDERR("Error getting type for %s\n", token_ref);
		return -EINVAL;
	}

	token = calloc(1, strlen(token_ref) - strlen(type) + 1);
	if (!token)
		return -ENOMEM;
	snprintf(token, strlen(token_ref) - strlen(type) + 1, "%s", token_ref);

	if (!array_name)
		str = strdup(type + 1);
	else
		str = tplg_snprintf("%s.%s", type + 1, array_name);
	if (!str) {
		ret = -ENOMEM;
		goto free;
	}

	tuple_cfg = tplg_find_config(top, data_name);
	if (!tuple_cfg) {
		/* add new SectionVendorTuples */
		ret = tplg_config_make_add(&tuple_cfg, data_name, SND_CONFIG_TYPE_COMPOUND, top);
		if (ret < 0) {
			SNDERR("Error creating new vendor tuples config %s\n", data_name);
			goto err;
		}

		ret = tplg_config_make_add(&child, "tokens", SND_CONFIG_TYPE_STRING,
					  tuple_cfg);
		if (ret < 0) {
			SNDERR("Error creating tokens config for '%s'\n", data_name);
			goto err;
		}

		ret = snd_config_set_string(child, token);
		if (ret < 0) {
			SNDERR("Error setting tokens config for '%s'\n", data_name);
			goto err;
		}

		ret = tplg_config_make_add(&child, "tuples", SND_CONFIG_TYPE_COMPOUND,
					  tuple_cfg);
		if (ret < 0) {
			SNDERR("Error creating tuples config for '%s'\n", data_name);
			goto err;
		}

		ret = tplg_config_make_add(&cfg, str, SND_CONFIG_TYPE_COMPOUND,
					  child);
		if (ret < 0) {
			SNDERR("Error creating tuples type config for '%s'\n", data_name);
			goto err;
		}
	} else {
		snd_config_t *tuples_cfg;

		ret = snd_config_search(tuple_cfg, "tuples" , &tuples_cfg);
		if (ret < 0) {
			SNDERR("can't find tuples config in %s\n", data_name);
			goto err;
		}

		cfg = tplg_find_config(tuples_cfg, str);
		if (!cfg) {
			ret = tplg_config_make_add(&cfg, str, SND_CONFIG_TYPE_COMPOUND,
						  tuples_cfg);
			if (ret < 0) {
				SNDERR("Error creating tuples config for '%s' and type name %s\n", data_name, str);
				goto err;
			}
		}
	}

	ret = snd_config_get_id(attr, &id);
	if (ret < 0)
		goto err;

	/* tuple exists already? */
	ret = snd_config_search(cfg, id, &child);
	if (ret >=0)
		goto err;

	/* add attribute to tuples */
	tuple_value = tplg_class_attribute_valid_tuple_value(tplg_pp, class_cfg, attr);
	if (tuple_value < 0) {
		/* just copy attribute cfg as is */
		ret = snd_config_copy(&new, attr);
		if (ret < 0) {
			SNDERR("can't copy attribute for %s\n", data_name);
			goto err;
		}
	} else {
		ret = snd_config_make(&new, id, SND_CONFIG_TYPE_INTEGER);
		if (ret < 0)
			goto err;

		ret = snd_config_set_integer(new, tuple_value);
		if (ret < 0)
			goto err;
	}

	ret = snd_config_add(cfg, new);
err:
	free(str);
free:
	free(token);
	return ret;
}

static int tplg_pp_add_object_data_section(struct tplg_pre_processor *tplg_pp,
					   snd_config_t *obj_data, char *data_name)
{
	snd_config_iterator_t i, next;
	snd_config_t *top, *data_cfg, *child;
	char *data_id;
	int ret, id = 0;

	ret = snd_config_search(tplg_pp->output_cfg, "SectionData", &top);
	if (ret < 0) {
		ret = tplg_config_make_add(&top, "SectionData", SND_CONFIG_TYPE_COMPOUND,
					  tplg_pp->output_cfg);
		if (ret < 0) {
			SNDERR("Failed to add SectionData\n");
			return ret;
		}
	}

	/* nothing to do if data section already exists */
	data_cfg = tplg_find_config(top, data_name);
	if (data_cfg)
		return 0;

	tplg_pp_debug("Building data section %s ...", data_name);

	/* add new SectionData */
	ret = tplg_config_make_add(&data_cfg, data_name, SND_CONFIG_TYPE_COMPOUND, top);
	if (ret < 0)
		return ret;

	ret = tplg_config_make_add(&child, "tuples", SND_CONFIG_TYPE_STRING, data_cfg);
	if (ret < 0) {
		SNDERR("error adding data ref for %s\n", data_name);
		return ret;
	}

	ret = snd_config_set_string(child, data_name);
	if (ret < 0) {
		SNDERR("error setting tuples ref for %s\n", data_name);
		return ret;
	}

	/* add data item to object */
	snd_config_for_each(i, next, obj_data)
		id++;

	data_id = tplg_snprintf("%d", id);
	if (!data_id)
		return -ENOMEM;

	ret = tplg_config_make_add(&child, data_id, SND_CONFIG_TYPE_STRING, obj_data);
	free(data_id);
	if (ret < 0) {
		SNDERR("error adding data ref %s\n", data_name);
		return ret;
	}

	return snd_config_set_string(child, data_name);
}

int tplg_add_object_data(struct tplg_pre_processor *tplg_pp, snd_config_t *obj_cfg,
			 snd_config_t *top, const char *array_name)
{
	snd_config_iterator_t i, next;
	snd_config_t *data_cfg, *class_cfg, *n, *obj;
	const char *object_id;
	int ret;

	if (snd_config_get_id(top, &object_id) < 0)
		return 0;

	obj = tplg_object_get_instance_config(tplg_pp, obj_cfg);

	class_cfg = tplg_class_lookup(tplg_pp, obj_cfg);
	if (!class_cfg)
		return -EINVAL;

	/* add data config to top */
	ret = snd_config_search(top, "data", &data_cfg);
	if (ret < 0) {
		ret = tplg_config_make_add(&data_cfg, "data", SND_CONFIG_TYPE_COMPOUND, top);
		if (ret < 0) {
			SNDERR("error creating data config for %s\n", object_id);
			return ret;
		}
	}

	/* add data items to object's data section */
	snd_config_for_each(i, next, obj) {
		const char *id, *token;
		char *data_cfg_name;

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		token = tplg_class_get_attribute_token_ref(tplg_pp, class_cfg, id);
		if (!token)
			continue;

		data_cfg_name = tplg_snprintf("%s.%s", object_id, token);
		if (!data_cfg_name)
			return -ENOMEM;

		ret = tplg_pp_add_object_data_section(tplg_pp, data_cfg, data_cfg_name);
		if (ret < 0) {
			SNDERR("Failed to add data section %s\n", data_cfg_name);
			free(data_cfg_name);
			return ret;
		}

		ret = tplg_pp_add_object_tuple_section(tplg_pp, class_cfg, n, data_cfg_name,
						       token, array_name);
		if (ret < 0) {
			SNDERR("Failed to add data section %s\n", data_cfg_name);
			free(data_cfg_name);
			return ret;
		}
		free(data_cfg_name);
	}

	return 0;
}

/* search for all template configs in the source config and copy them to the destination */
static int tplg_object_add_attributes(snd_config_t *dst, snd_config_t *template,
				      snd_config_t *src)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	int ret;

	snd_config_for_each(i, next, template) {
		snd_config_t *attr, *new;
		const char *id;

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		ret = snd_config_search(src, id, &attr);
		if (ret < 0)
			continue;

		/* skip if attribute is already set */
		ret = snd_config_search(dst, id, &new);
		if (ret >= 0)
			continue;

		ret = snd_config_copy(&new, attr);
		if (ret < 0) {
			SNDERR("failed to copy attribute %s\n", id);
			return ret;
		}

		ret = snd_config_add(dst, new);
		if (ret < 0) {
			snd_config_delete(new);
			SNDERR("failed to add attribute %s\n", id);
			return ret;
		}
	}

	return 0;
}

static const struct build_function_map *tplg_object_get_map(struct tplg_pre_processor *tplg_pp,
							    snd_config_t *obj);

/* Add object attributes to the private data of the parent object config */
static int tplg_build_parent_data(struct tplg_pre_processor *tplg_pp, snd_config_t *obj_cfg,
				  snd_config_t *parent)
{
	snd_config_t *obj, *parent_obj, *section_cfg, *top;
	const struct build_function_map *map;
	const char *id, *parent_id;
	int ret;

	/* nothing to do if parent is NULL */
	if (!parent)
		return 0;

	obj = tplg_object_get_instance_config(tplg_pp, obj_cfg);
	parent_obj = tplg_object_get_instance_config(tplg_pp, parent);

	/* get object ID */
	if (snd_config_get_id(obj, &id) < 0) {
		SNDERR("Invalid ID for object\n");
		return -EINVAL;
	}

	/* get parent object name or ID */
	parent_id = tplg_object_get_name(tplg_pp, parent_obj);
	if (!parent_id) {
		ret = snd_config_get_id(parent_obj, &parent_id);
		if (ret < 0) {
			SNDERR("Invalid ID for parent of object %s\n", id);
			return ret;
		}
	}

	map = tplg_object_get_map(tplg_pp, parent);
	if (!map) {
		SNDERR("Parent object %s not supported\n", parent_id);
		return -EINVAL;
	}

	/* find parent config with ID */
	ret = snd_config_search(tplg_pp->output_cfg, map->section_name, &section_cfg);
	if (ret < 0) {
		SNDERR("No SectionBE found\n");
		return ret;
	}

	top = tplg_find_config(section_cfg, parent_id);
	if (!top) {
		SNDERR("SectionBE %s not found\n", parent_id);
		return -EINVAL;
	}

	return tplg_add_object_data(tplg_pp, obj_cfg, top, id);
}

/*
 * Function to create a new "section" config based on the template. The new config will be
 * added to the output_cfg or the top_config input parameter.
 */
int tplg_build_object_from_template(struct tplg_pre_processor *tplg_pp, snd_config_t *obj_cfg,
				    snd_config_t **wtop, snd_config_t *top_config,
				    bool skip_name)
{
	snd_config_t *top, *template, *obj;
	const struct build_function_map *map;
	const char *object_name;
	int ret;

	/* look up object map */
	map = tplg_object_get_map(tplg_pp, obj_cfg);
	if (!map) {
		SNDERR("unknown object type or class name\n");
		return -EINVAL;
	}

	obj = tplg_object_get_instance_config(tplg_pp, obj_cfg);

	/* look up or create the corresponding section config for object */
	if (!top_config)
		top_config = tplg_pp->output_cfg;

	ret = snd_config_search(top_config, map->section_name, &top);
	if (ret < 0) {
		ret = tplg_config_make_add(&top, map->section_name, SND_CONFIG_TYPE_COMPOUND,
					   top_config);
		if (ret < 0) {
			SNDERR("Error creating %s config\n", map->section_name);
			return ret;
		}
	}

	/* get object name */
	object_name = tplg_object_get_name(tplg_pp, obj);
	if (!object_name) {
		ret = snd_config_get_id(obj, &object_name);
		if (ret < 0) {
			SNDERR("Invalid ID for %s\n", map->section_name);
			return ret;
		}
	}

	tplg_pp_debug("Building object: '%s' ...", object_name);

	/* create and add new object config with name, if needed */
	if (skip_name) {
		*wtop = top;
	} else {
		*wtop = tplg_find_config(top, object_name);
		if (*wtop)
			goto template;

		ret = tplg_config_make_add(wtop, object_name, SND_CONFIG_TYPE_COMPOUND,
					   top);
		if (ret < 0) {
			SNDERR("Error creating config for %s\n", object_name);
			return ret;
		}
	}

template:
	/* create template config */
	if (!map->template_items)
		return 0;

	ret = tplg_create_config_template(tplg_pp, &template, map->template_items);
	if (ret < 0) {
		SNDERR("Error creating template config for %s\n", object_name);
		return ret;
	}

	/* update section config based on template and the attribute values in the object */
	ret = tplg_object_add_attributes(*wtop, template, obj);
	snd_config_delete(template);
	if (ret < 0)
		SNDERR("Error adding attributes for object '%s'\n", object_name);

	return ret;
}

static int tplg_build_generic_object(struct tplg_pre_processor *tplg_pp, snd_config_t *obj_cfg,
				     snd_config_t *parent)
{
	snd_config_t *wtop;
	const char *name;
	int ret;

	ret = tplg_build_object_from_template(tplg_pp, obj_cfg, &wtop, NULL, false);
	if (ret < 0)
		return ret;

	ret = snd_config_get_id(wtop, &name);
	if (ret < 0)
		return ret;

	ret = tplg_add_object_data(tplg_pp, obj_cfg, wtop, NULL);
	if (ret < 0)
		SNDERR("Failed to add data section for %s\n", name);

	return ret;
}

const struct config_template_items pcm_caps_config = {
	.int_config_ids = {"rate_min", "rate_max", "channels_min", "channels_max", "periods_min",
			   "periods_max", "period_size_min", "period_size_max", "buffer_size_min",
			   "buffer_size_max", "sig_bits"},
	.string_config_ids = {"formats", "rates"},
};

const struct config_template_items fe_dai_config = {
	.int_config_ids = {"id"},
};

const struct config_template_items hwcfg_config = {
	.int_config_ids = {"id", "bclk_freq", "bclk_invert", "fsync_invert", "fsync_freq",
			   "mclk_freq", "pm_gate_clocks", "tdm_slots", "tdm_slot_width",
			   "tx_slots", "rx_slots", "tx_channels", "rx_channels"},
	.string_config_ids = {"format", "bclk", "fsync", "mclk"},
};

const struct config_template_items be_dai_config = {
	.int_config_ids = {"id", "default_hw_conf_id", "symmertic_rates", "symmetric_channels",
			   "symmetric_sample_bits"},
	.string_config_ids = {"stream_name"},
};

const struct config_template_items pcm_config = {
	.int_config_ids = {"id", "compress", "symmertic_rates", "symmetric_channels",
			   "symmetric_sample_bits"},
};

const struct config_template_items mixer_control_config = {
	.int_config_ids = {"index", "max", "invert"},
	.compound_config_ids = {"access"}
};

const struct config_template_items bytes_control_config = {
	.int_config_ids = {"index", "base", "num_regs", "max", "mask"},
	.compound_config_ids = {"access"}
};

const struct config_template_items scale_config = {
	.int_config_ids = {"min", "step", "mute"},
};

const struct config_template_items ops_config = {
	.int_config_ids = {"get", "put"},
	.string_config_ids = {"info"},
};

const struct config_template_items channel_config = {
	.int_config_ids = {"reg", "shift"},
};

const struct config_template_items widget_config = {
	.int_config_ids = {"index", "no_pm", "shift", "invert", "subseq", "event_type",
			    "event_flags"},
	.string_config_ids = {"type", "stream_name"},
};

const struct config_template_items data_config = {
	.string_config_ids = {"bytes"}
};

/*
 * Items without class name should be placed lower than those with one,
 * because they are much more generic.
 */
const struct build_function_map object_build_map[] = {
	{"Base", "manifest", "SectionManifest", &tplg_build_generic_object, NULL, NULL},
	{"Base", "data", "SectionData", &tplg_build_data_object, NULL, &data_config},
	{"Base", "tlv", "SectionTLV", &tplg_build_tlv_object, NULL, NULL},
	{"Base", "scale", "scale", &tplg_build_scale_object, NULL, &scale_config},
	{"Base", "ops", "ops" ,&tplg_build_ops_object, NULL, &ops_config},
	{"Base", "extops", "extops" ,&tplg_build_ops_object, NULL, &ops_config},
	{"Base", "channel", "channel", &tplg_build_channel_object, NULL, &channel_config},
	{"Base", "VendorToken", "SectionVendorTokens", &tplg_build_vendor_token_object,
	 NULL, NULL},
	{"Base", "hw_config", "SectionHWConfig", &tplg_build_hw_cfg_object, NULL,
	 &hwcfg_config},
	{"Base", "fe_dai", "dai", &tplg_build_fe_dai_object, NULL, &fe_dai_config},
	{"Base", "route", "SectionGraph", &tplg_build_dapm_route_object, NULL, NULL},
	{"Widget", "buffer", "SectionWidget", &tplg_build_generic_object, NULL, &widget_config},
	{"Widget", "", "SectionWidget", &tplg_build_generic_object, NULL, &widget_config},
	{"Control", "mixer", "SectionControlMixer", &tplg_build_mixer_control, NULL,
	 &mixer_control_config},
	{"Control", "bytes", "SectionControlBytes", &tplg_build_bytes_control, NULL,
	 &bytes_control_config},
	{"Dai", "", "SectionBE", &tplg_build_generic_object, NULL, &be_dai_config},
	{"PCM", "pcm", "SectionPCM", &tplg_build_generic_object, NULL, &pcm_config},
	{"PCM", "pcm_caps", "SectionPCMCapabilities", &tplg_build_pcm_caps_object,
	 NULL, &pcm_caps_config},
};

static const struct build_function_map *tplg_object_get_map(struct tplg_pre_processor *tplg_pp,
							    snd_config_t *obj)
{
	snd_config_iterator_t first;
	snd_config_t *class;
	const char *class_type, *class_name;
	unsigned int i;

	first = snd_config_iterator_first(obj);
	class = snd_config_iterator_entry(first);

	if (snd_config_get_id(class, &class_name) < 0)
		return NULL;

	if (snd_config_get_id(obj, &class_type) < 0)
		return NULL;

	for (i = 0; i < ARRAY_SIZE(object_build_map); i++) {
		if (!strcmp(class_type, "Widget") &&
		    !strcmp(object_build_map[i].class_type, "Widget") &&
			!strcmp(object_build_map[i].class_name, ""))
			return &object_build_map[i];

		if (!strcmp(class_type, "Dai") &&
		    !strcmp(object_build_map[i].class_type, "Dai"))
			return &object_build_map[i];

		/* for other type objects, also match the object class_name */
		if (!strcmp(class_type, object_build_map[i].class_type) &&
		    !strcmp(object_build_map[i].class_name, class_name))
			return &object_build_map[i];
	}

	return NULL;
}

/* search for section name based on class type and name and return the config in output_cfg */
snd_config_t *tplg_object_get_section(struct tplg_pre_processor *tplg_pp, snd_config_t *class)
{
	const struct build_function_map *map;
	snd_config_t *cfg = NULL;
	int ret;

	map = tplg_object_get_map(tplg_pp, class);
	if (!map)
		return NULL;

	ret = snd_config_search(tplg_pp->output_cfg, map->section_name, &cfg);
	if (ret < 0)
		SNDERR("Section config for %s not found\n", map->section_name);

	return cfg;
}

/* return 1 if attribute not found in search_config, 0 on success and negative value on error */
static int tplg_object_copy_and_add_param(struct tplg_pre_processor *tplg_pp,
					  snd_config_t *obj,
					  snd_config_t *attr_cfg,
					  snd_config_t *search_config)
{
	snd_config_iterator_t first = snd_config_iterator_first(obj);
	snd_config_t *attr, *new, *first_cfg;
	const char *id, *search_id;
	int ret;

	first_cfg = snd_config_iterator_entry(first);

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

	if (first_cfg) {
		/* prepend the new config */
		ret = snd_config_add_before(first_cfg, new);
		if (ret < 0) {
			snd_config_delete(new);
			SNDERR("error prepending attribute '%s' value to %s\n", id, search_id);
		}
	} else {
		ret = snd_config_add(obj, new);
		if (ret < 0) {
			snd_config_delete(new);
			SNDERR("error adding attribute '%s' value to %s\n", id, search_id);
		}
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
		continue;
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
		continue;
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
		continue;
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
		continue;
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
		continue;
check:
		if (tplg_class_is_attribute_mandatory(id, class_cfg)) {
			SNDERR("Mandatory attribute %s not set for class %s\n", id, class_name);
			return -EINVAL;
		}
	}

	return 0;
}

static int tplg_object_pre_process_children(struct tplg_pre_processor *tplg_pp,
					    snd_config_t *parent, snd_config_t *cfg)
{
	snd_config_iterator_t i, next;
	snd_config_t *children, *n;
	int ret;

	ret = snd_config_search(cfg, "Object", &children);
	if (ret < 0)
		return 0;

	/* create all embedded objects */
	snd_config_for_each(i, next, children) {
		const char *id;

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		ret = tplg_pre_process_objects(tplg_pp, n, parent);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int tplg_construct_object_name(struct tplg_pre_processor *tplg_pp, snd_config_t *obj,
				      snd_config_t *class_cfg)
{
	snd_config_iterator_t i, next;
	snd_config_t *args, *n;
	const char *id, *class_id, *obj_id, *s;
	char *new_name;
	int ret;

	/* find config for class constructor attributes. Nothing to do if not defined */
	ret = snd_config_search(class_cfg, "attributes.constructor", &args);
	if (ret < 0)
		return 0;

	/* set class name as the name prefix for the object */
	if (snd_config_get_id(obj, &obj_id) < 0)
		return -EINVAL;
	if (snd_config_get_id(class_cfg, &class_id) < 0)
		return -EINVAL;
	new_name = strdup(class_id);
	if (!new_name)
		return -ENOMEM;

	/* iterate through all class arguments and set object name */
	snd_config_for_each(i, next, args) {
		snd_config_t *arg;
		char *arg_value, *temp;

		n = snd_config_iterator_entry(i);

		if (snd_config_get_id(n, &id) < 0) {
			SNDERR("Invalid ID for constructor argument\n");
			ret = -EINVAL;
			goto err;
		}

		if (snd_config_get_string(n, &s) < 0) {
			SNDERR("Invalid value for constructor argument\n");
			ret = -EINVAL;
			goto err;
		}

		/* find and replace with value set in object */
		ret = snd_config_search(obj, s, &arg);
		if (ret < 0) {
			SNDERR("Argument %s not set for object '%s.%s'\n", s, class_id, obj_id);
			ret = -ENOENT;
			goto err;
		}

		/* concat arg value to object name. arg types must be either integer or string */
		switch (snd_config_get_type(arg)) {
		case SND_CONFIG_TYPE_INTEGER:
		{
			long v;
			ret = snd_config_get_integer(arg, &v);
			assert(ret >= 0);

			arg_value = tplg_snprintf("%ld", v);
			if (!arg_value) {
				ret = -ENOMEM;
				goto err;
			}
			break;
		}
		case SND_CONFIG_TYPE_STRING:
		{
			const char *s;

			ret = snd_config_get_string(arg, &s);
			assert(ret >= 0);

			arg_value = strdup(s);
			if (!arg_value) {
				ret = -ENOMEM;
				goto err;
			}
			break;
		}
		default:
			SNDERR("Argument '%s' in object '%s.%s' is not an integer or a string\n",
			       s, class_id, obj_id);
			ret = -EINVAL;
			goto err;
		}

		/* alloc and concat arg value to the name */
		temp = tplg_snprintf("%s.%s", new_name, arg_value);
		free(arg_value);
		if (!temp) {
			ret = -ENOMEM;
			goto err;
		}
		free(new_name);
		new_name = temp;
	}

	ret = snd_config_set_id(obj, new_name);
err:
	free(new_name);
	return ret;
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

#if SND_LIB_VER(1, 2, 5) < SND_LIB_VERSION
static int pre_process_find_variable(snd_config_t **dst, const char *str, snd_config_t *config)
{
	snd_config_iterator_t i, next;

	snd_config_for_each(i, next, config) {
		snd_config_t *n;
		const char *id;

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (strcmp(id, str))
			continue;

		/* found definition, copy config */
		return snd_config_copy(dst, n);
	}

	return -EINVAL;
}

static int
pre_process_object_variables_expand_fcn(snd_config_t **dst, const char *str, void *private_data)
{

	struct tplg_pre_processor *tplg_pp = private_data;
	snd_config_t *object_cfg = tplg_pp->current_obj_cfg;
	snd_config_t *conf_defines;
	const char *object_id;
	int ret;

	ret = snd_config_search(tplg_pp->input_cfg, "Define", &conf_defines);
	if (ret < 0)
		return 0;

	/* find variable from global definitions first */
	ret = pre_process_find_variable(dst, str, conf_defines);
	if (ret >= 0)
		return ret;

	if (snd_config_get_id(object_cfg, &object_id) < 0)
		return -EINVAL;

	/* find variable from object attribute values if not found in global definitions */
	ret = pre_process_find_variable(dst, str, object_cfg);
	if (ret < 0)
		SNDERR("Failed to find definition for attribute %s in '%s' object\n",
		       str, object_id);

	return ret;
}

/*
 * Searches for the first '$VAR_NAME' or '$[<contents>]' occurrence in
 * *stringp. Allocates memory for it and copies it there. The
 * allocated string is returned in '*varname'. If there was a prefix
 * before $VAR_NAME, it is returned in '*prefix'. The *stringp is
 * moved forward to the char after the $VAR_NAME.
 *
 * The end of $VAR_NAME is the first char that is not alpha numeric, '_',
 * or '\0'.
 *
 * In '$[<contents>]' case all letters but '[' and ']' are allow in
 * any sequence. Nested '[]' is also allowed if the number if '[' and
 * ']' match.
 *
 * The function modifies *stringp, and *prefix - if not NULL - points
 * to the original *stringp, *varname - if not NULL - is malloced and
 * should be freed by the caller.
 *
 * Returns 0		if the *stringp was an empty string.
 *         1		if *prefix or *varname was set
 *         -ENOMEM	if malloc failed
 */
static int tplg_get_varname(char **stringp, char **prefix, char **varname)
{
	size_t prefix_len, varname_len = 0;

	*prefix = NULL;
	*varname = NULL;

	prefix_len = strcspn(*stringp, "$");
	*prefix = *stringp;
	(*stringp) += prefix_len;
	if (**stringp == '$') {
		if ((*stringp)[1] == '[') {
			int brackets = 1;
			varname_len = 1;

			do {
				varname_len += strcspn((*stringp) + varname_len + 1, "[]") + 1;
				if ((*stringp)[varname_len] == '[')
					brackets++;
				else if ((*stringp)[varname_len] == ']')
					brackets--;
				else
					break;
			}  while (brackets > 0);
			if (brackets != 0)
				return -EINVAL;
			varname_len++;
		} else {
			varname_len = 1;
			while (isalnum((*stringp)[varname_len]) || (*stringp)[varname_len] == '_')
				varname_len++;
		}
	}

	if (varname_len == 0 && prefix_len == 0)
		return 0;

	if (varname_len) {
		*varname = malloc(varname_len + 1);
		if (*varname == NULL)
			return -ENOMEM;
		strncpy(*varname, *stringp, varname_len);
		(*varname)[varname_len] = '\0';
		(*stringp) += varname_len;
	}

	if (prefix_len)
		(*prefix)[prefix_len] = '\0';
	else
		*prefix = NULL;

	return 1;
}

static int tplg_evaluate_config_string(struct tplg_pre_processor *tplg_pp,
				    snd_config_t **dst, const char *s, const char *id)
{
	char *str = strdup(s);
	char *varname, *prefix, *freep = str;
	int ret;

	if (!str)
		return -ENOMEM;

	*dst = NULL;

	/* split the string and expand global definitions or object attribute values */
	while (tplg_get_varname(&str, &prefix, &varname) == 1) {
		const char *current_str;
		char *temp;

		if (prefix) {
			if (*dst == NULL) {
				ret = snd_config_make(dst, id, SND_CONFIG_TYPE_STRING);
				if (ret < 0)
					goto out;
				ret = snd_config_set_string(*dst, prefix);
				if (ret < 0)
					goto out;
			} else {
				/* concat the prefix */
				snd_config_get_string(*dst, &current_str);
				temp = tplg_snprintf("%s%s", current_str, prefix);
				if (!temp) {
					ret = -ENOMEM;
					goto out;
				}

				ret = snd_config_set_string(*dst, temp);
				free(temp);
				if (ret < 0)
					goto out;
			}
		}

		if (varname) {
			snd_config_t *tmp_config;

			ret = snd_config_evaluate_string(&tmp_config, varname,
							 pre_process_object_variables_expand_fcn,
							 tplg_pp);
			if (ret < 0)
				goto out;

			if (*dst == NULL) {
				*dst = tmp_config;
			} else {
				char *ascii;

				snd_config_get_string(*dst, &current_str);

				ret = snd_config_get_ascii(tmp_config, &ascii);
				if (ret)
					goto out;

				temp = tplg_snprintf("%s%s", current_str, ascii);
				free(ascii);

				if (!temp) {
					ret = -ENOMEM;
					goto out;
				}

				ret = snd_config_set_string(*dst, temp);
				free(temp);
				snd_config_delete(tmp_config);
				if (ret < 0)
					goto out;
			}
			free(varname);
		}
	}

	free(freep);
	snd_config_set_id(*dst, id);

	return 0;
out:
	if (*dst)
		snd_config_delete(*dst);
	free(varname);
	free(freep);
	return ret;
}

#endif

/* build object config and its child objects recursively */
static int tplg_build_object(struct tplg_pre_processor *tplg_pp, snd_config_t *new_obj,
			      snd_config_t *parent)
{
	snd_config_t *obj_local, *class_cfg;
	const struct build_function_map *map;
	snd_config_iterator_t i, next;
	build_func builder;
	update_auto_attr_func auto_attr_updater;
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
	if (ret < 0) {
		SNDERR("Failed to update attributes for object '%s.%s'\n", class_id, id);
		return ret;
	}

#if SND_LIB_VER(1, 2, 5) < SND_LIB_VERSION
	tplg_pp_config_debug(tplg_pp, obj_local);

	/* expand all non-compound type child configs in object */
	snd_config_for_each(i, next, obj_local) {
		snd_config_t *n, *new, *class_attr;
		const char *id, *s;
		char *attr_config_name;

		n = snd_config_iterator_entry(i);

		if (snd_config_get_type(n) == SND_CONFIG_TYPE_COMPOUND)
			continue;

		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (snd_config_get_string(n, &s) < 0)
			continue;

		if (!strstr(s, "$"))
			goto validate;

		tplg_pp->current_obj_cfg = obj_local;

		/* Expand definitions and object attribute references. */
		ret = tplg_evaluate_config_string(tplg_pp, &new, s, id);
		if (ret < 0) {
			SNDERR("Failed to evaluate attributes %s in %s, from '%s'\n",
			       id, class_id, s);
			return ret;
		}

		ret = snd_config_merge(n, new, true);
		if (ret < 0)
			return ret;
validate:
		/* validate attribute value */
		snd_config_get_id(n, &id);
		attr_config_name = tplg_snprintf("DefineAttribute.%s", id);
		if (!attr_config_name)
			return -ENOMEM;

		ret = snd_config_search(class_cfg, attr_config_name, &class_attr);
		free(attr_config_name);
		if (ret < 0)
			continue;

		if (!tplg_object_is_attribute_valid(tplg_pp, class_attr, n)) {
			SNDERR("Failed to validate attribute %s in %s\n", id, class_id);
			return -EINVAL;
		}
	}
#endif

	/* construct object name using class constructor */
	ret = tplg_construct_object_name(tplg_pp, obj_local, class_cfg);
	if (ret < 0) {
		SNDERR("Failed to construct object name for %s\n", id);
		return ret;
	}

	/*
	 * Build objects if object type is supported.
	 * If not, process object attributes and add to parent's data section
	 */
	map = tplg_object_get_map(tplg_pp, new_obj);
	if (map) {
		builder = map->builder;

		/* update automatic attribute for current object */
		auto_attr_updater = map->auto_attr_updater;
		if(auto_attr_updater) {
			ret = auto_attr_updater(tplg_pp, obj_local, parent);
			if (ret < 0) {
				SNDERR("Failed to update automatic attributes for %s\n", id);
				return ret;
			}
		}
	} else {
		builder = &tplg_build_parent_data;
	}

	ret = builder(tplg_pp, new_obj, parent);
	if (ret < 0)
		return ret;

	/* create child objects in the object instance */
	ret = tplg_object_pre_process_children(tplg_pp, new_obj, obj_local);
	if (ret < 0) {
		SNDERR("error processing child objects in object %s\n", id);
		return ret;
	}

	/* create child objects in the object's class definition */
	ret = tplg_object_pre_process_children(tplg_pp, new_obj, class_cfg);
	if (ret < 0)
		SNDERR("error processing child objects in class %s\n", class_id);

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
			snd_config_t *temp_n2;

			n2 = snd_config_iterator_entry(i2);
			if (snd_config_get_id(n2, &id) < 0) {
				SNDERR("Invalid id for object\n");
				return -EINVAL;
			}

			ret = snd_config_copy(&temp_n2, n2);
			if (ret < 0)
				return ret;

			/*
			 * An object declared within a class definition as follows:
			 * Class.Pipeline.volume-playback {
			 * 	Object.Widget.pga.0 {
			 * 		ramp_step_ms 250
            		 * 	}
			 * }
			 *
			 * While instantiating the volume-pipeline class, the pga object
			 * could be modified as follows:
			 * Object.Pipeline.volume-playback.0 {
			 * 	Object.Widget.pga.0 {
			 * 		format "s24le"
			 * 	}
			 * }
			 * When building the pga.0 object in the class definition, merge
			 * the attributes declared in the volume-playback.0 object to create
			 * a new config as follows to make sure that all attributes are
			 * set for the pga object.
			 * Object.Widget.pga.0 {
			 * 	ramp_step_ms 250
			 * 	format "s24le"
			 * }
			 */

			if (parent) {
				snd_config_t *parent_instance, *parent_obj, *temp;
				char *obj_cfg_name;

				obj_cfg_name = tplg_snprintf("%s%s.%s.%s", "Object.",
							     class_type, class_name, id);

				/* search for object instance in the parent */
				parent_instance = tplg_object_get_instance_config(tplg_pp, parent);
				if (!parent_instance)
					goto temp_cfg;

				ret = snd_config_search(parent_instance, obj_cfg_name, &parent_obj);
				free(obj_cfg_name);
				if (ret < 0)
					goto temp_cfg;

				/* don't merge if the object configs are the same */
				if (parent_obj == n2)
					goto temp_cfg;

				/* create a temp config copying the parent object config */
				ret = snd_config_copy(&temp, parent_obj);
				if (ret < 0) {
					snd_config_delete(temp_n2);
					return ret;
				}

				/*
				 * Merge parent object with the current object instance.
				 * temp will be deleted by merge
				 */
				ret = snd_config_merge(temp_n2, temp, false);
				if (ret < 0) {
					SNDERR("error merging parent object config for %s.%s.%s\n",
					       class_type, class_name, id);
					snd_config_delete(temp_n2);
					return ret;
				}
			}
temp_cfg:
			/* create a temp config for object with class type as the root node */
			ret = snd_config_make(&_obj_type, class_type, SND_CONFIG_TYPE_COMPOUND);
			if (ret < 0) {
				snd_config_delete(temp_n2);
				return ret;
			}

			ret = snd_config_make(&_obj_class, class_name, SND_CONFIG_TYPE_COMPOUND);
			if (ret < 0)
				goto err;

			ret = snd_config_add(_obj_type, _obj_class);
			if (ret < 0) {
				snd_config_delete(_obj_class);
				goto err;
			}

			ret = snd_config_copy(&_obj, temp_n2);
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
			snd_config_delete(temp_n2);
			snd_config_delete(_obj_type);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}
