/*
 *   ALSA command line mixer utility
 *   Copyright (c) 1999-2000 by Jaroslav Kysela <perex@suse.cz>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>
#include <assert.h>
#include <sys/asoundlib.h>
#include <sys/poll.h>
#include "amixer.h"

#define HELPID_HELP             1000
#define HELPID_CARD             1001
#define HELPID_QUIET		1002
#define HELPID_DEBUG            1003
#define HELPID_VERSION		1004

int quiet = 0;
int debugflag = 0;
char *card = "hw:0";

static void error(const char *fmt,...)
{
	va_list va;

	va_start(va, fmt);
	fprintf(stderr, "amixer: ");
	vfprintf(stderr, fmt, va);
	fprintf(stderr, "\n");
	va_end(va);
}

static int help(void)
{
	printf("Usage: amixer <options> command\n");
	printf("\nAvailable options:\n");
	printf("  -h,--help       this help\n");
	printf("  -c,--card N     use a ctl name, default %s\n", card);
	printf("  -D,--debug      debug mode\n");
	printf("  -v,--version    print version of this program\n");
	printf("\nAvailable commands:\n");
	printf("  scontrols       show all mixer simple controls\n");
	printf("  scontents	  show contents of all mixer simple controls (default command)\n");
	printf("  sset sID P      set contents for one mixer simple control\n");
	printf("  sget sID P      get contents for one mixer simple control\n");
	printf("  controls        show all controls for given card\n");
	printf("  contents        show contents of all controls for given card\n");
	printf("  cset cID P	  set control contents for one control\n");
	printf("  cget cID P	  get control contents for one control\n");
	return 0;
}

static int info(void)
{
	int err;
	snd_ctl_t *handle;
	snd_mixer_t *mhandle;
	snd_ctl_info_t *info;
	snd_control_list_t *clist;
	snd_mixer_simple_control_list_t slist;
	snd_ctl_info_alloca(&info);
	snd_control_list_alloca(&clist);
	
	if ((err = snd_ctl_open(&handle, card)) < 0) {
		error("Control device %i open error: %s", card, snd_strerror(err));
		return err;
	}
	if ((err = snd_ctl_info(handle, info)) < 0) {
		error("Control device %i hw info error: %s", card, snd_strerror(err));
		return err;
	}
	printf("Card %s '%s'/'%s'\n", card, snd_ctl_info_get_id(info),
	       snd_ctl_info_get_longname(info));
	printf("  Mixer ID	: '%s'\n", snd_ctl_info_get_mixerid(info));
	printf("  Mixer name	: '%s'\n", snd_ctl_info_get_mixername(info));
	if ((err = snd_ctl_clist(handle, clist)) < 0) {
		error("snd_ctl_clist failure: %s", snd_strerror(err));
	} else {
		printf("  Controls      : %i\n", snd_control_list_get_count(clist));
	}
	snd_ctl_close(handle);
	if ((err = snd_mixer_open(&mhandle, card)) < 0) {
		error("Mixer %s open error: %s", card, snd_strerror(err));
		return err;
	}
	if ((err = snd_mixer_simple_control_list(mhandle, &slist)) < 0) {
		error("snd_mixer_simple_control_list failure: %s\n", snd_strerror(err));
	} else {
		printf("  Simple ctrls  : %i\n", slist.controls);
	}
	snd_mixer_close(mhandle);
	return 0;
}

static const char *control_iface(snd_control_id_t *id)
{
	return snd_control_iface_name(snd_control_id_get_interface(id));
}

static const char *control_type(snd_control_info_t *info)
{
	return snd_control_type_name(snd_control_info_get_type(info));
}

static const char *control_access(snd_control_info_t *info)
{
	static char result[10];
	char *res = result;

	*res++ = snd_control_info_is_readable(info) ? 'r' : '-';
	*res++ = snd_control_info_is_writable(info) ? 'w' : '-';
	*res++ = snd_control_info_is_inactive(info) ? 'i' : '-';
	*res++ = snd_control_info_is_volatile(info) ? 'v' : '-';
	*res++ = snd_control_info_is_locked(info) ? 'l' : '-';
	*res++ = '\0';
	return result;
}

static snd_mixer_sid_t __simple_id ATTRIBUTE_UNUSED;
#define simple_name_size (sizeof(__simple_id.name)+1)

static char *simple_name(const char *name, char *result)
{
	strncpy(result, name, simple_name_size - 1);
	result[simple_name_size - 1] = '\0';
	return result;
}

static int check_range(int val, int min, int max)
{
	if (val < min)
		return min;
	if (val > max)
		return max;
	return val;
}

#if 0
static int convert_range(int val, int omin, int omax, int nmin, int nmax)
{
	int orange = omax - omin, nrange = nmax - nmin;
	
	if (orange == 0)
		return 0;
	return rint((((double)nrange * ((double)val - (double)omin)) + ((double)orange / 2.0)) / ((double)orange + (double)nmin));
}
#endif

#if 0
static int convert_db_range(int val, int omin, int omax, int nmin, int nmax)
{
	int orange = omax - omin, nrange = nmax - nmin;
	
	if (orange == 0)
		return 0;
	return rint((((double)nrange * ((double)val - (double)omin)) + ((double)orange / 2.0)) / (double)orange + (double)nmin);
}
#endif

/* Fuction to convert from volume to percentage. val = volume */

