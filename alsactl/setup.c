/*
 *  Advanced Linux Sound Architecture Control Program
 *  Copyright (c) 1997 by Perex, APS, University of South Bohemia
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

#include "alsactl.h"

#define SND_INTERFACE_CONTROL	0
#define SND_INTERFACE_MIXER	1
#define SND_INTERFACE_PCM	2
#define SND_INTERFACE_RAWMIDI	3

extern int yyparse(void);
extern int linecount;
extern FILE *yyin;
extern int yydebug;
struct soundcard *soundcards = NULL;
struct soundcard *rsoundcards = NULL;

/*
 *  misc functions
 */

static char *mixer_element_name(snd_mixer_eid_t *eid)
{
	static char str[32];
	
	if (!eid)
		return "???"; 
	strncpy(str, eid->name, sizeof(eid->name));
	str[sizeof(eid->name)] = '\0';
	return str;
}

char *mixer_element_id(snd_mixer_eid_t *eid)
{
	static char str[64];
	
	if (!eid)
		return "???"; 
	sprintf(str, "%s:%i:%i", mixer_element_name(eid), eid->index, eid->type);
	return str;
}

/*
 *  free functions
 */

static void soundcard_ctl_switch_free(struct ctl_switch *first)
{
	struct ctl_switch *next;

	while (first) {
		next = first->next;
		free(first);
		first = next;
	}
}

static void soundcard_mixer_element_free(struct mixer_element *first)
{
	struct mixer_element *next;

	while (first) {
		next = first->next;
		snd_mixer_element_info_free(&first->info);
		snd_mixer_element_free(&first->element);
		free(first);
		first = next;
	}
}

static void soundcard_mixer_free1(struct mixer *mixer)
{
	if (!mixer)
		return;
	soundcard_mixer_element_free(mixer->elements);
	soundcard_ctl_switch_free(mixer->switches);
	free(mixer);
}

static void soundcard_mixer_free(struct mixer *first)
{
	struct mixer *next;

	while (first) {
		next = first->next;
		soundcard_mixer_free1(first);
		first = next;
	}
}

static void soundcard_pcm_free1(struct pcm *pcm)
{
	if (!pcm)
		return;
	soundcard_ctl_switch_free(pcm->pswitches);
	soundcard_ctl_switch_free(pcm->rswitches);
	free(pcm);
}

static void soundcard_pcm_free(struct pcm *first)
{
	struct pcm *next;

	while (first) {
		next = first->next;
		soundcard_pcm_free1(first);
		first = next;
	}
}

static void soundcard_rawmidi_free1(struct rawmidi *rawmidi)
{
	if (!rawmidi)
		return;
	soundcard_ctl_switch_free(rawmidi->iswitches);
	soundcard_ctl_switch_free(rawmidi->oswitches);
	free(rawmidi);
}

static void soundcard_rawmidi_free(struct rawmidi *first)
{
	struct rawmidi *next;

	while (first) {
		next = first->next;
		soundcard_rawmidi_free1(first);
		first = next;
	}
}

static void soundcard_free1(struct soundcard *soundcard)
{
	if (!soundcard)
		return;
	soundcard_ctl_switch_free(soundcard->control.switches);
	soundcard_mixer_free(soundcard->mixers);
	soundcard_pcm_free(soundcard->pcms);
	soundcard_rawmidi_free(soundcard->rawmidis);
	free(soundcard);
}

static void soundcard_free(struct soundcard *first)
{
	struct soundcard *next;

	while (first) {
		next = first->next;
		soundcard_free1(first);
		first = next;
	}
}

static int soundcard_remove(int cardno)
{
	struct soundcard *first, *prev = NULL, *next;

	first = soundcards;
	while (first) {
		next = first->next;
		if (first->no == cardno) {
			soundcard_free1(first);
			if (!prev)
				soundcards = next;
			else
				prev->next = next;
			return 0;
		}
		prev = first;
		first = first->next;
	}
	return -1;
}

/*
 *  exported functions
 */

void soundcard_setup_init(void)
{
	soundcards = NULL;
}

void soundcard_setup_done(void)
{
	soundcard_free(soundcards);
	soundcard_free(rsoundcards);
	soundcards = NULL;
}

static int switch_list(void *handle, snd_switch_list_t *list, int interface, int device)
{
	switch (interface) {
	case 0:
		return snd_ctl_switch_list(handle, list);
	case 1:
		return snd_ctl_mixer_switch_list(handle, device, list);
	case 2:
		return snd_ctl_pcm_playback_switch_list(handle, device, list);
	case 3:
		return snd_ctl_pcm_capture_switch_list(handle, device, list);
	case 4:
		return snd_ctl_rawmidi_output_switch_list(handle, device, list);
	case 5:
		return snd_ctl_rawmidi_input_switch_list(handle, device, list);
	default:
		return -EINVAL;
	}
}

