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
#include <ansidecl.h>
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
char *card = "0";

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
	printf("  -c,--card #     use a card number (0-%i) or the card name, default %s\n", snd_cards() - 1, card);
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
	snd_ctl_hw_info_t info;
	snd_control_list_t clist;
	snd_mixer_simple_control_list_t slist;
	
	if ((err = snd_ctl_open(&handle, card)) < 0) {
		error("Control device %i open error: %s", card, snd_strerror(err));
		return err;
	}
	if ((err = snd_ctl_hw_info(handle, &info)) < 0) {
		error("Control device %i hw info error: %s", card, snd_strerror(err));
		return err;
	}
	printf("Card %s '%s'/'%s'\n", card, info.id, info.longname);
	printf("  Mixer ID	: '%s'\n", info.mixerid);
	printf("  Mixer name	: '%s'\n", info.mixername);
	memset(&clist, 0, sizeof(clist));
	if ((err = snd_ctl_clist(handle, &clist)) < 0) {
		error("snd_ctl_clist failure: %s", snd_strerror(err));
	} else {
		printf("  Controls      : %i\n", clist.controls);
	}
	snd_ctl_close(handle);
	if ((err = snd_mixer_open(&mhandle, card)) < 0) {
		error("Mixer device %i open error: %s", card, snd_strerror(err));
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

static snd_control_id_t __control_id ATTRIBUTE_UNUSED;
#define control_name_size (sizeof(__control_id.name)+1)

static char *control_name(const char *name, char *result)
{
	strncpy(result, name, control_name_size);
	result[control_name_size] = '\0';
	return result;
}

#define control_iface_size 16

static char *control_iface(snd_control_iface_t iface, char *result)
{
	char *s;

	switch (iface) {
	case SND_CONTROL_IFACE_CARD: s = "card"; break;
	case SND_CONTROL_IFACE_HWDEP: s = "hwdep"; break;
	case SND_CONTROL_IFACE_MIXER: s = "mixer"; break;
	case SND_CONTROL_IFACE_PCM: s = "pcm"; break;
	case SND_CONTROL_IFACE_RAWMIDI: s = "rawmidi"; break;
	case SND_CONTROL_IFACE_TIMER: s = "timer"; break;
	case SND_CONTROL_IFACE_SEQUENCER: s = "sequencer"; break;
	default: s = "unknown"; break;
	}
	return strcpy(result, s);
}

#define control_type_size 16

static char *control_type(snd_control_type_t type, char *result)
{
	char *s;

	switch (type) {
	case SND_CONTROL_TYPE_NONE: s = "none"; break;
	case SND_CONTROL_TYPE_BOOLEAN: s = "boolean"; break;
	case SND_CONTROL_TYPE_INTEGER: s = "integer"; break;
	case SND_CONTROL_TYPE_ENUMERATED: s = "enumerated"; break;
	case SND_CONTROL_TYPE_BYTES: s = "bytes"; break;
	default: s = "unknown"; break;
	}
	return strcpy(result, s);
}

#define control_access_size 32

static char *control_access(unsigned int access, char *result)
{
	char *res = result;

	*res++ = (access & SND_CONTROL_ACCESS_READ) ? 'r' : '-';
	*res++ = (access & SND_CONTROL_ACCESS_WRITE) ? 'w' : '-';
	*res++ = (access & SND_CONTROL_ACCESS_INACTIVE) ? 'i' : '-';
	*res++ = (access & SND_CONTROL_ACCESS_LOCK) ? 'l' : '-';
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
	char name[control_name_size];
	char iface[control_iface_size];

	printf("numid=%u,iface=%s,name='%s'", id->numid, control_iface(id->iface, iface), control_name(id->name, name));
	if (id->index)
		printf(",index=%i", id->index);
	if (id->device)
		printf(",device=%i", id->device);
	if (id->subdevice)
		printf(",subdevice=%i", id->subdevice);
}

static int show_control(const char *space, snd_ctl_t *handle, snd_control_id_t *id, int level)
{
	int err;
	unsigned int item, idx;
	snd_control_info_t info;
	snd_control_t control;
	char type[control_type_size];
	char access[control_access_size];
	
	memset(&info, 0, sizeof(info));
	info.id = *id;
	if ((err = snd_ctl_cinfo(handle, &info)) < 0) {
		error("Control %i cinfo error: %s\n", card, snd_strerror(err));
		return err;
	}
	if (level & 2) {
		show_control_id(&info.id);
		printf("\n");
	}
	printf("%s; type=%s,access=%s,values=%i", space, control_type(info.type, type), control_access(info.access, access), info.values_count);
	switch (info.type) {
	case SND_CONTROL_TYPE_INTEGER:
		printf(",min=%li,max=%li,step=%li\n", info.value.integer.min, info.value.integer.max, info.value.integer.step);
		break;
	case SND_CONTROL_TYPE_ENUMERATED:
		printf(",items=%u\n", info.value.enumerated.items);
		for (item = 0; item < info.value.enumerated.items; item++) {
			info.value.enumerated.item = item;
			if ((err = snd_ctl_cinfo(handle, &info)) < 0) {
				error("Control %i cinfo error: %s\n", card, snd_strerror(err));
				return err;
			}
			printf("%s; Item #%u '%s'\n", space, item, info.value.enumerated.name);
		}
		break;
	default:
		printf("\n");
		break;
	}
	if (level & 1) {
		memset(&control, 0, sizeof(control));
		control.id = *id;
		if ((err = snd_ctl_cread(handle, &control)) < 0) {
			error("Control %i cread error: %s\n", card, snd_strerror(err));
			return err;
		}
		printf("%s: values=", space);
		for (idx = 0; idx < info.values_count; idx++) {
			if (idx > 0)
				printf(",");
			switch (info.type) {
			case SND_CONTROL_TYPE_BOOLEAN:
				printf("%s", control.value.integer.value[idx] ? "on" : "off");
				break;
			case SND_CONTROL_TYPE_INTEGER:
				printf("%li", control.value.integer.value[idx]);
				break;
			case SND_CONTROL_TYPE_ENUMERATED:
				printf("%u", control.value.enumerated.item[idx]);
				break;
			case SND_CONTROL_TYPE_BYTES:
				printf("0x%02x", control.value.bytes.data[idx]);
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
	int err, idx;
	snd_ctl_t *handle;
	snd_hcontrol_list_t list;
	
	if ((err = snd_ctl_open(&handle, card)) < 0) {
		error("Control %i open error: %s", card, snd_strerror(err));
		return err;
	}
	if ((err = snd_ctl_hbuild(handle, NULL)) < 0) {
		error("Control %i hbuild error: %s\n", card, snd_strerror(err));
		return err;
	}
	memset(&list, 0, sizeof(list));
	if ((err = snd_ctl_hlist(handle, &list)) < 0) {
		error("Control %i clist error: %s", card, snd_strerror(err));
		return err;
	}
	list.pids = (snd_control_id_t *)malloc(list.controls * sizeof(snd_control_id_t));
	if (list.pids == NULL) {
		error("Not enough memory");
		return -ENOMEM;
	}
	list.controls_request = list.controls;
	if ((err = snd_ctl_hlist(handle, &list)) < 0) {
		error("Control %i hlist error: %s", card, snd_strerror(err));
		return err;
	}
	for (idx = 0; idx < list.controls; idx++) {
		show_control_id(list.pids + idx);
		printf("\n");
		if (level > 0)
			show_control("  ", handle, list.pids + idx, 1);
	}
	free(list.pids);
	snd_ctl_close(handle);
	return 0;
}

static int show_simple_control(void *handle, snd_mixer_sid_t *sid, const char *space, int level)
{
	int err, chn;
	snd_mixer_simple_control_t scontrol;
	
	bzero(&scontrol, sizeof(scontrol));
	scontrol.sid = *sid;
	if ((err = snd_mixer_simple_control_read(handle, &scontrol)) < 0) {
		error("Mixer %i simple_control error: %s", card, snd_strerror(err));
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
			for (chn = 0; chn <= SND_MIXER_CHN_LAST; chn++) {
				if (!(scontrol.channels & (1<<chn)))
					continue;
				printf("%s ", snd_mixer_simple_channel_name(chn));
			}
		}
		printf("\n");
		printf("%sLimits: min = %li, max = %li\n", space, scontrol.min, scontrol.max);
		if (scontrol.channels == SND_MIXER_CHN_MASK_MONO) {
			printf("%sMono: %s [%s]\n", space, get_percent(scontrol.volume.names.front_left, scontrol.min, scontrol.max), scontrol.mute & SND_MIXER_CHN_MASK_MONO ? "mute" : "on");
		} else {
			for (chn = 0; chn <= SND_MIXER_CHN_LAST; chn++) {
				if (!(scontrol.channels & (1<<chn)))
					continue;
				printf("%s%s: %s [%s] [%s]\n",
						space,
						snd_mixer_simple_channel_name(chn),
						get_percent(scontrol.volume.values[chn], scontrol.min, scontrol.max),
						scontrol.mute & (1<<chn) ? "mute" : "on",
						scontrol.capture & (1<<chn) ? "capture" : "---");
			}
		}
	}
	return 0;
}

static int simple_controls(int level)
{
	int err, idx;
	snd_mixer_t *handle;
	snd_mixer_simple_control_list_t list;
	snd_mixer_sid_t *sid;
	char name[simple_name_size];
	
	if ((err = snd_mixer_open(&handle, card)) < 0) {
		error("Mixer %i open error: %s", card, snd_strerror(err));
		return err;
	}
	memset(&list, 0, sizeof(list));
	if ((err = snd_mixer_simple_control_list(handle, &list)) < 0) {
		error("Mixer %i simple_control_list error: %s", card, snd_strerror(err));
		return err;
	}
	list.pids = (snd_mixer_sid_t *)malloc(list.controls * sizeof(snd_mixer_sid_t));
	if (list.pids == NULL) {
		error("Not enough memory");
		return -ENOMEM;
	}
	list.controls_request = list.controls;
	if ((err = snd_mixer_simple_control_list(handle, &list)) < 0) {
		error("Mixer %i simple_control_list (2) error: %s", card, snd_strerror(err));
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
	memset(id, 0, sizeof(*id));
	id->iface = SND_CONTROL_IFACE_MIXER;	/* default */
	while (*str) {
		if (!strncasecmp(str, "numid=", 6)) {
			id->numid = atoi(str += 6);
			while (isdigit(*str))
				str++;
		} else if (!strncasecmp(str, "iface=", 6)) {
			str += 6;
			if (!strncasecmp(str, "card", 4)) {
				id->iface = SND_CONTROL_IFACE_CARD;
				str += 4;
			} else if (!strncasecmp(str, "mixer", 5)) {
				id->iface = SND_CONTROL_IFACE_MIXER;
				str += 5;
			} else if (!strncasecmp(str, "pcm", 3)) {
				id->iface = SND_CONTROL_IFACE_PCM;
				str += 3;
			} else if (!strncasecmp(str, "rawmidi", 7)) {
				id->iface = SND_CONTROL_IFACE_RAWMIDI;
				str += 7;
			} else if (!strncasecmp(str, "timer", 5)) {
				id->iface = SND_CONTROL_IFACE_TIMER;
				str += 5;
			} else if (!strncasecmp(str, "sequencer", 9)) {
				id->iface = SND_CONTROL_IFACE_SEQUENCER;
				str += 9;
			} else {
				return -EINVAL;
			}
		} else if (!strncasecmp(str, "name=", 5)) {
			str += 5;
			ptr = id->name;
			size = 0;
			if (*str == '\'' || *str == '\"') {
				c = *str++;
				while (*str && *str != c) {
					if (size < sizeof(id->name)) {
						*ptr++ = *str;
						size++;
					}
					str++;
				}
				if (*str == c)
					str++;
			} else {
				while (*str && *str != ',') {
					if (size < sizeof(id->name)) {
						*ptr++ = *str;
						size++;
					}
					str++;
				}
				*ptr = '\0';
			}
		} else if (!strncasecmp(str, "index=", 6)) {
			id->index = atoi(str += 6);
			while (isdigit(*str))
				str++;
		} else if (!strncasecmp(str, "device=", 7)) {
			id->device = atoi(str += 7);
			while (isdigit(*str))
				str++;
		} else if (!strncasecmp(str, "subdevice=", 10)) {
			id->subdevice = atoi(str += 10);
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
	snd_control_info_t info;
	snd_control_id_t id;
	snd_control_t control;
	char *ptr;
	int idx;
	long tmp;

	if (argc < 1) {
		fprintf(stderr, "Specify a full control identifier: [[iface=<iface>,][name='name',][index=<index>,][device=<device>,][subdevice=<subdevice>]]|[numid=<numid>]\n");
		return -EINVAL;
	}
	if (parse_control_id(argv[0], &id)) {
		fprintf(stderr, "Wrong control identifier: %s\n", argv[0]);
		return -EINVAL;
	}
	if (debugflag) {
		printf("VERIFY ID: ");
		show_control_id(&id);
		printf("\n");
	}
	if ((err = snd_ctl_open(&handle, card)) < 0) {
		error("Control %i open error: %s\n", card, snd_strerror(err));
		return err;
	}
	memset(&info, 0, sizeof(info));
	memset(&control, 0, sizeof(control));
	info.id = id;
	control.id = id;
	if ((err = snd_ctl_cinfo(handle, &info)) < 0) {
		error("Control %i cinfo error: %s\n", card, snd_strerror(err));
		return err;
	}
	if (!roflag) {
		ptr = argv[1];
		for (idx = 0; idx < info.values_count && idx < 128 && *ptr; idx++) {
			switch (info.type) {
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
				control.value.integer.value[idx] = tmp;
				break;
			case SND_CONTROL_TYPE_INTEGER:
				tmp = get_integer(&ptr,
						  info.value.integer.min,
						  info.value.integer.max);
				control.value.integer.value[idx] = tmp;
				break;
			case SND_CONTROL_TYPE_ENUMERATED:
				tmp = get_integer(&ptr, 0, info.value.enumerated.items - 1);
				control.value.enumerated.item[idx] = tmp;
				break;
			case SND_CONTROL_TYPE_BYTES:
				tmp = get_integer(&ptr, 0, info.value.enumerated.items - 1);
				control.value.bytes.data[idx] = tmp;
				break;
			default:
				break;
			}
			if (!strchr(argv[1], ','))
				ptr = argv[1];
			else if (*ptr == ',')
				ptr++;
		}
		if ((err = snd_ctl_cwrite(handle, &control)) < 0) {
			error("Control %i cwrite error: %s\n", card, snd_strerror(err));
			return err;
		}
	}
	if (!quiet)
		show_control("  ", handle, &id, 3);
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
	{NULL}
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

static int sset(int argc, char *argv[], int roflag)
{
	int err, idx, chn;
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
		error("Mixer %i open error: %s\n", card, snd_strerror(err));
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
			for (chn = 0; chn <= SND_MIXER_CHN_LAST; chn++) {
				if (!(control.channels & (1<<chn)) ||
				    !(channels & (1<<chn)))
					continue;

				if (! multi)
					ptr = argv[idx];
				control.volume.values[chn] = get_volume_simple(&ptr, control.min, control.max, control.volume.values[chn]);
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

static void events_change(snd_ctl_t *handle, snd_hcontrol_t *hcontrol)
{
	printf("event change: ");
	show_control_id(&hcontrol->id);
	printf("\n");
}

static void events_value(snd_ctl_t *handle, snd_hcontrol_t *hcontrol)
{
	printf("event value: ");
	show_control_id(&hcontrol->id);
	printf("\n");
}

static void events_remove(snd_ctl_t *handle, snd_hcontrol_t *hcontrol)
{
	printf("event remove: ");
	show_control_id(&hcontrol->id);
	printf("\n");
}

static void events_rebuild(snd_ctl_t *handle, void *private_data)
{
	assert(private_data != (void *)1);
	printf("event rebuild\n");
}

static void events_add(snd_ctl_t *handle, void *private_data, snd_hcontrol_t *hcontrol)
{
	assert(private_data != (void *)1);
	printf("event add: ");
	show_control_id(&hcontrol->id);
	printf("\n");
	hcontrol->event_change = events_change;
	hcontrol->event_value = events_value;
	hcontrol->event_remove = events_remove;	
}

static int events(int argc, char *argv[])
{
	snd_ctl_t *handle;
	snd_hcontrol_t *hcontrol;
	int err;

	if ((err = snd_ctl_open(&handle, card)) < 0) {
		error("Control %i open error: %s\n", card, snd_strerror(err));
		return err;
	}
	if ((err = snd_ctl_hbuild(handle, NULL)) < 0) {
		error("Control %u hbuild error: %s\n", card, snd_strerror(err));
		return err;
	}
	if ((err = snd_ctl_hcallback_rebuild(handle, events_rebuild, (void *)1)) < 0) {
		error("Control %u hcallback_rebuild error: %s\n", card, snd_strerror(err));
		return err;
	}
	if ((err = snd_ctl_hcallback_add(handle, events_add, (void *)2)) < 0) {
		error("Control %u hcallback_add error: %s\n", card, snd_strerror(err));
		return err;
	}
	for (hcontrol = snd_ctl_hfirst(handle); hcontrol; hcontrol = snd_ctl_hnext(handle, hcontrol)) {
		hcontrol->event_change = events_change;
		hcontrol->event_value = events_value;
		hcontrol->event_remove = events_remove;
	}
	printf("Ready to listen...\n");
	while (1) {
		struct pollfd ctl_poll;
		int res;
		ctl_poll.fd = snd_ctl_file_descriptor(handle);
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

static void sevents_rebuild(snd_mixer_t *handle, void *private_data)
{
	printf("event rebuild\n");
}

static void sevents_value(snd_mixer_t *handle, void *private_data, snd_mixer_sid_t *sid)
{
	char name[simple_name_size];

	printf("event value: '%s',%i\n", simple_name(sid->name, name), sid->index);
}

static void sevents_change(snd_mixer_t *handle, void *private_data, snd_mixer_sid_t *sid)
{
	char name[simple_name_size];

	printf("event change: '%s',%i\n", simple_name(sid->name, name), sid->index);
}

static void sevents_add(snd_mixer_t *handle, void *private_data, snd_mixer_sid_t *sid)
{
	char name[simple_name_size];

	printf("event add: '%s',%i\n", simple_name(sid->name, name), sid->index);
}

static void sevents_remove(snd_mixer_t *handle, void *private_data, snd_mixer_sid_t *sid)
{
	char name[simple_name_size];

	printf("event remove: '%s',%i\n", simple_name(sid->name, name), sid->index);
}

static int sevents(int argc, char *argv[])
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
		error("Mixer %i open error: %s\n", card, snd_strerror(err));
		return err;
	}
	printf("Ready to listen...\n");
	while (1) {
		struct pollfd mixer_poll;
		int res;
		mixer_poll.fd = snd_mixer_file_descriptor(handle);
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
