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

static void soundcard_mixer_channel_free(struct mixer_channel *first)
{
	struct mixer_channel *next;

	while (first) {
		next = first->next;
		free(first);
		first = next;
	}
}

static void soundcard_mixer_free1(struct mixer *mixer)
{
	if (!mixer)
		return;
	soundcard_mixer_channel_free(mixer->channels);
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
		return snd_ctl_pcm_record_switch_list(handle, device, list);
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
		return snd_ctl_pcm_record_switch_read(handle, device, sw);
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
		return snd_ctl_pcm_record_switch_write(handle, device, sw);
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

int soundcard_setup_collect(int cardno)
{
	void *handle, *mhandle;
	struct soundcard *card, *first, *prev;
	int err, idx, count, device;
	struct mixer *mixer, *mixerprev;
	struct mixer_channel *mixerchannel, *mixerchannelprev;
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
			error("MIXER open error: %s\n", snd_strerror(err));
			return 1;
		}
		if ((err = snd_mixer_exact_mode(mhandle, 1)) < 0) {
			snd_mixer_close(mhandle);
			snd_ctl_close(handle);
			error("MIXER exact mode error: %s\n", snd_strerror(err));
			return 1;
		}
		if ((err = snd_mixer_info(mhandle, &mixer->info)) < 0) {
			snd_mixer_close(mhandle);
			snd_ctl_close(handle);
			error("MIXER info error: %s\n", snd_strerror(err));
			return 1;
		}
		count = snd_mixer_channels(mhandle);
		for (idx = 0, mixerchannelprev = NULL; idx < count; idx++) {
			mixerchannel = (struct mixer_channel *) malloc(sizeof(struct mixer_channel));
			if (!mixerchannel) {
				snd_mixer_close(mhandle);
				snd_ctl_close(handle);
				error("malloc problem");
				return 1;
			}
			bzero(mixerchannel, sizeof(struct mixer_channel));
			mixerchannel->no = idx;
			if ((err = snd_mixer_channel_info(mhandle, idx, &mixerchannel->info)) < 0) {
				free(mixerchannel);
				error("MIXER channel info error (%s) - skipping", snd_strerror(err));
				break;
			}
			if ((mixerchannel->info.caps & SND_MIXER_CINFO_CAP_OUTPUT) &&
			    (err = snd_mixer_channel_output_info(mhandle, idx, &mixerchannel->dinfo[OUTPUT])) < 0) {
				free(mixerchannel);
				error("MIXER channel output info error (%s) - skipping", snd_strerror(err));
				break;
			}
			if ((mixerchannel->info.caps & SND_MIXER_CINFO_CAP_INPUT) &&
			    (err = snd_mixer_channel_input_info(mhandle, idx, &mixerchannel->dinfo[INPUT])) < 0) {
				free(mixerchannel);
				error("MIXER channel input info error (%s) - skipping", snd_strerror(err));
				break;
			}
			if ((err = snd_mixer_channel_read(mhandle, idx, &mixerchannel->data)) < 0) {
				free(mixerchannel);
				error("MIXER channel read error (%s) - skipping", snd_strerror(err));
				break;
			}
			if ((mixerchannel->info.caps & SND_MIXER_CINFO_CAP_OUTPUT) &&
			    (err = snd_mixer_channel_output_read(mhandle, idx, &mixerchannel->ddata[OUTPUT])) < 0) {
				free(mixerchannel);
				error("MIXER channel output read error (%s) - skipping", snd_strerror(err));
				break;
			}
			if ((mixerchannel->info.caps & SND_MIXER_CINFO_CAP_INPUT) &&
			    (err = snd_mixer_channel_input_read(mhandle, idx, &mixerchannel->ddata[INPUT])) < 0) {
				free(mixerchannel);
				error("MIXER channel input read error (%s) - skipping", snd_strerror(err));
				break;
			}
			if (!mixerchannelprev) {
				mixer->channels = mixerchannel;
			} else {
				mixerchannelprev->next = mixerchannel;
			}
			mixerchannelprev = mixerchannel;
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
			error("PCM info error: %s\n", snd_strerror(err));
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
			error("RAWMIDI info error: %s\n", snd_strerror(err));
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
	fprintf(out, "%s; Type is '%s'.\n", space, s);
	if (sw->low != 0 || sw->high != 0)
		fprintf(out, "%s; Accepted switch range is from %u to %u.\n", space, sw->low, sw->high);
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
	    !strcmp(sw->name, SND_MIXER_SW_IEC958OUT)) {
		fprintf(out, "%sswitch( \"%s\", ", space, sw->name);
		if (sw->value.data32[1] == (('C' << 8) | 'S')) {	/* Cirrus Crystal */
			switchok = 0;
			fprintf(out, "iec958ocs( %s", sw->value.enable ? "enable" : "disable");
			if (sw->value.data16[4] & 0x2000)
				fprintf(out, " 3d");
			if (sw->value.data16[4] & 0x0040)
				fprintf(out, " reset");
			if (sw->value.data16[4] & 0x0020)
				fprintf(out, " user");
			if (sw->value.data16[4] & 0x0010)
				fprintf(out, " valid");
			if (sw->value.data16[5] & 0x0002)
				fprintf(out, " data");
			if (!(sw->value.data16[5] & 0x0004))
				fprintf(out, " protect");
			switch (sw->value.data16[5] & 0x0018) {
			case 0x0008:
				fprintf(out, " pre2");
				break;
			default:
				break;
			}
			if (sw->value.data16[5] & 0x0020)
				fprintf(out, " fsunlock");
			fprintf(out, " type( 0x%x )", (sw->value.data16[5] >> 6) & 0x7f);
			if (sw->value.data16[5] & 0x2000)
				fprintf(out, " gstatus");
			fprintf(out, " )");
			goto __end;
		}
	}
	fprintf(out, "%sswitch(\"%s\", ", space, sw->name);
	if (!switchok) {
		fprintf(out, v);
		if (sw->type < 0 || sw->type > SND_SW_TYPE_LIST_ITEM) {
			/* TODO: some well known types should be verbose */
			fprintf(out, " rawdata( ");
			for (idx = 0; idx < 31; idx++) {
				fprintf(out, "@%02x:", sw->value.data8[idx]);
			}
			fprintf(out, "%02x@ )\n", sw->value.data8[31]);
		}
	}
      __end:
	fprintf(out, " )\n");
}

