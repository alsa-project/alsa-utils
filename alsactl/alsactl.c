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
#include <alsa/asoundlib.h>

#define SYS_ASOUNDRC "/etc/asound.state"

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
	printf("  power [card #] [state]\n");
	printf("                  get/set power state for one or each soundcards\n");
}

char *id_str(snd_ctl_elem_id_t *id)
{
	static char str[128];
	assert(id);
	sprintf(str, "%i,%i,%i,%s,%i", 
		snd_ctl_elem_id_get_interface(id),
		snd_ctl_elem_id_get_device(id),
		snd_ctl_elem_id_get_subdevice(id),
		snd_ctl_elem_id_get_name(id),
		snd_ctl_elem_id_get_index(id));
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
	err = snd_config_make_integer(&leaf, id);
	if (err < 0)
		return err;
	err = snd_config_add(father, leaf);
	if (err < 0) {
		snd_config_delete(leaf);
		return err;
	}
	err = snd_config_set_integer(leaf, integer);
	if (err < 0) {
		snd_config_delete(leaf);
		return err;
	}
	return 0;
}

static int snd_config_string_add(snd_config_t *father, const char *id, const char *string)
{
	int err;
	snd_config_t *leaf;
	err = snd_config_make_string(&leaf, id);
	if (err < 0)
		return err;
	err = snd_config_add(father, leaf);
	if (err < 0) {
		snd_config_delete(leaf);
		return err;
	}
	err = snd_config_set_string(leaf, string);
	if (err < 0) {
		snd_config_delete(leaf);
		return err;
	}
	return 0;
}

