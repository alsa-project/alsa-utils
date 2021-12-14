/*
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __PRE_PROCESSOR_H
#define __PRE_PROCESSOR_H

#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include "topology.h"

#define DEBUG_MAX_LENGTH	256
#define ARRAY_SIZE(a) (sizeof (a) / sizeof (a)[0])

#define MAX_CONFIGS_IN_TEMPLATE	32

struct config_template_items {
	char *int_config_ids[MAX_CONFIGS_IN_TEMPLATE];
	char *string_config_ids[MAX_CONFIGS_IN_TEMPLATE];
	char *compound_config_ids[MAX_CONFIGS_IN_TEMPLATE];
};

typedef int (*build_func)(struct tplg_pre_processor *tplg_pp, snd_config_t *obj,
			  snd_config_t *parent);

typedef int (*update_auto_attr_func)(struct tplg_pre_processor *tplg_pp,
			  snd_config_t *obj, snd_config_t *parent);

struct build_function_map {
	char *class_type;
	char *class_name;
	char *section_name;
	build_func builder;
	update_auto_attr_func auto_attr_updater;
	const struct config_template_items *template_items;
};

extern const struct build_function_map object_build_map[];

/* debug helpers */
void tplg_pp_debug(char *fmt, ...);
void tplg_pp_config_debug(struct tplg_pre_processor *tplg_pp, snd_config_t *cfg);

/* object build helpers */
int tplg_build_object_from_template(struct tplg_pre_processor *tplg_pp, snd_config_t *obj_cfg,
				    snd_config_t **wtop, snd_config_t *top_config,
				    bool skip_name);
int tplg_build_tlv_object(struct tplg_pre_processor *tplg_pp, snd_config_t *obj_cfg,
			      snd_config_t *parent);
int tplg_build_scale_object(struct tplg_pre_processor *tplg_pp, snd_config_t *obj_cfg,
			      snd_config_t *parent);
int tplg_build_ops_object(struct tplg_pre_processor *tplg_pp, snd_config_t *obj_cfg,
			      snd_config_t *parent);
int tplg_build_channel_object(struct tplg_pre_processor *tplg_pp, snd_config_t *obj_cfg,
			      snd_config_t *parent);
int tplg_build_mixer_control(struct tplg_pre_processor *tplg_pp, snd_config_t *obj_cfg,
			      snd_config_t *parent);
int tplg_build_bytes_control(struct tplg_pre_processor *tplg_pp, snd_config_t *obj_cfg,
			      snd_config_t *parent);
int tplg_build_dapm_route_object(struct tplg_pre_processor *tplg_pp, snd_config_t *obj_cfg,
			      snd_config_t *parent);
int tplg_build_hw_cfg_object(struct tplg_pre_processor *tplg_pp,
			       snd_config_t *obj_cfg, snd_config_t *parent);
int tplg_build_fe_dai_object(struct tplg_pre_processor *tplg_pp, snd_config_t *obj_cfg,
			      snd_config_t *parent);
int tplg_build_base_object(struct tplg_pre_processor *tplg_pp, snd_config_t *obj_cfg,
			   snd_config_t *parent, bool skip_name);
int tplg_build_pcm_caps_object(struct tplg_pre_processor *tplg_pp,
			       snd_config_t *obj_cfg, snd_config_t *parent);
int tplg_parent_update(struct tplg_pre_processor *tplg_pp, snd_config_t *parent,
			  const char *section_name, const char *item_name);
int tplg_add_object_data(struct tplg_pre_processor *tplg_pp, snd_config_t *obj_cfg,
			 snd_config_t *top, const char *array_name);

/* object helpers */
int tplg_pre_process_objects(struct tplg_pre_processor *tplg_pp, snd_config_t *cfg,
			     snd_config_t *parent);
snd_config_t *tplg_object_get_instance_config(struct tplg_pre_processor *tplg_pp,
					snd_config_t *class_type);
const char *tplg_object_get_name(struct tplg_pre_processor *tplg_pp,
				 snd_config_t *object);
snd_config_t *tplg_object_get_section(struct tplg_pre_processor *tplg_pp, snd_config_t *class);

/* class helpers */
snd_config_t *tplg_class_lookup(struct tplg_pre_processor *tplg_pp, snd_config_t *cfg);
snd_config_t *tplg_class_find_attribute_by_name(struct tplg_pre_processor *tplg_pp,
						snd_config_t *class, const char *name);
bool tplg_class_is_attribute_mandatory(const char *attr, snd_config_t *class_cfg);
bool tplg_class_is_attribute_immutable(const char *attr, snd_config_t *class_cfg);
bool tplg_class_is_attribute_unique(const char *attr, snd_config_t *class_cfg);
const char *tplg_class_get_unique_attribute_name(struct tplg_pre_processor *tplg_pp,
						 snd_config_t *class);
snd_config_type_t tplg_class_get_attribute_type(struct tplg_pre_processor *tplg_pp,
						snd_config_t *attr);
const char *tplg_class_get_attribute_token_ref(struct tplg_pre_processor *tplg_pp,
					       snd_config_t *class, const char *attr_name);
long tplg_class_attribute_valid_tuple_value(struct tplg_pre_processor *tplg_pp,
					    snd_config_t *class, snd_config_t *attr);

/* config helpers */
snd_config_t *tplg_find_config(snd_config_t *config, const char *name);
int tplg_config_make_add(snd_config_t **config, const char *id, snd_config_type_t type,
			 snd_config_t *parent);

char *tplg_snprintf(char *fmt, ...);
#endif
