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
#include <limits.h>
#include <stdio.h>
#include <alsa/input.h>
#include <alsa/output.h>
#include <alsa/conf.h>
#include <alsa/error.h>
#include "topology.h"
#include "pre-processor.h"

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
