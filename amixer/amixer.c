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
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
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
#include <alsa/asoundlib.h>
#include <sys/poll.h>
#include "amixer.h"

#define HELPID_HELP             1000
#define HELPID_CARD             1001
#define HELPID_DEVICE		1002
#define HELPID_QUIET		1003
#define HELPID_INACTIVE		1004
#define HELPID_DEBUG            1005
#define HELPID_VERSION		1006
#define HELPID_NOCHECK		1007
#define HELPID_ABSTRACT		1008

#define LEVEL_BASIC		(1<<0)
#define LEVEL_INACTIVE		(1<<1)
#define LEVEL_ID		(1<<2)

int quiet = 0;
int debugflag = 0;
int no_check = 0;
int smixer_level = 0;
struct snd_mixer_selem_regopt smixer_options;
char card[64] = "default";

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
	printf("  -c,--card N     select the card\n");
	printf("  -D,--device N   select the device, default '%s'\n", card);
	printf("  -d,--debug      debug mode\n");
	printf("  -n,--nocheck    do not perform range checking\n");
	printf("  -v,--version    print version of this program\n");
	printf("  -q,--quiet      be quiet\n");
	printf("  -i,--inactive   show also inactive controls\n");
	printf("  -a,--abstract L select abstraction level (none or basic)\n");
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
		error("Control device %s open error: %s", card, snd_strerror(err));
		return err;
	}
	
	if ((err = snd_ctl_card_info(handle, info)) < 0) {
		error("Control device %s hw info error: %s", card, snd_strerror(err));
		return err;
	}
	printf("Card %s '%s'/'%s'\n", card, snd_ctl_card_info_get_id(info),
	       snd_ctl_card_info_get_longname(info));
	printf("  Mixer name	: '%s'\n", snd_ctl_card_info_get_mixername(info));
	printf("  Components	: '%s'\n", snd_ctl_card_info_get_components(info));
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
	if (smixer_level == 0 && (err = snd_mixer_attach(mhandle, card)) < 0) {
		error("Mixer attach %s error: %s", card, snd_strerror(err));
		snd_mixer_close(mhandle);
		return err;
	}
	if ((err = snd_mixer_selem_register(mhandle, smixer_level > 0 ? &smixer_options : NULL, NULL)) < 0) {
		error("Mixer register error: %s", snd_strerror(err));
		snd_mixer_close(mhandle);
		return err;
	}
	err = snd_mixer_load(mhandle);
	if (err < 0) {
		error("Mixer load %s error: %s", card, snd_strerror(err));
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
	if (no_check)
		return val;
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

static long get_integer64(char **ptr, long long min, long long max)
{
	long long tmp, tmp1, tmp2;

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

static int get_bool_simple(char **ptr, char *str, int invert, int orig)
{
	if (**ptr == ':')
		(*ptr)++;
	if (!strncasecmp(*ptr, str, strlen(str))) {
		orig = 1 ^ (invert ? 1 : 0);
		while (**ptr != '\0' && **ptr != ',' && **ptr != ':')
			(*ptr)++;
	}
	if (**ptr == ',' || **ptr == ':')
		(*ptr)++;
	return orig;
}
		
static int simple_skip_word(char **ptr, char *str)
{
	char *xptr = *ptr;
	if (*xptr == ':')
		xptr++;
	if (!strncasecmp(xptr, str, strlen(str))) {
		while (*xptr != '\0' && *xptr != ',' && *xptr != ':')
			xptr++;
		if (*xptr == ',' || *xptr == ':')
			xptr++;
		*ptr = xptr;
		return 1;
	}
	return 0;
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
		error("Control %s snd_hctl_elem_info error: %s\n", card, snd_strerror(err));
		return err;
	}
	if (level & LEVEL_ID) {
		snd_hctl_elem_get_id(elem, id);
		show_control_id(id);
		printf("\n");
	}
	count = snd_ctl_elem_info_get_count(info);
	type = snd_ctl_elem_info_get_type(info);
	printf("%s; type=%s,access=%s,values=%i", space, control_type(info), control_access(info), count);
	switch (type) {
	case SND_CTL_ELEM_TYPE_INTEGER:
		printf(",min=%li,max=%li,step=%li\n", 
		       snd_ctl_elem_info_get_min(info),
		       snd_ctl_elem_info_get_max(info),
		       snd_ctl_elem_info_get_step(info));
		break;
	case SND_CTL_ELEM_TYPE_INTEGER64:
		printf(",min=%Li,max=%Li,step=%Li\n", 
		       snd_ctl_elem_info_get_min64(info),
		       snd_ctl_elem_info_get_max64(info),
		       snd_ctl_elem_info_get_step64(info));
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
	if (level & LEVEL_BASIC) {
		if ((err = snd_hctl_elem_read(elem, control)) < 0) {
			error("Control %s element read error: %s\n", card, snd_strerror(err));
			return err;
		}
		printf("%s: values=", space);
		for (idx = 0; idx < count; idx++) {
			if (idx > 0)
				printf(",");
			switch (type) {
			case SND_CTL_ELEM_TYPE_BOOLEAN:
				printf("%s", snd_ctl_elem_value_get_boolean(control, idx) ? "on" : "off");
				break;
			case SND_CTL_ELEM_TYPE_INTEGER:
				printf("%li", snd_ctl_elem_value_get_integer(control, idx));
				break;
			case SND_CTL_ELEM_TYPE_INTEGER64:
				printf("%Li", snd_ctl_elem_value_get_integer64(control, idx));
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
	snd_ctl_elem_info_t *info;
	snd_ctl_elem_id_alloca(&id);
	snd_ctl_elem_info_alloca(&info);
	
	if ((err = snd_hctl_open(&handle, card, 0)) < 0) {
		error("Control %s open error: %s", card, snd_strerror(err));
		return err;
	}
	if ((err = snd_hctl_load(handle)) < 0) {
		error("Control %s local error: %s\n", card, snd_strerror(err));
		return err;
	}
	for (elem = snd_hctl_first_elem(handle); elem; elem = snd_hctl_elem_next(elem)) {
		if ((err = snd_hctl_elem_info(elem, info)) < 0) {
			error("Control %s snd_hctl_elem_info error: %s\n", card, snd_strerror(err));
			return err;
		}
		if (!(level & LEVEL_INACTIVE) && snd_ctl_elem_info_is_inactive(info))
			continue;
		snd_hctl_elem_get_id(elem, id);
		show_control_id(id);
		printf("\n");
		if (level & LEVEL_BASIC)
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
	int pmono, cmono, mono_ok = 0;
	snd_mixer_elem_t *elem;
	
	elem = snd_mixer_find_selem(handle, id);
	if (!elem) {
		error("Mixer %s simple element not found", card);
		return -ENOENT;
	}

	if (level & LEVEL_BASIC) {
		printf("%sCapabilities:", space);
		if (snd_mixer_selem_has_common_volume(elem)) {
			printf(" volume");
			if (snd_mixer_selem_has_playback_volume_joined(elem))
				printf(" volume-joined");
		} else {
			if (snd_mixer_selem_has_playback_volume(elem)) {
				printf(" pvolume");
				if (snd_mixer_selem_has_playback_volume_joined(elem))
					printf(" pvolume-joined");
			}
			if (snd_mixer_selem_has_capture_volume(elem)) {
				printf(" cvolume");
				if (snd_mixer_selem_has_capture_volume_joined(elem))
					printf(" cvolume-joined");
			}
		}
		if (snd_mixer_selem_has_common_switch(elem)) {
			printf(" switch");
			if (snd_mixer_selem_has_playback_switch_joined(elem))
				printf(" switch-joined");
		} else {
			if (snd_mixer_selem_has_playback_switch(elem)) {
				printf(" pswitch");
				if (snd_mixer_selem_has_playback_switch_joined(elem))
					printf(" pswitch-joined");
			}
			if (snd_mixer_selem_has_capture_switch(elem)) {
				printf(" cswitch");
				if (snd_mixer_selem_has_capture_switch_joined(elem))
					printf(" cswitch-joined");
				if (snd_mixer_selem_has_capture_switch_exclusive(elem))
					printf(" cswitch-exclusive");
			}
		}
		if (snd_mixer_selem_is_enumerated(elem)) {
			printf(" enum");
		}
		printf("\n");
		if (snd_mixer_selem_is_enumerated(elem)) {
			int i, items;
			unsigned int idx;
			char itemname[40];
			items = snd_mixer_selem_get_enum_items(elem);
			printf("  Items:");
			for (i = 0; i < items; i++) {
				snd_mixer_selem_get_enum_item_name(elem, i, sizeof(itemname) - 1, itemname);
				printf(" '%s'", itemname);
			}
			printf("\n");
			for (i = 0; !snd_mixer_selem_get_enum_item(elem, i, &idx); i++) {
				snd_mixer_selem_get_enum_item_name(elem, idx, sizeof(itemname) - 1, itemname);
				printf("  Item%d: '%s'\n", i, itemname);
			}
			return 0; /* no more thing to do */
		}
		if (snd_mixer_selem_has_capture_switch_exclusive(elem))
			printf("%sCapture exclusive group: %i\n", space,
			       snd_mixer_selem_get_capture_group(elem));
		if (snd_mixer_selem_has_playback_volume(elem) ||
		    snd_mixer_selem_has_playback_switch(elem)) {
			printf("%sPlayback channels:", space);
			if (snd_mixer_selem_is_playback_mono(elem)) {
				printf(" Mono");
			} else {
				int first = 1;
				for (chn = 0; chn <= SND_MIXER_SCHN_LAST; chn++){
					if (!snd_mixer_selem_has_playback_channel(elem, chn))
						continue;
					if (!first)
						printf(" -");
					printf(" %s", snd_mixer_selem_channel_name(chn));
					first = 0;
				}
			}
			printf("\n");
		}
		if (snd_mixer_selem_has_capture_volume(elem) ||
		    snd_mixer_selem_has_capture_switch(elem)) {
			printf("%sCapture channels:", space);
			if (snd_mixer_selem_is_capture_mono(elem)) {
				printf(" Mono");
			} else {
				int first = 1;
				for (chn = 0; chn <= SND_MIXER_SCHN_LAST; chn++){
					if (!snd_mixer_selem_has_capture_channel(elem, chn))
						continue;
					if (!first)
						printf(" -");
					printf(" %s", snd_mixer_selem_channel_name(chn));
					first = 0;
				}
			}
			printf("\n");
		}
		if (snd_mixer_selem_has_playback_volume(elem) ||
		    snd_mixer_selem_has_capture_volume(elem)) {
			printf("%sLimits:", space);
			if (snd_mixer_selem_has_common_volume(elem)) {
				snd_mixer_selem_get_playback_volume_range(elem, &pmin, &pmax);
				snd_mixer_selem_get_capture_volume_range(elem, &cmin, &cmax);
				printf(" %li - %li", pmin, pmax);
			} else {
				if (snd_mixer_selem_has_playback_volume(elem)) {
					snd_mixer_selem_get_playback_volume_range(elem, &pmin, &pmax);
					printf(" Playback %li - %li", pmin, pmax);
				}
				if (snd_mixer_selem_has_capture_volume(elem)) {
					snd_mixer_selem_get_capture_volume_range(elem, &cmin, &cmax);
					printf(" Capture %li - %li", cmin, cmax);
				}
			}
			printf("\n");
		}
		pmono = snd_mixer_selem_has_playback_channel(elem, SND_MIXER_SCHN_MONO) &&
		        (snd_mixer_selem_is_playback_mono(elem) || 
			 (!snd_mixer_selem_has_playback_volume(elem) &&
			  !snd_mixer_selem_has_playback_switch(elem)));
		cmono = snd_mixer_selem_has_capture_channel(elem, SND_MIXER_SCHN_MONO) &&
		        (snd_mixer_selem_is_capture_mono(elem) || 
			 (!snd_mixer_selem_has_capture_volume(elem) &&
			  !snd_mixer_selem_has_capture_switch(elem)));
#if 0
		printf("pmono = %i, cmono = %i (%i, %i, %i, %i)\n", pmono, cmono,
				snd_mixer_selem_has_capture_channel(elem, SND_MIXER_SCHN_MONO),
				snd_mixer_selem_is_capture_mono(elem),
				snd_mixer_selem_has_capture_volume(elem),
				snd_mixer_selem_has_capture_switch(elem));
#endif
		if (pmono || cmono) {
			if (!mono_ok) {
				printf("%s%s:", space, "Mono");
				mono_ok = 1;
			}
			if (snd_mixer_selem_has_common_volume(elem)) {
				snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_MONO, &pvol);
				printf(" %s", get_percent(pvol, pmin, pmax));
			}
			if (snd_mixer_selem_has_common_switch(elem)) {
				snd_mixer_selem_get_playback_switch(elem, SND_MIXER_SCHN_MONO, &psw);
				printf(" [%s]", psw ? "on" : "off");
			}
		}
		if (pmono && snd_mixer_selem_has_playback_channel(elem, SND_MIXER_SCHN_MONO)) {
			int title = 0;
			if (!mono_ok) {
				printf("%s%s:", space, "Mono");
				mono_ok = 1;
			}
			if (!snd_mixer_selem_has_common_volume(elem)) {
				if (snd_mixer_selem_has_playback_volume(elem)) {
					printf(" Playback");
					title = 1;
					snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_MONO, &pvol);
					printf(" %s", get_percent(pvol, pmin, pmax));
				}
			}
			if (!snd_mixer_selem_has_common_switch(elem)) {
				if (snd_mixer_selem_has_playback_switch(elem)) {
					if (!title)
						printf(" Playback");
					snd_mixer_selem_get_playback_switch(elem, SND_MIXER_SCHN_MONO, &psw);
					printf(" [%s]", psw ? "on" : "off");
				}
			}
		}
		if (cmono && snd_mixer_selem_has_capture_channel(elem, SND_MIXER_SCHN_MONO)) {
			int title = 0;
			if (!mono_ok) {
				printf("%s%s:", space, "Mono");
				mono_ok = 1;
			}
			if (!snd_mixer_selem_has_common_volume(elem)) {
				if (snd_mixer_selem_has_capture_volume(elem)) {
					printf(" Capture");
					title = 1;
					snd_mixer_selem_get_capture_volume(elem, SND_MIXER_SCHN_MONO, &cvol);
					printf(" %s", get_percent(cvol, cmin, cmax));
				}
			}
			if (!snd_mixer_selem_has_common_switch(elem)) {
				if (snd_mixer_selem_has_capture_switch(elem)) {
					if (!title)
						printf(" Capture");
					snd_mixer_selem_get_capture_switch(elem, SND_MIXER_SCHN_MONO, &csw);
					printf(" [%s]", csw ? "on" : "off");
				}
			}
		}
		if (pmono || cmono)
			printf("\n");
		if (!pmono || !cmono) {
			for (chn = 0; chn <= SND_MIXER_SCHN_LAST; chn++) {
				if ((pmono || !snd_mixer_selem_has_playback_channel(elem, chn)) &&
				    (cmono || !snd_mixer_selem_has_capture_channel(elem, chn)))
					continue;
				printf("%s%s:", space, snd_mixer_selem_channel_name(chn));
				if (!pmono && !cmono && snd_mixer_selem_has_common_volume(elem)) {
					snd_mixer_selem_get_playback_volume(elem, chn, &pvol);
					printf(" %s", get_percent(pvol, pmin, pmax));
				}
				if (!pmono && !cmono && snd_mixer_selem_has_common_switch(elem)) {
					snd_mixer_selem_get_playback_switch(elem, chn, &psw);
					printf(" [%s]", psw ? "on" : "off");
				}
				if (!pmono && snd_mixer_selem_has_playback_channel(elem, chn)) {
					int title = 0;
					if (!snd_mixer_selem_has_common_volume(elem)) {
						if (snd_mixer_selem_has_playback_volume(elem)) {
							printf(" Playback");
							title = 1;
							snd_mixer_selem_get_playback_volume(elem, chn, &pvol);
							printf(" %s", get_percent(pvol, pmin, pmax));
						}
					}
					if (!snd_mixer_selem_has_common_switch(elem)) {
						if (snd_mixer_selem_has_playback_switch(elem)) {
							if (!title)
								printf(" Playback");
							snd_mixer_selem_get_playback_switch(elem, chn, &psw);
							printf(" [%s]", psw ? "on" : "off");
						}
					}
				}
				if (!cmono && snd_mixer_selem_has_capture_channel(elem, chn)) {
					int title = 0;
					if (!snd_mixer_selem_has_common_volume(elem)) {
						if (snd_mixer_selem_has_capture_volume(elem)) {
							printf(" Capture");
							title = 1;
							snd_mixer_selem_get_capture_volume(elem, chn, &cvol);
							printf(" %s", get_percent(cvol, cmin, cmax));
						}
					}
					if (!snd_mixer_selem_has_common_switch(elem)) {
						if (snd_mixer_selem_has_capture_switch(elem)) {
							if (!title)
								printf(" Capture");
							snd_mixer_selem_get_capture_switch(elem, chn, &csw);
							printf(" [%s]", csw ? "on" : "off");
						}
					}
				}
				printf("\n");
			}
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
	if (smixer_level == 0 && (err = snd_mixer_attach(handle, card)) < 0) {
		error("Mixer attach %s error: %s", card, snd_strerror(err));
		snd_mixer_close(handle);
		return err;
	}
	if ((err = snd_mixer_selem_register(handle, smixer_level > 0 ? &smixer_options : NULL, NULL)) < 0) {
		error("Mixer register error: %s", snd_strerror(err));
		snd_mixer_close(handle);
		return err;
	}
	err = snd_mixer_load(handle);
	if (err < 0) {
		error("Mixer %s load error: %s", card, snd_strerror(err));
		snd_mixer_close(handle);
		return err;
	}
	for (elem = snd_mixer_first_elem(handle); elem; elem = snd_mixer_elem_next(elem)) {
		snd_mixer_selem_get_id(elem, sid);
		if (!(level & LEVEL_INACTIVE) && !snd_mixer_selem_is_active(elem))
			continue;
		printf("Simple mixer control '%s',%i\n", snd_mixer_selem_id_get_name(sid), snd_mixer_selem_id_get_index(sid));
		show_selem(handle, sid, "  ", level);
	}
	snd_mixer_close(handle);
	return 0;
}

static int parse_control_id(const char *str, snd_ctl_elem_id_t *id)
{
	int c, size, numid;
	char *ptr;

	while (*str == ' ' || *str == '\t')
		str++;
	if (!(*str))
		return -EINVAL;
	snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);	/* default */
	while (*str) {
		if (!strncasecmp(str, "numid=", 6)) {
			str += 6;
			numid = atoi(str);
			if (numid <= 0) {
				fprintf(stderr, "amixer: Invalid numid %d\n", numid);
				return -EINVAL;
			}
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
					if (size < (int)sizeof(buf)) {
						*ptr++ = *str;
						size++;
					}
					str++;
				}
				if (*str == c)
					str++;
			} else {
				while (*str && *str != ',') {
					if (size < (int)sizeof(buf)) {
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
	size = 1;	/* for '\0' */
	if (*str != '"' && *str != '\'') {
		while (*str && *str != ',') {
			if (size < (int)sizeof(buf)) {
				*ptr++ = *str;
				size++;
			}
			str++;
		}
	} else {
		c = *str++;
		while (*str && *str != c) {
			if (size < (int)sizeof(buf)) {
				*ptr++ = *str;
				size++;
			}
			str++;
		}
		if (*str == c)
			str++;
	}
	if (*str == '\0') {
		snd_mixer_selem_id_set_index(sid, 0);
		*ptr = 0;
		goto _set;
	}
	if (*str != ',')
		return -EINVAL;
	*ptr = 0;	/* terminate the string */
	str++;
	if (!isdigit(*str))
		return -EINVAL;
	snd_mixer_selem_id_set_index(sid, atoi(str));
       _set:
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
		error("Cannot find the given element from control %s\n", card);
		return err;
	}
	snd_ctl_elem_info_get_id(info, id);	/* FIXME: Remove it when hctl find works ok !!! */
	type = snd_ctl_elem_info_get_type(info);
	count = snd_ctl_elem_info_get_count(info);
	snd_ctl_elem_value_set_id(control, id);
	
	if (!roflag) {
		ptr = argv[1];
		for (idx = 0; idx < count && idx < 128 && ptr && *ptr; idx++) {
			switch (type) {
			case SND_CTL_ELEM_TYPE_BOOLEAN:
				tmp = 0;
				if (!strncasecmp(ptr, "on", 2) || !strncasecmp(ptr, "up", 2)) {
					tmp = 1;
					ptr += 2;
				} else if (!strncasecmp(ptr, "yes", 3)) {
					tmp = 1;
					ptr += 3;
				} else if (!strncasecmp(ptr, "toggle", 6)) {
					tmp = snd_ctl_elem_value_get_boolean(control, idx);
					tmp = tmp > 0 ? 0 : 1;
					ptr += 6;
				} else if (isdigit(*ptr)) {
					tmp = atoi(ptr) > 0 ? 1 : 0;
					while (isdigit(*ptr))
						ptr++;
				} else {
					while (*ptr && *ptr != ',')
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
			case SND_CTL_ELEM_TYPE_INTEGER64:
				tmp = get_integer64(&ptr,
						  snd_ctl_elem_info_get_min64(info),
						  snd_ctl_elem_info_get_max64(info));
				snd_ctl_elem_value_set_integer64(control, idx, tmp);
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
			show_control("  ", elem, LEVEL_BASIC | LEVEL_ID);
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

static unsigned int channels_mask(char **arg, unsigned int def)
{
	channel_mask_t *c;

	for (c = chanmask; c->name; c++) {
		if (strncasecmp(*arg, c->name, strlen(c->name)) == 0) {
			while (**arg != '\0' && **arg != ',' && **arg != ' ' && **arg != '\t')
				(*arg)++;
			if (**arg == ',' || **arg == ' ' || **arg == '\t')
				(*arg)++;
			return c->mask;
		}
	}
	return def;
}

static unsigned int dir_mask(char **arg, unsigned int def)
{
	int findend = 0;

	if (strncasecmp(*arg, "playback", 8) == 0)
		def = findend = 1;
	else if (strncasecmp(*arg, "capture", 8) == 0)
		def = findend = 2;
	if (findend) {
		while (**arg != '\0' && **arg != ',' && **arg != ' ' && **arg != '\t')
			(*arg)++;
		if (**arg == ',' || **arg == ' ' || **arg == '\t')
			(*arg)++;
	}
	return def;
}

static int get_enum_item_index(snd_mixer_elem_t *elem, char **ptrp)
{
	char *ptr = *ptrp;
	int items, i, len;
	char name[40];
	
	items = snd_mixer_selem_get_enum_items(elem);
	if (items <= 0)
		return -1;

	for (i = 0; i < items; i++) {
		if (snd_mixer_selem_get_enum_item_name(elem, i, sizeof(name)-1, name) < 0)
			continue;
		len = strlen(name);
		if (! strncmp(name, ptr, len)) {
			if (! ptr[len] || ptr[len] == ',' || ptr[len] == '\n') {
				ptr += len;
				*ptrp = ptr;
				return i;
			}
		}
	}
	return -1;
}

static int sset(unsigned int argc, char *argv[], int roflag)
{
	int err;
	unsigned int idx;
	snd_mixer_selem_channel_id_t chn;
	unsigned int channels = ~0U;
	unsigned int dir = 3, okflag = 3;
	long pmin, pmax, cmin, cmax;
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
	if (smixer_level == 0 && (err = snd_mixer_attach(handle, card)) < 0) {
		error("Mixer attach %s error: %s", card, snd_strerror(err));
		snd_mixer_close(handle);
		return err;
	}
	if ((err = snd_mixer_selem_register(handle, smixer_level > 0 ? &smixer_options : NULL, NULL)) < 0) {
		error("Mixer register error: %s", snd_strerror(err));
		snd_mixer_close(handle);
		return err;
	}
	err = snd_mixer_load(handle);
	if (err < 0) {
		error("Mixer %s load error: %s", card, snd_strerror(err));
		snd_mixer_close(handle);
		return err;
	}
	elem = snd_mixer_find_selem(handle, sid);
	if (!elem) {
		error("Unable to find simple control '%s',%i\n", snd_mixer_selem_id_get_name(sid), snd_mixer_selem_id_get_index(sid));
		snd_mixer_close(handle);
		return -ENOENT;
	}
	if (roflag)
		goto __skip_write;
	snd_mixer_selem_get_playback_volume_range(elem, &pmin, &pmax);
	snd_mixer_selem_get_capture_volume_range(elem, &cmin, &cmax);
	for (idx = 1; idx < argc; idx++) {
		char *ptr = argv[idx], *optr;
		int multi, firstchn = 1;
		channels = channels_mask(&ptr, channels);
		if (*ptr == '\0')
			continue;
		dir = dir_mask(&ptr, dir);
		if (*ptr == '\0')
			continue;
		multi = (strchr(ptr, ',') != NULL);
		optr = ptr;
		for (chn = 0; chn <= SND_MIXER_SCHN_LAST; chn++) {
			char *sptr = NULL;
			int ival;
			long lval;

			if (!(channels & (1 << chn)))
				continue;
			/* enum control */
			if (snd_mixer_selem_is_enumerated(elem)) {
				int idx = get_enum_item_index(elem, &ptr);
				if (idx < 0)
					break;
				else
					snd_mixer_selem_set_enum_item(elem, chn, idx);
				if (!multi)
					ptr = optr;
				continue;
			}

			if ((dir & 1) && snd_mixer_selem_has_playback_channel(elem, chn)) {
				sptr = ptr;
				if (!strncmp(ptr, "mute", 4) && snd_mixer_selem_has_playback_switch(elem)) {
					snd_mixer_selem_get_playback_switch(elem, chn, &ival);
					snd_mixer_selem_set_playback_switch(elem, chn, get_bool_simple(&ptr, "mute", 1, ival));
				} else if (!strncmp(ptr, "off", 3) && snd_mixer_selem_has_playback_switch(elem)) {
					snd_mixer_selem_get_playback_switch(elem, chn, &ival);
					snd_mixer_selem_set_playback_switch(elem, chn, get_bool_simple(&ptr, "off", 1, ival));
				} else if (!strncmp(ptr, "unmute", 6) && snd_mixer_selem_has_playback_switch(elem)) {
					snd_mixer_selem_get_playback_switch(elem, chn, &ival);
					snd_mixer_selem_set_playback_switch(elem, chn, get_bool_simple(&ptr, "unmute", 0, ival));
				} else if (!strncmp(ptr, "on", 2) && snd_mixer_selem_has_playback_switch(elem)) {
					snd_mixer_selem_get_playback_switch(elem, chn, &ival);
					snd_mixer_selem_set_playback_switch(elem, chn, get_bool_simple(&ptr, "on", 0, ival));
				} else if (!strncmp(ptr, "toggle", 6) && snd_mixer_selem_has_playback_switch(elem)) {
					if (firstchn || !snd_mixer_selem_has_playback_switch_joined(elem)) {
						snd_mixer_selem_get_playback_switch(elem, chn, &ival);
						snd_mixer_selem_set_playback_switch(elem, chn, (ival ? 1 : 0) ^ 1);
					}
					simple_skip_word(&ptr, "toggle");
				} else if (isdigit(*ptr) || *ptr == '-' || *ptr == '+') {
					if (snd_mixer_selem_has_playback_volume(elem)) {
						snd_mixer_selem_get_playback_volume(elem, chn, &lval);
						snd_mixer_selem_set_playback_volume(elem, chn, get_volume_simple(&ptr, pmin, pmax, lval));
					} else {
						get_volume_simple(&ptr, 0, 100, 0);
					}
				} else if (simple_skip_word(&ptr, "cap") || simple_skip_word(&ptr, "rec") ||
					   simple_skip_word(&ptr, "nocap") || simple_skip_word(&ptr, "norec")) {
					/* nothing */
				} else {
					okflag &= ~1;
				}
			}
			if ((dir & 2) && snd_mixer_selem_has_capture_channel(elem, chn)) {
				if (sptr != NULL)
					ptr = sptr;
				sptr = ptr;
				if (!strncmp(ptr, "cap", 3) && snd_mixer_selem_has_capture_switch(elem)) {
					snd_mixer_selem_get_capture_switch(elem, chn, &ival);
					snd_mixer_selem_set_capture_switch(elem, chn, get_bool_simple(&ptr, "cap", 0, ival));
				} else if (!strncmp(ptr, "rec", 3) && snd_mixer_selem_has_capture_switch(elem)) {
					snd_mixer_selem_get_capture_switch(elem, chn, &ival);
					snd_mixer_selem_set_capture_switch(elem, chn, get_bool_simple(&ptr, "rec", 0, ival));
				} else if (!strncmp(ptr, "nocap", 5) && snd_mixer_selem_has_capture_switch(elem)) {
					snd_mixer_selem_get_capture_switch(elem, chn, &ival);
					snd_mixer_selem_set_capture_switch(elem, chn, get_bool_simple(&ptr, "nocap", 1, ival));
				} else if (!strncmp(ptr, "norec", 5) && snd_mixer_selem_has_capture_switch(elem)) {
					snd_mixer_selem_get_capture_switch(elem, chn, &ival);
					snd_mixer_selem_set_capture_switch(elem, chn, get_bool_simple(&ptr, "norec", 1, ival));
				} else if (!strncmp(ptr, "toggle", 6) && snd_mixer_selem_has_capture_switch(elem)) {
					if (firstchn || !snd_mixer_selem_has_capture_switch_joined(elem)) {
						snd_mixer_selem_get_capture_switch(elem, chn, &ival);
						snd_mixer_selem_set_capture_switch(elem, chn, (ival ? 1 : 0) ^ 1);
					}
					simple_skip_word(&ptr, "toggle");
				} else if (isdigit(*ptr) || *ptr == '-' || *ptr == '+') {
					if (snd_mixer_selem_has_capture_volume(elem)) {
						snd_mixer_selem_get_capture_volume(elem, chn, &lval);
						snd_mixer_selem_set_capture_volume(elem, chn, get_volume_simple(&ptr, cmin, cmax, lval));
					} else {
						get_volume_simple(&ptr, 0, 100, 0);
					}
				} else if (simple_skip_word(&ptr, "mute") || simple_skip_word(&ptr, "off") ||
					   simple_skip_word(&ptr, "unmute") || simple_skip_word(&ptr, "on")) {
					/* nothing */
				} else {
					okflag &= ~2;
				}
			}
			if (okflag == 0) {
				if (dir & 1)
					error("Unknown playback setup '%s'..\n", ptr);
				if (dir & 2)
					error("Unknown capture setup '%s'..\n", ptr);
				snd_mixer_close(handle);
				return err;
			}
			if (!multi)
				ptr = optr;
			firstchn = 0;
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
	return 0;
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
	if (smixer_level == 0 && (err = snd_mixer_attach(handle, card)) < 0) {
		error("Mixer attach %s error: %s", card, snd_strerror(err));
		snd_mixer_close(handle);
		return err;
	}
	if ((err = snd_mixer_selem_register(handle, smixer_level > 0 ? &smixer_options : NULL, NULL)) < 0) {
		error("Mixer register error: %s", snd_strerror(err));
		snd_mixer_close(handle);
		return err;
	}
	snd_mixer_set_callback(handle, mixer_event);
	err = snd_mixer_load(handle);
	if (err < 0) {
		error("Mixer %s load error: %s", card, snd_strerror(err));
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
	return 0;
}

int main(int argc, char *argv[])
{
	int morehelp, level = 0;
	struct option long_option[] =
	{
		{"help", 0, NULL, HELPID_HELP},
		{"card", 1, NULL, HELPID_CARD},
		{"device", 1, NULL, HELPID_DEVICE},
		{"quiet", 0, NULL, HELPID_QUIET},
		{"inactive", 0, NULL, HELPID_INACTIVE},
		{"debug", 0, NULL, HELPID_DEBUG},
		{"nocheck", 0, NULL, HELPID_NOCHECK},
		{"version", 0, NULL, HELPID_VERSION},
		{"abstract", 1, NULL, HELPID_ABSTRACT},
		{NULL, 0, NULL, 0},
	};

	morehelp = 0;
	while (1) {
		int c;

		if ((c = getopt_long(argc, argv, "hc:D:qidnva:", long_option, NULL)) < 0)
			break;
		switch (c) {
		case 'h':
		case HELPID_HELP:
			help();
			return 0;
		case 'c':
		case HELPID_CARD:
			{
				int i;
				i = snd_card_get_index(optarg);
				if (i >= 0 && i < 32)
					sprintf(card, "hw:%i", i);
				else {
					fprintf(stderr, "\07Invalid card number.\n");
					morehelp++;
				}
			}
			break;
		case 'D':
		case HELPID_DEVICE:
			strncpy(card, optarg, sizeof(card)-1);
			card[sizeof(card)-1] = '\0';
			break;
		case 'q':
		case HELPID_QUIET:
			quiet = 1;
			break;
		case 'i':
		case HELPID_INACTIVE:
			level |= LEVEL_INACTIVE;
			break;
		case 'd':
		case HELPID_DEBUG:
			debugflag = 1;
			break;
		case 'n':
		case HELPID_NOCHECK:
			no_check = 1;
			break;
		case 'v':
		case HELPID_VERSION:
			printf("amixer version " SND_UTIL_VERSION_STR "\n");
			return 1;
		case 'a':
		case HELPID_ABSTRACT:
			smixer_level = 1;
			memset(&smixer_options, 0, sizeof(smixer_options));
			smixer_options.ver = 1;
			if (!strcmp(optarg, "none"))
				smixer_options.abstract = SND_MIXER_SABSTRACT_NONE;
			else if (!strcmp(optarg, "basic"))
				smixer_options.abstract = SND_MIXER_SABSTRACT_BASIC;
			else {
				fprintf(stderr, "Select correct abstraction level (none or basic)...\n");
				morehelp++;
			}
			break;
		default:
			fprintf(stderr, "\07Invalid switch or option needs an argument.\n");
			morehelp++;
		}
	}
	if (morehelp) {
		help();
		return 1;
	}
	smixer_options.device = card;
	if (argc - optind <= 0) {
		return selems(LEVEL_BASIC | level) ? 1 : 0;
	}
	if (!strcmp(argv[optind], "help")) {
		return help() ? 1 : 0;
	} else if (!strcmp(argv[optind], "info")) {
		return info() ? 1 : 0;
	} else if (!strcmp(argv[optind], "controls")) {
		return controls(level) ? 1 : 0;
	} else if (!strcmp(argv[optind], "contents")) {
		return controls(LEVEL_BASIC | level) ? 1 : 0;
	} else if (!strcmp(argv[optind], "scontrols") || !strcmp(argv[optind], "simple")) {
		return selems(level) ? 1 : 0;
	} else if (!strcmp(argv[optind], "scontents")) {
		return selems(LEVEL_BASIC | level) ? 1 : 0;
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
