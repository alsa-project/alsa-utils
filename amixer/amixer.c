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
#include <sys/asoundlib.h>
#include "amixer.h"

#define HELPID_HELP             1000
#define HELPID_CARD             1001
#define HELPID_QUIET		1002
#define HELPID_DEBUG            1003
#define HELPID_VERSION		1004

int quiet = 0;
int debugflag = 0;
int card;
int scontrol_contents_is_on = 0;

void error(const char *fmt,...)
{
	va_list va;

	va_start(va, fmt);
	fprintf(stderr, "amixer: ");
	vfprintf(stderr, fmt, va);
	fprintf(stderr, "\n");
	va_end(va);
}

static void help(void)
{
	printf("Usage: amixer <options> command\n");
	printf("\nAvailable options:\n");
	printf("  -h,--help       this help\n");
	printf("  -c,--card #     use a card number (0-%i) or the card name, default %i\n", snd_cards() - 1, card);
	printf("  -D,--debug      debug mode\n");
	printf("  -v,--version    print version of this program\n");
	printf("\nAvailable commands:\n");
	printf("  groups          show all mixer groups\n");
	printf("  gcontents	  show contents of all mixer groups (default command)\n");
	printf("  set G P         set group contents for one mixer group\n");
	printf("  get G P         get group contents for one mixer group\n");
	printf("  elements        show all mixer elements\n");
	printf("  contents        show contents of all mixer elements\n");
	printf("  eset E P	  set extended contents for one mixer element\n");
	printf("  eget E P	  get extended contents for one mixer element\n");
}

int info(void)
{
	int err;
	snd_ctl_t *handle;
	snd_mixer_t *mhandle;
	snd_ctl_hw_info_t info;
	snd_control_list_t clist;
	snd_mixer_simple_control_list_t slist;
	
	if ((err = snd_ctl_open(&handle, card)) < 0) {
		error("Control device %i open error: %s", card, snd_strerror(err));
		return -1;
	}
	if ((err = snd_ctl_hw_info(handle, &info)) < 0) {
		error("Control device %i hw info error: %s", card, snd_strerror(err));
		return -1;
	}
	printf("Card '%s/%s':\n", info.id, info.longname);
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
		return -1;
	}
	if ((err = snd_mixer_simple_control_list(mhandle, &slist)) < 0) {
		error("snd_mixer_simple_control_list failure: %s\n", snd_strerror(err));
	} else {
		printf("  Simple ctrls  : %i\n", clist.controls);
	}
	snd_mixer_close(mhandle);
	return 0;
}

static const char *element_name(const char *name)
{
	static char res[25];
	
	strncpy(res, name, 24);
	res[24] = '\0';
	return res;
}

static int check_range(int val, int min, int max)
{
	if (val < min)
		return min;
	if (val > max)
		return max;
	return val;
}

static int convert_range(int val, int omin, int omax, int nmin, int nmax)
{
	int orange = omax - omin, nrange = nmax - nmin;
	
	if (orange == 0)
		return 0;
	return rint((((double)nrange * ((double)val - (double)omin)) + ((double)orange / 2.0)) / ((double)orange + (double)nmin));
}

static int convert_db_range(int val, int omin, int omax, int nmin, int nmax)
{
	int orange = omax - omin, nrange = nmax - nmin;
	int tmp;
	
	if (orange == 0)
		return 0;
	tmp = rint((((double)nrange * ((double)val - (double)omin)) + ((double)orange / 2.0)) / (double)orange + (double)nmin);
	return tmp;
}

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

	tmp = rint((double)range * ((double)val*.01));
	tmp += min;
#if 0
	printf("%i %i %i %i", val, max, min, tmp);
#endif
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

static const char *get_percent1(int val, int min, int max, int min_dB, int max_dB)
{
	static char str[32];
	int p, db;

	p = convert_prange(val, min, max);
	db = convert_db_range(val, min, max, min_dB, max_dB);
	sprintf(str, "%i [%i%%] [%i.%02idB]", val, p, db / 100, abs(db % 100));
	return str;
}

