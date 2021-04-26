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
#include <alsa/input.h>
#include <alsa/output.h>
#include <alsa/conf.h>
#include <alsa/error.h>
#include "topology.h"
#include "pre-processor.h"

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
