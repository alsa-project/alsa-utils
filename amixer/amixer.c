/*
 *   ALSA command line mixer utility
 *   Copyright (c) 1999-2000 by Jaroslav Kysela <perex@perex.cz>
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
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "aconfig.h"
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
#include <poll.h>
#include <stdint.h>
#include "amixer.h"
#include "../alsamixer/volume_mapping.h"

#define LEVEL_BASIC		(1<<0)
#define LEVEL_INACTIVE		(1<<1)
#define LEVEL_ID		(1<<2)

static int quiet = 0;
static int debugflag = 0;
static int no_check = 0;
static int smixer_level = 0;
static int ignore_error = 0;
static struct snd_mixer_selem_regopt smixer_options;
static char card[64] = "default";

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
	printf("Usage: amixer <options> [command]\n");
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
	printf("  -s,--stdin      Read and execute commands from stdin sequentially\n");
	printf("  -R,--raw-volume Use the raw value (default)\n");
	printf("  -M,--mapped-volume Use the mapped volume\n");
	printf("\nAvailable commands:\n");
	printf("  scontrols       show all mixer simple controls\n");
	printf("  scontents	  show contents of all mixer simple controls (default command)\n");
	printf("  sset sID P      set contents for one mixer simple control\n");
	printf("  sget sID        get contents for one mixer simple control\n");
	printf("  controls        show all controls for given card\n");
	printf("  contents        show contents of all controls for given card\n");
	printf("  cset cID P      set control contents for one control\n");
	printf("  cget cID        get control contents for one control\n");
	printf("\nAvailable advanced commands:\n");
	printf("  sevents	  show the mixer events for simple controls\n");
	printf("  events	  show the mixer events for controls\n");
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
	*res++ = snd_ctl_elem_info_is_tlv_readable(info) ? 'R' : '-';
	*res++ = snd_ctl_elem_info_is_tlv_writable(info) ? 'W' : '-';
	*res++ = snd_ctl_elem_info_is_tlv_commandable(info) ? 'C' : '-';
	*res++ = '\0';
	return result;
}

#define check_range(val, min, max) \
	(no_check ? (val) : ((val < min) ? (min) : (val > max) ? (max) : (val))) 
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

static int convert_prange(long val, long min, long max)
{
	long range = max - min;
	int tmp;

	if (range == 0)
		return min;
	val -= min;
	tmp = rint((double)val/(double)range * 100);
	return tmp;
}

/* Function to convert from percentage to volume. perc = percentage */
static long convert_prange1(long perc, long min, long max)
{
	long tmp;

	tmp = rint((double)perc * (double)(max - min) * 0.01);
	if (tmp == 0 && perc > 0)
		tmp++;
	return tmp + min;
}

struct volume_ops {
	int (*get_range)(snd_mixer_elem_t *elem, long *min, long *max);
	int (*get)(snd_mixer_elem_t *elem, snd_mixer_selem_channel_id_t c,
		   long *value);
	int (*set)(snd_mixer_elem_t *elem, snd_mixer_selem_channel_id_t c,
		   long value, int dir);
};
	
enum { VOL_RAW, VOL_DB, VOL_MAP };

struct volume_ops_set {
	int (*has_volume)(snd_mixer_elem_t *elem);
	struct volume_ops v[3];
};

static int set_playback_dB(snd_mixer_elem_t *elem,
			   snd_mixer_selem_channel_id_t c, long value, int dir)
{
	return snd_mixer_selem_set_playback_dB(elem, c, value, dir);
}

static int set_capture_dB(snd_mixer_elem_t *elem,
			  snd_mixer_selem_channel_id_t c, long value, int dir)
{
	return snd_mixer_selem_set_capture_dB(elem, c, value, dir);
}

static int set_playback_raw_volume(snd_mixer_elem_t *elem,
				   snd_mixer_selem_channel_id_t c,
				   long value, int dir ATTRIBUTE_UNUSED)
{
	return snd_mixer_selem_set_playback_volume(elem, c, value);
}

static int set_capture_raw_volume(snd_mixer_elem_t *elem,
				  snd_mixer_selem_channel_id_t c,
				  long value, int dir ATTRIBUTE_UNUSED)
{
	return snd_mixer_selem_set_capture_volume(elem, c, value);
}

/* FIXME: normalize to int32 space to be compatible with other types */
#define MAP_VOL_RES	(INT32_MAX / 100)

static int get_mapped_volume_range(snd_mixer_elem_t *elem ATTRIBUTE_UNUSED,
				   long *pmin, long *pmax)
{
	*pmin = 0;
	*pmax = MAP_VOL_RES;
	return 0;
}