static int switch_read(void *handle, snd_switch_t *sw, int interface, int device)
{
	switch (interface) {
	case 0:
		return snd_ctl_switch_read(handle, sw);
	case 1:
		return snd_ctl_mixer_switch_read(handle, device, sw);
	case 2:
		return snd_ctl_pcm_playback_switch_read(handle, device, sw);
	case 3:
		return snd_ctl_pcm_capture_switch_read(handle, device, sw);
	case 4:
		return snd_ctl_rawmidi_output_switch_read(handle, device, sw);
	case 5:
		return snd_ctl_rawmidi_input_switch_read(handle, device, sw);
	default:
		return -EINVAL;
	}
}

#if 0
static int switch_write(void *handle, snd_switch_t *sw, int interface, int device)
{
	switch (interface) {
	case 0:
		return snd_ctl_switch_write(handle, sw);
	case 1:
		return snd_ctl_mixer_switch_write(handle, device, sw);
	case 2:
		return snd_ctl_pcm_playback_switch_write(handle, device, sw);
	case 3:
		return snd_ctl_pcm_capture_switch_write(handle, device, sw);
	case 4:
		return snd_ctl_rawmidi_output_switch_write(handle, device, sw);
	case 5:
		return snd_ctl_rawmidi_input_switch_write(handle, device, sw);
	default:
		return -EINVAL;
	}
}
#endif

static int determine_switches(void *handle, struct ctl_switch **csw, int interface, int device)
{
	int err, idx;
	snd_switch_list_t list;
	snd_switch_list_item_t *item;
	snd_switch_t sw;
	struct ctl_switch *prev_csw;
	struct ctl_switch *new_csw;

	*csw = NULL;
	bzero(&list, sizeof(list));
	if ((err = switch_list(handle, &list, interface, device)) < 0) {
		error("Cannot determine switches for interface %i and device %i: %s", interface, device, snd_strerror(err));
		return 1;
	}
	if (list.switches_over <= 0)
		return 0;
	list.switches_size = list.switches_over + 16;
	list.switches = list.switches_over = 0;
	list.pswitches = malloc(sizeof(snd_switch_list_item_t) * list.switches_size);
	if (!list.pswitches) {
		error("No enough memory...");
		return 1;
	}
	if ((err = switch_list(handle, &list, interface, device)) < 0) {
		error("Cannot determine switches (2) for interface %i and device %i: %s", interface, device, snd_strerror(err));
		return 1;
	}
	for (idx = 0, prev_csw = NULL; idx < list.switches; idx++) {
		item = &list.pswitches[idx];
		bzero(&sw, sizeof(sw));
		strncpy(sw.name, item->name, sizeof(sw.name));
		if ((err = switch_read(handle, &sw, interface, device)) < 0) {
			error("Cannot read switch for interface %i and device %i: %s", interface, device, snd_strerror(err));
			free(list.pswitches);
			return 1;
		}
		new_csw = malloc(sizeof(*new_csw));
		if (!new_csw) {
			error("No enough memory...");
			free(list.pswitches);
			return 1;
		}
		bzero(new_csw, sizeof(*new_csw));
		memcpy(&new_csw->s, &sw, sizeof(new_csw->s));
		if (*csw) {
			prev_csw->next = new_csw;
			prev_csw = new_csw;
		} else {
			*csw = prev_csw = new_csw;
		}
	}
	free(list.pswitches);
	return 0;
}