static int convert_prange(int val, int min, int max)
{
	int range = max - min;
	int tmp;

	if (range == 0)
		return 0;
	val -= min;
	tmp = rint((double)val/(double)range * 100);
	return tmp;
}

/* Function to convert from percentage to volume. val = percentage */

static int convert_prange1(int val, int min, int max)
{
	int range = max - min;
	int tmp;

	if (range == 0)
		return 0;

	tmp = rint((double)range * ((double)val*.01)) + min;
	return tmp;
}

static const char *get_percent(int val, int min, int max)
{
	static char str[32];
	int p;
	
	p = convert_prange(val, min, max);
	sprintf(str, "%i [%i%%]", val, p);
	return str;
}

#if 0
static const char *get_percent1(int val, int min, int max, int min_dB, int max_dB)
{
	static char str[32];
	int p, db;

	p = convert_prange(val, min, max);
	db = convert_db_range(val, min, max, min_dB, max_dB);
	sprintf(str, "%i [%i%%] [%i.%02idB]", val, p, db / 100, abs(db % 100));
	return str;
}
#endif

static long get_integer(char **ptr, long min, long max)
{
	int tmp, tmp1, tmp2;

	if (**ptr == ':')
		(*ptr)++;
	if (**ptr == '\0' || (!isdigit(**ptr) && **ptr != '-'))
		return min;
	tmp = strtol(*ptr, ptr, 10);
	tmp1 = tmp;
	tmp2 = 0;
	if (**ptr == '.') {
		(*ptr)++;
		tmp2 = strtol(*ptr, ptr, 10);
	}
	if (**ptr == '%') {
		tmp1 = convert_prange1(tmp, min, max);
		(*ptr)++;
	}
	tmp1 = check_range(tmp1, min, max);
	if (**ptr == ',')
		(*ptr)++;
	return tmp1;
}

static int get_volume_simple(char **ptr, int min, int max, int orig)
{
	int tmp, tmp1, tmp2;

	if (**ptr == ':')
		(*ptr)++;
	if (**ptr == '\0' || (!isdigit(**ptr) && **ptr != '-'))
		return min;
	tmp = atoi(*ptr);
	if (**ptr == '-')
		(*ptr)++;
	while (isdigit(**ptr))
		(*ptr)++;
	tmp1 = tmp;
	tmp2 = 0;
	if (**ptr == '.') {
		(*ptr)++;
		tmp2 = atoi(*ptr);
		while (isdigit(**ptr))
			(*ptr)++;
	}
	if (**ptr == '%') {
		tmp1 = convert_prange1(tmp, min, max);
		(*ptr)++;
	}
	if (**ptr == '+') {
		tmp1 = orig + tmp1;
		(*ptr)++;
	} else if (**ptr == '-') {
		tmp1 = orig - tmp1;
		(*ptr)++;
	}
	tmp1 = check_range(tmp1, min, max);
	if (**ptr == ',')
		(*ptr)++;
	return tmp1;
}

static void show_control_id(snd_control_id_t *id)
{
	unsigned int index, device, subdevice;
	printf("numid=%u,iface=%s,name='%s'",
	       snd_control_id_get_numid(id),
	       control_iface(id),
	       snd_control_id_get_name(id));
	index = snd_control_id_get_index(id);
	device = snd_control_id_get_device(id);
	subdevice = snd_control_id_get_subdevice(id);
	if (index)
		printf(",index=%i", index);
	if (device)
		printf(",device=%i", device);
	if (subdevice)
		printf(",subdevice=%i", subdevice);
}