static int get_playback_mapped_volume(snd_mixer_elem_t *elem,
				      snd_mixer_selem_channel_id_t c,
				      long *value)
{
	*value = (rint)(get_normalized_playback_volume(elem, c) * MAP_VOL_RES);
	return 0;
}

static int set_playback_mapped_volume(snd_mixer_elem_t *elem,
				      snd_mixer_selem_channel_id_t c,
				      long value, int dir)
{
	return set_normalized_playback_volume(elem, c,
					      (double)value / MAP_VOL_RES, dir);
}

static int get_capture_mapped_volume(snd_mixer_elem_t *elem,
				     snd_mixer_selem_channel_id_t c,
				     long *value)
{
	*value = (rint)(get_normalized_capture_volume(elem, c) * MAP_VOL_RES);
	return 0;
}

static int set_capture_mapped_volume(snd_mixer_elem_t *elem,
				     snd_mixer_selem_channel_id_t c,
				     long value, int dir)
{
	return set_normalized_capture_volume(elem, c,
					     (double)value / MAP_VOL_RES, dir);
}

static const struct volume_ops_set vol_ops[2] = {
	{
		.has_volume = snd_mixer_selem_has_playback_volume,
		.v = {{ snd_mixer_selem_get_playback_volume_range,
			snd_mixer_selem_get_playback_volume,
			set_playback_raw_volume },
		      { snd_mixer_selem_get_playback_dB_range,
			snd_mixer_selem_get_playback_dB,
			set_playback_dB },
		      { get_mapped_volume_range,
			get_playback_mapped_volume,
			set_playback_mapped_volume },
		},
	},
	{
		.has_volume = snd_mixer_selem_has_capture_volume,
		.v = {{ snd_mixer_selem_get_capture_volume_range,
			snd_mixer_selem_get_capture_volume,
			set_capture_raw_volume },
		      { snd_mixer_selem_get_capture_dB_range,
			snd_mixer_selem_get_capture_dB,
			set_capture_dB },
		      { get_mapped_volume_range,
			get_capture_mapped_volume,
			set_capture_mapped_volume },
		},
	},
};

static int std_vol_type = VOL_RAW;

static int set_volume_simple(snd_mixer_elem_t *elem,
			     snd_mixer_selem_channel_id_t chn,
			     char **ptr, int dir)
{
	long val, orig, pmin, pmax;
	char *p = *ptr, *s;
	int invalid = 0, percent = 0, err = 0;
	int vol_type;
	double scale = 1.0;
	int correct = 0;

	if (! vol_ops[dir].has_volume(elem))
		invalid = 1;

	if (*p == ':')
		p++;
	if (*p == '\0' || (!isdigit(*p) && *p != '-'))
		goto skip;

	s = p;
	val = strtol(s, &p, 10);
	if (*p == '.') {
		p++;
		strtol(p, &p, 10);
	}
	if (*p == '%') {
		vol_type = std_vol_type;
		percent = 1;
		p++;
	} else if (toupper(p[0]) == 'D' && toupper(p[1]) == 'B') {
		vol_type = VOL_DB;
		p += 2;
		scale = 100;
	} else {
		vol_type = VOL_RAW;
	}

	if (*p && !strchr(",:+-", *p))
		invalid = 1;

	val = (long)(strtod(s, NULL) * scale);
	if (vol_ops[dir].v[vol_type].get_range(elem, &pmin, &pmax) < 0)
		invalid = 1;
	if (percent)
		val = (long)convert_prange1(val, pmin, pmax);
	if (*p == '+' || *p == '-') {
		if (! invalid) {
			if (vol_ops[dir].v[vol_type].get(elem, chn, &orig) < 0)
				invalid = 1;
			if (*p == '+') {
				val = orig + val;
				correct = 1;
			} else {
				val = orig - val;
				correct = -1;
			}
		}
		p++;
	}

	if (*p && !strchr(",:", *p))
		invalid = 1;

	if (! invalid) {
		val = check_range(val, pmin, pmax);
		err = vol_ops[dir].v[vol_type].set(elem, chn, val, correct);
	}
 skip:
	if (*p == ',')
		p++;
	*ptr = p;
	return err ? err : (invalid ? -ENOENT : 0);
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
	char *str;

	str = snd_ctl_ascii_elem_id_get(id);
	if (str)
		printf("%s", str);
	free(str);
}

static void print_spaces(unsigned int spaces)
{
	while (spaces-- > 0)
		putc(' ', stdout);
}

static void print_dB(long dB)
{
	if (dB < 0) {
		printf("-%li.%02lidB", -dB / 100, -dB % 100);
	} else {
		printf("%li.%02lidB", dB / 100, dB % 100);
	}
}