static int soundcard_setup_collect_switches1(int cardno)
{
	snd_ctl_t *handle;
	snd_mixer_t *mhandle;
	struct soundcard *card, *first, *prev;
	int err, device;
	struct mixer *mixer, *mixerprev;
	struct pcm *pcm, *pcmprev;
	struct rawmidi *rawmidi, *rawmidiprev;

	soundcard_remove(cardno);
	if ((err = snd_ctl_open(&handle, cardno)) < 0) {
		error("SND CTL open error: %s", snd_strerror(err));
		return 1;
	}
	/* --- */
	card = (struct soundcard *) malloc(sizeof(struct soundcard));
	if (!card) {
		snd_ctl_close(handle);
		error("malloc error");
		return 1;
	}
	bzero(card, sizeof(struct soundcard));
	card->no = cardno;
	for (first = soundcards, prev = NULL; first; first = first->next) {
		if (first->no > cardno) {
			if (!prev) {
				soundcards = card;
			} else {
				prev->next = card;
			}
			card->next = first;
			break;
		}
		prev = first;
	}
	if (!first) {
		if (!soundcards) {
			soundcards = card;
		} else {
			prev->next = card;
		}
	}
	if ((err = snd_ctl_hw_info(handle, &card->control.hwinfo)) < 0) {
		snd_ctl_close(handle);
		error("SND CTL HW INFO error: %s", snd_strerror(err));
		return 1;
	}
	/* --- */
	if (determine_switches(handle, &card->control.switches, 0, 0)) {
		snd_ctl_close(handle);
		return 1;
	}
	/* --- */
	for (device = 0, mixerprev = NULL; device < card->control.hwinfo.mixerdevs; device++) {
		mixer = (struct mixer *) malloc(sizeof(struct mixer));
		if (!mixer) {
			snd_ctl_close(handle);
			error("malloc problem");
			return 1;
		}
		bzero(mixer, sizeof(struct mixer));
		mixer->no = device;
		if (determine_switches(handle, &mixer->switches, 1, device)) {
			snd_ctl_close(handle);
			return 1;
		}
		if (!mixerprev) {
			card->mixers = mixer;
		} else {
			mixerprev->next = mixer;
		}
		mixerprev = mixer;
		if ((err = snd_mixer_open(&mhandle, cardno, device)) < 0) {
			snd_ctl_close(handle);
			error("MIXER open error: %s", snd_strerror(err));
			return 1;
		}
		if ((err = snd_mixer_info(mhandle, &mixer->info)) < 0) {
			snd_mixer_close(mhandle);
			snd_ctl_close(handle);
			error("MIXER info error: %s", snd_strerror(err));
			return 1;
		}
		snd_mixer_close(mhandle);
	}
	/* --- */
	for (device = 0, pcmprev = NULL; device < card->control.hwinfo.pcmdevs; device++) {
		pcm = (struct pcm *) malloc(sizeof(struct pcm));
		if (!pcm) {
			snd_ctl_close(handle);
			error("malloc problem");
			return 1;
		}
		bzero(pcm, sizeof(struct pcm));
		pcm->no = device;
		if ((err = snd_ctl_pcm_info(handle, device, &pcm->info)) < 0) {
			snd_ctl_close(handle);
			error("PCM info error: %s", snd_strerror(err));
			return 1;
		}
		if (determine_switches(handle, &pcm->pswitches, 2, device)) {
			snd_ctl_close(handle);
			return 1;
		}
		if (determine_switches(handle, &pcm->rswitches, 3, device)) {
			snd_ctl_close(handle);
			return 1;
		}
		if (!pcmprev) {
			card->pcms = pcm;
		} else {
			pcmprev->next = pcm;
		}
		pcmprev = pcm;
	}
	/* --- */
	for (device = 0, rawmidiprev = NULL; device < card->control.hwinfo.mididevs; device++) {
		rawmidi = (struct rawmidi *) malloc(sizeof(struct rawmidi));
		if (!rawmidi) {
			snd_ctl_close(handle);
			error("malloc problem");
			return 1;
		}
		bzero(rawmidi, sizeof(struct rawmidi));
		rawmidi->no = device;
		if ((err = snd_ctl_rawmidi_info(handle, device, &rawmidi->info)) < 0) {
			snd_ctl_close(handle);
			error("RAWMIDI info error: %s", snd_strerror(err));
			return 1;
		}
		if (determine_switches(handle, &rawmidi->oswitches, 4, device)) {
			snd_ctl_close(handle);
			return 1;
		}
		if (determine_switches(handle, &rawmidi->iswitches, 5, device)) {
			snd_ctl_close(handle);
			return 1;
		}
		if (!rawmidiprev) {
			card->rawmidis = rawmidi;
		} else {
			rawmidiprev->next = rawmidi;
		}
		rawmidiprev = rawmidi;
	}
	/* --- */
	snd_ctl_close(handle);
	return 0;
}

int soundcard_setup_collect_switches(int cardno)
{
	int err;
	unsigned int mask;

	if (cardno >= 0) {
		return soundcard_setup_collect_switches1(cardno);
	} else {
		mask = snd_cards_mask();
		for (cardno = 0; cardno < SND_CARDS; cardno++) {
			if (!(mask & (1 << cardno)))
				continue;
			err = soundcard_setup_collect_switches1(cardno);
			if (err)
				return err;
		}
		return 0;
	}
}