static void soundcard_setup_write_switches(FILE *out, const char *space, int interface, struct ctl_switch **switches)
{
	struct ctl_switch *sw;

	if (!(*switches))
		return;
	for (sw = *switches; sw; sw = sw->next)
		soundcard_setup_write_switch(out, space, interface, &sw->s);
}

static void soundcard_setup_write_mixer_channel(FILE * out, struct mixer_channel * channel)
{
	int k, d;
	struct capdes {
		unsigned int flag;
		char* description;
	};
	static struct capdes caps[] = {
		{ SND_MIXER_CINFO_CAP_OUTPUT, "output" },
		{ SND_MIXER_CINFO_CAP_INPUT, "input" },
		{ SND_MIXER_CINFO_CAP_EXTINPUT, "external-input" },
		{ SND_MIXER_CINFO_CAP_EFFECT, "effect" }
	};
	static struct capdes dcaps[] = {
		{ SND_MIXER_CINFO_DCAP_STEREO, "stereo" },
		{ SND_MIXER_CINFO_DCAP_HWMUTE, "hardware-mute" },
		{ SND_MIXER_CINFO_DCAP_JOINMUTE, "join-mute" },
		{ SND_MIXER_CINFO_DCAP_ROUTE, "route" },
		{ SND_MIXER_CINFO_DCAP_SWAPROUTE, "swap-route" },
		{ SND_MIXER_CINFO_DCAP_DIGITAL, "digital" },
		{ SND_MIXER_CINFO_DCAP_RECORDBYMUTE, "recordbymute" },
	};

	fprintf(out, "    ; Capabilities:");
	for (k = 0; k < sizeof(caps)/sizeof(*caps); ++k) {
		if (channel->info.caps & caps[k].flag)
			fprintf(out, " %s", caps[k].description);
	}
	fprintf(out, "\n");
	for (d = OUTPUT; d <= INPUT; ++d) {
		snd_mixer_channel_direction_info_t *di;
		if (d == OUTPUT && 
		    !(channel->info.caps & SND_MIXER_CINFO_CAP_OUTPUT))
			continue;
		if (d == INPUT && 
		    !(channel->info.caps & SND_MIXER_CINFO_CAP_INPUT))
			continue;
		di = &channel->dinfo[d];
		fprintf(out, "    ; %s capabilities:",
			d == OUTPUT ? "Output" : "Input" );
		if (di->caps & SND_MIXER_CINFO_DCAP_VOLUME)
			fprintf(out, " volume(%i, %i)", di->min, di->max);
		for (k = 0; k < sizeof(caps)/sizeof(*caps); ++k) {
			if (di->caps & dcaps[k].flag)
				fprintf(out, " %s", dcaps[k].description);
		}
		fprintf(out, "\n");
	}
	fprintf(out, "    channel(\"%s\"", channel->info.name);
	for (d = OUTPUT; d <= INPUT; ++d) {
		snd_mixer_channel_direction_info_t *di;
		snd_mixer_channel_direction_t *dd;
		if (d == OUTPUT && 
		    !(channel->info.caps & SND_MIXER_CINFO_CAP_OUTPUT))
			continue;
		if (d == INPUT && 
		    !(channel->info.caps & SND_MIXER_CINFO_CAP_INPUT))
			continue;
		dd = &channel->ddata[d];
		di = &channel->dinfo[d];
		fprintf(out, ", %s ", d == OUTPUT ? "output" : "input" );
		if (di->caps & SND_MIXER_CINFO_DCAP_STEREO) {
			fprintf(out, "stereo(");
			if (di->caps & SND_MIXER_CINFO_DCAP_VOLUME)
				fprintf(out, " %i", dd->left);
			fprintf(out, "%s%s,", 
				dd->flags & SND_MIXER_DFLG_MUTE_LEFT ? " mute" : "",
				dd->flags & SND_MIXER_DFLG_LTOR ? " swap" : ""
				);
			if (di->caps & SND_MIXER_CINFO_DCAP_VOLUME)
				fprintf(out, " %i", dd->right);

			fprintf(out, "%s%s)",
				dd->flags & SND_MIXER_DFLG_MUTE_RIGHT ? " mute" : "",
				dd->flags & SND_MIXER_DFLG_RTOL ? " swap" : ""
				);
		}
		else {
			fprintf(out, "mono(");
			if (di->caps & SND_MIXER_CINFO_DCAP_VOLUME)
				fprintf(out, " %i", (dd->left + dd->right)/2);
			fprintf(out, "%s)",
				dd->flags & SND_MIXER_DFLG_MUTE ? " mute" : ""
				);
		}
	}
	fprintf(out, " )\n");
}