static int get_volume(char **ptr, int min, int max, int min_dB, int max_dB)
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
	} else if (**ptr == 'd') {
		tmp1 *= 100; tmp1 += tmp2 % 100;
		tmp1 = convert_range(tmp1, min_dB, max_dB, min, max);
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

int controls(void)
{
#if 0
	int err, idx;
	snd_mixer_t *handle;
	snd_mixer_elements_t elements;
	snd_mixer_eid_t *element;
	
	if ((err = snd_mixer_open(&handle, card, device)) < 0) {
		error("Mixer %i/%i open error: %s", card, device, snd_strerror(err));
		return -1;
	}
	bzero(&elements, sizeof(elements));
	if ((err = snd_mixer_elements(handle, &elements)) < 0) {
		error("Mixer %i/%i elements error: %s", card, device, snd_strerror(err));
		return -1;
	}
	elements.pelements = (snd_mixer_eid_t *)malloc(elements.elements_over * sizeof(snd_mixer_eid_t));
	if (!elements.pelements) {
		error("Not enough memory");
		return -1;
	}
	elements.elements_size = elements.elements_over;
	elements.elements_over = elements.elements = 0;
	if ((err = snd_mixer_elements(handle, &elements)) < 0) {
		error("Mixer %i/%i elements (2) error: %s", card, device, snd_strerror(err));
		return -1;
	}
	for (idx = 0; idx < elements.elements; idx++) {
		element = &elements.pelements[idx];
		printf("Element '%s',%i,%s\n", element_name(element->name), element->index, element_type(element->type));
		show_element(handle, element, "  ");
		show_element_info(handle, element, "  ");
	}
	free(elements.pelements);
	snd_mixer_close(handle);
#endif
	return 0;
}

int controls_contents(void)
{
#if 0
	int err, idx;
	snd_mixer_t *handle;
	snd_mixer_elements_t elements;
	snd_mixer_eid_t *element;
	
	if ((err = snd_mixer_open(&handle, card, device)) < 0) {
		error("Mixer %i/%i open error: %s", card, device, snd_strerror(err));
		return -1;
	}
	bzero(&elements, sizeof(elements));
	if ((err = snd_mixer_elements(handle, &elements)) < 0) {
		error("Mixer %i/%i elements error: %s", card, device, snd_strerror(err));
		return -1;
	}
	elements.pelements = (snd_mixer_eid_t *)malloc(elements.elements_over * sizeof(snd_mixer_eid_t));
	if (!elements.pelements) {
		error("Not enough memory");
		return -1;
	}
	elements.elements_size = elements.elements_over;
	elements.elements_over = elements.elements = 0;
	if ((err = snd_mixer_elements(handle, &elements)) < 0) {
		error("Mixer %i/%i elements (2) error: %s", card, device, snd_strerror(err));
		return -1;
	}
	for (idx = 0; idx < elements.elements; idx++) {
		element = &elements.pelements[idx];
		printf("Element '%s',%i,%s\n", element_name(element->name), element->index, element_type(element->type));
		show_element_info(handle, element, "  ");
		show_element_contents(handle, element, "  ");
	}
	free(elements.pelements);
	snd_mixer_close(handle);
#endif
	return 0;
}

static const char *group_name(const char *name)
{
	static char res[25];
	
	strncpy(res, name, 24);
	res[24] = '\0';
	return res;
}

int show_group(void *handle, snd_mixer_sid_t *sid, const char *space)
{
	int err, chn;
	snd_mixer_simple_control_t scontrol;
	
	bzero(&scontrol, sizeof(scontrol));
	scontrol.sid = *sid;
	if ((err = snd_mixer_simple_control_read(handle, &scontrol)) < 0) {
		error("Mixer %i simple_control error: %s", card, snd_strerror(err));
		return -1;
	}
	if (scontrol_contents_is_on && scontrol.channels) {
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
		printf("%sLimits: min = %i, max = %i\n", space, scontrol.min, scontrol.max);
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
	if (scontrol_contents_is_on)
		return 0;
	return 0;
}

int scontrols(void)
{
#if 0
	int err, idx;
	snd_mixer_t *handle;
	snd_mixer_scontrols_t scontrols;
	snd_mixer_sid_t *scontrol;
	
	if ((err = snd_mixer_open(&handle, card, device)) < 0) {
		error("Mixer %i/%i open error: %s", card, device, snd_strerror(err));
		return -1;
	}
	bzero(&scontrols, sizeof(scontrols));
	if ((err = snd_mixer_scontrols(handle, &scontrols)) < 0) {
		error("Mixer %i/%i scontrols error: %s", card, device, snd_strerror(err));
		return -1;
	}
	scontrols.pscontrols = (snd_mixer_sid_t *)malloc(scontrols.scontrols_over * sizeof(snd_mixer_eid_t));
	if (!scontrols.pscontrols) {
		error("Not enough memory");
		return -1;
	}
	scontrols.scontrols_size = scontrols.scontrols_over;
	scontrols.scontrols_over = scontrols.scontrols = 0;
	if ((err = snd_mixer_scontrols(handle, &scontrols)) < 0) {
		error("Mixer %i/%i scontrols (2) error: %s", card, device, snd_strerror(err));
		return -1;
	}
	for (idx = 0; idx < scontrols.scontrols; idx++) {
		scontrol = &scontrols.pscontrols[idx];
		printf("Group '%s',%i\n", scontrol_name(scontrol->name), scontrol->index);
		show_scontrol(handle, scontrol, "  ");
	}
	free(scontrols.pscontrols);
	snd_mixer_close(handle);
#endif
	return 0;
}

int scontrols_contents(void)
{
#if 0
	int err;

	scontrol_contents_is_on = 1;
	err = scontrols();
	scontrol_contents_is_on = 0;
	return err;
#else
	return 0;
#endif
}

static int parse_sid(const char *str, snd_mixer_sid_t *sid)
{
	int c, size;
	char *ptr;

	while (*str == ' ' || *str == '\t')
		str++;
	if (!(*str))
		return 1;
	bzero(sid, sizeof(*sid));
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
		return 1;
	str++;
	if (!isdigit(*str))
		return 1;
	sid->index = atoi(str);
	return 0;
}

int cset(int argc, char *argv[])
{
#if 0
	int err;
	snd_mixer_t *handle;
	snd_mixer_eid_t eid;

	if (argc < 1) {
		fprintf(stderr, "Specify a full element identifier: 'name',index,type\n");
		return 1;
	}
	if (parse_eid(argv[0], &eid)) {
		fprintf(stderr, "Wrong element identifier: %s\n", argv[0]);
		return 1;
	}
	if ((err = snd_mixer_open(&handle, card, device)) < 0) {
		error("Mixer %i/%i open error: %s\n", card, device, snd_strerror(err));
		return -1;
	}
	if (!quiet) {
		printf("Element '%s',%i,%s\n", element_name(eid.name), eid.index, element_type(eid.type));
	}
	switch (eid.type) {
	case SND_MIXER_ETYPE_SWITCH1:
		if (eset_switch1(argc - 1, argv + 1, handle, &eid))
			goto __end;
		break;
	case SND_MIXER_ETYPE_SWITCH2:
		if (eset_switch2(argc - 1, argv + 1, handle, &eid))
			goto __end;
		break;
	case SND_MIXER_ETYPE_VOLUME1:
		if (eset_volume1(argc - 1, argv + 1, handle, &eid))
			goto __end;
		break;
	case SND_MIXER_ETYPE_ACCU3:
		if (eset_accu3(argc - 1, argv + 1, handle, &eid))
			goto __end;
		break;
	case SND_MIXER_ETYPE_MUX1:
		if (eset_mux1(argc - 1, argv + 1, handle, &eid))
			goto __end;
		break;
	case SND_MIXER_ETYPE_MUX2:
		if (eset_mux2(argc - 1, argv + 1, handle, &eid))
			goto __end;
		break;
	}
	if (!quiet) {
		if (snd_mixer_element_has_info(&eid)) {
			show_element_info(handle, &eid, "  ");
		}
		if (snd_mixer_element_has_control(&eid)) {
			show_element_contents(handle, &eid, "  ");
		}
	}
      __end:
	snd_mixer_close(handle);
#endif
	return 0;
}

int cget(int argc, char *argv[])
{
#if 0
	int err;
	snd_mixer_t *handle;
	snd_mixer_eid_t eid;

	if (argc < 1) {
		fprintf(stderr, "Specify a full element identifier: 'name',index,type\n");
		return 1;
	}
	if (parse_eid(argv[0], &eid)) {
		fprintf(stderr, "Wrong element identifier: %s\n", argv[0]);
		return 1;
	}
	if ((err = snd_mixer_open(&handle, card, device)) < 0) {
		error("Mixer %i/%i open error: %s\n", card, device, snd_strerror(err));
		return -1;
	}
	printf("Element '%s',%i,%s\n", element_name(eid.name), eid.index, element_type(eid.type));
	if (show_element(handle, &eid, "  ") >= 0) {
		if (snd_mixer_element_has_info(&eid)) {
			show_element_info(handle, &eid, "  ");
		}
		if (snd_mixer_element_has_control(&eid)) {
			show_element_contents(handle, &eid, "  ");
		}
	}
	snd_mixer_close(handle);
#endif
	return 0;
}

#if 0

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

#endif

int sset(int argc, char *argv[])
{
#if 0
	int err, idx, chn;
	unsigned int channels;
	snd_mixer_t *handle;
	snd_mixer_sid_t sid;
	snd_mixer_scontrol_t scontrol;

	if (argc < 1) {
		fprintf(stderr, "Specify a scontrol identifier: 'name',index\n");
		return 1;
	}
	if (parse_sid(argv[0], &sid)) {
		fprintf(stderr, "Wrong scontrol identifier: %s\n", argv[0]);
		return 1;
	}
	if (argc < 2) {
		fprintf(stderr, "Specify what you want to set...\n");
		return 1;
	}
	if ((err = snd_mixer_open(&handle, card, device)) < 0) {
		error("Mixer %i/%i open error: %s\n", card, device, snd_strerror(err));
		return -1;
	}
	bzero(&scontrol, sizeof(scontrol));
	scontrol.sid = sid;
	if (snd_mixer_scontrol_read(handle, &scontrol)<0) {
		error("Unable to read scontrol '%s',%i: %s\n", scontrol_name(sid.name), sid.index, snd_strerror(err));
		snd_mixer_close(handle);
		return -1;
	}
	channels = scontrol.channels; /* all channels */
	for (idx = 1; idx < argc; idx++) {
		if (!strncmp(argv[idx], "mute", 4) ||
		    !strncmp(argv[idx], "off", 3)) {
			scontrol.mute = scontrol.channels;
			continue;
		} else if (!strncmp(argv[idx], "unmute", 6) ||
		           !strncmp(argv[idx], "on", 2)) {
			scontrol.mute = 0;
			continue;
		} else if (!strncmp(argv[idx], "cap", 3) ||
		           !strncmp(argv[idx], "rec", 3)) {
			scontrol.capture = scontrol.channels;
			continue;
		} else if (!strncmp(argv[idx], "nocap", 5) ||
		           !strncmp(argv[idx], "norec", 5)) {
			scontrol.capture = 0;
			continue;
		}
		if (check_channels(argv[idx], scontrol.channels, &channels))
			continue;
		if (isdigit(argv[idx][0]) ||
		    argv[idx][0] == '+' ||
		    argv[idx][0] == '-') {
			char *ptr;
			int multi;
		
			multi = (strchr(argv[idx], ',') != NULL);
			ptr = argv[idx];
			for (chn = 0; chn <= SND_MIXER_CHN_LAST; chn++) {
				if (!(scontrol.channels & (1<<chn)) ||
				    !(channels & (1<<chn)))
					continue;

				if (! multi)
					ptr = argv[idx];
				scontrol.volume.values[chn] = get_volume_simple(&ptr, scontrol.min, scontrol.max, scontrol.volume.values[chn]);
			}
		} else {
			error("Unknown setup '%s'..\n", argv[idx]);
			snd_mixer_close(handle);
			return -1;
		}
	} 
	if (snd_mixer_scontrol_write(handle, &scontrol)<0) {
		error("Unable to write scontrol '%s',%i: %s\n", scontrol_name(sid.name), sid.index, snd_strerror(err));
		snd_mixer_close(handle);
		return -1;
	}
	if (!quiet) {
		printf("Group '%s',%i\n", scontrol_name(sid.name), sid.index);
		scontrol_contents_is_on = 1;
		show_scontrol(handle, &sid, "  ");
		scontrol_contents_is_on = 0;
	}
	snd_mixer_close(handle);
#endif
	return 0;
}

int sget(int argc, char *argv[])
{
#if 0
	int err;
	snd_mixer_t *handle;
	snd_mixer_sid_t sid;

	if (argc < 1) {
		fprintf(stderr, "Specify a scontrol identifier: 'name',index\n");
		return 1;
	}
	if (parse_sid(argv[0], &sid)) {
		fprintf(stderr, "Wrong scontrol identifier: %s\n", argv[0]);
		return 1;
	}
	if ((err = snd_mixer_open(&handle, card, device)) < 0) {
		error("Mixer %i/%i open error: %s\n", card, device, snd_strerror(err));
		return -1;
	}
	if (!quiet) {
		printf("Group '%s',%i\n", scontrol_name(sid.name), sid.index);
		scontrol_contents_is_on = 1;
		show_scontrol(handle, &sid, "  ");
		scontrol_contents_is_on = 0;
	}
	snd_mixer_close(handle);
#endif
	return 0;
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
	card = snd_defaults_mixer_card();
	if (card < 0) {
		fprintf(stderr, "The ALSA sound driver was not detected in this system.\n");
		return 1;
	}
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
			card = snd_card_name(optarg);
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
		return scontrols_contents() ? 1 : 0;
	}
	if (!strcmp(argv[optind], "info")) {
		return info() ? 1 : 0;
	} else if (!strcmp(argv[optind], "controls")) {
		return controls() ? 1 : 0;
	} else if (!strcmp(argv[optind], "contents")) {
		return controls_contents() ? 1 : 0;
	} else if (!strcmp(argv[optind], "simple")) {
		return scontrols() ? 1 : 0;
	} else if (!strcmp(argv[optind], "scontents")) {
		return scontrols_contents() ? 1 : 0;
	} else if (!strcmp(argv[optind], "sset")) {
		return sset(argc - optind - 1, argc - optind > 1 ? argv + optind + 1 : NULL) ? 1 : 0;
	} else if (!strcmp(argv[optind], "sget")) {
		return sget(argc - optind - 1, argc - optind > 1 ? argv + optind + 1 : NULL) ? 1 : 0;
	} else if (!strcmp(argv[optind], "cset")) {
		return cset(argc - optind - 1, argc - optind > 1 ? argv + optind + 1 : NULL) ? 1 : 0;
	} else if (!strcmp(argv[optind], "cget")) {
		return cget(argc - optind - 1, argc - optind > 1 ? argv + optind + 1 : NULL) ? 1 : 0;
	} else {
		fprintf(stderr, "amixer: Unknown command '%s'...\n", argv[optind]);
	}

	return 0;
}