static int show_control(const char *space, snd_ctl_t *handle, snd_control_id_t *id, int level)
{
	int err;
	unsigned int item, idx;
	snd_control_info_t *info;
	snd_control_t *control;
	unsigned int count;
	snd_control_type_t type;
	snd_control_info_alloca(&info);
	snd_control_alloca(&control);
	snd_control_info_set_id(info, id);
	if ((err = snd_ctl_cinfo(handle, info)) < 0) {
		error("Control %s cinfo error: %s\n", card, snd_strerror(err));
		return err;
	}
	if (level & 2) {
		snd_control_info_get_id(info, id);
		show_control_id(id);
		printf("\n");
	}
	count = snd_control_info_get_count(info);
	type = snd_control_info_get_type(info);
	printf("%s; type=%s,access=%s,values=%i", space, control_type(info), control_access(info), count);
	switch (snd_enum_to_int(type)) {
	case SND_CONTROL_TYPE_INTEGER:
		printf(",min=%li,max=%li,step=%li\n", 
		       snd_control_info_get_min(info),
		       snd_control_info_get_max(info),
		       snd_control_info_get_step(info));
		break;
	case SND_CONTROL_TYPE_ENUMERATED:
	{
		unsigned int items = snd_control_info_get_items(info);
		printf(",items=%u\n", items);
		for (item = 0; item < items; item++) {
			snd_control_info_set_item(info, item);
			if ((err = snd_ctl_cinfo(handle, info)) < 0) {
				error("Control %s cinfo error: %s\n", card, snd_strerror(err));
				return err;
			}
			printf("%s; Item #%u '%s'\n", space, item, snd_control_info_get_item_name(info));
		}
		break;
	}
	default:
		printf("\n");
		break;
	}
	if (level & 1) {
		snd_control_set_id(control, id);
		if ((err = snd_ctl_cread(handle, control)) < 0) {
			error("Control %s cread error: %s\n", card, snd_strerror(err));
			return err;
		}
		printf("%s: values=", space);
		for (idx = 0; idx < count; idx++) {
			if (idx > 0)
				printf(",");
			switch (snd_enum_to_int(type)) {
			case SND_CONTROL_TYPE_BOOLEAN:
				printf("%s", snd_control_get_boolean(control, idx) ? "on" : "off");
				break;
			case SND_CONTROL_TYPE_INTEGER:
				printf("%li", snd_control_get_integer(control, idx));
				break;
			case SND_CONTROL_TYPE_ENUMERATED:
				printf("%u", snd_control_get_enumerated(control, idx));
				break;
			case SND_CONTROL_TYPE_BYTES:
				printf("0x%02x", snd_control_get_byte(control, idx));
				break;
			default:
				printf("?");
				break;
			}
		}
		printf("\n");
	}
	return 0;
}

static int controls(int level)
{
	int err;
	unsigned int idx;
	snd_ctl_t *handle;
	unsigned int count;
	snd_hcontrol_list_t *list;
	snd_control_id_t *id;
	snd_hcontrol_list_alloca(&list);
	snd_control_id_alloca(&id);
	
	if ((err = snd_ctl_open(&handle, card)) < 0) {
		error("Control %s open error: %s", card, snd_strerror(err));
		return err;
	}
	if ((err = snd_ctl_hbuild(handle, NULL)) < 0) {
		error("Control %s hbuild error: %s\n", card, snd_strerror(err));
		return err;
	}
	if ((err = snd_ctl_hlist(handle, list)) < 0) {
		error("Control %s clist error: %s", card, snd_strerror(err));
		return err;
	}
	count = snd_hcontrol_list_get_count(list);
	snd_hcontrol_list_set_offset(list, 0);
	if (snd_hcontrol_list_alloc_space(list, count) < 0) {
		error("Not enough memory");
		return -ENOMEM;
	}
	if ((err = snd_ctl_hlist(handle, list)) < 0) {
		error("Control %s hlist error: %s", card, snd_strerror(err));
		return err;
	}
	for (idx = 0; idx < count; idx++) {
		snd_hcontrol_list_get_id(list, idx, id);
		show_control_id(id);
		printf("\n");
		if (level > 0)
			show_control("  ", handle, id, 1);
	}
	snd_hcontrol_list_free_space(list);
	snd_ctl_close(handle);
	return 0;
}