static int soundcard_setup_collect_data1(int cardno)
{
	snd_ctl_t *handle; 
	snd_mixer_t *mhandle;
	struct soundcard *card;
	int err, idx;
	struct mixer *mixer;
	snd_mixer_elements_t elements;
	struct mixer_element *mixerelement, *mixerelementprev;

	if ((err = snd_ctl_open(&handle, cardno)) < 0) {
		error("SND CTL open error: %s", snd_strerror(err));
		return 1;
	}
	/* --- */
	for (card = soundcards; card && card->no != cardno; card = card->next);
	if (!card) {
		snd_ctl_close(handle);
		error("The soundcard %i does not exist.", cardno);
		return 1;
	}
	for (mixer = card->mixers; mixer; mixer = mixer->next) {
		if ((err = snd_mixer_open(&mhandle, cardno, mixer->no)) < 0) {
			snd_ctl_close(handle);
			error("MIXER open error: %s", snd_strerror(err));
			return 1;
		}
		if (mixer->elements)
			soundcard_mixer_element_free(mixer->elements);
		mixer->elements = NULL;
		bzero(&elements, sizeof(elements));
		if ((err = snd_mixer_elements(mhandle, &elements)) < 0) {
			snd_mixer_close(mhandle);
			snd_ctl_close(handle);
			error("MIXER elements error: %s", snd_strerror(err));
			return 1;
		}
		elements.elements_size = elements.elements_over + 16;
		elements.elements = elements.elements_over = 0;
		elements.pelements = (snd_mixer_eid_t *)malloc(elements.elements_size * sizeof(snd_mixer_eid_t));
		if ((err = snd_mixer_elements(mhandle, &elements)) < 0) {
			snd_mixer_close(mhandle);
			snd_ctl_close(handle);
			error("MIXER elements (2) error: %s", snd_strerror(err));
			return 1;
		}
		for (idx = 0, mixerelementprev = NULL; idx < elements.elements; idx++) {
			mixerelement = (struct mixer_element *) malloc(sizeof(struct mixer_element));
			if (!mixerelement) {
				snd_mixer_close(mhandle);
				snd_ctl_close(handle);
				error("malloc problem");
				return 1;
			}
			bzero(mixerelement, sizeof(*mixerelement));
			mixerelement->info.eid = elements.pelements[idx];
			mixerelement->element.eid = elements.pelements[idx];
			if (snd_mixer_element_has_info(&elements.pelements[idx]) == 1) {
				if ((err = snd_mixer_element_info_build(mhandle, &mixerelement->info)) < 0) {
					free(mixerelement);
					error("MIXER element %s info error (%s) - skipping", mixer_element_id(&mixerelement->info.eid), snd_strerror(err));
					break;
				}
			}
			if (snd_mixer_element_has_control(&elements.pelements[idx]) == 1) {
				if ((err = snd_mixer_element_build(mhandle, &mixerelement->element)) < 0) {
					free(mixerelement);
					error("MIXER element %s build error (%s) - skipping", mixer_element_id(&mixerelement->element.eid), snd_strerror(err));
					break;
				}
			}
			if (!mixerelementprev) {
				mixer->elements = mixerelement;
			} else {
				mixerelementprev->next = mixerelement;
			}
			mixerelementprev = mixerelement;
		}
		free(elements.pelements);
		snd_mixer_close(mhandle);
	}
	/* --- */
	snd_ctl_close(handle);
	return 0;
}

int soundcard_setup_collect_data(int cardno)
{
	int err;
	unsigned int mask;

	if (cardno >= 0) {
		return soundcard_setup_collect_data1(cardno);
	} else {
		mask = snd_cards_mask();
		for (cardno = 0; cardno < SND_CARDS; cardno++) {
			if (!(mask & (1 << cardno)))
				continue;
			err = soundcard_setup_collect_data1(cardno);
			if (err)
				return err;
		}
		return 0;
	}
}



int soundcard_setup_load(const char *cfgfile, int skip)
{
	extern int yyparse(void);
	extern int linecount;
	extern FILE *yyin;
	extern int yydebug;
	int xtry;

#ifdef YYDEBUG
	yydebug = 1;
#endif
	if (debugflag)
		printf("cfgfile = '%s'\n", cfgfile);
	if (skip && access(cfgfile, R_OK))
		return 0;
	if ((yyin = fopen(cfgfile, "r")) == NULL) {
		error("Cannot open configuration file '%s'...", cfgfile);
		return 1;
	}
	linecount = 0;
	xtry = yyparse();
	fclose(yyin);
	if (debugflag)
		printf("Config ok..\n");
	if (xtry)
		error("Ignored error in configuration file '%s'...", cfgfile);
	return 0;
}