static int snd_config_compound_add(snd_config_t *father, const char *id, int join,
				   snd_config_t **node)
{
	int err;
	snd_config_t *leaf;
	err = snd_config_make_compound(&leaf, id, join);
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

static int get_control(snd_ctl_t *handle, snd_ctl_elem_id_t *id, snd_config_t *top)
{
	snd_ctl_elem_value_t *ctl;
	snd_ctl_elem_info_t *info;
	snd_config_t *control, *comment, *item, *value;
	const char *s;
	char buf[256];
	unsigned int idx;
	int err;
	unsigned int device, subdevice, index;
	const char *name;
	snd_ctl_elem_type_t type;
	unsigned int count;
	snd_ctl_elem_value_alloca(&ctl);
	snd_ctl_elem_info_alloca(&info);
	snd_ctl_elem_info_set_id(info, id);
	err = snd_ctl_elem_info(handle, info);
	if (err < 0) {
		error("Cannot read control info '%s': %s", id_str(id), snd_strerror(err));
		return err;
	}

	if (!snd_ctl_elem_info_is_readable(info))
		return 0;
	snd_ctl_elem_value_set_id(ctl, id);
	err = snd_ctl_elem_read(handle, ctl);
	if (err < 0) {
		error("Cannot read control '%s': %s", id_str(id), snd_strerror(err));
		return err;
	}

	err = snd_config_compound_add(top, num_str(snd_ctl_elem_info_get_numid(info)), 0, &control);
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
	if (snd_ctl_elem_info_is_readable(info))
		strcat(buf, " read");
	if (snd_ctl_elem_info_is_writable(info))
		strcat(buf, " write");
	if (snd_ctl_elem_info_is_inactive(info))
		strcat(buf, " inactive");
	if (snd_ctl_elem_info_is_volatile(info))
		strcat(buf, " volatile");
	if (snd_ctl_elem_info_is_locked(info))
		strcat(buf, " locked");
	if (snd_ctl_elem_info_is_indirect(info))
		strcat(buf, " indirect");
	err = snd_config_string_add(comment, "access", buf + 1);
	if (err < 0) {
		error("snd_config_string_add: %s", snd_strerror(err));
		return err;
	}

	type = snd_ctl_elem_info_get_type(info);
	device = snd_ctl_elem_info_get_device(info);
	subdevice = snd_ctl_elem_info_get_subdevice(info);
	index = snd_ctl_elem_info_get_index(info);
	name = snd_ctl_elem_info_get_name(info);
	count = snd_ctl_elem_info_get_count(info);
	s = snd_ctl_elem_type_name(type);
	err = snd_config_string_add(comment, "type", s);
	if (err < 0) {
		error("snd_config_string_add: %s", snd_strerror(err));
		return err;
	}

	switch (type) {
	case SND_CTL_ELEM_TYPE_BOOLEAN:
		break;
	case SND_CTL_ELEM_TYPE_INTEGER:
	{
		long min = snd_ctl_elem_info_get_min(info);
		long max = snd_ctl_elem_info_get_max(info);
		long step = snd_ctl_elem_info_get_step(info);
		if (step)
			sprintf(buf, "%li - %li (step %li)", min, max, step);
		else
			sprintf(buf, "%li - %li", min, max);
		err = snd_config_string_add(comment, "range", buf);
		if (err < 0) {
			error("snd_config_string_add: %s", snd_strerror(err));
			return err;
		}
		break;
	}
	case SND_CTL_ELEM_TYPE_ENUMERATED:
	{
		unsigned int items;
		err = snd_config_compound_add(comment, "item", 1, &item);
		if (err < 0) {
			error("snd_config_compound_add: %s", snd_strerror(err));
			return err;
		}
		items = snd_ctl_elem_info_get_items(info);
		for (idx = 0; idx < items; idx++) {
			snd_ctl_elem_info_set_item(info, idx);
			err = snd_ctl_elem_info(handle, info);
			if (err < 0) {
				error("snd_ctl_card_info: %s", snd_strerror(err));
				return err;
			}
			err = snd_config_string_add(item, num_str(idx), snd_ctl_elem_info_get_item_name(info));
			if (err < 0) {
				error("snd_config_string_add: %s", snd_strerror(err));
				return err;
			}
		}
		break;
	}
	default:
		break;
	}
	s = snd_ctl_elem_iface_name(snd_ctl_elem_info_get_interface(info));
	err = snd_config_string_add(control, "iface", s);
	if (err < 0) {
		error("snd_config_string_add: %s", snd_strerror(err));
		return err;
	}
	if (device != 0) {
		err = snd_config_integer_add(control, "device", device);
		if (err < 0) {
			error("snd_config_integer_add: %s", snd_strerror(err));
			return err;
		}
	}
	if (subdevice != 0) {
		err = snd_config_integer_add(control, "subdevice", subdevice);
		if (err < 0) {
			error("snd_config_integer_add: %s", snd_strerror(err));
			return err;
		}
	}
	err = snd_config_string_add(control, "name", name);
	if (err < 0) {
		error("snd_config_string_add: %s", snd_strerror(err));
		return err;
	}
	if (index != 0) {
		err = snd_config_integer_add(control, "index", index);
		if (err < 0) {
			error("snd_config_integer_add: %s", snd_strerror(err));
			return err;
		}
	}

	switch (type) {
	case SND_CTL_ELEM_TYPE_BYTES:
	case SND_CTL_ELEM_TYPE_IEC958:
	{
		size_t size = type == SND_CTL_ELEM_TYPE_BYTES ?
			count : sizeof(snd_aes_iec958_t);
		char buf[size * 2 + 1];
		char *p = buf;
		char *hex = "0123456789abcdef";
		const unsigned char *bytes = snd_ctl_elem_value_get_bytes(ctl);
		for (idx = 0; idx < size; idx++) {
			int v = bytes[idx];
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
	default:
		break;
	}

	if (count == 1) {
		switch (type) {
		case SND_CTL_ELEM_TYPE_BOOLEAN:
			err = snd_config_string_add(control, "value", snd_ctl_elem_value_get_boolean(ctl, 0) ? "true" : "false");
			if (err < 0) {
				error("snd_config_string_add: %s", snd_strerror(err));
				return err;
			}
			return 0;
		case SND_CTL_ELEM_TYPE_INTEGER:
			err = snd_config_integer_add(control, "value", snd_ctl_elem_value_get_integer(ctl, 0));
			if (err < 0) {
				error("snd_config_integer_add: %s", snd_strerror(err));
				return err;
			}
			return 0;
		case SND_CTL_ELEM_TYPE_ENUMERATED:
		{
			unsigned int v = snd_ctl_elem_value_get_enumerated(ctl, 0);
			snd_config_t *c;
			err = snd_config_search(item, num_str(v), &c);
			if (err == 0) {
				err = snd_config_get_string(c, &s);
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
			error("Unknown control type: %d\n", type);
			return -EINVAL;
		}
	}

	err = snd_config_compound_add(control, "value", 1, &value);
	if (err < 0) {
		error("snd_config_compound_add: %s", snd_strerror(err));
		return err;
	}

	switch (type) {
	case SND_CTL_ELEM_TYPE_BOOLEAN:
		for (idx = 0; idx < count; idx++) {
			err = snd_config_string_add(value, num_str(idx), snd_ctl_elem_value_get_boolean(ctl, idx) ? "true" : "false");
			if (err < 0) {
				error("snd_config_string_add: %s", snd_strerror(err));
				return err;
			}
		}
		break;
	case SND_CTL_ELEM_TYPE_INTEGER:
		for (idx = 0; idx < count; idx++) {
			err = snd_config_integer_add(value, num_str(idx), snd_ctl_elem_value_get_integer(ctl, idx));
			if (err < 0) {
				error("snd_config_integer_add: %s", snd_strerror(err));
				return err;
			}
		}
		break;
	case SND_CTL_ELEM_TYPE_ENUMERATED:
		for (idx = 0; idx < count; idx++) {
			unsigned int v = snd_ctl_elem_value_get_enumerated(ctl, idx);
			snd_config_t *c;
			err = snd_config_search(item, num_str(v), &c);
			if (err == 0) {
				err = snd_config_get_string(c, &s);
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
		error("Unknown control type: %d\n", type);
		return -EINVAL;
	}
	
	return 0;
}
	
static int get_controls(int cardno, snd_config_t *top)
{
	snd_ctl_t *handle;
	snd_ctl_card_info_t *info;
	snd_config_t *state, *card, *control;
	snd_ctl_elem_list_t *list;
	unsigned int idx;
	int err;
	char name[32];
	unsigned int count;
	const char *id;
	snd_ctl_card_info_alloca(&info);
	snd_ctl_elem_list_alloca(&list);

	sprintf(name, "hw:%d", cardno);
	err = snd_ctl_open(&handle, name, 0);
	if (err < 0) {
		error("snd_ctl_open error: %s", snd_strerror(err));
		return err;
	}
	err = snd_ctl_card_info(handle, info);
	if (err < 0) {
		error("snd_ctl_card_info error: %s", snd_strerror(err));
		goto _close;
	}
	id = snd_ctl_card_info_get_id(info);
	err = snd_config_search(top, "state", &state);
	if (err == 0 &&
	    snd_config_get_type(state) != SND_CONFIG_TYPE_COMPOUND) {
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
	err = snd_config_search(state, id, &card);
	if (err == 0 &&
	    snd_config_get_type(state) != SND_CONFIG_TYPE_COMPOUND) {
		error("config state.%s node is not a compound", id);
		err = -EINVAL;
		goto _close;
	}
	if (err < 0) {
		err = snd_config_compound_add(state, id, 0, &card);
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
	err = snd_ctl_elem_list(handle, list);
	if (err < 0) {
		error("Cannot determine controls: %s", snd_strerror(err));
		goto _close;
	}
	count = snd_ctl_elem_list_get_count(list);
	if (count <= 0) {
		err = 0;
		goto _close;
	}
	snd_ctl_elem_list_set_offset(list, 0);
	if (snd_ctl_elem_list_alloc_space(list, count) < 0) {
		error("No enough memory...");
		goto _close;
	}
	if ((err = snd_ctl_elem_list(handle, list)) < 0) {
		error("Cannot determine controls (2): %s", snd_strerror(err));
		goto _free;
	}
	for (idx = 0; idx < count; ++idx) {
		snd_ctl_elem_id_t *id;
		snd_ctl_elem_id_alloca(&id);
		snd_ctl_elem_list_get_id(list, idx, id);
		err = get_control(handle, id, control);
		if (err < 0)
			goto _free;
	}		
		
	err = 0;
 _free:
	snd_ctl_elem_list_free_space(list);
 _close:
	snd_ctl_close(handle);
	return err;
}


static int config_iface(snd_config_t *n)
{
	unsigned long i;
	snd_ctl_elem_iface_t idx;
	const char *str;
	switch (snd_config_get_type(n)) {
	case SND_CONFIG_TYPE_INTEGER:
		snd_config_get_integer(n, &i);
		return i;
	case SND_CONFIG_TYPE_STRING:
		snd_config_get_string(n, &str);
		break;
	default:
		return -1;
	}
	for (idx = 0; idx <= SND_CTL_ELEM_IFACE_LAST; idx++) {
		if (strcasecmp(snd_ctl_elem_iface_name(idx), str) == 0)
			return idx;
	}
	return -1;
}

static int config_bool(snd_config_t *n)
{
	const char *str;
	long val;
	switch (snd_config_get_type(n)) {
	case SND_CONFIG_TYPE_INTEGER:
		snd_config_get_integer(n, &val);
		if (val < 0 || val > 1)
			return -1;
		return val;
	case SND_CONFIG_TYPE_STRING:
		snd_config_get_string(n, &str);
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
			     snd_ctl_elem_info_t *info)
{
	const char *str;
	long val;
	unsigned int idx, items;
	switch (snd_config_get_type(n)) {
	case SND_CONFIG_TYPE_INTEGER:
		snd_config_get_integer(n, &val);
		return val;
	case SND_CONFIG_TYPE_STRING:
		snd_config_get_string(n, &str);
		break;
	default:
		return -1;
	}
	items = snd_ctl_elem_info_get_items(info);
	for (idx = 0; idx < items; idx++) {
		int err;
		snd_ctl_elem_info_set_item(info, idx);
		err = snd_ctl_elem_info(handle, info);
		if (err < 0) {
			error("snd_ctl_elem_info: %s", snd_strerror(err));
			return err;
		}
		if (strcmp(str, snd_ctl_elem_info_get_item_name(info)) == 0)
			return idx;
	}
	return -1;
}

static int set_control(snd_ctl_t *handle, snd_config_t *control)
{
	snd_ctl_elem_value_t *ctl;
	snd_ctl_elem_info_t *info;
	snd_config_iterator_t i, next;
	unsigned int numid1, iface1, device1, subdevice1, index1;
	const char *name1;
	unsigned int numid;
	snd_ctl_elem_type_t type;
	unsigned int count;
	long iface = -1;
	long device = -1;
	long subdevice = -1;
	const char *name = NULL;
	long index = -1;
	snd_config_t *value = NULL;
	long val;
	unsigned int idx;
	int err;
	char *set;
	const char *id;
	snd_ctl_elem_value_alloca(&ctl);
	snd_ctl_elem_info_alloca(&info);
	if (snd_config_get_type(control) != SND_CONFIG_TYPE_COMPOUND) {
		error("control is not a compound");
		return -EINVAL;
	}
	err = snd_config_get_id(control, &id);
	if (err < 0) {
		error("unable to get id");
		return -EINVAL;
	}
	numid = atoi(id);
	snd_config_for_each(i, next, control) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *fld;
		if (snd_config_get_id(n, &fld) < 0)
			continue;
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
			if (snd_config_get_type(n) != SND_CONFIG_TYPE_INTEGER) {
				error("control.%d.%s is invalid", numid, fld);
				return -EINVAL;
			}
			snd_config_get_integer(n, &device);
			continue;
		}
		if (strcmp(fld, "subdevice") == 0) {
			if (snd_config_get_type(n) != SND_CONFIG_TYPE_INTEGER) {
				error("control.%d.%s is invalid", numid, fld);
				return -EINVAL;
			}
			snd_config_get_integer(n, &subdevice);
			continue;
		}
		if (strcmp(fld, "name") == 0) {
			if (snd_config_get_type(n) != SND_CONFIG_TYPE_STRING) {
				error("control.%d.%s is invalid", numid, fld);
				return -EINVAL;
			}
			snd_config_get_string(n, &name);
			continue;
		}
		if (strcmp(fld, "index") == 0) {
			if (snd_config_get_type(n) != SND_CONFIG_TYPE_INTEGER) {
				error("control.%d.%s is invalid", numid, fld);
				return -EINVAL;
			}
			snd_config_get_integer(n, &index);
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

	snd_ctl_elem_info_set_numid(info, numid);
	err = snd_ctl_elem_info(handle, info);
	if (err < 0) {
		if (iface >= 0 && name) {
			snd_ctl_elem_info_set_numid(info, 0);
			snd_ctl_elem_info_set_interface(info, iface);
			snd_ctl_elem_info_set_device(info, device);
			snd_ctl_elem_info_set_subdevice(info, subdevice);
			snd_ctl_elem_info_set_name(info, name);
			snd_ctl_elem_info_set_index(info, index);
			err = snd_ctl_elem_info(handle, info);
		}
	}
	if (err < 0) {
		error("failed to obtain info for control #%d (%s)", numid, snd_strerror(err));
		return -ENOENT;
	}
	numid1 = snd_ctl_elem_info_get_numid(info);
	iface1 = snd_ctl_elem_info_get_interface(info);
	device1 = snd_ctl_elem_info_get_device(info);
	subdevice1 = snd_ctl_elem_info_get_subdevice(info);
	name1 = snd_ctl_elem_info_get_name(info);
	index1 = snd_ctl_elem_info_get_index(info);
	count = snd_ctl_elem_info_get_count(info);
	type = snd_ctl_elem_info_get_type(info);
	if (numid != numid1)
		error("warning: numid mismatch (%d/%d) for control #%d", 
		      numid, numid1, numid);
	if (iface != iface1)
		error("warning: iface mismatch (%ld/%d) for control #%d", iface, iface1, numid);
	if (device != device1)
		error("warning: device mismatch (%ld/%d) for control #%d", device, device1, numid);
	if (subdevice != subdevice1)
		error("warning: subdevice mismatch (%ld/%d) for control #%d", subdevice, subdevice1, numid);
	if (strcmp(name, name1))
		error("warning: name mismatch (%s/%s) for control #%d", name, name1, numid);
	if (index != index1)
		error("warning: index mismatch (%ld/%d) for control #%d", index, index1, numid);

	if (!snd_ctl_elem_info_is_writable(info))
		return 0;
	snd_ctl_elem_value_set_numid(ctl, numid);

	if (count == 1) {
		switch (type) {
		case SND_CTL_ELEM_TYPE_BOOLEAN:
			val = config_bool(value);
			if (val >= 0) {
				snd_ctl_elem_value_set_boolean(ctl, 0, val);
				goto _ok;
			}
			break;
		case SND_CTL_ELEM_TYPE_INTEGER:
			err = snd_config_get_integer(value, &val);
			if (err == 0) {
				snd_ctl_elem_value_set_integer(ctl, 0, val);
				goto _ok;
			}
			break;
		case SND_CTL_ELEM_TYPE_ENUMERATED:
			val = config_enumerated(value, handle, info);
			if (val >= 0) {
				snd_ctl_elem_value_set_enumerated(ctl, 0, val);
				goto _ok;
			}
			break;
		case SND_CTL_ELEM_TYPE_BYTES:
		case SND_CTL_ELEM_TYPE_IEC958:
			break;
		default:
			error("Unknow control type: %d", type);
			return -EINVAL;
		}
	}
	switch (type) {
	case SND_CTL_ELEM_TYPE_BYTES:
	case SND_CTL_ELEM_TYPE_IEC958:
	{
		const char *buf;
		err = snd_config_get_string(value, &buf);
		if (err >= 0) {
			int c1 = 0;
			int len = strlen(buf);
			unsigned int idx = 0;
			int size = type == SND_CTL_ELEM_TYPE_BYTES ?
				count : sizeof(snd_aes_iec958_t);
			if (size * 2 != len) {
				error("bad control.%d.value contents\n", numid);
				return -EINVAL;
			}
			while (*buf) {
				int c = *buf++;
				if (c >= '0' && c <= '9')
					c -= '0';
				else if (c >= 'a' && c <= 'f')
					c = c - 'a' + 10;
				else if (c >= 'A' && c <= 'F')
					c = c - 'A' + 10;
				else {
					error("bad control.%d.value contents\n", numid);
					return -EINVAL;
				}
				if (idx % 2 == 1)
					snd_ctl_elem_value_set_byte(ctl, idx / 2, c1 << 4 | c);
				else
					c1 = c;
				idx++;
			}
			goto _ok;
		}
	}
	default:
		break;
	}
	if (snd_config_get_type(value) != SND_CONFIG_TYPE_COMPOUND) {
		error("bad control.%d.value type", numid);
		return -EINVAL;
	}

	set = alloca(count);
	memset(set, 0, count);
	snd_config_for_each(i, next, value) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id;
		if (snd_config_get_id(n, &id) < 0)
			continue;
		idx = atoi(id);
		if (idx < 0 || idx >= count || 
		    set[idx]) {
			error("bad control.%d.value index", numid);
			return -EINVAL;
		}
		switch (type) {
		case SND_CTL_ELEM_TYPE_BOOLEAN:
			val = config_bool(n);
			if (val < 0) {
				error("bad control.%d.value.%d content", numid, idx);
				return -EINVAL;
			}
			snd_ctl_elem_value_set_boolean(ctl, idx, val);
			break;
		case SND_CTL_ELEM_TYPE_INTEGER:
			err = snd_config_get_integer(n, &val);
			if (err < 0) {
				error("bad control.%d.value.%d content", numid, idx);
				return -EINVAL;
			}
			snd_ctl_elem_value_set_integer(ctl, idx, val);
			break;
		case SND_CTL_ELEM_TYPE_ENUMERATED:
			val = config_enumerated(n, handle, info);
			if (val < 0) {
				error("bad control.%d.value.%d content", numid, idx);
				return -EINVAL;
			}
			snd_ctl_elem_value_set_enumerated(ctl, idx, val);
			break;
		case SND_CTL_ELEM_TYPE_BYTES:
		case SND_CTL_ELEM_TYPE_IEC958:
			err = snd_config_get_integer(n, &val);
			if (err < 0 || val < 0 || val > 255) {
				error("bad control.%d.value.%d content", numid, idx);
				return -EINVAL;
			}
			snd_ctl_elem_value_set_byte(ctl, idx, val);
			break;
		default:
			break;
		}
		set[idx] = 1;
	}
	for (idx = 0; idx < count; ++idx) {
		if (!set[idx]) {
			error("control.%d.value.%d is not specified", numid, idx);
			return -EINVAL;
		}
	}

 _ok:
	err = snd_ctl_elem_write(handle, ctl);
	if (err < 0) {
		snd_ctl_elem_id_t *id;
		snd_ctl_elem_id_alloca(&id);
		snd_ctl_elem_value_get_id(ctl, id);
		error("Cannot write control '%s': %s", id_str(id), snd_strerror(err));
		return err;
	}
	return 0;
}

static int set_controls(int card, snd_config_t *top)
{
	snd_ctl_t *handle;
	snd_ctl_card_info_t *info;
	snd_config_t *control;
	snd_config_iterator_t i, next;
	int err;
	char name[32];
	const char *id;
	snd_ctl_card_info_alloca(&info);

	sprintf(name, "hw:%d", card);
	err = snd_ctl_open(&handle, name, 0);
	if (err < 0) {
		error("snd_ctl_open error: %s", snd_strerror(err));
		return err;
	}
	err = snd_ctl_card_info(handle, info);
	if (err < 0) {
		error("snd_ctl_card_info error: %s", snd_strerror(err));
		goto _close;
	}
	id = snd_ctl_card_info_get_id(info);
	err = snd_config_searchv(top, &control, "state", id, "control", 0);
	if (err < 0) {
		err = 0;
		fprintf(stderr, "No state is present for card %s\n", id);
		goto _close;
	}
	if (snd_config_get_type(control) != SND_CONFIG_TYPE_COMPOUND) {
		error("state.%s.control is not a compound\n", id);
		return -EINVAL;
	}
	snd_config_for_each(i, next, control) {
		snd_config_t *n = snd_config_iterator_entry(i);
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
	snd_input_t *in;
	snd_output_t *out;
	int stdio;

	err = snd_config_top(&config);
	if (err < 0) {
		error("snd_config_top error: %s", snd_strerror(err));
		return err;
	}
	stdio = !strcmp(file, "-");
	if (!stdio && (err = snd_input_stdio_open(&in, file, "r")) >= 0) {
		err = snd_config_load(config, in);
		snd_input_close(in);
#if 0
		if (err < 0) {
			error("snd_config_load error: %s", snd_strerror(err));
			return err;
		}
#endif
	}

	if (!cardname) {
		int card, first = 1;

		card = -1;
		/* find each installed soundcards */
		while (1) {
			if (snd_card_next(&card) < 0)
				break;
			if (card < 0) {
				if (first) {
					error("No soundcards found...");
					return EXIT_FAILURE;
				}
				break;
			}
			first = 0;
			if ((err = get_controls(card, config)))
				return err;
		}
	} else {
		int cardno;

		cardno = snd_card_get_index(cardname);
		if (cardno < 0) {
			error("Cannot find soundcard '%s'...", cardname);
			return EXIT_FAILURE;
		}
		if ((err = get_controls(cardno, config))) {
			return err;
		}
	}
	
	if (stdio)
		err = snd_output_stdio_attach(&out, stdout, 0);
	else
		err = snd_output_stdio_open(&out, file, "w");
	if (err < 0) {
		error("Cannot open %s for writing", file);
		return -errno;
	}
	err = snd_config_save(config, out);
	snd_output_close(out);
	if (err < 0)
		error("snd_config_save: %s", snd_strerror(err));
	return 0;
}


static int load_state(char *file, const char *cardname)
{
	int err;
	snd_config_t *config;
	snd_input_t *in;
	int stdio;

	err = snd_config_top(&config);
	if (err < 0) {
		error("snd_config_top error: %s", snd_strerror(err));
		return err;
	}
	stdio = !strcmp(file, "-");
	if (stdio)
		err = snd_input_stdio_attach(&in, stdin, 0);
	else
		err = snd_input_stdio_open(&in, file, "r");
	if (err >= 0) {
		err = snd_config_load(config, in);
		snd_input_close(in);
		if (err < 0) {
			error("snd_config_load error: %s", snd_strerror(err));
			return err;
		}
	}

	if (!cardname) {
		int card, first = 1;

		card = -1;
		/* find each installed soundcards */
		while (1) {
			if (snd_card_next(&card) < 0)
				break;
			if (card < 0) {
				if (first) {
					error("No soundcards found...");
					return EXIT_FAILURE;
				}
				break;
			}
			first = 0;
			if ((err = set_controls(card, config)))
				return err;
		}
	} else {
		int cardno;

		cardno = snd_card_get_index(cardname);
		if (cardno < 0) {
			error("Cannot find soundcard '%s'...", cardname);
			return EXIT_FAILURE;
		}
		if ((err = set_controls(cardno, config))) {
			return err;
		}
	}
	return 0;
}

static int get_int_state(const char *str)
{
	if (!strcasecmp(str, "on"))
		return SND_CTL_POWER_D0;
	if (!strcasecmp(str, "off"))
		return SND_CTL_POWER_D3hot;
	if (*str == 'D' || *str == 'd') {
		str++;
		if (!strcmp(str, "0"))
			return SND_CTL_POWER_D0;
		if (!strcmp(str, "1"))
			return SND_CTL_POWER_D1;
		if (!strcmp(str, "2"))
			return SND_CTL_POWER_D2;
		if (!strcmp(str, "3"))
			return SND_CTL_POWER_D3;
		if (!strcmp(str, "3hot"))
			return SND_CTL_POWER_D3hot;
		if (!strcmp(str, "3cold"))
			return SND_CTL_POWER_D3cold;
	}
	return -1;
}

static const char *get_str_state(int power_state)
{
	static char str[16];

	switch (power_state) {
	case SND_CTL_POWER_D0:
		return "D0";
	case SND_CTL_POWER_D1:
		return "D1";
	case SND_CTL_POWER_D2:
		return "D2";
	// return SND_CTL_POWER_D3;	/* it's same as D3hot */
	case SND_CTL_POWER_D3hot:
		return "D3hot";
	case SND_CTL_POWER_D3cold:
		return "D3cold";
	default:
		sprintf(str, "???0x%x", power_state);
		return str;
	}
}

static int show_power(int cardno)
{
	snd_ctl_t *handle;
	char name[16];
	int power_state, err;

	sprintf(name, "hw:%d", cardno);
	err = snd_ctl_open(&handle, name, 0);
	if (err < 0) {
		error("snd_ctl_open error: %s", snd_strerror(err));
		return err;
	}
	err = snd_ctl_get_power_state(handle, &power_state);
	if (err < 0) {
		error("snd_ctl_get_power_state error: %s", snd_strerror(err));
		snd_ctl_close(handle);
		return err;
	}
	snd_ctl_close(handle);
	printf("Power state for card #%d is %s\n", cardno, get_str_state(power_state));
	return 0;
}

static int set_power(int cardno, int power_state)
{
	snd_ctl_t *handle;
	char name[16];
	int err;

	sprintf(name, "hw:%d", cardno);
	err = snd_ctl_open(&handle, name, 0);
	if (err < 0) {
		error("snd_ctl_open error: %s", snd_strerror(err));
		return err;
	}
	err = snd_ctl_set_power_state(handle, power_state);
	if (err < 0) {
		error("snd_ctl_set_power_state error: %s", snd_strerror(err));
		snd_ctl_close(handle);
		return err;
	}
	err = snd_ctl_get_power_state(handle, &power_state);
	if (err < 0) {
		error("snd_ctl_get_power_state error: %s", snd_strerror(err));
		snd_ctl_close(handle);
		return err;
	}
	snd_ctl_close(handle);
	printf("Power state for card #%d is %s\n", cardno, get_str_state(power_state));
	return 0;
}

static int power(const char *argv[], int argc)
{
	int power_state, err;
	
	if (argc == 0) {		/* show status only */
		int card, first = 1;

		card = -1;
		/* find each installed soundcards */
		while (1) {
			if (snd_card_next(&card) < 0)
				break;
			if (card < 0) {
				if (first) {
					error("No soundcards found...");
					return EXIT_FAILURE;
				}
				break;
			}
			first = 0;
			if ((err = show_power(card)) < 0)
				return err;
		}
		return 0;
	}
	power_state = get_int_state(argv[0]);
	if (power_state >= 0) {
		int card, first = 1;

		card = -1;
		/* find each installed soundcards */
		while (1) {
			if (snd_card_next(&card) < 0)
				break;
			if (card < 0) {
				if (first) {
					error("No soundcards found...");
					return EXIT_FAILURE;
				}
				break;
			}
			first = 0;
			if ((err = set_power(card, power_state)))
				return err;
		}
	} else {
		int cardno;

		cardno = snd_card_get_index(argv[0]);
		if (cardno < 0) {
			error("Cannot find soundcard '%s'...", argv[0]);
			return EXIT_FAILURE;
		}
		if (argc > 1) {
			power_state = get_int_state(argv[1]);
			if (power_state < 0) {
				error("Invalid power state '%s'...", argv[1]);
				return EXIT_FAILURE;
			}
			if ((err = set_power(cardno, power_state)) < 0)
				return err;
		} else {
			if ((err = show_power(cardno)) < 0)
				return err;
		}
	}
	return EXIT_SUCCESS;
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
			return EXIT_SUCCESS;
		default:
			fprintf(stderr, "\07Invalid switch or option needs an argument.\n");
			morehelp++;
		}
	}
	if (morehelp) {
		help();
		return EXIT_SUCCESS;
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
	} else if (!strcmp(argv[optind], "power")) {
		return power((const char **)argv + optind + 1, argc - optind - 1);
	} else {
		fprintf(stderr, "alsactl: Unknown command '%s'...\n", argv[optind]);
	}

	return 0;
}
