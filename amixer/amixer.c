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
	printf("  -c,--card N     select the card, default %s\n", card);
	printf("  -D,--debug      debug mode\n");
	printf("  -v,--version    print version of this program\n");
	printf("\nAvailable commands:\n");
	printf("  scontrols       show all mixer simple controls\n");
	printf("  scontents	  show contents of all mixer simple controls (default command)\n");
	printf("  sset sID P      set contents for one mixer simple control\n");
	printf("  sget sID        get contents for one mixer simple control\n");
	printf("  controls        show all controls for given card\n");
	printf("  contents        show contents of all controls for given card\n");
	printf("  cset cID P      set control contents for one control\n");
	printf("  cget cID        get control contents for one control\n");
	return 0;
}

static int info(void)
{
	int err;
	snd_ctl_t *handle;
	snd_mixer_t *mhandle;
	snd_ctl_card_info_t *info;
	snd_ctl_elem_list_t *clist;
	snd_ctl_card_info_alloca(&info);
	snd_ctl_elem_list_alloca(&clist);
	
	if ((err = snd_ctl_open(&handle, card, 0)) < 0) {
		error("Control device %i open error: %s", card, snd_strerror(err));
		return err;
	}
	if ((err = snd_ctl_card_info(handle, info)) < 0) {
		error("Control device %i hw info error: %s", card, snd_strerror(err));
		return err;
	}
	printf("Card %s '%s'/'%s'\n", card, snd_ctl_card_info_get_id(info),
	       snd_ctl_card_info_get_longname(info));
	printf("  Mixer ID	: '%s'\n", snd_ctl_card_info_get_mixerid(info));
	printf("  Mixer name	: '%s'\n", snd_ctl_card_info_get_mixername(info));
	if ((err = snd_ctl_elem_list(handle, clist)) < 0) {
		error("snd_ctl_elem_list failure: %s", snd_strerror(err));
	} else {
		printf("  Controls      : %i\n", snd_ctl_elem_list_get_count(clist));
	}
	snd_ctl_close(handle);
	if ((err = snd_mixer_open(&mhandle, 0)) < 0) {
		error("Mixer open error: %s", snd_strerror(err));
		return err;
	}
	if ((err = snd_mixer_attach(mhandle, card)) < 0) {
		error("Mixer attach %s error: %s", card, snd_strerror(err));
		snd_mixer_close(mhandle);
		return err;
	}
	if ((err = snd_mixer_selem_register(mhandle, NULL, NULL)) < 0) {
		error("Mixer register error: %s", snd_strerror(err));
		snd_mixer_close(mhandle);
		return err;
	}
	err = snd_mixer_load(mhandle);
	if (err < 0) {
		error("Mixer load error: %s", card, snd_strerror(err));
		snd_mixer_close(mhandle);
		return err;
	}
	printf("  Simple ctrls  : %i\n", snd_mixer_get_count(mhandle));
	snd_mixer_close(mhandle);
	return 0;
}

static const char *control_iface(snd_ctl_elem_id_t *id)
{
	return snd_ctl_elem_iface_name(snd_ctl_elem_id_get_interface(id));
}

static const char *control_type(snd_ctl_elem_info_t *info)
{
	return snd_ctl_elem_type_name(snd_ctl_elem_info_get_type(info));
}