static void soundcard_setup_write_switch(FILE * out, const char *space, int interface, snd_switch_t *sw)
{
	char *s, v[16];
	int idx, first, switchok = 0;

	v[0] = '\0';
	switch (sw->type) {
	case SND_SW_TYPE_BOOLEAN:
		s = "bool";
		strcpy(v, sw->value.enable ? "true" : "false");
		break;
	case SND_SW_TYPE_BYTE:
		s = "byte";
		sprintf(v, "%u", (unsigned int) sw->value.data8[0]);
		break;
	case SND_SW_TYPE_WORD:
		s = "word";
		if (interface == SND_INTERFACE_CONTROL &&
		    !strcmp(sw->name, SND_CTL_SW_JOYSTICK_ADDRESS)) {
		    	sprintf(v, "0x%x", (unsigned int) sw->value.data16[0]);
		} else {
			sprintf(v, "%u", (unsigned int) sw->value.data16[0]);
		}
		break;
	case SND_SW_TYPE_DWORD:
		s = "dword";
		sprintf(v, "%u", sw->value.data32[0]);
		break;
	case SND_SW_TYPE_USER:
		s = "user";
		break;
	case SND_SW_TYPE_LIST:
		s = "list";
		sprintf(v, "%u", sw->value.item_number);
		break;
	default:
		s = "unknown";
	}
	fprintf(out, "%s; The type is '%s'.\n", space, s);
	if (sw->low != 0 || sw->high != 0)
		fprintf(out, "%s; The accepted switch range is from %u to %u.\n", space, sw->low, sw->high);
	if (interface == SND_INTERFACE_CONTROL && sw->type == SND_SW_TYPE_WORD &&
	    !strcmp(sw->name, SND_CTL_SW_JOYSTICK_ADDRESS)) {
		for (idx = 1, first = 1; idx < 16; idx++) {
			if (sw->value.data16[idx]) {
				if (first) {
					fprintf(out, "%s; Available addresses - 0x%x", space, sw->value.data16[idx]);
					first = 0;
				} else {
					fprintf(out, ", 0x%x", sw->value.data16[idx]);
				}
			}
		}
		if (!first)
			fprintf(out, "\n");
	}
	if (interface == SND_INTERFACE_MIXER && sw->type == SND_SW_TYPE_BOOLEAN &&
	    !strcmp(sw->name, SND_MIXER_SW_IEC958_OUTPUT)) {
		fprintf(out, "%sswitch(\"%s\",", space, sw->name);
		if (sw->value.data32[1] == (('C' << 8) | 'S')) {	/* Cirrus Crystal */
			switchok = 0;
			fprintf(out, "iec958ocs(%s", sw->value.enable ? "enable" : "disable");
			if (sw->value.data16[4] & 0x2000)
				fprintf(out, ",3d");
			if (sw->value.data16[4] & 0x0040)
				fprintf(out, ",reset");
			if (sw->value.data16[4] & 0x0020)
				fprintf(out, ",user");
			if (sw->value.data16[4] & 0x0010)
				fprintf(out, ",valid");
			if (sw->value.data16[5] & 0x0002)
				fprintf(out, ",data");
			if (!(sw->value.data16[5] & 0x0004))
				fprintf(out, ",protect");
			switch (sw->value.data16[5] & 0x0018) {
			case 0x0008:
				fprintf(out, ",pre2");
				break;
			default:
				break;
			}
			if (sw->value.data16[5] & 0x0020)
				fprintf(out, ",fsunlock");
			fprintf(out, ",type(0x%x)", (sw->value.data16[5] >> 6) & 0x7f);
			if (sw->value.data16[5] & 0x2000)
				fprintf(out, ",gstatus");
			fprintf(out, ")");
			goto __end;
		}
	}
	fprintf(out, "%sswitch(\"%s\", ", space, sw->name);
	if (!switchok) {
		fprintf(out, v);
		if (sw->type < 0 || sw->type > SND_SW_TYPE_LIST_ITEM) {
			/* TODO: some well known types should be verbose */
			fprintf(out, "rawdata(");
			for (idx = 0; idx < 31; idx++) {
				fprintf(out, "@%02x:", sw->value.data8[idx]);
			}
			fprintf(out, "%02x@)\n", sw->value.data8[31]);
		}
	}
      __end:
	fprintf(out, ")\n");
}

static void soundcard_setup_write_switches(FILE *out, const char *space, int interface, struct ctl_switch **switches)
{
	struct ctl_switch *sw;

	if (!(*switches))
		return;
	for (sw = *switches; sw; sw = sw->next)
		soundcard_setup_write_switch(out, space, interface, &sw->s);
}