static void decode_tlv(unsigned int spaces, unsigned int *tlv, unsigned int tlv_size)
{
	unsigned int type = tlv[0];
	unsigned int size;
	unsigned int idx = 0;
	const char *chmap_type = NULL;
	int lf = 1;

	if (tlv_size < 2 * sizeof(unsigned int)) {
		printf("TLV size error!\n");
		return;
	}
	print_spaces(spaces);
	printf("| ");
	type = tlv[idx++];
	size = tlv[idx++];
	tlv_size -= 2 * sizeof(unsigned int);
	if (size > tlv_size) {
		printf("TLV size error (%u, %u, %u)!\n", type, size, tlv_size);
		return;
	}
	switch (type) {
	case SND_CTL_TLVT_CONTAINER:
		printf("container\n");
		size += sizeof(unsigned int) -1;
		size /= sizeof(unsigned int);
		while (idx < size) {
			if (tlv[idx+1] > (size - idx) * sizeof(unsigned int)) {
				printf("TLV size error in compound!\n");
				return;
			}
			decode_tlv(spaces + 2, tlv + idx, tlv[idx+1] + 8);
			idx += 2 + (tlv[idx+1] + sizeof(unsigned int) - 1) / sizeof(unsigned int);
		}
		lf = 0;
		break;
	case SND_CTL_TLVT_DB_SCALE:
		printf("dBscale-");
		if (size != 2 * sizeof(unsigned int)) {
			while (size > 0) {
				printf("0x%08x,", tlv[idx++]);
				size -= sizeof(unsigned int);
			}
		} else {
			printf("min=");
			print_dB((int)tlv[2]);
			printf(",step=");
			print_dB(tlv[3] & 0xffff);
			printf(",mute=%i", (tlv[3] >> 16) & 1);
		}
		break;
#ifdef SND_CTL_TLVT_DB_LINEAR
	case SND_CTL_TLVT_DB_LINEAR:
		printf("dBlinear-");
		if (size != 2 * sizeof(unsigned int)) {
			while (size > 0) {
				printf("0x%08x,", tlv[idx++]);
				size -= sizeof(unsigned int);
			}
		} else {
			printf("min=");
			print_dB((int)tlv[2]);
			printf(",max=");
			print_dB((int)tlv[3]);
		}
		break;
#endif
#ifdef SND_CTL_TLVT_DB_RANGE
	case SND_CTL_TLVT_DB_RANGE:
		printf("dBrange-\n");
		if ((size % (6 * sizeof(unsigned int))) != 0) {
			while (size > 0) {
				printf("0x%08x,", tlv[idx++]);
				size -= sizeof(unsigned int);
			}
			break;
		}
		while (size > 0) {
			print_spaces(spaces + 2);
			printf("rangemin=%i,", tlv[idx++]);
			printf(",rangemax=%i\n", tlv[idx++]);
			decode_tlv(spaces + 4, tlv + idx, 4 * sizeof(unsigned int));
			idx += 4;
			size -= 6 * sizeof(unsigned int);
		}
		break;
#endif
#ifdef SND_CTL_TLVT_DB_MINMAX
	case SND_CTL_TLVT_DB_MINMAX:
	case SND_CTL_TLVT_DB_MINMAX_MUTE:
		if (type == SND_CTL_TLVT_DB_MINMAX_MUTE)
			printf("dBminmaxmute-");
		else
			printf("dBminmax-");
		if (size != 2 * sizeof(unsigned int)) {
			while (size > 0) {
				printf("0x%08x,", tlv[idx++]);
				size -= sizeof(unsigned int);
			}
		} else {
			printf("min=");
			print_dB((int)tlv[2]);
			printf(",max=");
			print_dB((int)tlv[3]);
		}
		break;
#endif
#ifdef SND_CTL_TLVT_CHMAP_FIXED
	case SND_CTL_TLVT_CHMAP_FIXED:
		chmap_type = "fixed";
		/* Fall through */
	case SND_CTL_TLVT_CHMAP_VAR:
		if (!chmap_type)
			chmap_type = "variable";
		/* Fall through */
	case SND_CTL_TLVT_CHMAP_PAIRED:
		if (!chmap_type)
			chmap_type = "paired";
		printf("chmap-%s=", chmap_type);

		while (size > 0) {
			printf("%s", snd_pcm_chmap_name(tlv[idx++]));
			size -= sizeof(unsigned int);
			if (size > 0)
				printf(",");
		}
		break;
#endif
	default:
		printf("unk-%u-", type);
		while (size > sizeof(unsigned int)) {
			printf("0x%08x,", tlv[idx++]);
			size -= sizeof(unsigned int);
		}
		if (size > 0) {
			unsigned char *b = (void *)&tlv[idx];
			while (size > 0) {
				printf("E-0x%02x,", *b++);
				size--;
			}
		}
		break;
	}
	if (lf)
		putc('\n', stdout);
}