static int show_simple_control(void *handle, snd_mixer_sid_t *sid, const char *space, int level)
{
	int err;
	snd_mixer_channel_id_t chn;
	snd_mixer_simple_control_t scontrol;
	
	bzero(&scontrol, sizeof(scontrol));
	scontrol.sid = *sid;
	if ((err = snd_mixer_simple_control_read(handle, &scontrol)) < 0) {
		error("Mixer %s simple_control error: %s", card, snd_strerror(err));
		return err;
	}
	if ((level & 1) != 0 && scontrol.channels) {
		printf("%sCapabilities:", space);
		if (scontrol.caps & SND_MIXER_SCTCAP_VOLUME)
			printf(" volume");
		if (scontrol.caps & SND_MIXER_SCTCAP_MUTE)
			printf(" mute");
		if (scontrol.caps & SND_MIXER_SCTCAP_JOINTLY_MUTE)
			printf(" jointly-mute");
		if (scontrol.caps & SND_MIXER_SCTCAP_CAPTURE) {
			printf(" capture");
		} else {
			scontrol.capture = 0;
		}
		if (scontrol.caps & SND_MIXER_SCTCAP_JOINTLY_CAPTURE)
			printf(" jointly-capture");
		if (scontrol.caps & SND_MIXER_SCTCAP_EXCL_CAPTURE)
			printf(" exclusive-capture");
		printf("\n");
		if ((scontrol.caps & SND_MIXER_SCTCAP_CAPTURE) &&
		    (scontrol.caps & SND_MIXER_SCTCAP_EXCL_CAPTURE))
			printf("%sCapture exclusive scontrol: %i\n", space, scontrol.capture_group);
		printf("%sChannels: ", space);
		if (scontrol.channels == SND_MIXER_CHN_MASK_MONO) {
			printf("Mono");
		} else {
			for (chn = 0; chn <= SND_MIXER_CHN_LAST; snd_enum_incr(chn)){
				if (!(scontrol.channels & (1<<snd_enum_to_int(chn))))
					continue;
				printf("%s ", snd_mixer_simple_channel_name(chn));
			}
		}
		printf("\n");
		printf("%sLimits: min = %li, max = %li\n", space, scontrol.min, scontrol.max);
		if (scontrol.channels == SND_MIXER_CHN_MASK_MONO) {
			printf("%sMono: %s [%s]\n", space, get_percent(scontrol.volume.names.front_left, scontrol.min, scontrol.max), scontrol.mute & SND_MIXER_CHN_MASK_MONO ? "mute" : "on");
		} else {
			for (chn = 0; chn <= SND_MIXER_CHN_LAST; snd_enum_incr(chn)) {
				int c = snd_enum_to_int(chn);
				if (!(scontrol.channels & (1<<c)))
					continue;
				printf("%s%s: %s [%s] [%s]\n",
						space,
						snd_mixer_simple_channel_name(chn),
						get_percent(scontrol.volume.values[c], scontrol.min, scontrol.max),
						scontrol.mute & (1<<c) ? "mute" : "on",
						scontrol.capture & (1<<c) ? "capture" : "---");
			}
		}
	}
	return 0;
}

static int simple_controls(int level)
{
	int err;
	unsigned int idx;
	snd_mixer_t *handle;
	snd_mixer_simple_control_list_t list;
	snd_mixer_sid_t *sid;
	char name[simple_name_size];
	
	if ((err = snd_mixer_open(&handle, card)) < 0) {
		error("Mixer %s open error: %s", card, snd_strerror(err));
		return err;
	}
	memset(&list, 0, sizeof(list));
	if ((err = snd_mixer_simple_control_list(handle, &list)) < 0) {
		error("Mixer %s simple_control_list error: %s", card, snd_strerror(err));
		return err;
	}
	list.pids = (snd_mixer_sid_t *)malloc(list.controls * sizeof(snd_mixer_sid_t));
	if (list.pids == NULL) {
		error("Not enough memory");
		return -ENOMEM;
	}
	list.controls_request = list.controls;
	if ((err = snd_mixer_simple_control_list(handle, &list)) < 0) {
		error("Mixer %s simple_control_list (2) error: %s", card, snd_strerror(err));
		return err;
	}
	for (idx = 0; idx < list.controls_count; idx++) {
		sid = &list.pids[idx];
		printf("Simple mixer control '%s',%i\n", simple_name(sid->name, name), sid->index);
		show_simple_control(handle, sid, "  ", level);
	}
	free(list.pids);
	snd_mixer_close(handle);
	return 0;
}

static int parse_control_id(const char *str, snd_control_id_t *id)
{
	int c, size;
	char *ptr;

	while (*str == ' ' || *str == '\t')
		str++;
	if (!(*str))
		return -EINVAL;
	snd_control_id_set_interface(id, SND_CONTROL_IFACE_MIXER);	/* default */
	while (*str) {
		if (!strncasecmp(str, "numid=", 6)) {
			str += 6;
			snd_control_id_set_numid(id, atoi(str));
			while (isdigit(*str))
				str++;
		} else if (!strncasecmp(str, "iface=", 6)) {
			str += 6;
			if (!strncasecmp(str, "card", 4)) {
				snd_control_id_set_interface(id, SND_CONTROL_IFACE_CARD);
				str += 4;
			} else if (!strncasecmp(str, "mixer", 5)) {
				snd_control_id_set_interface(id, SND_CONTROL_IFACE_MIXER);
				str += 5;
			} else if (!strncasecmp(str, "pcm", 3)) {
				snd_control_id_set_interface(id, SND_CONTROL_IFACE_PCM);
				str += 3;
			} else if (!strncasecmp(str, "rawmidi", 7)) {
				snd_control_id_set_interface(id, SND_CONTROL_IFACE_RAWMIDI);
				str += 7;
			} else if (!strncasecmp(str, "timer", 5)) {
				snd_control_id_set_interface(id, SND_CONTROL_IFACE_TIMER);
				str += 5;
			} else if (!strncasecmp(str, "sequencer", 9)) {
				snd_control_id_set_interface(id, SND_CONTROL_IFACE_SEQUENCER);
				str += 9;
			} else {
				return -EINVAL;
			}
		} else if (!strncasecmp(str, "name=", 5)) {
			char buf[64];
			str += 5;
			ptr = buf;
			size = 0;
			if (*str == '\'' || *str == '\"') {
				c = *str++;
				while (*str && *str != c) {
					if (size < sizeof(buf)) {
						*ptr++ = *str;
						size++;
					}
					str++;
				}
				if (*str == c)
					str++;
			} else {
				while (*str && *str != ',') {
					if (size < sizeof(buf)) {
						*ptr++ = *str;
						size++;
					}
					str++;
				}
				*ptr = '\0';
			}
			snd_control_id_set_name(id, buf);
		} else if (!strncasecmp(str, "index=", 6)) {
			str += 6;
			snd_control_id_set_index(id, atoi(str));
			while (isdigit(*str))
				str++;
		} else if (!strncasecmp(str, "device=", 7)) {
			str += 7;
			snd_control_id_set_device(id, atoi(str));
			while (isdigit(*str))
				str++;
		} else if (!strncasecmp(str, "subdevice=", 10)) {
			str += 10;
			snd_control_id_set_subdevice(id, atoi(str));
			while (isdigit(*str))
				str++;
		}
		if (*str == ',') {
			str++;
		} else {
			if (*str)
				return -EINVAL;
		}
	}			
	return 0;
}