static void soundcard_setup_write_mixer_element(FILE * out, struct mixer_element * xelement)
{
	snd_mixer_element_info_t *info;
	snd_mixer_element_t *element;
	int idx;
	
	info = &xelement->info;
	element = &xelement->element;
	if (snd_mixer_element_has_control(&element->eid) != 1)
		return;
	switch (element->eid.type) {
	case SND_MIXER_ETYPE_SWITCH1:
		fprintf(out, "    element(\"%s\",%i,%i,Switch1(", mixer_element_name(&element->eid), element->eid.index, element->eid.type);
		for (idx = 0; idx < element->data.switch1.sw; idx++)
			fprintf(out, "%s%s", idx > 0 ? "," : "", snd_mixer_get_bit(element->data.switch1.psw, idx) ? "on" : "off");
		fprintf(out, "))\n");
		break;
	case SND_MIXER_ETYPE_SWITCH2:
		fprintf(out, "    element(\"%s\",%i,%i,Switch2(%s))\n", mixer_element_name(&element->eid), element->eid.index, element->eid.type, element->data.switch2.sw ? "on" : "off");
		break;
	case SND_MIXER_ETYPE_SWITCH3:
		fprintf(out, "    element(\"%s\",%i,%i,Switch3(", mixer_element_name(&element->eid), element->eid.index, element->eid.type);
		for (idx = 0; idx < element->data.switch3.rsw; idx++)
			fprintf(out, "%s%s", idx > 0 ? "," : "", snd_mixer_get_bit(element->data.switch3.prsw, idx) ? "on" : "off");
		fprintf(out, "))\n");
		break;
	case SND_MIXER_ETYPE_VOLUME1:
		for (idx = 0; idx < info->data.volume1.range; idx++)
			fprintf(out, "    ; Voice %i : Min %i Max %i\n", idx, info->data.volume1.prange[idx].min, info->data.volume1.prange[idx].max);
		fprintf(out, "    element(\"%s\",%i,%i,Volume1(", mixer_element_name(&element->eid), element->eid.index, element->eid.type);
		for (idx = 0; idx < element->data.volume1.voices; idx++)
			fprintf(out, "%s%i", idx > 0 ? "," : "", element->data.volume1.pvoices[idx]);
		fprintf(out, "))\n");
		break;
	case SND_MIXER_ETYPE_ACCU3:
		for (idx = 0; idx < info->data.accu3.range; idx++)
			fprintf(out, "    ; Voice %i : Min %i Max %i\n", idx, info->data.accu3.prange[idx].min, info->data.accu3.prange[idx].max);
		fprintf(out, "    element(\"%s\",%i,%i,Accu3(", mixer_element_name(&element->eid), element->eid.index, element->eid.type);
		for (idx = 0; idx < element->data.accu3.voices; idx++)
			fprintf(out, "%s%i", idx > 0 ? "," : "", element->data.accu3.pvoices[idx]);
		fprintf(out, "))\n");
		break;
	case SND_MIXER_ETYPE_MUX1:
		fprintf(out, "    element(\"%s\",%i,%i,Mux1(", mixer_element_name(&element->eid), element->eid.index, element->eid.type);
		for (idx = 0; idx < element->data.mux1.output; idx++) {
			fprintf(out, "%selement(\"%s\",%i,%i)", idx > 0 ? "," : "", mixer_element_name(&element->data.mux1.poutput[idx]), element->data.mux1.poutput[idx].index, element->data.mux1.poutput[idx].type);
		}
		fprintf(out, "))\n");
		break;
	case SND_MIXER_ETYPE_MUX2:
		fprintf(out, "    element(\"%s\",%i,%i,Mux2(", mixer_element_name(&element->eid), element->eid.index, element->eid.type);
		fprintf(out, "element(\"%s\",%i,%i)", mixer_element_name(&element->data.mux2.output), element->data.mux2.output.index, element->data.mux2.output.type);
		fprintf(out, "))\n");
		break;
	case SND_MIXER_ETYPE_TONE_CONTROL1:
		if (info->data.tc1.tc & SND_MIXER_TC1_SW)
			fprintf(out, "    ; The tone control has an on/off switch.\n");
		if (info->data.tc1.tc & SND_MIXER_TC1_BASS) 
			fprintf(out, "    ; Bass : Min %i Max %i\n", info->data.tc1.min_bass, info->data.tc1.max_bass);
		if (info->data.tc1.tc & SND_MIXER_TC1_TREBLE)
			fprintf(out, "    ; Treble : Min %i Max %i\n", info->data.tc1.min_treble, info->data.tc1.max_treble);
		fprintf(out, "    element(\"%s\",%i,%i,ToneControl1(", mixer_element_name(&element->eid), element->eid.index, element->eid.type);
		idx = 0;
		if (element->data.tc1.tc & SND_MIXER_TC1_SW) 
			fprintf(out, "%ssw=%s", idx++ > 0 ? "," : "", element->data.tc1.sw ? "on" : "off");
		if (element->data.tc1.tc & SND_MIXER_TC1_BASS) 
			fprintf(out, "%sbass=%i", idx++ > 0 ? "," : "", element->data.tc1.bass);
		if (element->data.tc1.tc & SND_MIXER_TC1_TREBLE)
			fprintf(out, "%streble=%i", idx++ > 0 ? "," : "", element->data.tc1.treble);
		fprintf(out, "))\n");
		break; 
	case SND_MIXER_ETYPE_3D_EFFECT1:
		if (info->data.teffect1.effect & SND_MIXER_EFF1_SW) 
			fprintf(out, "    ; The 3D effect has an on/off switch.\n");
		if (info->data.teffect1.effect & SND_MIXER_EFF1_MONO_SW) 
			fprintf(out, "    ; The 3D effect has an mono processing on/off switch.\n");
		if (info->data.teffect1.effect & SND_MIXER_EFF1_WIDE) 
			fprintf(out, "    ; Wide : Min %i Max %i\n", info->data.teffect1.min_wide, info->data.teffect1.max_wide);
		if (info->data.teffect1.effect & SND_MIXER_EFF1_VOLUME)
			fprintf(out, "    ; Volume : Min %i Max %i\n", info->data.teffect1.min_volume, info->data.teffect1.max_volume);
		if (info->data.teffect1.effect & SND_MIXER_EFF1_CENTER)
			fprintf(out, "    ; Center : Min %i Max %i\n", info->data.teffect1.min_center, info->data.teffect1.max_center);
		if (info->data.teffect1.effect & SND_MIXER_EFF1_SPACE)
			fprintf(out, "    ; Space : Min %i Max %i\n", info->data.teffect1.min_space, info->data.teffect1.max_space);
		if (info->data.teffect1.effect & SND_MIXER_EFF1_DEPTH)
			fprintf(out, "    ; Depth : Min %i Max %i\n", info->data.teffect1.min_depth, info->data.teffect1.max_depth);
		if (info->data.teffect1.effect & SND_MIXER_EFF1_DELAY)
			fprintf(out, "    ; Delay : Min %i Max %i\n", info->data.teffect1.min_delay, info->data.teffect1.max_delay);
		if (info->data.teffect1.effect & SND_MIXER_EFF1_FEEDBACK)
			fprintf(out, "    ; Feedback : Min %i Max %i\n", info->data.teffect1.min_feedback, info->data.teffect1.max_feedback);
		fprintf(out, "    element(\"%s\",%i,%i,_3D_Effect1(", mixer_element_name(&element->eid), element->eid.index, element->eid.type);
		idx = 0;
		if (element->data.teffect1.effect & SND_MIXER_EFF1_SW) 
			fprintf(out, "%ssw=%s", idx++ > 0 ? "," : "", element->data.teffect1.sw ? "on" : "off");
		if (element->data.teffect1.effect & SND_MIXER_EFF1_MONO_SW) 
			fprintf(out, "%smono_sw=%s", idx++ > 0 ? "," : "", element->data.teffect1.mono_sw ? "on" : "off");
		if (element->data.teffect1.effect & SND_MIXER_EFF1_WIDE) 
			fprintf(out, "%swide=%i", idx++ > 0 ? "," : "", element->data.teffect1.wide);
		if (element->data.teffect1.effect & SND_MIXER_EFF1_VOLUME)
			fprintf(out, "%svolume=%i", idx++ > 0 ? "," : "", element->data.teffect1.volume);
		if (element->data.teffect1.effect & SND_MIXER_EFF1_CENTER)
			fprintf(out, "%scenter=%i", idx++ > 0 ? "," : "", element->data.teffect1.center);
		if (element->data.teffect1.effect & SND_MIXER_EFF1_SPACE)
			fprintf(out, "%sspace=%i", idx++ > 0 ? "," : "", element->data.teffect1.space);
		if (element->data.teffect1.effect & SND_MIXER_EFF1_DEPTH)
			fprintf(out, "%sdepth=%i", idx++ > 0 ? "," : "", element->data.teffect1.depth);
		if (element->data.teffect1.effect & SND_MIXER_EFF1_DELAY)
			fprintf(out, "%sdelay=%i", idx++ > 0 ? "," : "", element->data.teffect1.delay);
		if (element->data.teffect1.effect & SND_MIXER_EFF1_FEEDBACK)
			fprintf(out, "%sfeedback=%i", idx++ > 0 ? "," : "", element->data.teffect1.feedback);
		fprintf(out, "))\n");
		break; 
	default:
		fprintf(out, "    ; Unknown element %s\n", mixer_element_id(&element->eid));
	}
}