static int show_control(const char *space, snd_hctl_elem_t *elem,
			int level)
{
	int err;
	unsigned int item, idx, count, *tlv;
	snd_ctl_elem_type_t type;
	snd_ctl_elem_id_t *id;
	snd_ctl_elem_info_t *info;
	snd_ctl_elem_value_t *control;
	snd_aes_iec958_t iec958;
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
	printf("%s; type=%s,access=%s,values=%u", space, control_type(info), control_access(info), count);
	switch (type) {
	case SND_CTL_ELEM_TYPE_INTEGER:
		printf(",min=%li,max=%li,step=%li\n", 
		       snd_ctl_elem_info_get_min(info),
		       snd_ctl_elem_info_get_max(info),
		       snd_ctl_elem_info_get_step(info));
		break;
	case SND_CTL_ELEM_TYPE_INTEGER64:
		printf(",min=%lli,max=%lli,step=%lli\n",
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
		if (!snd_ctl_elem_info_is_readable(info))
			goto __skip_read;
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
				printf("%lli", snd_ctl_elem_value_get_integer64(control, idx));
				break;
			case SND_CTL_ELEM_TYPE_ENUMERATED:
				printf("%u", snd_ctl_elem_value_get_enumerated(control, idx));
				break;
			case SND_CTL_ELEM_TYPE_BYTES:
				printf("0x%02x", snd_ctl_elem_value_get_byte(control, idx));
				break;
			case SND_CTL_ELEM_TYPE_IEC958:
				snd_ctl_elem_value_get_iec958(control, &iec958);
				printf("[AES0=0x%02x AES1=0x%02x AES2=0x%02x AES3=0x%02x]",
				       iec958.status[0], iec958.status[1],
				       iec958.status[2], iec958.status[3]);
				break;
			default:
				printf("?");
				break;
			}
		}
		printf("\n");
	      __skip_read:
		if (!snd_ctl_elem_info_is_tlv_readable(info))
			goto __skip_tlv;
		/* skip ASoC ext bytes controls that may have huge binary TLV data */
		if (type == SND_CTL_ELEM_TYPE_BYTES &&
				!snd_ctl_elem_info_is_readable(info) &&
				!snd_ctl_elem_info_is_writable(info)) {
			printf("%s; ASoC TLV Byte control, skipping bytes dump\n", space);
			goto __skip_tlv;
		}

		tlv = malloc(4096);
		if ((err = snd_hctl_elem_tlv_read(elem, tlv, 4096)) < 0) {
			error("Control %s element TLV read error: %s\n", card, snd_strerror(err));
			free(tlv);
			return err;
		}
		decode_tlv(strlen(space), tlv, 4096);
		free(tlv);
	}
      __skip_tlv:
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

static void show_selem_volume(snd_mixer_elem_t *elem, 
			      snd_mixer_selem_channel_id_t chn, int dir,
			      long min, long max)
{
	long raw, val;
	vol_ops[dir].v[VOL_RAW].get(elem, chn, &raw);
	if (std_vol_type == VOL_RAW)
		val = convert_prange(raw, min, max);
	else {
		vol_ops[dir].v[std_vol_type].get(elem, chn, &val);
		val = convert_prange(val, 0, MAP_VOL_RES);
	}
	printf(" %li [%li%%]", raw, val);
	if (!vol_ops[dir].v[VOL_DB].get(elem, chn, &val)) {
		printf(" [");
		print_dB(val);
		printf("]");
	}
}