static int parse_simple_id(const char *str, snd_mixer_sid_t *sid)
{
	int c, size;
	char *ptr;

	while (*str == ' ' || *str == '\t')
		str++;
	if (!(*str))
		return -EINVAL;
	memset(sid, 0, sizeof(*sid));
	ptr = sid->name;
	size = 0;
	if (*str != '"' && *str != '\'') {
		while (*str && *str != ',') {
			if (size < sizeof(sid->name)) {
				*ptr++ = *str;
				size++;
			}
			str++;
		}
	} else {
		c = *str++;
		while (*str && *str != c) {
			if (size < sizeof(sid->name)) {
				*ptr++ = *str;
				size++;
			}
			str++;
		}
		if (*str == c)
			str++;
	}
	if (*str == '\0')
		return 0;
	if (*str != ',')
		return -EINVAL;
	str++;
	if (!isdigit(*str))
		return -EINVAL;
	sid->index = atoi(str);
	return 0;
}

static int cset(int argc, char *argv[], int roflag)
{
	int err;
	snd_ctl_t *handle;
	snd_control_info_t *info;
	snd_control_id_t *id;
	snd_control_t *control;
	char *ptr;
	unsigned int idx, count;
	long tmp;
	snd_control_type_t type;
	snd_control_info_alloca(&info);
	snd_control_id_alloca(&id);
	snd_control_alloca(&control);

	if (argc < 1) {
		fprintf(stderr, "Specify a full control identifier: [[iface=<iface>,][name='name',][index=<index>,][device=<device>,][subdevice=<subdevice>]]|[numid=<numid>]\n");
		return -EINVAL;
	}
	if (parse_control_id(argv[0], id)) {
		fprintf(stderr, "Wrong control identifier: %s\n", argv[0]);
		return -EINVAL;
	}
	if (debugflag) {
		printf("VERIFY ID: ");
		show_control_id(id);
		printf("\n");
	}
	if ((err = snd_ctl_open(&handle, card)) < 0) {
		error("Control %s open error: %s\n", card, snd_strerror(err));
		return err;
	}
	snd_control_info_set_id(info, id);
	if ((err = snd_ctl_cinfo(handle, info)) < 0) {
		error("Control %s cinfo error: %s\n", card, snd_strerror(err));
		return err;
	}
	type = snd_control_info_get_type(info);
	count = snd_control_info_get_count(info);
	snd_control_set_id(control, id);
	
	if (!roflag) {
		ptr = argv[1];
		for (idx = 0; idx < count && idx < 128 && *ptr; idx++) {
			switch (snd_enum_to_int(type)) {
			case SND_CONTROL_TYPE_BOOLEAN:
				tmp = 0;
				if (!strncasecmp(ptr, "on", 2) || !strncasecmp(ptr, "up", 2)) {
					tmp = 1;
					ptr += 2;
				} else if (!strncasecmp(ptr, "yes", 3)) {
					tmp = 1;
					ptr += 3;
				} else if (atoi(ptr)) {
					tmp = 1;
					while (isdigit(*ptr))
						ptr++;
				}
				snd_control_set_boolean(control, idx, tmp);
				break;
			case SND_CONTROL_TYPE_INTEGER:
				tmp = get_integer(&ptr,
						  snd_control_info_get_min(info),
						  snd_control_info_get_max(info));
				snd_control_set_integer(control, idx, tmp);
				break;
			case SND_CONTROL_TYPE_ENUMERATED:
				tmp = get_integer(&ptr, 0, snd_control_info_get_items(info) - 1);
				snd_control_set_enumerated(control, idx, tmp);
				break;
			case SND_CONTROL_TYPE_BYTES:
				tmp = get_integer(&ptr, 0, 255);
				snd_control_set_byte(control, idx, tmp);
				break;
			default:
				break;
			}
			if (!strchr(argv[1], ','))
				ptr = argv[1];
			else if (*ptr == ',')
				ptr++;
		}
		if ((err = snd_ctl_cwrite(handle, control)) < 0) {
			error("Control %s cwrite error: %s\n", card, snd_strerror(err));
			return err;
		}
	}
	if (!quiet)
		show_control("  ", handle, id, 3);
	snd_ctl_close(handle);
	return 0;
}

