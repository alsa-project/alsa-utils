/*
 *   ALSA command line mixer utility
 *   Copyright (c) 1999 by Jaroslav Kysela <perex@suse.cz>
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
#define HELPID_DEVICE		1002
#define HELPID_QUIET		1003
#define HELPID_DEBUG            1004
#define HELPID_VERSION		1005

int quiet = 0;
int debugflag = 0;
int card;
int device;
int group_contents_is_on = 0;

struct mixer_types {
	int type;
	char *name;
};

struct mixer_types mixer_types[] = {
	{ SND_MIXER_ETYPE_INPUT, 	"Input" },
	{ SND_MIXER_ETYPE_OUTPUT,	"Output" },
	{ SND_MIXER_ETYPE_CAPTURE,	"Capture" },
	{ SND_MIXER_ETYPE_PLAYBACK,	"Playback" },
	{ SND_MIXER_ETYPE_ADC,		"ADC" },
	{ SND_MIXER_ETYPE_DAC,		"DAC" },
	{ SND_MIXER_ETYPE_SWITCH1,	"Switch1" },
	{ SND_MIXER_ETYPE_SWITCH2,	"Switch2" },
	{ SND_MIXER_ETYPE_SWITCH3,	"Switch3" },
	{ SND_MIXER_ETYPE_VOLUME1,	"Volume1" },
	{ SND_MIXER_ETYPE_VOLUME2,	"Volume2" },
	{ SND_MIXER_ETYPE_ACCU1,	"Accumulator1" },
	{ SND_MIXER_ETYPE_ACCU2,	"Accumulator2" },
	{ SND_MIXER_ETYPE_ACCU3,	"Accumulator3" },
	{ SND_MIXER_ETYPE_MUX1,		"Mux1" },
	{ SND_MIXER_ETYPE_MUX2,		"Mux2" },
	{ SND_MIXER_ETYPE_TONE_CONTROL1, "ToneControl1" },
	{ SND_MIXER_ETYPE_EQUALIZER1,	"Equalizer1" },
	{ SND_MIXER_ETYPE_3D_EFFECT1,	"3D-Effect1" },
	{ SND_MIXER_ETYPE_PRE_EFFECT1,	"PredefinedEffect1" },
	{ -1,				NULL }
};

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
	printf("  -d,--device #   use a device number, default %i\n", device);
	printf("  -D,--debug      debug mode\n");
	printf("  -v,--version    print version of this program\n");
	printf("\nAvailable commands:\n");
	printf("  info            show useful information for the selected mixer\n");
	printf("  groups          show all mixer groups\n");
	printf("  gcontents	  show contents of all mixer groups\n");
	printf("  set G P         set group setup\n");
	printf("  get G P         get group setup\n");
	printf("  elements        show information about all mixer elements\n");
	printf("  contents        show contents of all mixer elements\n");
	printf("  eset E P	  set extended setup for one mixer element\n");
	printf("  eget E P	  get extended setup for one mixer element\n");
}

int info(void)
{
	int err;
	snd_mixer_t *handle;
	snd_mixer_info_t info;
	
	if ((err = snd_mixer_open(&handle, card, device)) < 0) {
		error("Mixer %i/%i open error: %s", card, device, snd_strerror(err));
		return -1;
	}
	if ((err = snd_mixer_info(handle, &info)) < 0) {
		error("Mixer %i/%i info error: %s", card, device, snd_strerror(err));
		return -1;
	}
	printf("Mixer '%s/%s':\n", info.id, info.name);
	printf("  Elements      : %i\n", info.elements);
	printf("  Groups        : %i\n", info.groups);
	printf("  Switches      : %i\n", info.switches);
	printf("  Attribute     : 0x%x\n", info.attrib);
	snd_mixer_close(handle);
	return 0;
}

static const char *element_name(const char *name)
{
	static char res[25];
	
	strncpy(res, name, 24);
	res[24] = '\0';
	return res;
}

static const char *element_type(int type)
{
	int idx;
	static char str[32];

	for (idx = 0; mixer_types[idx].type >= 0; idx++)
		if (type == mixer_types[idx].type)
			return mixer_types[idx].name;
	sprintf(str, "Type%i", type);
	return str;
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
	return rint((((double)nrange * ((double)val - (double)omin)) + ((double)orange / 2.0)) / (double)orange + (double)nmin);
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

static int convert_prange(int val, int min, int max)
{
	return convert_range(val, 0, 100, min, max);
}

static const char *get_percent(int val, int min, int max)
{
	static char str[32];
	int p;
	
	p = convert_range(val, min, max, 0, 100);
	sprintf(str, "%i [%i%%]", val, p);
	return str;
}

static const char *get_percent1(int val, int min, int max, int min_dB, int max_dB)
{
	static char str[32];
	int p, db;

	p = convert_range(val, min, max, 0, 100);
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
		tmp1 = convert_prange(tmp, min, max);
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

static int get_volume_simple(char **ptr, int min, int max)
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
		tmp1 = convert_prange(tmp, min, max);
		(*ptr)++;
	}
	tmp1 = check_range(tmp1, min, max);
	if (**ptr == ',')
		(*ptr)++;
	return tmp1;
}

int show_element(void *handle, snd_mixer_eid_t *eid, const char *space)
{
	int err, idx;
	snd_mixer_routes_t routes;
	snd_mixer_eid_t *element;
	
	bzero(&routes, sizeof(routes));
	routes.eid = *eid;
	if ((err = snd_mixer_routes(handle, &routes)) < 0) {
		error("Mixer %i/%i route error: %s", card, device, snd_strerror(err));
		return -1;
	}
	if (!routes.routes_over)
		return 0;
	routes.proutes = (snd_mixer_eid_t *)malloc(routes.routes_over * sizeof(snd_mixer_eid_t));
	if (!routes.proutes) {
		error("Not enough memory...");
		return -1;
	}
	routes.routes_size = routes.routes_over;
	routes.routes = routes.routes_over = 0;
	if ((err = snd_mixer_routes(handle, &routes)) < 0) {
		error("Mixer %i/%i group (2) error: %s", card, device, snd_strerror(err));
		return -1;
	}
	for (idx = 0; idx < routes.routes; idx++) {
		element = &routes.proutes[idx];
		printf("%sRoute to element '%s',%i,%s\n", space, element_name(element->name), element->index, element_type(element->type));
	}
	free(routes.proutes);
	return 0;
}

static const char *speaker_position(int position)
{
	static char str[32];

	switch (position) {
	case SND_MIXER_VOICE_UNUSED:
		return "Unused";
	case SND_MIXER_VOICE_MONO:
		return "Mono";
	case SND_MIXER_VOICE_LEFT:
		return "Left";
	case SND_MIXER_VOICE_RIGHT:
		return "Right";
	case SND_MIXER_VOICE_CENTER:
		return "Center";
	case SND_MIXER_VOICE_REAR_LEFT:
		return "Rear-Left";
	case SND_MIXER_VOICE_REAR_RIGHT:
		return "Rear-Right";
	default:
		sprintf(str, "Speaker%i", position);
		return str;
	}
}

int show_mux1_info(void *handle, snd_mixer_element_info_t *info, const char *space)
{
	int idx, idx1, err;
	snd_mixer_elements_t elements;
	snd_mixer_routes_t routes;
	snd_mixer_eid_t *element;

	printf("%sMux supports none input: %s\n", space, (info->data.mux1.attrib & SND_MIXER_MUX1_NONE) ? "YES" : "NO");
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
		bzero(&routes, sizeof(routes));
		routes.eid = elements.pelements[idx];
		if ((err = snd_mixer_routes(handle, &routes)) < 0) {
			error("Mixer %i/%i route error: %s", card, device, snd_strerror(err));
			free(elements.pelements);
			return -1;
		}
		if (!routes.routes_over)
			continue;
		routes.proutes = (snd_mixer_eid_t *)malloc(routes.routes_over * sizeof(snd_mixer_eid_t));
		if (!routes.proutes) {
			error("Not enough memory...");
			free(elements.pelements);
			return -1;
		}
		routes.routes_size = routes.routes_over;
		routes.routes = routes.routes_over = 0;
		if ((err = snd_mixer_routes(handle, &routes)) < 0) {
			error("Mixer %i/%i group (2) error: %s", card, device, snd_strerror(err));
			free(elements.pelements);
			return -1;
		}
		for (idx1 = 0; idx1 < routes.routes; idx1++) {
			element = &routes.proutes[idx1];
			if (!strncmp(element->name, info->eid.name, sizeof(element->name)) &&
			    element->index == info->eid.index &&
			    element->type == info->eid.type)
				printf("%sInput element '%s',%i,%s\n", space, element_name(routes.eid.name), routes.eid.index, element_type(routes.eid.type));
		}
		free(routes.proutes);
	}
	free(elements.pelements);
	return 0;
} 

int show_element_info(void *handle, snd_mixer_eid_t *eid, const char *space)
{
	int err, idx;
	snd_mixer_element_info_t info;
	
	if (snd_mixer_element_has_info(eid) != 1)
		return 0;
	bzero(&info, sizeof(info));
	info.eid = *eid;
	if ((err = snd_mixer_element_info_build(handle, &info)) < 0) {
		error("Mixer %i/%i info error: %s", card, device, snd_strerror(err));
		return -1;
	}
	switch (info.eid.type) {
	case SND_MIXER_ETYPE_INPUT:
	case SND_MIXER_ETYPE_OUTPUT:
		if (info.data.io.attrib) {
			printf("%sAttributes%s\n", space,
				info.data.io.attrib & SND_MIXER_EIO_DIGITAL ? " digital" : "");
		}
		for (idx = 0; idx < info.data.io.voices; idx++) {
			if (!info.data.io.pvoices[idx].vindex) {
				printf("%sVoice %i: %s\n",
					space,
					idx,
					speaker_position(info.data.io.pvoices[idx].voice));
			} else {
				printf("%sVoice %i: %i\n",
					space,
					idx,
					info.data.io.pvoices[idx].voice);
			}
		}
		break;
	case SND_MIXER_ETYPE_CAPTURE:
	case SND_MIXER_ETYPE_PLAYBACK:
		for (idx = 0; idx < info.data.pcm.devices; idx++) {
			printf("%sPCM device %i %i\n",
					space,
					idx,
					info.data.pcm.pdevices[idx]);
		}
		break;
	case SND_MIXER_ETYPE_ADC:
	case SND_MIXER_ETYPE_DAC:
		printf("%sResolution %i-bits\n", space, info.data.converter.resolution);
		break;
	case SND_MIXER_ETYPE_SWITCH3:
		printf("%sSwitch type is ", space);
		switch (info.data.switch3.type) {
		case SND_MIXER_SWITCH3_FULL_FEATURED:
			printf("full featured\n");
			break;
		case SND_MIXER_SWITCH3_ALWAYS_DESTINATION:
			printf("always destination\n");
			break;
		case SND_MIXER_SWITCH3_ONE_DESTINATION:
			printf("one destination\n");
			break;
		case SND_MIXER_SWITCH3_ALWAYS_ONE_DESTINATION:
			printf("always one destination\n");
			break;
		default:
			printf("unknown %i\n", info.data.switch3.type);
		}
		for (idx = 0; idx < info.data.switch3.voices; idx++) {
			snd_mixer_voice_t voice = info.data.switch3.pvoices[idx];
			if (voice.vindex) {
				printf("%sVoice %i: %i\n", space, idx, voice.voice);
			} else {
				printf("%sSpeaker %i: %s\n", space, idx, speaker_position(voice.voice));
			}
		}
		break;
	case SND_MIXER_ETYPE_VOLUME1:
		for (idx = 0; idx < info.data.volume1.range; idx++) {
			struct snd_mixer_element_volume1_range *range = &info.data.volume1.prange[idx];
			printf("%sVoice %i: Min %i (%i.%02idB), Max %i (%i.%02idB)\n",
				space,
				idx,
				range->min, range->min_dB / 100, abs(range->min_dB % 100),
				range->max, range->max_dB / 100, abs(range->max_dB % 100));
		}
		break;
	case SND_MIXER_ETYPE_ACCU1:
		printf("%sAttenuation %i.%02idB\n", space,
				info.data.accu1.attenuation / 100,
				abs(info.data.accu1.attenuation % 100));
		break;
	case SND_MIXER_ETYPE_ACCU2:
		printf("%sAttenuation %i.%02idB\n", space,
				info.data.accu2.attenuation / 100,
				abs(info.data.accu1.attenuation % 100));
		break;
	case SND_MIXER_ETYPE_ACCU3:
		for (idx = 0; idx < info.data.accu3.range; idx++) {
			struct snd_mixer_element_accu3_range *range = &info.data.accu3.prange[idx];
			printf("%sVoice %i: Min %i (%i.%02idB), Max %i (%i.%02idB)\n",
				space,
				idx,
				range->min, range->min_dB / 100, abs(range->min_dB % 100),
				range->max, range->max_dB / 100, abs(range->max_dB % 100));
		}
		break;
	case SND_MIXER_ETYPE_MUX1:
		show_mux1_info(handle, &info, space);
		break;
	case SND_MIXER_ETYPE_TONE_CONTROL1:
		if (info.data.tc1.tc & SND_MIXER_TC1_SW)
			printf("%sOn/Off switch\n", space);
		if (info.data.tc1.tc & SND_MIXER_TC1_BASS)
			printf("%sBass control: Min %i (%i.%02idB), Max %i (%i.%02idB)\n",
					space,
					info.data.tc1.min_bass,
					info.data.tc1.min_bass_dB / 100,
					abs(info.data.tc1.min_bass_dB % 100),
					info.data.tc1.max_bass,
					info.data.tc1.max_bass_dB / 100,
					abs(info.data.tc1.max_bass_dB % 100));
		if (info.data.tc1.tc & SND_MIXER_TC1_TREBLE)
			printf("%sTreble control: Min %i (%i.%02idB), Max %i (%i.%02idB)\n",
					space,
					info.data.tc1.min_treble,
					info.data.tc1.min_treble_dB / 100,
					abs(info.data.tc1.min_treble_dB % 100),
					info.data.tc1.max_treble,
					info.data.tc1.max_treble_dB / 100,
					abs(info.data.tc1.max_treble_dB % 100));
		break;
	case SND_MIXER_ETYPE_3D_EFFECT1:
		if (info.data.teffect1.effect & SND_MIXER_EFF1_SW)
			printf("%sOn/Off switch\n", space);
		if (info.data.teffect1.effect & SND_MIXER_EFF1_MONO_SW)
			printf("%sMono processing switch\n", space);
		if (info.data.teffect1.effect & SND_MIXER_EFF1_WIDE)
			printf("%sWide: Min %i, Max %i\n", space,
					info.data.teffect1.min_wide,
					info.data.teffect1.max_wide);
		if (info.data.teffect1.effect & SND_MIXER_EFF1_VOLUME)
			printf("%sVolume: Min %i, Max %i\n", space,
					info.data.teffect1.min_volume,
					info.data.teffect1.max_volume);
		if (info.data.teffect1.effect & SND_MIXER_EFF1_CENTER)
			printf("%sCenter: Min %i, Max %i\n", space,
					info.data.teffect1.min_center,
					info.data.teffect1.max_center);
		if (info.data.teffect1.effect & SND_MIXER_EFF1_SPACE)
			printf("%sSpace: Min %i, Max %i\n", space,
					info.data.teffect1.min_space,
					info.data.teffect1.max_space);
		if (info.data.teffect1.effect & SND_MIXER_EFF1_DEPTH)
			printf("%sDepth: Min %i, Max %i\n", space,
					info.data.teffect1.min_depth,
					info.data.teffect1.max_depth);
		if (info.data.teffect1.effect & SND_MIXER_EFF1_DELAY)
			printf("%sDelay: Min %i, Max %i\n", space,
					info.data.teffect1.min_delay,
					info.data.teffect1.max_delay);
		if (info.data.teffect1.effect & SND_MIXER_EFF1_FEEDBACK)
			printf("%sFeedback: Min %i, Max %i\n", space,
					info.data.teffect1.min_feedback,
					info.data.teffect1.max_feedback);
		break;
	default:
		printf("%sInfo handler for type %i is not available\n", space, info.eid.type);
	}
	snd_mixer_element_info_free(&info);
	return 0;
}

int show_element_contents(void *handle, snd_mixer_eid_t *eid, const char *space)
{
	int err, idx;
	snd_mixer_element_t element;
	snd_mixer_element_info_t info;
	
	if (snd_mixer_element_has_control(eid) != 1)
		return 0;
	bzero(&element, sizeof(element));
	bzero(&info, sizeof(info));
	element.eid = info.eid = *eid;
	if ((err = snd_mixer_element_build(handle, &element)) < 0) {
		error("Mixer %i/%i element error: %s", card, device, snd_strerror(err));
		return -1;
	}
	if (snd_mixer_element_has_info(eid) == 1) {
		if ((err = snd_mixer_element_info_build(handle, &info)) < 0) {
			error("Mixer %i/%i element error: %s", card, device, snd_strerror(err));
			return -1;
		}
	}
	switch (element.eid.type) {
	case SND_MIXER_ETYPE_SWITCH1:
		for (idx = 0; idx < element.data.switch1.sw; idx++) {
			int val = snd_mixer_get_bit(element.data.switch1.psw, idx);
			printf("%sVoice %i: Switch is %s\n", space, idx, val ? "ON" : "OFF");
		}
		break;
	case SND_MIXER_ETYPE_SWITCH2:
		printf("%sSwitch is %s\n", space, element.data.switch2.sw ? "ON" : "OFF");
		break;
	case SND_MIXER_ETYPE_SWITCH3:
		if (element.data.switch3.rsw != info.data.switch3.voices * info.data.switch3.voices) {
			error("Switch3 !!!\n");
			goto __end;
		}
		for (idx = 0; idx < element.data.switch3.rsw; idx++) {
			snd_mixer_voice_t input, output;
			int val = snd_mixer_get_bit(element.data.switch3.prsw, idx);
			printf("%sInput <", space);
			input = info.data.switch3.pvoices[idx / info.data.switch3.voices];
			output = info.data.switch3.pvoices[idx % info.data.switch3.voices];
			if (input.vindex) {
				printf("voice %i", input.voice);
			} else {
				printf(speaker_position(input.voice));
			}
			printf("> Output <");
			if (output.vindex) {
				printf("voice %i", output.voice);
			} else {
				printf(speaker_position(output.voice));
			}
			printf(">: Switch is %s\n", val ? "ON" : "OFF");
		}
		break;
	case SND_MIXER_ETYPE_VOLUME1:
		for (idx = 0; idx < element.data.volume1.voices; idx++) {
			int val = element.data.volume1.pvoices[idx];
			printf("%sVoice %i: Value %s\n", space, idx,
				get_percent1(val, info.data.volume1.prange[idx].min,
						  info.data.volume1.prange[idx].max,
						  info.data.volume1.prange[idx].min_dB,
						  info.data.volume1.prange[idx].max_dB));
		}
		break;
	case SND_MIXER_ETYPE_ACCU3:
		for (idx = 0; idx < element.data.accu3.voices; idx++) {
			int val = element.data.accu3.pvoices[idx];
			printf("%sVoice %i: Value %s\n", space, idx,
				get_percent1(val, info.data.accu3.prange[idx].min,
						  info.data.accu3.prange[idx].max,
						  info.data.accu3.prange[idx].min_dB,
						  info.data.accu3.prange[idx].max_dB));
		}
		break;
	case SND_MIXER_ETYPE_MUX1:
		for (idx = 0; idx < element.data.mux1.output; idx++) {
			snd_mixer_eid_t *eid = &element.data.mux1.poutput[idx];
			printf("%sVoice %i: Element '%s',%i,%i\n", space, idx,
					element_name(eid->name),
					eid->index, eid->type);
		}
		break;
	case SND_MIXER_ETYPE_TONE_CONTROL1:
		if (element.data.tc1.tc & SND_MIXER_TC1_SW)
			printf("%sOn/Off switch is %s\n", space, element.data.tc1.sw ? "ON" : "OFF");
		if (element.data.tc1.tc & SND_MIXER_TC1_BASS)
			printf("%sBass: %s\n", space, get_percent1(element.data.tc1.bass, info.data.tc1.min_bass, info.data.tc1.max_bass, info.data.tc1.min_bass_dB, info.data.tc1.max_bass_dB));
		if (element.data.tc1.tc & SND_MIXER_TC1_TREBLE)
			printf("%sTreble: %s\n", space, get_percent1(element.data.tc1.treble, info.data.tc1.min_treble, info.data.tc1.max_treble, info.data.tc1.min_treble_dB, info.data.tc1.max_treble_dB));
		break;
	case SND_MIXER_ETYPE_3D_EFFECT1:
		if (element.data.teffect1.effect & SND_MIXER_EFF1_SW)
			printf("%sOn/Off switch is %s\n", space, element.data.teffect1.sw ? "ON" : "OFF");
		if (element.data.teffect1.effect & SND_MIXER_EFF1_MONO_SW)
			printf("%sMono processing switch is %s\n", space, element.data.teffect1.mono_sw ? "ON" : "OFF");
		if (element.data.teffect1.effect & SND_MIXER_EFF1_WIDE)
			printf("%sWide: %s\n", space, get_percent(element.data.teffect1.wide, info.data.teffect1.min_wide, info.data.teffect1.max_wide));
		if (element.data.teffect1.effect & SND_MIXER_EFF1_VOLUME)
			printf("%sVolume: %s\n", space, get_percent(element.data.teffect1.volume, info.data.teffect1.min_volume, info.data.teffect1.max_volume));
		if (element.data.teffect1.effect & SND_MIXER_EFF1_CENTER)
			printf("%sCenter: %s\n", space, get_percent(element.data.teffect1.center, info.data.teffect1.min_center, info.data.teffect1.max_center));
		if (element.data.teffect1.effect & SND_MIXER_EFF1_SPACE)
			printf("%sSpace: %s\n", space, get_percent(element.data.teffect1.space, info.data.teffect1.min_space, info.data.teffect1.max_space));
		if (element.data.teffect1.effect & SND_MIXER_EFF1_DEPTH)
			printf("%sDepth: %s\n", space, get_percent(element.data.teffect1.depth, info.data.teffect1.min_depth, info.data.teffect1.max_depth));
		if (element.data.teffect1.effect & SND_MIXER_EFF1_DELAY)
			printf("%sDelay: %s\n", space, get_percent(element.data.teffect1.delay, info.data.teffect1.min_delay, info.data.teffect1.max_delay));
		if (element.data.teffect1.effect & SND_MIXER_EFF1_FEEDBACK)
			printf("%sFeedback: %s\n", space, get_percent(element.data.teffect1.feedback, info.data.teffect1.min_feedback, info.data.teffect1.max_feedback));
		break;
	default:
		printf("%sRead handler for type %i is not available\n", space, element.eid.type);
	}
      __end:
	snd_mixer_element_free(&element);
	if (snd_mixer_element_has_info(eid))
		snd_mixer_element_info_free(&info);
	return 0;
}

int elements(void)
{
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
	return 0;
}

int elements_contents(void)
{
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
	return 0;
}

static const char *group_name(const char *name)
{
	static char res[25];
	
	strncpy(res, name, 24);
	res[24] = '\0';
	return res;
}

int show_group(void *handle, snd_mixer_gid_t *gid, const char *space)
{
	int err, idx, chn;
	snd_mixer_group_t group;
	snd_mixer_eid_t *element;
	
	bzero(&group, sizeof(group));
	group.gid = *gid;
	if ((err = snd_mixer_group_read(handle, &group)) < 0) {
		error("Mixer %i/%i group error: %s", card, device, snd_strerror(err));
		return -1;
	}
	if (group_contents_is_on && group.channels) {
		printf("%sCapabilities:", space);
		if (group.caps & SND_MIXER_GRPCAP_VOLUME)
			printf(" volume");
		if (group.caps & SND_MIXER_GRPCAP_MUTE)
			printf(" mute");
		if (group.caps & SND_MIXER_GRPCAP_JOINTLY_MUTE)
			printf(" jointly-mute");
		if (group.caps & SND_MIXER_GRPCAP_CAPTURE) {
			printf(" capture");
		} else {
			group.capture = 0;
		}
		if (group.caps & SND_MIXER_GRPCAP_JOINTLY_CAPTURE)
			printf(" jointly-capture");
		if (group.caps & SND_MIXER_GRPCAP_EXCL_CAPTURE)
			printf(" exclusive-capture");
		printf("\n");
		if ((group.caps & SND_MIXER_GRPCAP_CAPTURE) &&
		    (group.caps & SND_MIXER_GRPCAP_EXCL_CAPTURE))
			printf("%sCapture exclusive group: %i\n", space, group.capture_group);
		printf("%sChannels: ", space);
		if (group.channels == SND_MIXER_CHN_MASK_MONO) {
			printf("Mono");
		} else {
			for (chn = 0; chn <= SND_MIXER_CHN_LAST; chn++) {
				if (!(group.channels & (1<<chn)))
					continue;
				printf("%s ", snd_mixer_channel_name(chn));
			}
		}
		printf("\n");
		printf("%sLimits: min = %i, max = %i\n", space, group.min, group.max);
		if (group.channels == SND_MIXER_CHN_MASK_MONO) {
			printf("%sMono: %s [%s]\n", space, get_percent(group.volume.names.front_left, group.min, group.max), group.mute & SND_MIXER_CHN_MASK_MONO ? "mute" : "on");
		} else {
			for (chn = 0; chn <= SND_MIXER_CHN_LAST; chn++) {
				if (!(group.channels & (1<<chn)))
					continue;
				printf("%s%s: %s [%s] [%s]\n",
						space,
						snd_mixer_channel_name(chn),
						get_percent(group.volume.values[chn], group.min, group.max),
						group.mute & (1<<chn) ? "mute" : "on",
						group.capture & (1<<chn) ? "capture" : "---");
			}
		}
	}
	if (group_contents_is_on)
		return 0;
	group.pelements = (snd_mixer_eid_t *)malloc(group.elements_over * sizeof(snd_mixer_eid_t));
	if (!group.pelements) {
		error("Not enough memory...");
		return -1;
	}
	group.elements_size = group.elements_over;
	group.elements = group.elements_over = 0;
	if ((err = snd_mixer_group_read(handle, &group)) < 0) {
		error("Mixer %i/%i group (2) error: %s", card, device, snd_strerror(err));
		return -1;
	}
	for (idx = 0; idx < group.elements; idx++) {
		element = &group.pelements[idx];
		printf("%sElement '%s',%i,%s\n", space, element_name(element->name), element->index, element_type(element->type));
	}
	free(group.pelements);
	return 0;
}

int groups(void)
{
	int err, idx;
	snd_mixer_t *handle;
	snd_mixer_groups_t groups;
	snd_mixer_gid_t *group;
	
	if ((err = snd_mixer_open(&handle, card, device)) < 0) {
		error("Mixer %i/%i open error: %s", card, device, snd_strerror(err));
		return -1;
	}
	bzero(&groups, sizeof(groups));
	if ((err = snd_mixer_groups(handle, &groups)) < 0) {
		error("Mixer %i/%i groups error: %s", card, device, snd_strerror(err));
		return -1;
	}
	groups.pgroups = (snd_mixer_gid_t *)malloc(groups.groups_over * sizeof(snd_mixer_eid_t));
	if (!groups.pgroups) {
		error("Not enough memory");
		return -1;
	}
	groups.groups_size = groups.groups_over;
	groups.groups_over = groups.groups = 0;
	if ((err = snd_mixer_groups(handle, &groups)) < 0) {
		error("Mixer %i/%i groups (2) error: %s", card, device, snd_strerror(err));
		return -1;
	}
	for (idx = 0; idx < groups.groups; idx++) {
		group = &groups.pgroups[idx];
		printf("Group '%s',%i\n", group_name(group->name), group->index);
		show_group(handle, group, "  ");
	}
	free(groups.pgroups);
	snd_mixer_close(handle);
	return 0;
}

int groups_contents(void)
{
	int err;

	group_contents_is_on = 1;
	err = groups();
	group_contents_is_on = 0;
	return err;
}

static int parse_eid(const char *str, snd_mixer_eid_t *eid)
{
	int c, size, idx;
	char *ptr;

	while (*str == ' ' || *str == '\t')
		str++;
	if (!(*str))
		return 1;
	bzero(eid, sizeof(*eid));
	ptr = eid->name;
	size = 0;
	if (*str != '"' && *str != '\'') {
		while (*str && *str != ',') {
			if (size < sizeof(eid->name)) {
				*ptr++ = *str;
				size++;
			}
			str++;
		}
	} else {
		c = *str++;
		while (*str && *str != c) {
			if (size < sizeof(eid->name)) {
				*ptr++ = *str;
				size++;
			}
			str++;
		}
		if (*str == c)
			str++;
	}
	if (*str != ',')
		return 1;
	str++;
	if (!isdigit(*str))
		return 1;
	eid->index = atoi(str);
	while (isdigit(*str))
		str++;
	if (*str != ',')
		return 1;
	str++;
	if (isdigit(*str)) {
		eid->type = atoi(str);
		return 0;
	} else {
		for (idx = 0; mixer_types[idx].type >= 0; idx++)
			if (!strncmp(mixer_types[idx].name, str, strlen(mixer_types[idx].name))) {
				eid->type = mixer_types[idx].type;
				return 0;
			}
	}
	return 1;
}

static int parse_gid(const char *str, snd_mixer_gid_t *gid)
{
	int c, size;
	char *ptr;

	while (*str == ' ' || *str == '\t')
		str++;
	if (!(*str))
		return 1;
	bzero(gid, sizeof(*gid));
	ptr = gid->name;
	size = 0;
	if (*str != '"' && *str != '\'') {
		while (*str && *str != ',') {
			if (size < sizeof(gid->name)) {
				*ptr++ = *str;
				size++;
			}
			str++;
		}
	} else {
		c = *str++;
		while (*str && *str != c) {
			if (size < sizeof(gid->name)) {
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
	gid->index = atoi(str);
	return 0;
}

int eset_switch1(int argc, char *argv[], void *handle, snd_mixer_eid_t *eid)
{
	int err, tmp, idx = 0;
	snd_mixer_element_t element;
	char *ptr;

	if (argc != 1) {
		fprintf(stderr, "The set Switch1 command requires an argument:\n");
		fprintf(stderr, "     on/off[,on/off] ...\n");
		return 1;
	}
	bzero(&element, sizeof(element));
	element.eid = *eid;
	if ((err = snd_mixer_element_build(handle, &element)) < 0) {
		error("Mixer element build error: %s", snd_strerror(err));
		return 1;
	}
	if (!strcmp(argv[0], "on") || !strcmp(argv[0], "off")) {
		tmp = !strcmp(argv[0], "on");
		for (idx = 0; idx < element.data.switch1.sw; idx++)
			snd_mixer_set_bit(element.data.switch1.psw, idx, tmp);
	} else {
		ptr = argv[idx];
		for (idx = 0; idx < element.data.switch1.sw; idx++) {
			tmp = !strncmp(ptr, "on", 2);
			snd_mixer_set_bit(element.data.switch1.psw, idx, tmp);
			while (*ptr && *ptr != ',')
				ptr++;
			if (*ptr == ',')
				ptr++;
		}
	}
	if ((err = snd_mixer_element_write(handle, &element)) < 0) {
		error("Mixer element write error: %s\n", snd_strerror(err));
		snd_mixer_element_free(&element);
		return 1;
	}
	snd_mixer_element_free(&element);
	return 0;
}

int eset_switch2(int argc, char *argv[], void *handle, snd_mixer_eid_t *eid)
{
	int err;
	snd_mixer_element_t element;

	if (argc != 1) {
		fprintf(stderr, "The set Switch2 command requires an argument:\n");
		fprintf(stderr, "     on/off\n");
		return 1;
	}
	bzero(&element, sizeof(element));
	element.eid = *eid;
	if ((err = snd_mixer_element_build(handle, &element)) < 0) {
		error("Mixer element build error: %s", snd_strerror(err));
		return 1;
	}
	element.data.switch2.sw = !strcmp(argv[0], "on") ? 1 : 0;
	if ((err = snd_mixer_element_write(handle, &element)) < 0) {
		error("Mixer element write error: %s\n", snd_strerror(err));
		snd_mixer_element_free(&element);
		return 1;
	}
	snd_mixer_element_free(&element);
	return 0;
}

int eset_volume1(int argc, char *argv[], void *handle, snd_mixer_eid_t *eid)
{
	int err, tmp, idx = 0;
	snd_mixer_element_t element;
	snd_mixer_element_info_t info;
	char *ptr;
	
	if (argc != 1 || (!isdigit(*argv[0]) && *argv[0] != ':')) {
		fprintf(stderr, "The set Volume1 command requires an argument:\n");
		fprintf(stderr, "     vol[,vol] ...\n");
		return 1;
	}
	bzero(&info, sizeof(info));
	info.eid = *eid;
	if ((err = snd_mixer_element_info_build(handle, &info)) < 0) {
		error("Mixer element read error: %s", snd_strerror(err));
		return 1;
	}
	bzero(&element, sizeof(element));
	element.eid = *eid;
	if ((err = snd_mixer_element_build(handle, &element)) < 0) {
		error("Mixer element read error: %s", snd_strerror(err));
		snd_mixer_element_info_free(&info);
		return 1;
	}
	if (!strchr(argv[0], ',')) {
		for (idx = 0; idx < element.data.volume1.voices; idx++) {
			ptr = argv[0];
			tmp = get_volume(&ptr, 
					info.data.volume1.prange[idx].min,
					info.data.volume1.prange[idx].max,
					info.data.volume1.prange[idx].min_dB,
					info.data.volume1.prange[idx].max_dB);
			element.data.volume1.pvoices[idx] = tmp;
		}
	} else {
		ptr = argv[idx];
		for (idx = 0; idx < element.data.volume1.voices; idx++) {
			tmp = get_volume(&ptr, 
					info.data.volume1.prange[idx].min,
					info.data.volume1.prange[idx].max,
					info.data.volume1.prange[idx].min_dB,
					info.data.volume1.prange[idx].max_dB);
			element.data.volume1.pvoices[idx] = tmp;
		}
	}
	if ((err = snd_mixer_element_write(handle, &element)) < 0) {
		error("Mixer element write error: %s\n", snd_strerror(err));
		snd_mixer_element_free(&element);
		snd_mixer_element_info_free(&info);
		return 1;
	}
	snd_mixer_element_free(&element);
	snd_mixer_element_info_free(&info);
	return 0;
}

int eset_accu3(int argc, char *argv[], void *handle, snd_mixer_eid_t *eid)
{
	int err, tmp, idx = 0;
	snd_mixer_element_t element;
	snd_mixer_element_info_t info;
	char *ptr;
	
	if (argc != 1 || (!isdigit(*argv[0]) && *argv[0] != ':')) {
		fprintf(stderr, "The set Accu3 command requires an argument:\n");
		fprintf(stderr, "     vol[,vol] ...\n");
		return 1;
	}
	bzero(&info, sizeof(info));
	info.eid = *eid;
	if ((err = snd_mixer_element_info_build(handle, &info)) < 0) {
		error("Mixer element read error: %s", snd_strerror(err));
		return 1;
	}
	bzero(&element, sizeof(element));
	element.eid = *eid;
	if ((err = snd_mixer_element_build(handle, &element)) < 0) {
		error("Mixer element read error: %s", snd_strerror(err));
		snd_mixer_element_info_free(&info);
		return 1;
	}
	if (!strchr(argv[0], ',')) {
		for (idx = 0; idx < element.data.accu3.voices; idx++) {
			ptr = argv[0];
			tmp = get_volume(&ptr, 
					info.data.accu3.prange[idx].min,
					info.data.accu3.prange[idx].max,
					info.data.accu3.prange[idx].min_dB,
					info.data.accu3.prange[idx].max_dB);
			element.data.accu3.pvoices[idx] = tmp;
		}
	} else {
		ptr = argv[idx];
		for (idx = 0; idx < element.data.volume1.voices; idx++) {
			tmp = get_volume(&ptr, 
					info.data.accu3.prange[idx].min,
					info.data.accu3.prange[idx].max,
					info.data.accu3.prange[idx].min_dB,
					info.data.accu3.prange[idx].max_dB);
			element.data.accu3.pvoices[idx] = tmp;
		}
	}
	if ((err = snd_mixer_element_write(handle, &element)) < 0) {
		error("Mixer element write error: %s\n", snd_strerror(err));
		snd_mixer_element_free(&element);
		snd_mixer_element_info_free(&info);
		return 1;
	}
	snd_mixer_element_free(&element);
	snd_mixer_element_info_free(&info);
	return 0;
}

int eset_mux1(int argc, char *argv[], void *handle, snd_mixer_eid_t *eid)
{
	int err, idx = 0;
	snd_mixer_element_t element;
	snd_mixer_element_info_t info;
	snd_mixer_eid_t xeid;
	
	if (argc < 1) {
		fprintf(stderr, "The set Mux1 command requires an argument:\n");
		fprintf(stderr, "     element[ element] ...\n");
		return 1;
	}
	bzero(&info, sizeof(info));
	info.eid = *eid;
	if ((err = snd_mixer_element_info_build(handle, &info)) < 0) {
		error("Mixer element read error: %s", snd_strerror(err));
		return 1;
	}
	bzero(&element, sizeof(element));
	element.eid = *eid;
	if ((err = snd_mixer_element_build(handle, &element)) < 0) {
		error("Mixer element read error: %s", snd_strerror(err));
		snd_mixer_element_info_free(&info);
		return 1;
	}
	if (argc == 1) {
		if (parse_eid(argv[0], &xeid)) {
			fprintf(stderr, "Wrong element identifier: %s\n", argv[0]);
			snd_mixer_element_free(&element);
			snd_mixer_element_info_free(&info);
			return 1;
		}
		for (idx = 0; idx < element.data.mux1.output; idx++)
			element.data.mux1.poutput[idx] = xeid;
	} else {
		for (idx = 0; idx < element.data.volume1.voices; idx++) {
			if (parse_eid(argv[idx >= argc ? argc - 1 : idx], &xeid)) {
				fprintf(stderr, "Wrong element identifier: %s\n", argv[0]);
				snd_mixer_element_free(&element);
				snd_mixer_element_info_free(&info);
				return 1;
			}
			element.data.mux1.poutput[idx] = xeid;
		}
	}
	if ((err = snd_mixer_element_write(handle, &element)) < 0) {
		error("Mixer element write error: %s\n", snd_strerror(err));
		snd_mixer_element_free(&element);
		snd_mixer_element_info_free(&info);
		return 1;
	}
	snd_mixer_element_free(&element);
	snd_mixer_element_info_free(&info);
	return 0;
}

int eset(int argc, char *argv[])
{
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
	return 0;
}

int eget(int argc, char *argv[])
{
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
	return 0;
}

int gset(int argc, char *argv[])
{
	int err, idx, chn;
	snd_mixer_t *handle;
	snd_mixer_gid_t gid;
	snd_mixer_group_t group;

	if (argc < 1) {
		fprintf(stderr, "Specify a group identifier: 'name',index\n");
		return 1;
	}
	if (parse_gid(argv[0], &gid)) {
		fprintf(stderr, "Wrong group identifier: %s\n", argv[0]);
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
	bzero(&group, sizeof(group));
	group.gid = gid;
	if (snd_mixer_group_read(handle, &group)<0) {
		error("Unable to read group '%s',%i: %s\n", group_name(gid.name), gid.index, snd_strerror(err));
		snd_mixer_close(handle);
		return -1;
	}
	for (idx = 1; idx < argc; idx++) {
		if (!strncmp(argv[idx], "mute", 4) ||
		    !strncmp(argv[idx], "off", 3)) {
			group.mute = group.channels;
		} else if (!strncmp(argv[idx], "unmute", 6) ||
		           !strncmp(argv[idx], "on", 2)) {
			group.mute = 0;
		} else if (!strncmp(argv[idx], "capture", 7) ||
		           !strncmp(argv[idx], "rec", 3)) {
			group.capture = group.channels;
		} else if (!strncmp(argv[idx], "nocapture", 9) ||
		           !strncmp(argv[idx], "norec", 5)) {
			group.capture = 0;
		} else if (isdigit(argv[idx][0])) {
			char *ptr;
			int vol;
		
			ptr = argv[idx];
			vol = get_volume_simple(&ptr, group.min, group.max);
			for (chn = 0; chn <= SND_MIXER_CHN_LAST; chn++) {
				if (!(group.channels & (1<<chn)))
					continue;
				group.volume.values[chn] = vol;
			}
		} else {
			error("Unknown setup '%s'..\n", argv[idx]);
			snd_mixer_close(handle);
			return -1;
		}
	} 
	if (snd_mixer_group_write(handle, &group)<0) {
		error("Unable to write group '%s',%i: %s\n", group_name(gid.name), gid.index, snd_strerror(err));
		snd_mixer_close(handle);
		return -1;
	}
	if (!quiet) {
		printf("Group '%s',%i\n", group_name(gid.name), gid.index);
		group_contents_is_on = 1;
		show_group(handle, &gid, "  ");
		group_contents_is_on = 0;
	}
	snd_mixer_close(handle);
	return 0;
}

int gget(int argc, char *argv[])
{
	int err;
	snd_mixer_t *handle;
	snd_mixer_gid_t gid;

	if (argc < 1) {
		fprintf(stderr, "Specify a group identifier: 'name',index\n");
		return 1;
	}
	if (parse_gid(argv[0], &gid)) {
		fprintf(stderr, "Wrong group identifier: %s\n", argv[0]);
		return 1;
	}
	if ((err = snd_mixer_open(&handle, card, device)) < 0) {
		error("Mixer %i/%i open error: %s\n", card, device, snd_strerror(err));
		return -1;
	}
	if (!quiet) {
		printf("Group '%s',%i\n", group_name(gid.name), gid.index);
		group_contents_is_on = 1;
		show_group(handle, &gid, "  ");
		group_contents_is_on = 0;
	}
	snd_mixer_close(handle);
	return 0;
}

int main(int argc, char *argv[])
{
	int morehelp;
	struct option long_option[] =
	{
		{"help", 0, NULL, HELPID_HELP},
		{"card", 1, NULL, HELPID_CARD},
		{"device", 1, NULL, HELPID_DEVICE},
		{"quiet", 0, NULL, HELPID_QUIET},
		{"debug", 0, NULL, HELPID_DEBUG},
		{"version", 0, NULL, HELPID_VERSION},
		{NULL, 0, NULL, 0},
	};

	morehelp = 0;
	card = snd_defaults_mixer_card();
	device = snd_defaults_mixer_device();
	if (card < 0 || device < 0) {
		fprintf(stderr, "The ALSA sound driver was not detected in this system.\n");
		return 1;
	}
	while (1) {
		int c;

		if ((c = getopt_long(argc, argv, "hc:d:qDv", long_option, NULL)) < 0)
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
		case 'd':
		case HELPID_DEVICE:
			device = atoi(optarg);
			if (device < 0 || device > 32) {
			  fprintf(stderr, "Error: device %i is invalid\n", device);
			  return 1;
			}
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
		return groups_contents() ? 1 : 0;
	}
	if (!strcmp(argv[optind], "info")) {
		return info() ? 1 : 0;
	} else if (!strcmp(argv[optind], "elements")) {
		return elements() ? 1 : 0;
	} else if (!strcmp(argv[optind], "contents")) {
		return elements_contents() ? 1 : 0;
	} else if (!strcmp(argv[optind], "groups")) {
		return groups() ? 1 : 0;
	} else if (!strcmp(argv[optind], "gcontents")) {
		return groups_contents() ? 1 : 0;
	} else if (!strcmp(argv[optind], "set")) {
		return gset(argc - optind - 1, argc - optind > 1 ? argv + optind + 1 : NULL) ? 1 : 0;
	} else if (!strcmp(argv[optind], "get")) {
		return gget(argc - optind - 1, argc - optind > 1 ? argv + optind + 1 : NULL) ? 1 : 0;
	} else if (!strcmp(argv[optind], "eset")) {
		return eset(argc - optind - 1, argc - optind > 1 ? argv + optind + 1 : NULL) ? 1 : 0;
	} else if (!strcmp(argv[optind], "eget")) {
		return eget(argc - optind - 1, argc - optind > 1 ? argv + optind + 1 : NULL) ? 1 : 0;
	} else {
		fprintf(stderr, "amixer: Unknown command '%s'...\n", argv[optind]);
	}

	return 0;
}
