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

#ifndef __TOPOLOGY_H
#define __TOPOLOGY_H

#include <stdlib.h>

/* pre_processor */
struct tplg_pre_processor {
	snd_config_t *input_cfg;
	snd_config_t *output_cfg;
	snd_output_t *output;
	snd_output_t *dbg_output;
};

int pre_process(struct tplg_pre_processor *tplg_pp, char *config, size_t config_size,
		const char *pre_processor_defs);
int init_pre_precessor(struct tplg_pre_processor **tplg_pp, snd_output_type_t type,
		       const char *output_file);
void free_pre_preprocessor(struct tplg_pre_processor *tplg_pp);
#endif
