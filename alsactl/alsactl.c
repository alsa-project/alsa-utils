/*
 *  Advanced Linux Sound Architecture Control Program
 *  Copyright (c) by Abramo Bagnara <abramo@alsa-project.org>
 *                   Jaroslav Kysela <perex@suse.cz>
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "aconfig.h"
#include "version.h"
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <sys/asoundlib.h>

#define SYS_ASOUNDRC "/etc/asound.conf"

int debugflag = 0;
char *command;

#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95)
#define error(...) do {\
	fprintf(stderr, "%s: %s:%d: ", command, __FUNCTION__, __LINE__); \
	fprintf(stderr, __VA_ARGS__); \
	putc('\n', stderr); \
} while (0)
#else
#define error(args...) do {\
	fprintf(stderr, "%s: %s:%d: ", command, __FUNCTION__, __LINE__); \
	fprintf(stderr, ##args); \
	putc('\n', stderr); \
} while (0)
#endif	

static void help(void)
{
	printf("Usage: alsactl <options> command\n");
	printf("\nAvailable options:\n");
	printf("  -h,--help       this help\n");
	printf("  -f,--file #     configuration file (default " SYS_ASOUNDRC ")\n");
	printf("  -d,--debug      debug mode\n");
	printf("  -v,--version    print version of this program\n");
	printf("\nAvailable commands:\n");
	printf("  store <card #>  save current driver setup for one or each soundcards\n");
	printf("                  to configuration file\n");
	printf("  restore<card #> load current driver setup for one or each soundcards\n");
	printf("                  from configuration file\n");
}

char *id_str(snd_control_id_t *id)
{
	static char str[128];
	assert(id);
	sprintf(str, "%i,%i,%i,%s,%i", id->iface, id->device, id->subdevice, id->name, id->index);
	return str;
}

char *num_str(long n)
{
	static char str[32];
	sprintf(str, "%ld", n);
	return str;
}

static int snd_config_integer_add(snd_config_t *father, char *id, long integer)
{
	int err;
	snd_config_t *leaf;
	err = snd_config_integer_make(&leaf, id);
	if (err < 0)
		return err;
	err = snd_config_add(father, leaf);
	if (err < 0) {
		snd_config_delete(leaf);
		return err;
	}
	err = snd_config_integer_set(leaf, integer);
	if (err < 0) {
		snd_config_delete(leaf);
		return err;
	}
	return 0;
}

static int snd_config_string_add(snd_config_t *father, char *id, char *string)
{
	int err;
	snd_config_t *leaf;
	err = snd_config_string_make(&leaf, id);
	if (err < 0)
		return err;
	err = snd_config_add(father, leaf);
	if (err < 0) {
		snd_config_delete(leaf);
		return err;
	}
	err = snd_config_string_set(leaf, string);
	if (err < 0) {
		snd_config_delete(leaf);
		return err;
	}
	return 0;
}

static int snd_config_compound_add(snd_config_t *father, char *id, int join,
				   snd_config_t **node)
{
	int err;
	snd_config_t *leaf;
	err = snd_config_compound_make(&leaf, id, join);
	if (err < 0)
		return err;
	err = snd_config_add(father, leaf);
	if (err < 0) {
		snd_config_delete(leaf);
		return err;
	}
	*node = leaf;
	return 0;
}

static int get_control(snd_ctl_t *handle, snd_control_id_t *id, snd_config_t *top)
{
	snd_control_t ctl;
	snd_control_info_t info;
	snd_config_t *control, *comment, *item, *value;
	char *s;
	char buf[256];
	int idx, err;

	memset(&info, 0, sizeof(info));
	info.id = *id;
	err = snd_ctl_cinfo(handle, &info);
	if (err < 0) {
		error("Cannot read control info '%s': %s", id_str(id), snd_strerror(err));
		return err;
	}

	if (!(info.access & SND_CONTROL_ACCESS_READ))
		return 0;
	memset(&ctl, 0, sizeof(ctl));
	ctl.id = info.id;
	err = snd_ctl_cread(handle, &ctl);
	if (err < 0) {
		error("Cannot read control '%s': %s", id_str(id), snd_strerror(err));
		return err;
	}

	err = snd_config_compound_add(top, num_str(info.id.numid), 0, &control);
	if (err < 0) {
		error("snd_config_compound_add: %s", snd_strerror(err));
		return err;
	}
	err = snd_config_compound_add(control, "comment", 1, &comment);
	if (err < 0) {
		error("snd_config_compound_add: %s", snd_strerror(err));
		return err;
	}

	buf[0] = '\0';
	buf[1] = '\0';
	if (info.access & SND_CONTROL_ACCESS_READ)
		strcat(buf, " read");
	if (info.access & SND_CONTROL_ACCESS_WRITE)
		strcat(buf, " write");
	if (info.access & SND_CONTROL_ACCESS_INACTIVE)
		strcat(buf, " inactive");
	err = snd_config_string_add(comment, "access", buf + 1);
	if (err < 0) {
		error("snd_config_string_add: %s", snd_strerror(err));
		return err;
	}

	switch (info.type) {
	case SND_CONTROL_TYPE_BOOLEAN:
		s = "bool";
		break;
	case SND_CONTROL_TYPE_INTEGER:
		s = "integer";
		break;
	case SND_CONTROL_TYPE_ENUMERATED:
		s = "enumerated";
		break;
	case SND_CONTROL_TYPE_BYTES:
		s = "bytes";
		break;
	default:
		s = "unknown";
		break;
	}
	err = snd_config_string_add(comment, "type", s);
	if (err < 0) {
		error("snd_config_string_add: %s", snd_strerror(err));
		return err;
	}

	switch (info.type) {
	case SND_CONTROL_TYPE_BOOLEAN:
		if (info.value.integer.min != 0 || info.value.integer.max != 1 ||
		    info.value.integer.step != 0)
			error("Bad boolean control '%s'", id_str(id));
		
		break;
	case SND_CONTROL_TYPE_INTEGER:
		if (info.value.integer.step)
			sprintf(buf, "%li - %li (step %li)", info.value.integer.min, info.value.integer.max, info.value.integer.step);
		else
			sprintf(buf, "%li - %li", info.value.integer.min, info.value.integer.max);
		err = snd_config_string_add(comment, "range", buf);
		if (err < 0) {
			error("snd_config_string_add: %s", snd_strerror(err));
			return err;
		}
		break;
	case SND_CONTROL_TYPE_ENUMERATED:
		err = snd_config_compound_add(comment, "item", 1, &item);
		if (err < 0) {
			error("snd_config_compound_add: %s", snd_strerror(err));
			return err;
		}
		for (idx = 0; idx < info.value.enumerated.items; idx++) {
			info.value.enumerated.item = idx;
			err = snd_ctl_cinfo(handle, &info);
			if (err < 0) {
				error("snd_ctl_info: %s", snd_strerror(err));
				return err;
			}
			err = snd_config_string_add(item, num_str(idx), info.value.enumerated.name);
			if (err < 0) {
				error("snd_config_string_add: %s", snd_strerror(err));
				return err;
			}
		}
		break;
	default:
		break;
	}
	switch (info.id.iface) {
	case SND_CONTROL_IFACE_CARD:
		s = "card";
		break;
	case SND_CONTROL_IFACE_HWDEP:
		s = "hwdep";
		break;
	case SND_CONTROL_IFACE_MIXER:
		s = "mixer";
		break;
	case SND_CONTROL_IFACE_PCM:
		s = "pcm";
		break;
	case SND_CONTROL_IFACE_RAWMIDI:
		s = "rawmidi";
		break;
	case SND_CONTROL_IFACE_TIMER:
		s = "timer";
		break;
	case SND_CONTROL_IFACE_SEQUENCER:
		s = "sequencer";
		break;
	default:
		s = num_str(info.id.iface);
		break;
	}
	err = snd_config_string_add(control, "iface", s);
	if (err < 0) {
		error("snd_config_string_add: %s", snd_strerror(err));
		return err;
	}
	if (info.id.device != 0) {
		err = snd_config_integer_add(control, "device", info.id.device);
		if (err < 0) {
			error("snd_config_integer_add: %s", snd_strerror(err));
			return err;
		}
	}
	if (info.id.subdevice != 0) {
		err = snd_config_integer_add(control, "subdevice", info.id.subdevice);
		if (err < 0) {
			error("snd_config_integer_add: %s", snd_strerror(err));
			return err;
		}
	}
	err = snd_config_string_add(control, "name", info.id.name);
	if (err < 0) {
		error("snd_config_string_add: %s", snd_strerror(err));
		return err;
	}
	if (info.id.index != 0) {
		err = snd_config_integer_add(control, "index", info.id.index);
		if (err < 0) {
			error("snd_config_integer_add: %s", snd_strerror(err));
			return err;
		}
	}

	if (info.type == SND_CONTROL_TYPE_BYTES) {
		char buf[info.values_count * 2 + 1];
		char *p = buf;
		char *hex = "0123456789abcdef";
		for (idx = 0; idx < info.values_count; idx++) {
			int v = ctl.value.bytes.data[idx];
			*p++ = hex[v >> 4];
			*p++ = hex[v & 0x0f];
		}
		*p = '\0';
		err = snd_config_string_add(control, "value", buf);
		if (err < 0) {
			error("snd_config_string_add: %s", snd_strerror(err));
			return err;
		}
		return 0;
	}

	if (info.values_count == 1) {
		switch (info.type) {
		case SND_CONTROL_TYPE_BOOLEAN:
			err = snd_config_string_add(control, "value", ctl.value.integer.value[0] ? "true" : "false");
			if (err < 0) {
				error("snd_config_string_add: %s", snd_strerror(err));
				return err;
			}
			return 0;
		case SND_CONTROL_TYPE_INTEGER:
			err = snd_config_integer_add(control, "value", ctl.value.integer.value[0]);
			if (err < 0) {
				error("snd_config_integer_add: %s", snd_strerror(err));
				return err;
			}
			return 0;
		case SND_CONTROL_TYPE_ENUMERATED:
		{
			unsigned int v = ctl.value.enumerated.item[0];
			snd_config_t *c;
			err = snd_config_search(item, num_str(v), &c);
			if (err == 0) {
				err = snd_config_string_get(c, &s);
				assert(err == 0);
				err = snd_config_string_add(control, "value", s);
			} else {
				err = snd_config_integer_add(control, "value", v);
			}
			if (err < 0)
				error("snd_config add: %s", snd_strerror(err));
			return 0;
		}
		default:
			error("Unknown control type: %d\n", info.type);
			return -EINVAL;
		}
	}

	err = snd_config_compound_add(control, "value", 1, &value);
	if (err < 0) {
		error("snd_config_compound_add: %s", snd_strerror(err));
		return err;
	}

	switch (info.type) {
	case SND_CONTROL_TYPE_BOOLEAN:
		for (idx = 0; idx < info.values_count; idx++) {
			err = snd_config_string_add(value, num_str(idx), ctl.value.integer.value[idx] ? "true" : "false");
			if (err < 0) {
				error("snd_config_string_add: %s", snd_strerror(err));
				return err;
			}
		}
		break;
	case SND_CONTROL_TYPE_INTEGER:
		for (idx = 0; idx < info.values_count; idx++) {
			err = snd_config_integer_add(value, num_str(idx), ctl.value.integer.value[idx]);
			if (err < 0) {
				error("snd_config_integer_add: %s", snd_strerror(err));
				return err;
			}
		}
		break;
	case SND_CONTROL_TYPE_ENUMERATED:
		for (idx = 0; idx < info.values_count; idx++) {
			unsigned int v = ctl.value.enumerated.item[idx];
			snd_config_t *c;
			err = snd_config_search(item, num_str(v), &c);
			if (err == 0) {
				err = snd_config_string_get(c, &s);
				assert(err == 0);
				err = snd_config_string_add(value, num_str(idx), s);
			} else {
				err = snd_config_integer_add(value, num_str(idx), v);
			}
			if (err < 0) {
				error("snd_config add: %s", snd_strerror(err));
				return err;
			}
		}
		break;
	default:
		error("Unknown control type: %d\n", info.type);
		return -EINVAL;
	}
	
	return 0;
}
	
static int get_controls(int cardno, snd_config_t *top)
{
	snd_ctl_t *handle;
	snd_ctl_hw_info_t info;
	snd_config_t *state, *card, *control;
	snd_control_list_t list;
	int idx, err;

	err = snd_ctl_hw_open(&handle, NULL, cardno);
	if (err < 0) {
		error("snd_ctl_open error: %s", snd_strerror(err));
		return err;
	}
	err = snd_ctl_hw_info(handle, &info);
	if (err < 0) {
		error("snd_ctl_hw_info error: %s", snd_strerror(err));
		goto _close;
	}
	err = snd_config_search(top, "state", &state);
	if (err == 0 &&
	    snd_config_type(state) != SND_CONFIG_TYPE_COMPOUND) {
		error("config state node is not a compound");
		err = -EINVAL;
		goto _close;
	}
	if (err < 0) {
		err = snd_config_compound_add(top, "state", 1, &state);
		if (err < 0) {
			error("snd_config_compound_add: %s", snd_strerror(err));
			goto _close;
		}
	}
	err = snd_config_search(state, info.id, &card);
	if (err == 0 &&
	    snd_config_type(state) != SND_CONFIG_TYPE_COMPOUND) {
		error("config state.%s node is not a compound", info.id);
		err = -EINVAL;
		goto _close;
	}
	if (err < 0) {
		err = snd_config_compound_add(state, info.id, 0, &card);
		if (err < 0) {
			error("snd_config_compound_add: %s", snd_strerror(err));
			goto _close;
		}
	}
	err = snd_config_search(card, "control", &control);
	if (err == 0) {
		err = snd_config_delete(control);
		if (err < 0) {
			error("snd_config_delete: %s", snd_strerror(err));
			goto _close;
		}
	}
	err = snd_config_compound_add(card, "control", 1, &control);
	if (err < 0) {
		error("snd_config_compound_add: %s", snd_strerror(err));
		goto _close;
	}
	memset(&list, 0, sizeof(list));
	err = snd_ctl_clist(handle, &list);
	if (err < 0) {
		error("Cannot determine controls: %s", snd_strerror(err));
		goto _close;
	}
	if (list.controls <= 0) {
		err = 0;
		goto _close;
	}
	list.controls_request = list.controls;
	list.controls_offset = list.controls_count = 0;
	list.pids = malloc(sizeof(snd_control_id_t) * list.controls_request);
	if (!list.pids) {
		error("No enough memory...");
		goto _close;
	}
	if ((err = snd_ctl_clist(handle, &list)) < 0) {
		error("Cannot determine controls (2): %s", snd_strerror(err));
		goto _free;
	}
	for (idx = 0; idx < list.controls_count; ++idx) {
		err = get_control(handle, &list.pids[idx], control);
		if (err < 0)
			goto _free;
	}		
		
	err = 0;
 _free:
	free(list.pids);
 _close:
	snd_ctl_close(handle);
	return err;
}


static int config_iface(snd_config_t *n)
{
	static struct {
		int val;
		char *str;
	} v[] = {
		{ SND_CONTROL_IFACE_CARD, "card" },
		{ SND_CONTROL_IFACE_HWDEP, "hwdep" },
		{ SND_CONTROL_IFACE_MIXER, "mixer" },
		{ SND_CONTROL_IFACE_PCM, "pcm" },
		{ SND_CONTROL_IFACE_RAWMIDI, "rawmidi" },
		{ SND_CONTROL_IFACE_TIMER, "timer" },
		{ SND_CONTROL_IFACE_SEQUENCER, "sequencer" }
	};
	long idx;
	char *str;
	switch (snd_config_type(n)) {
	case SND_CONFIG_TYPE_INTEGER:
		snd_config_integer_get(n, &idx);
		return idx;
	case SND_CONFIG_TYPE_STRING:
		snd_config_string_get(n, &str);
		break;
	default:
		return -1;
	}
	for (idx = 0; idx < sizeof(v) / sizeof(v[0]); ++idx) {
		if (strcmp(v[idx].str, str) == 0)
			return idx;
	}
	return -1;
}

static int config_bool(snd_config_t *n)
{
	char *str;
	long val;
	switch (snd_config_type(n)) {
	case SND_CONFIG_TYPE_INTEGER:
		snd_config_integer_get(n, &val);
		if (val < 0 || val > 1)
			return -1;
		return val;
	case SND_CONFIG_TYPE_STRING:
		snd_config_string_get(n, &str);
		break;
	default:
		return -1;
	}
	if (strcmp(str, "on") == 0 || strcmp(str, "true") == 0)
		return 1;
	if (strcmp(str, "off") == 0 || strcmp(str, "false") == 0)
		return 0;
	return -1;
}

static int config_enumerated(snd_config_t *n, snd_ctl_t *handle,
			     snd_control_info_t *info)
{
	char *str;
	long val;
	int idx;
	switch (snd_config_type(n)) {
	case SND_CONFIG_TYPE_INTEGER:
		snd_config_integer_get(n, &val);
		return val;
	case SND_CONFIG_TYPE_STRING:
		snd_config_string_get(n, &str);
		break;
	default:
		return -1;
	}
	for (idx = 0; idx < info->value.enumerated.items; idx++) {
		int err;
		info->value.enumerated.item = idx;
		err = snd_ctl_cinfo(handle, info);
		if (err < 0) {
			error("snd_ctl_info: %s", snd_strerror(err));
			return err;
		}
		if (strcmp(str, info->value.enumerated.name) == 0)
			return idx;
	}
	return -1;
}

static int set_control(snd_ctl_t *handle, snd_config_t *control)
{
	snd_control_t ctl;
	snd_control_info_t info;
	snd_config_iterator_t i;
	int numid;
	long iface = -1;
	long device = -1;
	long subdevice = -1;
	char *name = NULL;
	long index = -1;
	snd_config_t *value = NULL;
	long val;
	int idx, err;
	char *set;
	if (snd_config_type(control) != SND_CONFIG_TYPE_COMPOUND) {
		error("control is not a compound");
		return -EINVAL;
	}
	numid = atoi(snd_config_id(control));
	snd_config_foreach(i, control) {
		snd_config_t *n = snd_config_entry(i);
		char *fld = snd_config_id(n);
		if (strcmp(fld, "comment") == 0)
			continue;
		if (strcmp(fld, "iface") == 0) {
			iface = config_iface(n);
			if (iface < 0) {
				error("control.%d.%s is invalid", numid, fld);
				return -EINVAL;
			}
			continue;
		}
		if (strcmp(fld, "device") == 0) {
			if (snd_config_type(n) != SND_CONFIG_TYPE_INTEGER) {
				error("control.%d.%s is invalid", numid, fld);
				return -EINVAL;
			}
			snd_config_integer_get(n, &device);
			continue;
		}
		if (strcmp(fld, "subdevice") == 0) {
			if (snd_config_type(n) != SND_CONFIG_TYPE_INTEGER) {
				error("control.%d.%s is invalid", numid, fld);
				return -EINVAL;
			}
			snd_config_integer_get(n, &subdevice);
			continue;
		}
		if (strcmp(fld, "name") == 0) {
			if (snd_config_type(n) != SND_CONFIG_TYPE_STRING) {
				error("control.%d.%s is invalid", numid, fld);
				return -EINVAL;
			}
			snd_config_string_get(n, &name);
			continue;
		}
		if (strcmp(fld, "index") == 0) {
			if (snd_config_type(n) != SND_CONFIG_TYPE_INTEGER) {
				error("control.%d.%s is invalid", numid, fld);
				return -EINVAL;
			}
			snd_config_integer_get(n, &index);
			continue;
		}
		if (strcmp(fld, "value") == 0) {
			value = n;
			continue;
		}
		error("unknown control.%d.%s field", numid, fld);
	}
	if (!value) {
		error("missing control.%d.value", numid);
		return -EINVAL;
	}
	if (device < 0)
		device = 0;
	if (subdevice < 0)
		subdevice = 0;
	if (index < 0)
		index = 0;
	memset(&info, 0, sizeof(info));
	info.id.numid = numid;
	err = snd_ctl_cinfo(handle, &info);
	if (err < 0) {
		if (iface >= 0 && name) {
			info.id.numid = 0;
			info.id.iface = iface;
			info.id.device = device;
			info.id.subdevice = subdevice;
			strncmp(info.id.name, name, sizeof(info.id.name));
			info.id.index = index;
			err = snd_ctl_cinfo(handle, &info);
		}
	}
	if (err < 0) {
		error("failed to obtain info for control #%d (%s)", numid, snd_strerror(err));
		return -ENOENT;
	}
	if (info.id.numid != numid)
		error("warning: numid mismatch (%d/%d) for control #%d", numid, info.id.numid, numid);
	if (info.id.iface != iface)
		error("warning: iface mismatch (%ld/%d) for control #%d", iface, info.id.iface, numid);
	if (info.id.device != device)
		error("warning: device mismatch (%ld/%d) for control #%d", device, info.id.device, numid);
	if (info.id.subdevice != subdevice)
		error("warning: subdevice mismatch (%ld/%d) for control #%d", subdevice, info.id.subdevice, numid);
	if (strcmp(info.id.name, name))
		error("warning: name mismatch (%s/%s) for control #%d", name, info.id.name, numid);
	if (info.id.index != index)
		error("warning: index mismatch (%ld/%d) for control #%d", index, info.id.index, numid);

	if (!(info.access & SND_CONTROL_ACCESS_WRITE))
		return 0;

	memset(&ctl, 0, sizeof(ctl));
	ctl.id = info.id;

	if (info.values_count == 1) {
		switch (info.type) {
		case SND_CONTROL_TYPE_BOOLEAN:
			val = config_bool(value);
			if (val >= 0) {
				ctl.value.integer.value[0] = val;
				goto _ok;
			}
			break;
		case SND_CONTROL_TYPE_INTEGER:
			err = snd_config_integer_get(value, &val);
			if (err == 0) {
				ctl.value.integer.value[0] = val;
				goto _ok;
			}
			break;
		case SND_CONTROL_TYPE_ENUMERATED:
			val = config_enumerated(value, handle, &info);
			if (val >= 0) {
				ctl.value.enumerated.item[0] = val;
				goto _ok;
			}
			break;
		default:
			error("Unknow control type: %d", info.type);
			return -EINVAL;
		}
	}
	if (info.type == SND_CONTROL_TYPE_BYTES) {
		char *buf;
		err = snd_config_string_get(value, &buf);
		if (err >= 0) {
			int c1 = 0;
			int len = strlen(buf);
			int idx = 0;
			if (info.values_count * 2 != len) {
				error("bad control.%d.value contents\n", numid);
				return -EINVAL;
			}
			while (*buf) {
				int c = *buf++;
				if (c >= '0' && c <= '9')
					c -= '0';
				else if (c <= 'a' && c <= 'f')
					c = c - 'a' + 10;
				else if (c <= 'A' && c <= 'F')
					c = c - 'A' + 10;
				else {
					error("bad control.%d.value contents\n", numid);
					return -EINVAL;
				}
				idx++;
				if (idx % 2 == 0)
					ctl.value.bytes.data[idx / 2] = c1 << 4 | c;
				else
					c1 = c;
			}
			goto _ok;
		}
	}
	if (snd_config_type(value) != SND_CONFIG_TYPE_COMPOUND) {
		error("bad control.%d.value type", numid);
		return -EINVAL;
	}

	set = alloca(info.values_count);
	memset(set, 0, info.values_count);
	snd_config_foreach(i, value) {
		snd_config_t *n = snd_config_entry(i);
		idx = atoi(snd_config_id(n));
		if (idx < 0 || idx >= info.values_count || 
		    set[idx]) {
			error("bad control.%d.value index", numid);
			return -EINVAL;
		}
		switch (info.type) {
		case SND_CONTROL_TYPE_BOOLEAN:
			val = config_bool(n);
			if (val < 0) {
				error("bad control.%d.value.%d content", numid, idx);
				return -EINVAL;
			}
			ctl.value.integer.value[idx] = val;
			break;
		case SND_CONTROL_TYPE_INTEGER:
			err = snd_config_integer_get(n, &val);
			if (err < 0) {
				error("bad control.%d.value.%d content", numid, idx);
				return -EINVAL;
			}
			ctl.value.integer.value[idx] = val;
			break;
		case SND_CONTROL_TYPE_ENUMERATED:
			val = config_enumerated(n, handle, &info);
			if (val < 0) {
				error("bad control.%d.value.%d content", numid, idx);
				return -EINVAL;
			}
			ctl.value.enumerated.item[idx] = val;
			break;
		case SND_CONTROL_TYPE_BYTES:
			err = snd_config_integer_get(n, &val);
			if (err < 0 || val < 0 || val > 255) {
				error("bad control.%d.value.%d content", numid, idx);
				return -EINVAL;
			}
			ctl.value.integer.value[idx] = val;
			break;
		default:
			break;
		}
		set[idx] = 1;
	}
	for (idx = 0; idx < info.values_count; ++idx) {
		if (!set[idx]) {
			error("control.%d.value.%d is not specified", numid, idx);
			return -EINVAL;
		}
	}

 _ok:
	err = snd_ctl_cwrite(handle, &ctl);
	if (err < 0) {
		error("Cannot write control '%s': %s", id_str(&ctl.id), snd_strerror(err));
		return err;
	}
	return 0;
}

static int set_controls(int card, snd_config_t *top)
{
	snd_ctl_t *handle;
	snd_ctl_hw_info_t info;
	snd_config_t *control;
	snd_config_iterator_t i;
	int err;

	err = snd_ctl_hw_open(&handle, NULL, card);
	if (err < 0) {
		error("snd_ctl_open error: %s", snd_strerror(err));
		return err;
	}
	err = snd_ctl_hw_info(handle, &info);
	if (err < 0) {
		error("snd_ctl_hw_info error: %s", snd_strerror(err));
		goto _close;
	}
	err = snd_config_searchv(top, &control, "state", info.id, "control", 0);
	if (err < 0) {
		err = 0;
		fprintf(stderr, "No state is present for card %s\n", info.id);
		goto _close;
	}
	if (snd_config_type(control) != SND_CONFIG_TYPE_COMPOUND) {
		error("state.%s.control is not a compound\n", info.id);
		return -EINVAL;
	}
	snd_config_foreach(i, control) {
		snd_config_t *n = snd_config_entry(i);
		err = set_control(handle, n);
		if (err < 0)
			goto _close;
	}

 _close:
	snd_ctl_close(handle);
	return err;
}


static int save_state(char *file, const char *cardname)
{
	int err;
	snd_config_t *config;
	FILE *fp;
	int stdio;

	err = snd_config_top(&config);
	if (err < 0) {
		error("snd_config_top error: %s", snd_strerror(err));
		return err;
	}
	stdio = !strcmp(file, "-");
	if (!stdio && (fp = fopen(file, "r"))) {
		err = snd_config_load(config, fp);
		if (!stdio)
			fclose(fp);
#if 0
		if (err < 0) {
			error("snd_config_load error: %s", snd_strerror(err));
			return err;
		}
#endif
	}

	if (!cardname) {
		unsigned int card_mask, idx;

		card_mask = snd_cards_mask();
		if (!card_mask) {
			error("No soundcards found...");
			return 1;
		}
		for (idx = 0; idx < 32; idx++) {
			if (card_mask & (1 << idx)) {	/* find each installed soundcards */
				if ((err = get_controls(idx, config))) {
					return err;
				}
			}
		}
	} else {
		int cardno;

		cardno = snd_card_get_index(cardname);
		if (cardno < 0) {
			error("Cannot find soundcard '%s'...", cardname);
			return 1;
		}
		if ((err = get_controls(cardno, config))) {
			return err;
		}
	}
	
	if (stdio) 
		fp = stdout;
	else
		fp = fopen(file, "w");
	if (!fp) {
		error("Cannot open %s for writing", file);
		return -errno;
	}
	err = snd_config_save(config, fp);
	if (!stdio)
		fclose(fp);
	if (err < 0)
		error("snd_config_save: %s", snd_strerror(err));
	return 0;
}


