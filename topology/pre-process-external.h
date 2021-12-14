/*
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation; either version 2.1 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
#ifndef __PRE_PROCESS_EXTERNAL_H
#define __PRE_PROCESS_EXTERNAL_H

#define PROCESS_FUNC_PREFIX "_snd_topology_"
#define PROCESS_FUNC_POSTFIX "_process"
#define PROCESS_LIB_PREFIX "libalsatplg_module_"
#define PROCESS_LIB_POSTFIX ".so"

/**
 * Define the object entry for external pre-process plugins
 */
#define SND_TOPOLOGY_PLUGIN_ENTRY(name) _snd_topology_##name##_process

/**
 * Define the plugin
 */
#define SND_TOPOLOGY_PLUGIN_DEFINE_FUNC(plugin) \
	__attribute__ ((visibility ("default"))) \
	int SND_TOPOLOGY_PLUGIN_ENTRY(plugin) (snd_config_t *input, snd_config_t *output)

typedef int (*plugin_pre_process)(snd_config_t *input, snd_config_t *output);

#endif /* __PRE_PROCESS_EXTERNAL_H */