int soundcard_setup_write(const char *cfgfile)
{
	FILE *out;
	struct soundcard *first;
	struct mixer *mixer;
	struct mixer_channel *mixerchannel;
	struct pcm *pcm;
	struct rawmidi *rawmidi;

	if ((out = fopen(cfgfile, "w+")) == NULL) {
		error("Cannot open file '%s' for writing...\n", cfgfile);
		return 1;
	}
	fprintf(out, "# ALSA driver configuration\n");
	fprintf(out, "# Generated by alsactl\n");
	fprintf(out, "\n");
	for (first = soundcards; first; first = first->next) {
		fprintf(out, "soundcard(\"%s\") {\n", first->control.hwinfo.id);
		if (first->control.switches) {
			fprintf(out, "  control {\n");
			soundcard_setup_write_switches(out, "    ", SND_INTERFACE_CONTROL, &first->control.switches);
			fprintf(out, "  }\n");
		}
		for (mixer = first->mixers; mixer; mixer = mixer->next) {
			fprintf(out, "  mixer(\"%s\") {\n", mixer->info.name);
			soundcard_setup_write_switches(out, "    ", SND_INTERFACE_MIXER, &mixer->switches);
			for (mixerchannel = mixer->channels; mixerchannel; mixerchannel = mixerchannel->next)
				soundcard_setup_write_mixer_channel(out, mixerchannel);
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
				fprintf(out, "    record {");
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
		fprintf(out, "}\n%s", first->next ? "\n" : "");
	}
	fclose(out);
	return 0;
}

static int soundcard_open_ctl(void **ctlhandle, struct soundcard *soundcard)
{
	int err;

	if (*ctlhandle)
		return 0;
	if ((err = snd_ctl_open(ctlhandle, soundcard->no)) < 0) {
		error("Cannot open control interface for soundcard #%i.", soundcard->no + 1);
		return 1;
	}
	return 0;
}

static int soundcard_open_mix(void **mixhandle, struct soundcard *soundcard, struct mixer *mixer)
{
	int err;

	if (*mixhandle)
		return 0;
	if ((err = snd_mixer_open(mixhandle, soundcard->no, mixer->no)) < 0) {
		error("Cannot open mixer interface for soundcard #%i.", soundcard->no + 1);
		return 1;
	}
	if ((err = snd_mixer_exact_mode(*mixhandle, 1)) < 0) {
		error("Cannot setup exact mode for mixer #%i/#%i: %s", soundcard->no + 1, mixer->no, snd_strerror(err));
		return 1;
	}
	return 0;
}

int soundcard_setup_process(int cardno)
{
	int err;
	void *ctlhandle = NULL;
	void *mixhandle = NULL;
	struct soundcard *soundcard;
	struct ctl_switch *ctlsw;
	struct mixer *mixer;
	struct mixer_channel *channel;
	struct pcm *pcm;
	struct rawmidi *rawmidi;

	for (soundcard = soundcards; soundcard; soundcard = soundcard->next) {
		if (cardno >= 0 && soundcard->no != cardno)
			continue;
		for (ctlsw = soundcard->control.switches; ctlsw; ctlsw = ctlsw->next) {
			if (ctlsw->change)
				if (!soundcard_open_ctl(&ctlhandle, soundcard)) {
					if ((err = snd_ctl_switch_write(ctlhandle, &ctlsw->s)) < 0)
						error("Control switch '%s' write error: %s", ctlsw->s.name, snd_strerror(err));
				}
		}
		for (mixer = soundcard->mixers; mixer; mixer = mixer->next) {
			for (channel = mixer->channels; channel; channel = channel->next)
				if (!soundcard_open_mix(&mixhandle, soundcard, mixer)) {
					if ((channel->info.caps & SND_MIXER_CINFO_CAP_OUTPUT) &&
					    (err = snd_mixer_channel_output_write(mixhandle, channel->no, &channel->ddata[OUTPUT])) < 0)
						error("Mixer channel '%s' write error: %s", channel->info.name, snd_strerror(err));
					if ((channel->info.caps & SND_MIXER_CINFO_CAP_INPUT) &&
					    (err = snd_mixer_channel_input_write(mixhandle, channel->no, &channel->ddata[INPUT])) < 0)
						error("Mixer channel '%s' record write error: %s", channel->info.name, snd_strerror(err));
				}
			if (mixhandle) {
				snd_mixer_close(mixhandle);
				mixhandle = NULL;
			}
			for (ctlsw = mixer->switches; ctlsw; ctlsw = ctlsw->next)
				if (ctlsw->change)
					if (!soundcard_open_ctl(&ctlhandle, soundcard)) {
						if ((err = snd_ctl_mixer_switch_write(ctlhandle, mixer->no, &ctlsw->s)) < 0)
							error("Mixer switch '%s' write error: %s", ctlsw->s.name, snd_strerror(err));
					}
		}
		for (pcm = soundcard->pcms; pcm; pcm = pcm->next) {
			for (ctlsw = pcm->pswitches; ctlsw; ctlsw = ctlsw->next) {
				if (ctlsw->change)
					if (!soundcard_open_ctl(&ctlhandle, soundcard)) {
						if ((err = snd_ctl_pcm_playback_switch_write(ctlhandle, pcm->no, &ctlsw->s)) < 0)
							error("PCM playback switch '%s' write error: %s", ctlsw->s.name, snd_strerror(err));
					}
			}
			for (ctlsw = pcm->rswitches; ctlsw; ctlsw = ctlsw->next) {
				if (ctlsw->change)
					if (!soundcard_open_ctl(&ctlhandle, soundcard)) {
						if ((err = snd_ctl_pcm_record_switch_write(ctlhandle, pcm->no, &ctlsw->s)) < 0)
							error("PCM record switch '%s' write error: %s", ctlsw->s.name, snd_strerror(err));
					}
			}
		}
		for (rawmidi = soundcard->rawmidis; rawmidi; rawmidi = rawmidi->next) {
			for (ctlsw = rawmidi->oswitches; ctlsw; ctlsw = ctlsw->next) {
				if (ctlsw->change)
					if (!soundcard_open_ctl(&ctlhandle, soundcard)) {
						if ((err = snd_ctl_rawmidi_output_switch_write(ctlhandle, rawmidi->no, &ctlsw->s)) < 0)
							error("RAWMIDI output switch '%s' write error: %s", ctlsw->s.name, snd_strerror(err));
					}
			}
			for (ctlsw = rawmidi->iswitches; ctlsw; ctlsw = ctlsw->next) {
				if (ctlsw->change)
					if (!soundcard_open_ctl(&ctlhandle, soundcard)) {
						if ((err = snd_ctl_rawmidi_output_switch_write(ctlhandle, rawmidi->no, &ctlsw->s)) < 0)
							error("RAWMIDI input switch '%s' write error: %s", ctlsw->s.name, snd_strerror(err));
					}
			}
		}
		if(ctlhandle) {
			snd_ctl_close(ctlhandle);
			ctlhandle = NULL;
		}
	}
	return 0;
}