static int load_state(char *file, const char *cardname)
{
	int err;
	snd_config_t *config;
	FILE *fp;
	int stdio;

	err = snd_config_top(&config);
	if (err < 0) {
		error("snd_config_top error: %s", snd_strerror(err));
		return err;
	}
	stdio = !strcmp(file, "-");
	if (stdio)
		fp = stdin;
	else
		fp = fopen(file, "r");
	if (fp) {
		err = snd_config_load(config, fp);
		if (!stdio)
			fclose(fp);
		if (err < 0) {
			error("snd_config_load error: %s", snd_strerror(err));
			return err;
		}
	}

	if (!cardname) {
		unsigned int card_mask, idx;

		card_mask = snd_cards_mask();
		if (!card_mask) {
			error("No soundcards found...");
			return 1;
		}
		for (idx = 0; idx < 32; idx++) {
			if (card_mask & (1 << idx)) {	/* find each installed soundcards */
				if ((err = set_controls(idx, config))) {
					return err;
				}
			}
		}
	} else {
		int cardno;

		cardno = snd_card_get_index(cardname);
		if (cardno < 0) {
			error("Cannot find soundcard '%s'...", cardname);
			return 1;
		}
		if ((err = set_controls(cardno, config))) {
			return err;
		}
	}
	return 0;
}