static const char *control_access(snd_ctl_elem_info_t *info)
{
	static char result[10];
	char *res = result;

	*res++ = snd_ctl_elem_info_is_readable(info) ? 'r' : '-';
	*res++ = snd_ctl_elem_info_is_writable(info) ? 'w' : '-';
	*res++ = snd_ctl_elem_info_is_inactive(info) ? 'i' : '-';
	*res++ = snd_ctl_elem_info_is_volatile(info) ? 'v' : '-';
	*res++ = snd_ctl_elem_info_is_locked(info) ? 'l' : '-';
	*res++ = '\0';
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

static void show_control_id(snd_ctl_elem_id_t *id)
{
	unsigned int index, device, subdevice;
	printf("numid=%u,iface=%s,name='%s'",
	       snd_ctl_elem_id_get_numid(id),
	       control_iface(id),
	       snd_ctl_elem_id_get_name(id));
	index = snd_ctl_elem_id_get_index(id);
	device = snd_ctl_elem_id_get_device(id);
	subdevice = snd_ctl_elem_id_get_subdevice(id);
	if (index)
		printf(",index=%i", index);
	if (device)
		printf(",device=%i", device);
	if (subdevice)
		printf(",subdevice=%i", subdevice);
}

static int show_control(const char *space, snd_hctl_elem_t *elem,
			int level)
{
	int err;
	unsigned int item, idx;
	unsigned int count;
	snd_ctl_elem_type_t type;
	snd_ctl_elem_id_t *id;
	snd_ctl_elem_info_t *info;
	snd_ctl_elem_value_t *control;
	snd_ctl_elem_id_alloca(&id);
	snd_ctl_elem_info_alloca(&info);
	snd_ctl_elem_value_alloca(&control);
	if ((err = snd_hctl_elem_info(elem, info)) < 0) {
		error("Control %s cinfo error: %s\n", card, snd_strerror(err));
		return err;
	}
	if (level & 2) {
		snd_hctl_elem_get_id(elem, id);
		show_control_id(id);
		printf("\n");
	}
	count = snd_ctl_elem_info_get_count(info);
	type = snd_ctl_elem_info_get_type(info);
	printf("%s; type=%s,access=%s,values=%i", space, control_type(info), control_access(info), count);
	switch (snd_enum_to_int(type)) {
	case SND_CTL_ELEM_TYPE_INTEGER:
		printf(",min=%li,max=%li,step=%li\n", 
		       snd_ctl_elem_info_get_min(info),
		       snd_ctl_elem_info_get_max(info),
		       snd_ctl_elem_info_get_step(info));
		break;
	case SND_CTL_ELEM_TYPE_ENUMERATED:
	{
		unsigned int items = snd_ctl_elem_info_get_items(info);
		printf(",items=%u\n", items);
		for (item = 0; item < items; item++) {
			snd_ctl_elem_info_set_item(info, item);
			if ((err = snd_hctl_elem_info(elem, info)) < 0) {
				error("Control %s element info error: %s\n", card, snd_strerror(err));
				return err;
			}
			printf("%s; Item #%u '%s'\n", space, item, snd_ctl_elem_info_get_item_name(info));
		}
		break;
	}
	default:
		printf("\n");
		break;
	}
	if (level & 1) {
		if ((err = snd_hctl_elem_read(elem, control)) < 0) {
			error("Control %s element read error: %s\n", card, snd_strerror(err));
			return err;
		}
		printf("%s: values=", space);
		for (idx = 0; idx < count; idx++) {
			if (idx > 0)
				printf(",");
			switch (snd_enum_to_int(type)) {
			case SND_CTL_ELEM_TYPE_BOOLEAN:
				printf("%s", snd_ctl_elem_value_get_boolean(control, idx) ? "on" : "off");
				break;
			case SND_CTL_ELEM_TYPE_INTEGER:
				printf("%li", snd_ctl_elem_value_get_integer(control, idx));
				break;
			case SND_CTL_ELEM_TYPE_ENUMERATED:
				printf("%u", snd_ctl_elem_value_get_enumerated(control, idx));
				break;
			case SND_CTL_ELEM_TYPE_BYTES:
				printf("0x%02x", snd_ctl_elem_value_get_byte(control, idx));
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
	snd_hctl_t *handle;
	snd_hctl_elem_t *elem;
	snd_ctl_elem_id_t *id;
	snd_ctl_elem_id_alloca(&id);
	
	if ((err = snd_hctl_open(&handle, card, 0)) < 0) {
		error("Control %s open error: %s", card, snd_strerror(err));
		return err;
	}
	if ((err = snd_hctl_load(handle)) < 0) {
		error("Control %s local error: %s\n", card, snd_strerror(err));
		return err;
	}
	for (elem = snd_hctl_first_elem(handle); elem; elem = snd_hctl_elem_next(elem)) {
		snd_hctl_elem_get_id(elem, id);
		show_control_id(id);
		printf("\n");
		if (level > 0)
			show_control("  ", elem, 1);
	}
	snd_hctl_close(handle);
	return 0;
}

static int show_selem(snd_mixer_t *handle, snd_mixer_selem_id_t *id, const char *space, int level)
{
	snd_mixer_selem_channel_id_t chn;
	long pmin = 0, pmax = 0;
	long cmin = 0, cmax = 0;
	long pvol, cvol;
	int psw, csw;
	int mono;
	snd_mixer_elem_t *elem;
	
	elem = snd_mixer_find_selem(handle, id);
	if (!elem) {
		error("Mixer %s simple element not found", card);
		return -ENOENT;
	}

	if ((level & 1) != 0) {
		printf("%sCapabilities:", space);
		if (snd_mixer_selem_has_common_volume(elem)) {
			printf(" volume");
			if (snd_mixer_selem_has_playback_volume_joined(elem))
				printf(" volume-joined");
		} else if (snd_mixer_selem_has_playback_volume(elem)) {
			printf(" pvolume");
			if (snd_mixer_selem_has_playback_volume_joined(elem))
				printf(" pvolume-joined");
		}
		if (snd_mixer_selem_has_common_switch(elem)) {
			printf(" switch");
			if (snd_mixer_selem_has_playback_switch_joined(elem))
				printf(" switch-joined");
		} else if (snd_mixer_selem_has_playback_switch(elem)) {
			printf(" pswitch");
			if (snd_mixer_selem_has_playback_switch_joined(elem))
				printf(" pswitch-joined");
		}
		if (snd_mixer_selem_has_capture_volume(elem))
			printf(" cvolume");
		if (snd_mixer_selem_has_capture_volume_joined(elem))
			printf(" cvolume-joined");
		if (snd_mixer_selem_has_capture_switch(elem))
			printf(" cswitch");
		if (snd_mixer_selem_has_capture_switch_joined(elem))
			printf(" cswitch-joined");
		if (snd_mixer_selem_has_capture_switch_exclusive(elem))
			printf(" cswitch_exclusive");
		printf("\n");
		if (snd_mixer_selem_has_capture_switch_exclusive(elem))
			printf("%sCapture exclusive group: %i\n", space,
			       snd_mixer_selem_get_capture_group(elem));
		if (snd_mixer_selem_has_playback_volume(elem) ||
		    snd_mixer_selem_has_playback_switch(elem)) {
			printf("%sPlayback channels: ", space);
			if (snd_mixer_selem_is_playback_mono(elem)) {
				printf("Mono");
			} else {
				for (chn = 0; chn <= SND_MIXER_SCHN_LAST; snd_enum_incr(chn)){
					if (!snd_mixer_selem_has_playback_channel(elem, chn))
						continue;
					printf("%s ", snd_mixer_selem_channel_name(chn));
				}
			}
			printf("\n");
		}
		if (snd_mixer_selem_has_capture_volume(elem) ||
		    snd_mixer_selem_has_capture_switch(elem)) {
			printf("%sCapture channels: ", space);
			if (snd_mixer_selem_is_capture_mono(elem)) {
				printf("Mono");
			} else {
				for (chn = 0; chn <= SND_MIXER_SCHN_LAST; snd_enum_incr(chn)){
					if (!snd_mixer_selem_has_capture_channel(elem, chn))
						continue;
					printf("%s ", snd_mixer_selem_channel_name(chn));
				}
			}
			printf("\n");
		}
		if (snd_mixer_selem_has_playback_volume(elem) ||
		    snd_mixer_selem_has_capture_volume(elem)) {
			printf("%sLimits: ", space);
			if (snd_mixer_selem_has_playback_volume(elem)) {
			  
				snd_mixer_selem_get_playback_volume_range(elem, &pmin, &pmax);
				if (!snd_mixer_selem_has_common_volume(elem))
					printf("Playback ");
				printf("%li - %li ", pmin, pmax);
			}
			if (snd_mixer_selem_has_capture_volume(elem)) {
				snd_mixer_selem_get_capture_volume_range(elem, &cmin, &cmax);
				printf("Capture %li - %li", cmin, cmax);
			}
			printf("\n");
		}
		mono = ((snd_mixer_selem_is_playback_mono(elem) || 
			 (!snd_mixer_selem_has_playback_volume(elem) &&
			  !snd_mixer_selem_has_playback_switch(elem))) &&
			(snd_mixer_selem_is_capture_mono(elem) || 
			 (!snd_mixer_selem_has_capture_volume(elem) &&
			  !snd_mixer_selem_has_capture_switch(elem))));
		for (chn = 0; chn <= SND_MIXER_SCHN_LAST; snd_enum_incr(chn)) {
			if (!snd_mixer_selem_has_playback_channel(elem, chn) &&
			    !snd_mixer_selem_has_capture_channel(elem, chn))
				continue;
			printf("%s%s: ", space, mono ? "Mono" : snd_mixer_selem_channel_name(chn));
			if (snd_mixer_selem_has_playback_channel(elem, chn)) {
				if (!snd_mixer_selem_has_common_volume(elem))
					printf("Playback ");
				if (snd_mixer_selem_has_playback_volume(elem)) {
					snd_mixer_selem_get_playback_volume(elem, chn, &pvol);
					printf("%s ", get_percent(pvol, pmin, pmax));
				}
				if (snd_mixer_selem_has_playback_switch(elem)) {
					snd_mixer_selem_get_playback_switch(elem, chn, &psw);
					printf("[%s] ", psw ? "on" : "off");
				}
			}
			if (snd_mixer_selem_has_capture_channel(elem, chn)) {
				printf("Capture ");
				if (snd_mixer_selem_has_capture_volume(elem)) {
					snd_mixer_selem_get_capture_volume(elem, chn, &cvol);
					printf("%s ", get_percent(cvol, cmin, cmax));
				}
				if (snd_mixer_selem_has_capture_switch(elem)) {
					snd_mixer_selem_get_capture_switch(elem, chn, &csw);
					printf("[%s] ", csw ? "on" : "off");
				}
			}
			printf("\n");
		}
	}
	return 0;
}

static int selems(int level)
{
	int err;
	snd_mixer_t *handle;
	snd_mixer_selem_id_t *sid;
	snd_mixer_elem_t *elem;
	snd_mixer_selem_id_alloca(&sid);
	
	if ((err = snd_mixer_open(&handle, 0)) < 0) {
		error("Mixer %s open error: %s", card, snd_strerror(err));
		return err;
	}
	if ((err = snd_mixer_attach(handle, card)) < 0) {
		error("Mixer attach %s error: %s", card, snd_strerror(err));
		snd_mixer_close(handle);
		return err;
	}
	if ((err = snd_mixer_selem_register(handle, NULL, NULL)) < 0) {
		error("Mixer register error: %s", snd_strerror(err));
		snd_mixer_close(handle);
		return err;
	}
	err = snd_mixer_load(handle);
	if (err < 0) {
		error("Mixer load error: %s", card, snd_strerror(err));
		snd_mixer_close(handle);
		return err;
	}
	for (elem = snd_mixer_first_elem(handle); elem; elem = snd_mixer_elem_next(elem)) {
		snd_mixer_selem_get_id(elem, sid);
		printf("Simple mixer control '%s',%i\n", snd_mixer_selem_id_get_name(sid), snd_mixer_selem_id_get_index(sid));
		show_selem(handle, sid, "  ", level);
	}
	snd_mixer_close(handle);
	return 0;
}

static int parse_control_id(const char *str, snd_ctl_elem_id_t *id)
{
	int c, size;
	char *ptr;

	while (*str == ' ' || *str == '\t')
		str++;
	if (!(*str))
		return -EINVAL;
	snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);	/* default */
	while (*str) {
		if (!strncasecmp(str, "numid=", 6)) {
			str += 6;
			snd_ctl_elem_id_set_numid(id, atoi(str));
			while (isdigit(*str))
				str++;
		} else if (!strncasecmp(str, "iface=", 6)) {
			str += 6;
			if (!strncasecmp(str, "card", 4)) {
				snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_CARD);
				str += 4;
			} else if (!strncasecmp(str, "mixer", 5)) {
				snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);
				str += 5;
			} else if (!strncasecmp(str, "pcm", 3)) {
				snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_PCM);
				str += 3;
			} else if (!strncasecmp(str, "rawmidi", 7)) {
				snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_RAWMIDI);
				str += 7;
			} else if (!strncasecmp(str, "timer", 5)) {
				snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_TIMER);
				str += 5;
			} else if (!strncasecmp(str, "sequencer", 9)) {
				snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_SEQUENCER);
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
			snd_ctl_elem_id_set_name(id, buf);
		} else if (!strncasecmp(str, "index=", 6)) {
			str += 6;
			snd_ctl_elem_id_set_index(id, atoi(str));
			while (isdigit(*str))
				str++;
		} else if (!strncasecmp(str, "device=", 7)) {
			str += 7;
			snd_ctl_elem_id_set_device(id, atoi(str));
			while (isdigit(*str))
				str++;
		} else if (!strncasecmp(str, "subdevice=", 10)) {
			str += 10;
			snd_ctl_elem_id_set_subdevice(id, atoi(str));
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

static int parse_simple_id(const char *str, snd_mixer_selem_id_t *sid)
{
	int c, size;
	char buf[128];
	char *ptr = buf;

	while (*str == ' ' || *str == '\t')
		str++;
	if (!(*str))
		return -EINVAL;
	size = 0;
	if (*str != '"' && *str != '\'') {
		while (*str && *str != ',') {
			if (size < sizeof(buf)) {
				*ptr++ = *str;
				size++;
			}
			str++;
		}
	} else {
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
	}
	if (*str == '\0')
		return 0;
	if (*str != ',')
		return -EINVAL;
	*ptr = 0;	/* terminate the string */
	str++;
	if (!isdigit(*str))
		return -EINVAL;
	snd_mixer_selem_id_set_index(sid, atoi(str));
	snd_mixer_selem_id_set_name(sid, buf);
	return 0;
}

static int cset(int argc, char *argv[], int roflag)
{
	int err;
	snd_ctl_t *handle;
	snd_ctl_elem_info_t *info;
	snd_ctl_elem_id_t *id;
	snd_ctl_elem_value_t *control;
	char *ptr;
	unsigned int idx, count;
	long tmp;
	snd_ctl_elem_type_t type;
	snd_ctl_elem_info_alloca(&info);
	snd_ctl_elem_id_alloca(&id);
	snd_ctl_elem_value_alloca(&control);

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
	if ((err = snd_ctl_open(&handle, card, 0)) < 0) {
		error("Control %s open error: %s\n", card, snd_strerror(err));
		return err;
	}
	snd_ctl_elem_info_set_id(info, id);
	if ((err = snd_ctl_elem_info(handle, info)) < 0) {
		error("Control %s cinfo error: %s\n", card, snd_strerror(err));
		return err;
	}
	snd_ctl_elem_info_get_id(info, id);	/* FIXME: Remove it when hctl find works ok !!! */
	type = snd_ctl_elem_info_get_type(info);
	count = snd_ctl_elem_info_get_count(info);
	snd_ctl_elem_value_set_id(control, id);
	
	if (!roflag) {
		ptr = argv[1];
		for (idx = 0; idx < count && idx < 128 && *ptr; idx++) {
			switch (snd_enum_to_int(type)) {
			case SND_CTL_ELEM_TYPE_BOOLEAN:
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
				snd_ctl_elem_value_set_boolean(control, idx, tmp);
				break;
			case SND_CTL_ELEM_TYPE_INTEGER:
				tmp = get_integer(&ptr,
						  snd_ctl_elem_info_get_min(info),
						  snd_ctl_elem_info_get_max(info));
				snd_ctl_elem_value_set_integer(control, idx, tmp);
				break;
			case SND_CTL_ELEM_TYPE_ENUMERATED:
				tmp = get_integer(&ptr, 0, snd_ctl_elem_info_get_items(info) - 1);
				snd_ctl_elem_value_set_enumerated(control, idx, tmp);
				break;
			case SND_CTL_ELEM_TYPE_BYTES:
				tmp = get_integer(&ptr, 0, 255);
				snd_ctl_elem_value_set_byte(control, idx, tmp);
				break;
			default:
				break;
			}
			if (!strchr(argv[1], ','))
				ptr = argv[1];
			else if (*ptr == ',')
				ptr++;
		}
		if ((err = snd_ctl_elem_write(handle, control)) < 0) {
			error("Control %s element write error: %s\n", card, snd_strerror(err));
			return err;
		}
	}
	snd_ctl_close(handle);
	if (!quiet) {
		snd_hctl_t *hctl;
		snd_hctl_elem_t *elem;
		if ((err = snd_hctl_open(&hctl, card, 0)) < 0) {
			error("Control %s open error: %s\n", card, snd_strerror(err));
			return err;
		}
		if ((err = snd_hctl_load(hctl)) < 0) {
			error("Control %s load error: %s\n", card, snd_strerror(err));
			return err;
		}
		elem = snd_hctl_find_elem(hctl, id);
		if (elem)
			show_control("  ", elem, 3);
		else
			printf("Could not find the specified element\n");
		snd_hctl_close(hctl);
	}
	return 0;
}

typedef struct channel_mask {
	char *name;
	unsigned int mask;
} channel_mask_t;
static channel_mask_t chanmask[] = {
	{"frontleft", 1 << SND_MIXER_SCHN_FRONT_LEFT},
	{"frontright", 1 << SND_MIXER_SCHN_FRONT_RIGHT},
	{"frontcenter", 1 << SND_MIXER_SCHN_FRONT_CENTER},
	{"front", ((1 << SND_MIXER_SCHN_FRONT_LEFT) |
		   (1 << SND_MIXER_SCHN_FRONT_RIGHT))},
	{"center", 1 << SND_MIXER_SCHN_FRONT_CENTER},
	{"rearleft", 1 << SND_MIXER_SCHN_REAR_LEFT},
	{"rearright", 1 << SND_MIXER_SCHN_REAR_RIGHT},
	{"rear", ((1 << SND_MIXER_SCHN_REAR_LEFT) |
		  (1 << SND_MIXER_SCHN_REAR_RIGHT))},
	{"woofer", 1 << SND_MIXER_SCHN_WOOFER},
	{NULL, 0}
};

static unsigned int channels_mask(char *arg)
{
	channel_mask_t *c;

	for (c = chanmask; c->name; c++) {
		if (strncmp(arg, c->name, strlen(c->name)) == 0)
			return c->mask;
	}
	return ~0U;
}

static int sset(unsigned int argc, char *argv[], int roflag)
{
	int err;
	unsigned int idx;
	snd_mixer_selem_channel_id_t chn;
	unsigned int channels;
	long min, max;
	snd_mixer_t *handle;
	snd_mixer_elem_t *elem;
	snd_mixer_selem_id_t *sid;
	snd_mixer_selem_id_alloca(&sid);

	if (argc < 1) {
		fprintf(stderr, "Specify a scontrol identifier: 'name',index\n");
		return 1;
	}
	if (parse_simple_id(argv[0], sid)) {
		fprintf(stderr, "Wrong scontrol identifier: %s\n", argv[0]);
		return 1;
	}
	if (!roflag && argc < 2) {
		fprintf(stderr, "Specify what you want to set...\n");
		return 1;
	}
	if ((err = snd_mixer_open(&handle, 0)) < 0) {
		error("Mixer %s open error: %s\n", card, snd_strerror(err));
		return err;
	}
	if ((err = snd_mixer_attach(handle, card)) < 0) {
		error("Mixer attach %s error: %s", card, snd_strerror(err));
		snd_mixer_close(handle);
		return err;
	}
	if ((err = snd_mixer_selem_register(handle, NULL, NULL)) < 0) {
		error("Mixer register error: %s", snd_strerror(err));
		snd_mixer_close(handle);
		return err;
	}
	err = snd_mixer_load(handle);
	if (err < 0) {
		error("Mixer load error: %s", card, snd_strerror(err));
		snd_mixer_close(handle);
		return err;
	}
	elem = snd_mixer_find_selem(handle, sid);
	if (!elem) {
		error("Unable to find simple control '%s',%i: %s\n", snd_mixer_selem_id_get_name(sid), snd_mixer_selem_id_get_index(sid), snd_strerror(err));
		snd_mixer_close(handle);
		return -ENOENT;
	}
	snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
	if (roflag)
		goto __skip_write;
	for (idx = 1; idx < argc; idx++) {
		if (!strncmp(argv[idx], "mute", 4) ||
		    !strncmp(argv[idx], "off", 3)) {
			snd_mixer_selem_set_playback_switch_all(elem, 0);
			continue;
		} else if (!strncmp(argv[idx], "unmute", 6) ||
		           !strncmp(argv[idx], "on", 2)) {
			snd_mixer_selem_set_playback_switch_all(elem, 1);
			continue;
		} else if (!strncmp(argv[idx], "cap", 3) ||
		           !strncmp(argv[idx], "rec", 3)) {
			snd_mixer_selem_set_capture_switch_all(elem, 1);
			continue;
		} else if (!strncmp(argv[idx], "nocap", 5) ||
		           !strncmp(argv[idx], "norec", 5)) {
			snd_mixer_selem_set_capture_switch_all(elem, 0);
			continue;
		}
		channels = channels_mask(argv[idx]);
		if (isdigit(argv[idx][0]) ||
		    argv[idx][0] == '+' ||
		    argv[idx][0] == '-') {
			char *ptr;
			int multi;
		
			multi = (strchr(argv[idx], ',') != NULL);
			ptr = argv[idx];
			for (chn = 0; chn <= SND_MIXER_SCHN_LAST; snd_enum_incr(chn)) {
				long vol;
				if (!(channels & (1 << chn)) ||
				    !snd_mixer_selem_has_playback_channel(elem, chn))
					continue;

				if (! multi)
					ptr = argv[idx];
				snd_mixer_selem_get_playback_volume(elem, chn, &vol);
				snd_mixer_selem_set_playback_volume(elem, chn, get_volume_simple(&ptr, min, max, vol));
			}
		} else {
			error("Unknown setup '%s'..\n", argv[idx]);
			snd_mixer_close(handle);
			return err;
		}
	} 
      __skip_write:
	if (!quiet) {
		printf("Simple mixer control '%s',%i\n", snd_mixer_selem_id_get_name(sid), snd_mixer_selem_id_get_index(sid));
		show_selem(handle, sid, "  ", 1);
	}
	snd_mixer_close(handle);
	return 0;
}

static void events_info(snd_hctl_elem_t *helem)
{
	snd_ctl_elem_id_t *id;
	snd_ctl_elem_id_alloca(&id);
	snd_hctl_elem_get_id(helem, id);
	printf("event info: ");
	show_control_id(id);
	printf("\n");
}

static void events_value(snd_hctl_elem_t *helem)
{
	snd_ctl_elem_id_t *id;
	snd_ctl_elem_id_alloca(&id);
	snd_hctl_elem_get_id(helem, id);
	printf("event value: ");
	show_control_id(id);
	printf("\n");
}

static void events_remove(snd_hctl_elem_t *helem)
{
	snd_ctl_elem_id_t *id;
	snd_ctl_elem_id_alloca(&id);
	snd_hctl_elem_get_id(helem, id);
	printf("event remove: ");
	show_control_id(id);
	printf("\n");
}

int element_callback(snd_hctl_elem_t *elem, unsigned int mask)
{
	if (mask == SND_CTL_EVENT_MASK_REMOVE) {
		events_remove(elem);
		return 0;
	}
	if (mask & SND_CTL_EVENT_MASK_INFO) 
		events_info(elem);
	if (mask & SND_CTL_EVENT_MASK_VALUE) 
		events_value(elem);
	return 0;
}

static void events_add(snd_hctl_elem_t *helem)
{
	snd_ctl_elem_id_t *id;
	snd_ctl_elem_id_alloca(&id);
	snd_hctl_elem_get_id(helem, id);
	printf("event add: ");
	show_control_id(id);
	printf("\n");
	snd_hctl_elem_set_callback(helem, element_callback);
}

int ctl_callback(snd_hctl_t *ctl, unsigned int mask,
		 snd_hctl_elem_t *elem)
{
	if (mask & SND_CTL_EVENT_MASK_ADD)
		events_add(elem);
	return 0;
}

static int events(int argc ATTRIBUTE_UNUSED, char *argv[] ATTRIBUTE_UNUSED)
{
	snd_hctl_t *handle;
	snd_hctl_elem_t *helem;
	int err;

	if ((err = snd_hctl_open(&handle, card, 0)) < 0) {
		error("Control %s open error: %s\n", card, snd_strerror(err));
		return err;
	}
	snd_hctl_set_callback(handle, ctl_callback);
	if ((err = snd_hctl_load(handle)) < 0) {
		error("Control %s hbuild error: %s\n", card, snd_strerror(err));
		return err;
	}
	for (helem = snd_hctl_first_elem(handle); helem; helem = snd_hctl_elem_next(helem)) {
		snd_hctl_elem_set_callback(helem, element_callback);
	}
	printf("Ready to listen...\n");
	while (1) {
		int res = snd_hctl_wait(handle, -1);
		if (res >= 0) {
			printf("Poll ok: %i\n", res);
			res = snd_hctl_handle_events(handle);
			assert(res > 0);
		}
	}
	snd_hctl_close(handle);
}

static void sevents_value(snd_mixer_selem_id_t *sid)
{
	printf("event value: '%s',%i\n", snd_mixer_selem_id_get_name(sid), snd_mixer_selem_id_get_index(sid));
}

static void sevents_info(snd_mixer_selem_id_t *sid)
{
	printf("event info: '%s',%i\n", snd_mixer_selem_id_get_name(sid), snd_mixer_selem_id_get_index(sid));
}

static void sevents_remove(snd_mixer_selem_id_t *sid)
{
	printf("event remove: '%s',%i\n", snd_mixer_selem_id_get_name(sid), snd_mixer_selem_id_get_index(sid));
}

int melem_event(snd_mixer_elem_t *elem, unsigned int mask)
{
	snd_mixer_selem_id_t *sid;
	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_get_id(elem, sid);
	if (mask == SND_CTL_EVENT_MASK_REMOVE) {
		sevents_remove(sid);
		return 0;
	}
	if (mask & SND_CTL_EVENT_MASK_INFO) 
		sevents_info(sid);
	if (mask & SND_CTL_EVENT_MASK_VALUE) 
		sevents_value(sid);
	return 0;
}

static void sevents_add(snd_mixer_elem_t *elem)
{
	snd_mixer_selem_id_t *sid;
	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_get_id(elem, sid);
	printf("event add: '%s',%i\n", snd_mixer_selem_id_get_name(sid), snd_mixer_selem_id_get_index(sid));
	snd_mixer_elem_set_callback(elem, melem_event);
}

int mixer_event(snd_mixer_t *mixer, unsigned int mask,
		snd_mixer_elem_t *elem)
{
	if (mask & SND_CTL_EVENT_MASK_ADD)
		sevents_add(elem);
	return 0;
}

static int sevents(int argc ATTRIBUTE_UNUSED, char *argv[] ATTRIBUTE_UNUSED)
{
	snd_mixer_t *handle;
	int err;

	if ((err = snd_mixer_open(&handle, 0)) < 0) {
		error("Mixer %s open error: %s", card, snd_strerror(err));
		return err;
	}
	if ((err = snd_mixer_attach(handle, card)) < 0) {
		error("Mixer attach %s error: %s", card, snd_strerror(err));
		snd_mixer_close(handle);
		return err;
	}
	if ((err = snd_mixer_selem_register(handle, NULL, NULL)) < 0) {
		error("Mixer register error: %s", snd_strerror(err));
		snd_mixer_close(handle);
		return err;
	}
	snd_mixer_set_callback(handle, mixer_event);
	err = snd_mixer_load(handle);
	if (err < 0) {
		error("Mixer load error: %s", card, snd_strerror(err));
		snd_mixer_close(handle);
		return err;
	}

	printf("Ready to listen...\n");
	while (1) {
		int res;
		res = snd_mixer_wait(handle, -1);
		if (res >= 0) {
			printf("Poll ok: %i\n", res);
			res = snd_mixer_handle_events(handle);
			assert(res >= 0);
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
		return selems(1) ? 1 : 0;
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
		return selems(0) ? 1 : 0;
	} else if (!strcmp(argv[optind], "scontents")) {
		return selems(1) ? 1 : 0;
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
