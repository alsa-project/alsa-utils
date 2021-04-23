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
#include "topology.h"

#define DEBUG_MAX_LENGTH	256

/* debug helpers */
void tplg_pp_debug(char *fmt, ...);
void tplg_pp_config_debug(struct tplg_pre_processor *tplg_pp, snd_config_t *cfg);

/* class helpers */
snd_config_t *tplg_class_lookup(struct tplg_pre_processor *tplg_pp, snd_config_t *cfg);

/* config helpers */
snd_config_t *tplg_find_config(snd_config_t *config, const char *name);
int tplg_config_make_add(snd_config_t **config, const char *id, snd_config_type_t type,
			 snd_config_t *parent);

char *tplg_snprintf(char *fmt, ...);
#endif