#define MAX_LINE	(32 * 1024)

int soundcard_setup_write(const char *cfgfile, int cardno)
{
	FILE *out, *out1, *out2, *in;
	char *tmpfile1, *tmpfile2;
	struct soundcard *first, *sel = NULL;
	struct mixer *mixer;
	struct mixer_element *mixerelement;
	struct pcm *pcm;
	struct rawmidi *rawmidi;
	char *line, cardname[sizeof(first->control.hwinfo.name)+16], *ptr1;
	int mark, size, ok;

	tmpfile1 = (char *)malloc(strlen(cfgfile) + 16);
	tmpfile2 = (char *)malloc(strlen(cfgfile) + 16);
	if (!tmpfile1 || !tmpfile2) {
		error("No enough memory...\n");
		if (tmpfile1)
			free(tmpfile1);
		if (tmpfile2)
			free(tmpfile2);
		return 1;
	}
	strcpy(tmpfile1, cfgfile);
	strcat(tmpfile1, ".new");
	strcpy(tmpfile2, cfgfile);
	strcat(tmpfile2, ".insert");
	
	if (cardno >= 0) {
		line = (char *)malloc(MAX_LINE);
		if (!line) {
			error("No enough memory...\n");
			return 1;
		}
		if ((in = fopen(cfgfile, "r")) == NULL)
			cardno = -1;
	} else {
		line = NULL;
		in = NULL;
	}
	if ((out = out1 = fopen(tmpfile1, "w+")) == NULL) {
		error("Cannot open file '%s' for writing...\n", tmpfile1);
		return 1;
	}
	fprintf(out, "# ALSA driver configuration\n");
	fprintf(out, "# This configuration is generated with the alsactl program.\n");
	fprintf(out, "\n");
	if (cardno >= 0) {
		if ((out = out2 = fopen(tmpfile2, "w+")) == NULL) {
			error("Cannot open file '%s' for writing...\n", tmpfile2);
			return 1;
		}
	} else {
		out2 = NULL;
	}
	for (first = soundcards; first; first = first->next) {
		if (cardno >= 0 && first->no != cardno)
			continue;
		sel = first;
		fprintf(out, "soundcard(\"%s\") {\n", first->control.hwinfo.id);
		if (first->control.switches) {
			fprintf(out, "  control {\n");
			soundcard_setup_write_switches(out, "    ", SND_INTERFACE_CONTROL, &first->control.switches);
			fprintf(out, "  }\n");
		}
		for (mixer = first->mixers; mixer; mixer = mixer->next) {
			fprintf(out, "  mixer(\"%s\") {\n", mixer->info.name);
			soundcard_setup_write_switches(out, "    ", SND_INTERFACE_MIXER, &mixer->switches);
			for (mixerelement = mixer->elements; mixerelement; mixerelement = mixerelement->next)
				soundcard_setup_write_mixer_element(out, mixerelement);
			fprintf(out, "  }\n");
		}
		for (pcm = first->pcms; pcm; pcm = pcm->next) {
			if (!pcm->pswitches && !pcm->rswitches)
				continue;
			fprintf(out, "  pcm(\"%s\") {\n", pcm->info.name);
			if (pcm->pswitches) {
				fprintf(out, "    playback {");
				soundcard_setup_write_switches(out, "      ", SND_INTERFACE_PCM, &pcm->pswitches);
				fprintf(out, "    }\n");
			}
			if (pcm->rswitches) {
				fprintf(out, "    capture {");
				soundcard_setup_write_switches(out, "      ", SND_INTERFACE_PCM, &pcm->rswitches);
				fprintf(out, "    }\n");
			}
			fprintf(out, "  }\n");
		}
		for (rawmidi = first->rawmidis; rawmidi; rawmidi = rawmidi->next) {
			if (!rawmidi->oswitches && !rawmidi->iswitches)
				continue;
			fprintf(out, "  rawmidi(\"%s\") {\n", rawmidi->info.name);
			if (rawmidi->oswitches) {
				fprintf(out, "    output {");
				soundcard_setup_write_switches(out, "      ", SND_INTERFACE_RAWMIDI, &rawmidi->oswitches);
				fprintf(out, "    }\n");
			}
			if (rawmidi->iswitches) {
				fprintf(out, "    input {");
				soundcard_setup_write_switches(out, "      ", SND_INTERFACE_RAWMIDI, &rawmidi->iswitches);
				fprintf(out, "    }\n");
			}
			fprintf(out, "  }\n");
		}
		fprintf(out, "}\n%s", cardno < 0 && first->next ? "\n" : "");
	}
	/* merge the old and new text */
	if (cardno >= 0) {
		fseek(out2, 0, SEEK_SET);
		mark = ok = 0;
	      __1:
		while (fgets(line, MAX_LINE - 1, in)) {
			line[MAX_LINE - 1] = '\0';
			if (!strncmp(line, "soundcard(", 10))
				break;
		}
		while (!feof(in)) {
			ptr1 = line + 10;
			while (*ptr1 && *ptr1 != '"')
				ptr1++;
			if (*ptr1)
				ptr1++;
			strncpy(cardname, sel->control.hwinfo.id, sizeof(sel->control.hwinfo.id));
			cardname[sizeof(sel->control.hwinfo.id)] = '\0';
			strcat(cardname, "\"");
			if (!strncmp(ptr1, cardname, strlen(cardname))) {
				if (mark)
					fprintf(out1, "\n");
				do {
					size = fread(line, 1, MAX_LINE, out2);
					if (size > 0)
						fwrite(line, 1, size, out1);
				} while (size > 0);
				mark = ok = 1;
				goto __1;
			} else {
				if (mark)
					fprintf(out1, "\n");
				fprintf(out1, line);
				while (fgets(line, MAX_LINE - 1, in)) {
					line[MAX_LINE - 1] = '\0';
					fprintf(out1, line);
					if (line[0] == '}') {
						mark = 1;
						goto __1;
					}
				}
			}
		}
		if (!ok) {
			if (mark)
				fprintf(out1, "\n");
			do {
				size = fread(line, 1, MAX_LINE, out2);
				printf("size = %i\n", size);
				if (size > 0)
					fwrite(line, 1, size, out1);
			} while (size > 0);			
		}
	}
	if (in)
		fclose(in);
	if (out2)
		fclose(out2);
	if (!access(cfgfile, F_OK) && remove(cfgfile))
		error("Cannot remove file '%s'...", cfgfile);
	if (rename(tmpfile1, cfgfile) < 0)
		error("Cannot rename file '%s' to '%s'...", tmpfile1, cfgfile);
	fclose(out1);
	if (line)
		free(line);
	if (tmpfile2) {
		remove(tmpfile2);
		free(tmpfile2);
	}
	if (tmpfile1) {
		remove(tmpfile1);
		free(tmpfile1);
	}
	return 0;
}