int main(int argc, char *argv[])
{
	int morehelp;
	struct option long_option[] =
	{
		{"help", 0, NULL, 'h'},
		{"file", 1, NULL, 'f'},
		{"debug", 0, NULL, 'd'},
		{"version", 0, NULL, 'v'},
		{NULL, 0, NULL, 0},
	};
	char *cfgfile = SYS_ASOUNDRC;

	command = argv[0];
	morehelp = 0;
	while (1) {
		int c;

		if ((c = getopt_long(argc, argv, "hf:dv", long_option, NULL)) < 0)
			break;
		switch (c) {
		case 'h':
			morehelp++;
			break;
		case 'f':
			cfgfile = optarg;
			break;
		case 'd':
			debugflag = 1;
			break;
		case 'v':
			printf("alsactl version " SND_UTIL_VERSION_STR "\n");
			return 1;
		default:
			fprintf(stderr, "\07Invalid switch or option needs an argument.\n");
			morehelp++;
		}
	}
	if (morehelp) {
		help();
		return 1;
	}
	if (argc - optind <= 0) {
		fprintf(stderr, "alsactl: Specify command...\n");
		return 0;
	}
	if (!strcmp(argv[optind], "store")) {
		return save_state(cfgfile, argc - optind > 1 ? argv[optind + 1] : NULL) ?
		    1 : 0;
	} else if (!strcmp(argv[optind], "restore")) {
		return load_state(cfgfile, argc - optind > 1 ? argv[optind + 1] : NULL) ?
		    1 : 0;
	} else {
		fprintf(stderr, "alsactl: Unknown command '%s'...\n", argv[optind]);
	}

	return 0;
}