static int show_selem(snd_mixer_t *handle, snd_mixer_selem_id_t *id, const char *space, int level)
{
	snd_mixer_selem_channel_id_t chn;
	long pmin = 0, pmax = 0;
	long cmin = 0, cmax = 0;
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
		if (snd_mixer_selem_is_enum_playback(elem)) {
			printf(" penum");
		} else if (snd_mixer_selem_is_enum_capture(elem)) {
			printf(" cenum");
		} else if (snd_mixer_selem_is_enumerated(elem)) {
			printf(" enum");
		}
		printf("\n");
		if (snd_mixer_selem_is_enumerated(elem)) {
			int i, items;
			unsigned int idx;
			/*
			 * See snd_ctl_elem_init_enum_names() in
			 * sound/core/control.c.
			 */
			char itemname[64];
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
				show_selem_volume(elem, SND_MIXER_SCHN_MONO, 0, pmin, pmax);
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
					show_selem_volume(elem, SND_MIXER_SCHN_MONO, 0, pmin, pmax);
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
					show_selem_volume(elem, SND_MIXER_SCHN_MONO, 1, cmin, cmax);
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
					show_selem_volume(elem, chn, 0, pmin, pmax);
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
							show_selem_volume(elem, chn, 0, pmin, pmax);
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
							show_selem_volume(elem, chn, 1, cmin, cmax);
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

static int cset(int argc, char *argv[], int roflag, int keep_handle)
{
	int err;
	static snd_ctl_t *handle = NULL;
	snd_ctl_elem_info_t *info;
	snd_ctl_elem_id_t *id;
	snd_ctl_elem_value_t *control;
	snd_ctl_elem_info_alloca(&info);
	snd_ctl_elem_id_alloca(&id);
	snd_ctl_elem_value_alloca(&control);

	if (argc < 1) {
		fprintf(stderr, "Specify a full control identifier: [[iface=<iface>,][name='name',][index=<index>,][device=<device>,][subdevice=<subdevice>]]|[numid=<numid>]\n");
		return -EINVAL;
	}
	if (snd_ctl_ascii_elem_id_parse(id, argv[0])) {
		fprintf(stderr, "Wrong control identifier: %s\n", argv[0]);
		return -EINVAL;
	}
	if (debugflag) {
		printf("VERIFY ID: ");
		show_control_id(id);
		printf("\n");
	}
	if (handle == NULL &&
	    (err = snd_ctl_open(&handle, card, 0)) < 0) {
		error("Control %s open error: %s\n", card, snd_strerror(err));
		return err;
	}
	snd_ctl_elem_info_set_id(info, id);
	if ((err = snd_ctl_elem_info(handle, info)) < 0) {
		if (ignore_error)
			return 0;
		error("Cannot find the given element from control %s\n", card);
		if (! keep_handle) {
			snd_ctl_close(handle);
			handle = NULL;
		}
		return err;
	}
	snd_ctl_elem_info_get_id(info, id);     /* FIXME: Remove it when hctl find works ok !!! */
	if (!roflag) {
		snd_ctl_elem_value_set_id(control, id);
		if ((err = snd_ctl_elem_read(handle, control)) < 0) {
			if (ignore_error)
				return 0;
			error("Cannot read the given element from control %s\n", card);
			if (! keep_handle) {
				snd_ctl_close(handle);
				handle = NULL;
			}
			return err;
		}
		err = snd_ctl_ascii_value_parse(handle, control, info, argv[1]);
		if (err < 0) {
 			if (!ignore_error)
				error("Control %s parse error: %s\n", card, snd_strerror(err));
			if (!keep_handle) {
				snd_ctl_close(handle);
				handle = NULL;
			}
			return ignore_error ? 0 : err;
		}
		if ((err = snd_ctl_elem_write(handle, control)) < 0) {
			if (!ignore_error)
				error("Control %s element write error: %s\n", card, snd_strerror(err));
			if (!keep_handle) {
				snd_ctl_close(handle);
				handle = NULL;
			}
			return ignore_error ? 0 : err;
		}
	}
	if (! keep_handle) {
		snd_ctl_close(handle);
		handle = NULL;
	}
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
static const channel_mask_t chanmask[] = {
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
	const channel_mask_t *c;

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

	/* See snd_ctl_elem_init_enum_names() in sound/core/control.c. */
	char name[64];
	
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

static int sset_enum(snd_mixer_elem_t *elem, unsigned int argc, char **argv)
{
	unsigned int idx, item = 0;
	int check_flag = ignore_error ? 0 : -1;

	for (idx = 1; idx < argc; idx++) {
		char *ptr = argv[idx];
		while (*ptr) {
			int ival = get_enum_item_index(elem, &ptr);
			if (ival < 0)
				return check_flag;
			if (snd_mixer_selem_set_enum_item(elem, item++, ival) >= 0)
				check_flag = 1;
			/* skip separators */
			while (*ptr == ',' || isspace(*ptr))
				ptr++;
		}
	}
	return check_flag;
}

static int sset_channels(snd_mixer_elem_t *elem, unsigned int argc, char **argv)
{
	unsigned int channels = ~0U;
	unsigned int dir = 3, okflag = 3;
	unsigned int idx;
	snd_mixer_selem_channel_id_t chn;
	int check_flag = ignore_error ? 0 : -1;

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

			if (!(channels & (1 << chn)))
				continue;

			if ((dir & 1) && snd_mixer_selem_has_playback_channel(elem, chn)) {
				sptr = ptr;
				if (!strncmp(ptr, "mute", 4) && snd_mixer_selem_has_playback_switch(elem)) {
					snd_mixer_selem_get_playback_switch(elem, chn, &ival);
					if (snd_mixer_selem_set_playback_switch(elem, chn, get_bool_simple(&ptr, "mute", 1, ival)) >= 0)
						check_flag = 1;
				} else if (!strncmp(ptr, "off", 3) && snd_mixer_selem_has_playback_switch(elem)) {
					snd_mixer_selem_get_playback_switch(elem, chn, &ival);
					if (snd_mixer_selem_set_playback_switch(elem, chn, get_bool_simple(&ptr, "off", 1, ival)) >= 0)
						check_flag = 1;
				} else if (!strncmp(ptr, "unmute", 6) && snd_mixer_selem_has_playback_switch(elem)) {
					snd_mixer_selem_get_playback_switch(elem, chn, &ival);
					if (snd_mixer_selem_set_playback_switch(elem, chn, get_bool_simple(&ptr, "unmute", 0, ival)) >= 0)
						check_flag = 1;
				} else if (!strncmp(ptr, "on", 2) && snd_mixer_selem_has_playback_switch(elem)) {
					snd_mixer_selem_get_playback_switch(elem, chn, &ival);
					if (snd_mixer_selem_set_playback_switch(elem, chn, get_bool_simple(&ptr, "on", 0, ival)) >= 0)
						check_flag = 1;
				} else if (!strncmp(ptr, "toggle", 6) && snd_mixer_selem_has_playback_switch(elem)) {
					if (firstchn || !snd_mixer_selem_has_playback_switch_joined(elem)) {
						snd_mixer_selem_get_playback_switch(elem, chn, &ival);
						if (snd_mixer_selem_set_playback_switch(elem, chn, (ival ? 1 : 0) ^ 1) >= 0)
							check_flag = 1;
					}
					simple_skip_word(&ptr, "toggle");
				} else if (isdigit(*ptr) || *ptr == '-' || *ptr == '+') {
					if (set_volume_simple(elem, chn, &ptr, 0) >= 0)
						check_flag = 1;
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
					if (snd_mixer_selem_set_capture_switch(elem, chn, get_bool_simple(&ptr, "cap", 0, ival)) >= 0)
						check_flag = 1;
				} else if (!strncmp(ptr, "rec", 3) && snd_mixer_selem_has_capture_switch(elem)) {
					snd_mixer_selem_get_capture_switch(elem, chn, &ival);
					if (snd_mixer_selem_set_capture_switch(elem, chn, get_bool_simple(&ptr, "rec", 0, ival)) >= 0)
						check_flag = 1;
				} else if (!strncmp(ptr, "nocap", 5) && snd_mixer_selem_has_capture_switch(elem)) {
					snd_mixer_selem_get_capture_switch(elem, chn, &ival);
					if (snd_mixer_selem_set_capture_switch(elem, chn, get_bool_simple(&ptr, "nocap", 1, ival)) >= 0)
						check_flag = 1;
				} else if (!strncmp(ptr, "norec", 5) && snd_mixer_selem_has_capture_switch(elem)) {
					snd_mixer_selem_get_capture_switch(elem, chn, &ival);
					if (snd_mixer_selem_set_capture_switch(elem, chn, get_bool_simple(&ptr, "norec", 1, ival)) >= 0)
						check_flag = 1;
				} else if (!strncmp(ptr, "toggle", 6) && snd_mixer_selem_has_capture_switch(elem)) {
					if (firstchn || !snd_mixer_selem_has_capture_switch_joined(elem)) {
						snd_mixer_selem_get_capture_switch(elem, chn, &ival);
						if (snd_mixer_selem_set_capture_switch(elem, chn, (ival ? 1 : 0) ^ 1) >= 0)
							check_flag = 1;
					}
					simple_skip_word(&ptr, "toggle");
				} else if (isdigit(*ptr) || *ptr == '-' || *ptr == '+') {
					if (set_volume_simple(elem, chn, &ptr, 1) >= 0)
						check_flag = 1;
				} else if (simple_skip_word(&ptr, "mute") || simple_skip_word(&ptr, "off") ||
					   simple_skip_word(&ptr, "unmute") || simple_skip_word(&ptr, "on")) {
					/* nothing */
				} else {
					okflag &= ~2;
				}
			}
			if (okflag == 0) {
				if (debugflag) {
					if (dir & 1)
						error("Unknown playback setup '%s'..", ptr);
					if (dir & 2)
						error("Unknown capture setup '%s'..", ptr);
				}
				return 0; /* just skip it */
			}
			if (!multi)
				ptr = optr;
			firstchn = 0;
		}
	}
	return check_flag;
}

static int sset(unsigned int argc, char *argv[], int roflag, int keep_handle)
{
	int err = 0;
	static snd_mixer_t *handle = NULL;
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
	if (handle == NULL) {
		if ((err = snd_mixer_open(&handle, 0)) < 0) {
			error("Mixer %s open error: %s\n", card, snd_strerror(err));
			return err;
		}
		if (smixer_level == 0 && (err = snd_mixer_attach(handle, card)) < 0) {
			error("Mixer attach %s error: %s", card, snd_strerror(err));
			snd_mixer_close(handle);
			handle = NULL;
			return err;
		}
		if ((err = snd_mixer_selem_register(handle, smixer_level > 0 ? &smixer_options : NULL, NULL)) < 0) {
			error("Mixer register error: %s", snd_strerror(err));
			snd_mixer_close(handle);
			handle = NULL;
			return err;
		}
		err = snd_mixer_load(handle);
		if (err < 0) {
			error("Mixer %s load error: %s", card, snd_strerror(err));
			snd_mixer_close(handle);
			handle = NULL;
			return err;
		}
	}
	elem = snd_mixer_find_selem(handle, sid);
	if (!elem) {
		if (ignore_error)
			return 0;
		error("Unable to find simple control '%s',%i\n", snd_mixer_selem_id_get_name(sid), snd_mixer_selem_id_get_index(sid));
		snd_mixer_close(handle);
		handle = NULL;
		return -ENOENT;
	}
	if (!roflag) {
		/* enum control */
		if (snd_mixer_selem_is_enumerated(elem))
			err = sset_enum(elem, argc, argv);
		else
			err = sset_channels(elem, argc, argv);

		if (!err)
			goto done;
		if (err < 0) {
			error("Invalid command!");
			goto done;
		}
	}
	if (!quiet) {
		printf("Simple mixer control '%s',%i\n", snd_mixer_selem_id_get_name(sid), snd_mixer_selem_id_get_index(sid));
		show_selem(handle, sid, "  ", 1);
	}
 done:
	if (! keep_handle) {
		snd_mixer_close(handle);
		handle = NULL;
	}
	return err < 0 ? 1 : 0;
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

static int element_callback(snd_hctl_elem_t *elem, unsigned int mask)
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

static int ctl_callback(snd_hctl_t *ctl ATTRIBUTE_UNUSED, unsigned int mask,
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
			if (res < 0)
				printf("ERR: %s (%d)\n", snd_strerror(res), res);
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

static int melem_event(snd_mixer_elem_t *elem, unsigned int mask)
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

static int mixer_event(snd_mixer_t *mixer ATTRIBUTE_UNUSED, unsigned int mask,
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

/*
 * split a line into tokens
 * the content in the line buffer is modified
 */
static int split_line(char *buf, char **token, int max_token)
{
	char *dst;
	int n, esc, quote;

	for (n = 0; n < max_token; n++) {
		while (isspace(*buf))
			buf++;
		if (! *buf || *buf == '\n')
			return n;
		/* skip comments */
		if (*buf == '#' || *buf == '!')
			return n;
		esc = 0;
		quote = 0;
		token[n] = buf;
		for (dst = buf; *buf && *buf != '\n'; buf++) {
			if (esc)
				esc = 0;
			else if (isspace(*buf) && !quote) {
				buf++;
				break;
			} else if (*buf == '\\') {
				esc = 1;
				continue;
			} else if (*buf == '\'' || *buf == '"') {
				if (! quote) {
					quote = *buf;
					continue;
				} else if (*buf == quote) {
					quote = 0;
					continue;
				}
			}
			*dst++ = *buf;
		}
		*dst = 0;
	}
	return n;
}

#define MAX_ARGS	32

static int exec_stdin(void)
{
	int narg;
	char buf[256], *args[MAX_ARGS];
	int err = 0;

	/* quiet = 1; */
	ignore_error = 1;

	while (fgets(buf, sizeof(buf), stdin)) {
		narg = split_line(buf, args, MAX_ARGS);
		if (narg > 0) {
			if (!strcmp(args[0], "sset") || !strcmp(args[0], "set"))
				err = sset(narg - 1, args + 1, 0, 1);
			else if (!strcmp(args[0], "cset"))
				err = cset(narg - 1, args + 1, 0, 1);
			if (err < 0)
				return 1;
		}
	}
	return 0;
}


int main(int argc, char *argv[])
{
	int badopt, retval, level = 0;
	int read_stdin = 0;
	static const struct option long_option[] =
	{
		{"help", 0, NULL, 'h'},
		{"card", 1, NULL, 'c'},
		{"device", 1, NULL, 'D'},
		{"quiet", 0, NULL, 'q'},
		{"inactive", 0, NULL, 'i'},
		{"debug", 0, NULL, 'd'},
		{"nocheck", 0, NULL, 'n'},
		{"version", 0, NULL, 'v'},
		{"abstract", 1, NULL, 'a'},
		{"stdin", 0, NULL, 's'},
		{"raw-volume", 0, NULL, 'R'},
		{"mapped-volume", 0, NULL, 'M'},
		{NULL, 0, NULL, 0},
	};

	badopt = 0;
	while (1) {
		int c;

		if ((c = getopt_long(argc, argv, "hc:D:qidnva:sRM", long_option, NULL)) < 0)
			break;
		switch (c) {
		case 'h':
			help();
			return 0;
		case 'c':
			{
				int i;
				i = snd_card_get_index(optarg);
				if (i >= 0 && i < 32)
#if defined(SND_LIB_VER) && SND_LIB_VER(1, 2, 5) <= SND_LIB_VERSION
					sprintf(card, "sysdefault:%i", i);
#else
					sprintf(card, "hw:%i", i);
#endif
				else {
					fprintf(stderr, "Invalid card number '%s'.\n", optarg);
					badopt++;
				}
			}
			break;
		case 'D':
			strncpy(card, optarg, sizeof(card)-1);
			card[sizeof(card)-1] = '\0';
			break;
		case 'q':
			quiet = 1;
			break;
		case 'i':
			level |= LEVEL_INACTIVE;
			break;
		case 'd':
			debugflag = 1;
			break;
		case 'n':
			no_check = 1;
			break;
		case 'v':
			printf("amixer version " SND_UTIL_VERSION_STR "\n");
			return 0;
		case 'a':
			smixer_level = 1;
			memset(&smixer_options, 0, sizeof(smixer_options));
			smixer_options.ver = 1;
			if (!strcmp(optarg, "none"))
				smixer_options.abstract = SND_MIXER_SABSTRACT_NONE;
			else if (!strcmp(optarg, "basic"))
				smixer_options.abstract = SND_MIXER_SABSTRACT_BASIC;
			else {
				fprintf(stderr, "Select correct abstraction level (none or basic)...\n");
				badopt++;
			}
			break;
		case 's':
			read_stdin = 1;
			break;
		case 'R':
			std_vol_type = VOL_RAW;
			break;
		case 'M':
			std_vol_type = VOL_MAP;
			break;
		default:
			fprintf(stderr, "Invalid switch or option -%c needs an argument.\n", c);
			badopt++;
		}
	}
	if (badopt)
		return 1;

	smixer_options.device = card;

	if (read_stdin) {
		retval = exec_stdin();
		goto finish;
	}

	if (argc - optind <= 0) {
		retval = selems(LEVEL_BASIC | level) ? 1 : 0;
		goto finish;
	}
	if (!strcmp(argv[optind], "help")) {
		retval = help() ? 1 : 0;
	} else if (!strcmp(argv[optind], "info")) {
		retval = info() ? 1 : 0;
	} else if (!strcmp(argv[optind], "controls")) {
		retval = controls(level) ? 1 : 0;
	} else if (!strcmp(argv[optind], "contents")) {
		retval = controls(LEVEL_BASIC | level) ? 1 : 0;
	} else if (!strcmp(argv[optind], "scontrols") || !strcmp(argv[optind], "simple")) {
		retval = selems(level) ? 1 : 0;
	} else if (!strcmp(argv[optind], "scontents")) {
		retval = selems(LEVEL_BASIC | level) ? 1 : 0;
	} else if (!strcmp(argv[optind], "sset") || !strcmp(argv[optind], "set")) {
		retval = sset(argc - optind - 1, argc - optind > 1 ? argv + optind + 1 : NULL, 0, 0) ? 1 : 0;
	} else if (!strcmp(argv[optind], "sget") || !strcmp(argv[optind], "get")) {
		retval = sset(argc - optind - 1, argc - optind > 1 ? argv + optind + 1 : NULL, 1, 0) ? 1 : 0;
	} else if (!strcmp(argv[optind], "cset")) {
		retval = cset(argc - optind - 1, argc - optind > 1 ? argv + optind + 1 : NULL, 0, 0) ? 1 : 0;
	} else if (!strcmp(argv[optind], "cget")) {
		retval = cset(argc - optind - 1, argc - optind > 1 ? argv + optind + 1 : NULL, 1, 0) ? 1 : 0;
	} else if (!strcmp(argv[optind], "events")) {
		retval = events(argc - optind - 1, argc - optind > 1 ? argv + optind + 1 : NULL);
	} else if (!strcmp(argv[optind], "sevents")) {
		retval = sevents(argc - optind - 1, argc - optind > 1 ? argv + optind + 1 : NULL);
	} else {
		fprintf(stderr, "amixer: Unknown command '%s'...\n", argv[optind]);
		retval = 0;
	}

finish:
	snd_config_update_free_global();

	return retval;
}