typedef struct channel_mask {
	char *name;
	unsigned int mask;
} channel_mask_t;
static channel_mask_t chanmask[] = {
	{"frontleft", SND_MIXER_CHN_MASK_FRONT_LEFT},
	{"frontright", SND_MIXER_CHN_MASK_FRONT_RIGHT},
	{"frontcenter", SND_MIXER_CHN_MASK_FRONT_CENTER},
	{"front", SND_MIXER_CHN_MASK_FRONT_LEFT|SND_MIXER_CHN_MASK_FRONT_RIGHT},
	{"center", SND_MIXER_CHN_MASK_FRONT_CENTER},
	{"rearleft", SND_MIXER_CHN_MASK_REAR_LEFT},
	{"rearright", SND_MIXER_CHN_MASK_REAR_RIGHT},
	{"rear", SND_MIXER_CHN_MASK_REAR_LEFT|SND_MIXER_CHN_MASK_REAR_RIGHT},
	{"woofer", SND_MIXER_CHN_MASK_WOOFER},
	{NULL, 0}
};

static int check_channels(char *arg, unsigned int mask, unsigned int *mask_return)
{
	channel_mask_t *c;

	for (c = chanmask; c->name; c++) {
		if (! strncmp(arg, c->name, strlen(c->name))) {
			*mask_return = c->mask & mask;
			return 1;
		}
	}
	return 0;
}

static int sset(unsigned int argc, char *argv[], int roflag)
{
	int err;
	unsigned int idx;
	snd_mixer_channel_id_t chn;
	unsigned int channels;
	snd_mixer_t *handle;
	snd_mixer_sid_t sid;
	snd_mixer_simple_control_t control;
	char name[simple_name_size];

	if (argc < 1) {
		fprintf(stderr, "Specify a scontrol identifier: 'name',index\n");
		return 1;
	}
	if (parse_simple_id(argv[0], &sid)) {
		fprintf(stderr, "Wrong scontrol identifier: %s\n", argv[0]);
		return 1;
	}
	if (!roflag && argc < 2) {
		fprintf(stderr, "Specify what you want to set...\n");
		return 1;
	}
	if ((err = snd_mixer_open(&handle, card)) < 0) {
		error("Mixer %s open error: %s\n", card, snd_strerror(err));
		return err;
	}
	memset(&control, 0, sizeof(control));
	control.sid = sid;
	if ((err = snd_mixer_simple_control_read(handle, &control))<0) {
		error("Unable to read simple control '%s',%i: %s\n", simple_name(sid.name, name), sid.index, snd_strerror(err));
		snd_mixer_close(handle);
		return err;
	}
	if (roflag)
		goto __skip_write;
	channels = control.channels; /* all channels */
	for (idx = 1; idx < argc; idx++) {
		if (!strncmp(argv[idx], "mute", 4) ||
		    !strncmp(argv[idx], "off", 3)) {
			control.mute = control.channels;
			continue;
		} else if (!strncmp(argv[idx], "unmute", 6) ||
		           !strncmp(argv[idx], "on", 2)) {
			control.mute = 0;
			continue;
		} else if (!strncmp(argv[idx], "cap", 3) ||
		           !strncmp(argv[idx], "rec", 3)) {
			control.capture = control.channels;
			continue;
		} else if (!strncmp(argv[idx], "nocap", 5) ||
		           !strncmp(argv[idx], "norec", 5)) {
			control.capture = 0;
			continue;
		}
		if (check_channels(argv[idx], control.channels, &channels))
			continue;
		if (isdigit(argv[idx][0]) ||
		    argv[idx][0] == '+' ||
		    argv[idx][0] == '-') {
			char *ptr;
			int multi;
		
			multi = (strchr(argv[idx], ',') != NULL);
			ptr = argv[idx];
			for (chn = 0; chn <= SND_MIXER_CHN_LAST; snd_enum_incr(chn)) {
				int c = snd_enum_to_int(chn);
				if (!(control.channels & (1<<c)) ||
				    !(channels & (1<<c)))
					continue;

				if (! multi)
					ptr = argv[idx];
				control.volume.values[c] = get_volume_simple(&ptr, control.min, control.max, control.volume.values[c]);
			}
		} else {
			error("Unknown setup '%s'..\n", argv[idx]);
			snd_mixer_close(handle);
			return err;
		}
	} 
	if ((err = snd_mixer_simple_control_write(handle, &control))<0) {
		error("Unable to write control '%s',%i: %s\n", simple_name(sid.name, name), sid.index, snd_strerror(err));
		snd_mixer_close(handle);
		return err;
	}
      __skip_write:
	if (!quiet) {
		printf("Simple mixer control '%s',%i\n", simple_name(sid.name, name), sid.index);
		show_simple_control(handle, &sid, "  ", 1);
	}
	snd_mixer_close(handle);
	return 0;
}

static void events_change(snd_ctl_t *handle ATTRIBUTE_UNUSED, snd_hcontrol_t *hcontrol)
{
	snd_control_id_t *id;
	snd_control_id_alloca(&id);
	snd_hcontrol_get_id(hcontrol, id);
	printf("event change: ");
	show_control_id(id);
	printf("\n");
}

static void events_value(snd_ctl_t *handle ATTRIBUTE_UNUSED, snd_hcontrol_t *hcontrol)
{
	snd_control_id_t *id;
	snd_control_id_alloca(&id);
	snd_hcontrol_get_id(hcontrol, id);
	printf("event value: ");
	show_control_id(id);
	printf("\n");
}

static void events_remove(snd_ctl_t *handle ATTRIBUTE_UNUSED, snd_hcontrol_t *hcontrol)
{
	snd_control_id_t *id;
	snd_control_id_alloca(&id);
	snd_hcontrol_get_id(hcontrol, id);
	printf("event remove: ");
	show_control_id(id);
	printf("\n");
}

static void events_rebuild(snd_ctl_t *handle ATTRIBUTE_UNUSED, void *private_data)
{
	assert(private_data != (void *)1);
	printf("event rebuild\n");
}

static void events_add(snd_ctl_t *handle ATTRIBUTE_UNUSED, void *private_data, snd_hcontrol_t *hcontrol)
{
	snd_control_id_t *id;
	snd_control_id_alloca(&id);
	snd_hcontrol_get_id(hcontrol, id);
	assert(private_data != (void *)1);
	printf("event add: ");
	show_control_id(id);
	printf("\n");
	snd_hcontrol_set_callback_change(hcontrol, events_change);
	snd_hcontrol_set_callback_value(hcontrol, events_value);
	snd_hcontrol_set_callback_remove(hcontrol, events_remove);	
}

static int events(int argc ATTRIBUTE_UNUSED, char *argv[] ATTRIBUTE_UNUSED)
{
	snd_ctl_t *handle;
	snd_hcontrol_t *hcontrol;
	int err;

	if ((err = snd_ctl_open(&handle, card)) < 0) {
		error("Control %s open error: %s\n", card, snd_strerror(err));
		return err;
	}
	if ((err = snd_ctl_hbuild(handle, NULL)) < 0) {
		error("Control %s hbuild error: %s\n", card, snd_strerror(err));
		return err;
	}
	if ((err = snd_ctl_hcallback_rebuild(handle, events_rebuild, (void *)1)) < 0) {
		error("Control %s hcallback_rebuild error: %s\n", card, snd_strerror(err));
		return err;
	}
	if ((err = snd_ctl_hcallback_add(handle, events_add, (void *)2)) < 0) {
		error("Control %s hcallback_add error: %s\n", card, snd_strerror(err));
		return err;
	}
	for (hcontrol = snd_ctl_hfirst(handle); hcontrol; hcontrol = snd_ctl_hnext(handle, hcontrol)) {
		snd_hcontrol_set_callback_change(hcontrol, events_change);
		snd_hcontrol_set_callback_value(hcontrol, events_value);
		snd_hcontrol_set_callback_remove(hcontrol, events_remove);
	}
	printf("Ready to listen...\n");
	while (1) {
		struct pollfd ctl_poll;
		int res;
		ctl_poll.fd = snd_ctl_poll_descriptor(handle);
		ctl_poll.events = POLLIN;
		ctl_poll.revents = 0;
		if ((res = poll(&ctl_poll, 1, -1)) > 0) {
			printf("Poll ok: %i\n", res);
			res = snd_ctl_hevent(handle);
			if (res > 0)
				printf("%i events processed\n", res);
		}
	}
	snd_ctl_close(handle);
}

static void sevents_rebuild(snd_mixer_t *handle ATTRIBUTE_UNUSED, void *private_data ATTRIBUTE_UNUSED)
{
	printf("event rebuild\n");
}

static void sevents_value(snd_mixer_t *handle ATTRIBUTE_UNUSED, void *private_data ATTRIBUTE_UNUSED, snd_mixer_sid_t *sid)
{
	char name[simple_name_size];

	printf("event value: '%s',%i\n", simple_name(sid->name, name), sid->index);
}

static void sevents_change(snd_mixer_t *handle ATTRIBUTE_UNUSED, void *private_data ATTRIBUTE_UNUSED, snd_mixer_sid_t *sid)
{
	char name[simple_name_size];

	printf("event change: '%s',%i\n", simple_name(sid->name, name), sid->index);
}

static void sevents_add(snd_mixer_t *handle ATTRIBUTE_UNUSED, void *private_data ATTRIBUTE_UNUSED, snd_mixer_sid_t *sid)
{
	char name[simple_name_size];

	printf("event add: '%s',%i\n", simple_name(sid->name, name), sid->index);
}

static void sevents_remove(snd_mixer_t *handle ATTRIBUTE_UNUSED, void *private_data ATTRIBUTE_UNUSED, snd_mixer_sid_t *sid)
{
	char name[simple_name_size];

	printf("event remove: '%s',%i\n", simple_name(sid->name, name), sid->index);
}

static int sevents(int argc ATTRIBUTE_UNUSED, char *argv[] ATTRIBUTE_UNUSED)
{
	snd_mixer_t *handle;
	static snd_mixer_simple_callbacks_t callbacks = {
		private_data: NULL,
		rebuild: sevents_rebuild,
		value: sevents_value,
		change: sevents_change,
		add: sevents_add,
		remove: sevents_remove,
		reserved: { NULL, }
	};
	int err;

	if ((err = snd_mixer_open(&handle, card)) < 0) {
		error("Mixer %s open error: %s\n", card, snd_strerror(err));
		return err;
	}
	printf("Ready to listen...\n");
	while (1) {
		struct pollfd mixer_poll;
		int res;
		mixer_poll.fd = snd_mixer_poll_descriptor(handle);
		mixer_poll.events = POLLIN;
		mixer_poll.revents = 0;
		if ((res = poll(&mixer_poll, 1, -1)) > 0) {
			printf("Poll ok: %i\n", res);
			res = snd_mixer_simple_read(handle, &callbacks);
			if (res > 0)
				printf("%i events processed\n", res);
		}
	}
	snd_mixer_close(handle);
}

int main(int argc, char *argv[])
{
	int morehelp;
	struct option long_option[] =
	{
		{"help", 0, NULL, HELPID_HELP},
		{"card", 1, NULL, HELPID_CARD},
		{"quiet", 0, NULL, HELPID_QUIET},
		{"debug", 0, NULL, HELPID_DEBUG},
		{"version", 0, NULL, HELPID_VERSION},
		{NULL, 0, NULL, 0},
	};

	morehelp = 0;
	while (1) {
		int c;

		if ((c = getopt_long(argc, argv, "hc:qDv", long_option, NULL)) < 0)
			break;
		switch (c) {
		case 'h':
		case HELPID_HELP:
			morehelp++;
			break;
		case 'c':
		case HELPID_CARD:
			card = optarg;
			break;
		case 'q':
		case HELPID_QUIET:
			quiet = 1;
			break;
		case 'D':
		case HELPID_DEBUG:
			debugflag = 1;
			break;
		case 'v':
		case HELPID_VERSION:
			printf("amixer version " SND_UTIL_VERSION_STR "\n");
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
		return simple_controls(1) ? 1 : 0;
	}
	if (!strcmp(argv[optind], "help")) {
		return help() ? 1 : 0;
	} else if (!strcmp(argv[optind], "info")) {
		return info() ? 1 : 0;
	} else if (!strcmp(argv[optind], "controls")) {
		return controls(0) ? 1 : 0;
	} else if (!strcmp(argv[optind], "contents")) {
		return controls(1) ? 1 : 0;
	} else if (!strcmp(argv[optind], "scontrols") || !strcmp(argv[optind], "simple")) {
		return simple_controls(0) ? 1 : 0;
	} else if (!strcmp(argv[optind], "scontents")) {
		return simple_controls(1) ? 1 : 0;
	} else if (!strcmp(argv[optind], "sset") || !strcmp(argv[optind], "set")) {
		return sset(argc - optind - 1, argc - optind > 1 ? argv + optind + 1 : NULL, 0) ? 1 : 0;
	} else if (!strcmp(argv[optind], "sget") || !strcmp(argv[optind], "get")) {
		return sset(argc - optind - 1, argc - optind > 1 ? argv + optind + 1 : NULL, 1) ? 1 : 0;
	} else if (!strcmp(argv[optind], "cset")) {
		return cset(argc - optind - 1, argc - optind > 1 ? argv + optind + 1 : NULL, 0) ? 1 : 0;
	} else if (!strcmp(argv[optind], "cget")) {
		return cset(argc - optind - 1, argc - optind > 1 ? argv + optind + 1 : NULL, 1) ? 1 : 0;
	} else if (!strcmp(argv[optind], "events")) {
		return events(argc - optind - 1, argc - optind > 1 ? argv + optind + 1 : NULL);
	} else if (!strcmp(argv[optind], "sevents")) {
		return sevents(argc - optind - 1, argc - optind > 1 ? argv + optind + 1 : NULL);
	} else {
		fprintf(stderr, "amixer: Unknown command '%s'...\n", argv[optind]);
	}

	return 0;
}
