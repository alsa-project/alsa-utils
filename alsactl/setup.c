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

static void soundcard_mixer_switch_free(struct mixer_switch *first)
{
	struct mixer_switch *next;

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
	soundcard_mixer_switch_free(mixer->switches);
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

static void soundcard_pcm_switch_free(struct pcm_switch *first)
{
	struct pcm_switch *next;

	while (first) {
		next = first->next;
		free(first);
		first = next;
	}
}

static void soundcard_pcm_free1(struct pcm *pcm)
{
	if (!pcm)
		return;
	soundcard_pcm_switch_free(pcm->pswitches);
	soundcard_pcm_switch_free(pcm->rswitches);
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

static void soundcard_rawmidi_switch_free(struct rawmidi_switch *first)
{
	struct rawmidi_switch *next;

	while (first) {
		next = first->next;
		free(first);
		first = next;
	}
}

static void soundcard_rawmidi_free1(struct rawmidi *rawmidi)
{
	if (!rawmidi)
		return;
	soundcard_rawmidi_switch_free(rawmidi->iswitches);
	soundcard_rawmidi_switch_free(rawmidi->oswitches);
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

int soundcard_setup_collect(int cardno)
{
	void *handle, *mhandle;
	struct soundcard *card, *first, *prev;
	int err, idx, count, device;
	struct ctl_switch *ctl, *ctlprev;
	struct mixer *mixer, *mixerprev;
	struct mixer_switch *mixsw, *mixswprev;
	struct mixer_channel *mixerchannel, *mixerchannelprev;
	struct pcm *pcm, *pcmprev;
	struct pcm_switch *pcmsw, *pcmswprev;
	struct rawmidi *rawmidi, *rawmidiprev;
	struct rawmidi_switch *rawmidisw, *rawmidiswprev;

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
	count = snd_ctl_switches(handle);
	for (idx = 0, ctlprev = NULL; idx < count; idx++) {
		ctl = (struct ctl_switch *) malloc(sizeof(struct ctl_switch));
		if (!ctl) {
			snd_ctl_close(handle);
			error("malloc error");
			return 1;
		}
		bzero(ctl, sizeof(struct ctl_switch));
		ctl->no = idx;
		if ((err = snd_ctl_switch_read(handle, idx, &ctl->s)) < 0) {
			free(ctl);
			error("CTL switch read error (%s) - skipping", snd_strerror(err));
			break;
		}
		if (!ctlprev) {
			card->control.switches = ctl;
		} else {
			ctlprev->next = ctl;
		}
		ctlprev = ctl;
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
		count = snd_ctl_mixer_switches(handle, device);
		for (idx = 0, mixswprev = NULL; idx < count; idx++) {
			mixsw = (struct mixer_switch *) malloc(sizeof(struct mixer_switch));
			if (!mixsw) {
				snd_ctl_close(handle);
				error("malloc error");
				return 1;
			}
			bzero(mixsw, sizeof(struct mixer_switch));
			mixsw->no = idx;
			if ((err = snd_ctl_mixer_switch_read(handle, device, idx, &mixsw->s)) < 0) {
				free(mixsw);
				error("MIXER switch read error (%s) - skipping", snd_strerror(err));
				break;
			}
			if (!mixswprev) {
				mixer->switches = mixsw;
			} else {
				mixswprev->next = mixsw;
			}
			mixswprev = mixsw;
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
			if ((err = snd_mixer_channel_info(mhandle, idx, &mixerchannel->i)) < 0) {
				free(mixerchannel);
				error("MIXER channel info error (%s) - skipping", snd_strerror(err));
				break;
			}
			if ((err = snd_mixer_channel_read(mhandle, idx, &mixerchannel->c)) < 0) {
				free(mixerchannel);
				error("MIXER channel read error (%s) - skipping", snd_strerror(err));
				break;
			}
			if ((mixerchannel->i.caps & SND_MIXER_CINFO_CAP_RECORDVOLUME) &&
			    (err = snd_mixer_channel_record_read(mhandle, idx, &mixerchannel->cr)) < 0) {
				free(mixerchannel);
				error("MIXER channel record read error (%s) - skipping", snd_strerror(err));
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
		count = snd_ctl_pcm_playback_switches(handle, device);
		for (idx = 0, pcmswprev = NULL; idx < count; idx++) {
			pcmsw = (struct pcm_switch *) malloc(sizeof(struct pcm_switch));
			if (!pcmsw) {
				snd_ctl_close(handle);
				error("malloc error");
				return 1;
			}
			bzero(pcmsw, sizeof(struct mixer_switch));
			pcmsw->no = idx;
			if ((err = snd_ctl_pcm_playback_switch_read(handle, device, idx, &pcmsw->s)) < 0) {
				free(pcmsw);
				error("PCM playback switch read error (%s) - skipping", snd_strerror(err));
				break;
			}
			if (!pcmswprev) {
				pcm->pswitches = pcmsw;
			} else {
				pcmswprev->next = pcmsw;
			}
			pcmswprev = pcmsw;
		}
		count = snd_ctl_pcm_record_switches(handle, device);
		for (idx = 0, pcmswprev = NULL; idx < count; idx++) {
			pcmsw = (struct pcm_switch *) malloc(sizeof(struct pcm_switch));
			if (!pcmsw) {
				snd_ctl_close(handle);
				error("malloc error");
				return 1;
			}
			bzero(pcmsw, sizeof(struct mixer_switch));
			pcmsw->no = idx;
			if ((err = snd_ctl_pcm_record_switch_read(handle, device, idx, &pcmsw->s)) < 0) {
				free(pcmsw);
				error("PCM record switch read error (%s) - skipping", snd_strerror(err));
				break;
			}
			if (!pcmswprev) {
				pcm->rswitches = pcmsw;
			} else {
				pcmswprev->next = pcmsw;
			}
			pcmswprev = pcmsw;
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
		count = snd_ctl_rawmidi_input_switches(handle, device);
		for (idx = 0, rawmidiswprev = NULL; idx < count; idx++) {
			rawmidisw = (struct rawmidi_switch *) malloc(sizeof(struct rawmidi_switch));
			if (!rawmidisw) {
				snd_ctl_close(handle);
				error("malloc error");
				return 1;
			}
			bzero(rawmidisw, sizeof(struct rawmidi_switch));
			rawmidisw->no = idx;
			if ((err = snd_ctl_rawmidi_input_switch_read(handle, device, idx, &rawmidisw->s)) < 0) {
				free(rawmidisw);
				error("RAWMIDI input switch read error (%s) - skipping", snd_strerror(err));
				break;
			}
			if (!rawmidiswprev) {
				rawmidi->iswitches = rawmidisw;
			} else {
				rawmidiswprev->next = rawmidisw;
			}
			rawmidiswprev = rawmidisw;
		}
		count = snd_ctl_rawmidi_output_switches(handle, device);
		for (idx = 0, rawmidiswprev = NULL; idx < count; idx++) {
			rawmidisw = (struct rawmidi_switch *) malloc(sizeof(struct rawmidi_switch));
			if (!rawmidisw) {
				snd_ctl_close(handle);
				error("malloc error");
				return 1;
			}
			bzero(rawmidisw, sizeof(struct rawmidi_switch));
			rawmidisw->no = idx;
			if ((err = snd_ctl_rawmidi_output_switch_read(handle, device, idx, &rawmidisw->s)) < 0) {
				free(rawmidisw);
				error("RAWMIDI output switch read error (%s) - skipping", snd_strerror(err));
				break;
			}
			if (!rawmidiswprev) {
				rawmidi->oswitches = rawmidisw;
			} else {
				rawmidiswprev->next = rawmidisw;
			}
			rawmidiswprev = rawmidisw;
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

static void soundcard_setup_write_switch(FILE * out, int interface, const unsigned char *name, unsigned int type, unsigned int low, unsigned int high, void *data)
{
	union {
		unsigned int enable;
		unsigned char data8[32];
		unsigned short data16[16];
		unsigned int data32[8];
	} *pdata = data;
	char *s, v[16];
	int idx, first, switchok = 0;
	const char *space = "    ";

	v[0] = '\0';
	switch (type) {
	case SND_CTL_SW_TYPE_BOOLEAN:
		s = "bool";
		strcpy(v, pdata->enable ? "true" : "false");
		break;
	case SND_CTL_SW_TYPE_BYTE:
		s = "byte";
		sprintf(v, "%u", (unsigned int) pdata->data8[0]);
		break;
	case SND_CTL_SW_TYPE_WORD:
		s = "word";
		if (interface == SND_INTERFACE_CONTROL &&
		    !strcmp(name, SND_CTL_SW_JOYSTICK_ADDRESS)) {
		    	sprintf(v, "0x%x", (unsigned int) pdata->data16[0]);
		} else {
			sprintf(v, "%u", (unsigned int) pdata->data16[0]);
		}
		break;
	case SND_CTL_SW_TYPE_DWORD:
		s = "dword";
		sprintf(v, "%u", pdata->data32[0]);
		break;
	case SND_CTL_SW_TYPE_USER:
		s = "user";
		break;
	default:
		s = "unknown";
	}
	fprintf(out, "%s; Type is '%s'.\n", space, s);
	if (low != 0 || high != 0)
		fprintf(out, "%s; Accepted switch range is from %u to %u.\n", space, low, high);
	if (interface == SND_INTERFACE_CONTROL && type == SND_CTL_SW_TYPE_WORD &&
	    !strcmp(name, SND_CTL_SW_JOYSTICK_ADDRESS)) {
		for (idx = 1, first = 1; idx < 16; idx++) {
			if (pdata->data16[idx]) {
				if (first) {
					fprintf(out, "%s; Available addresses - 0x%x", space, pdata->data16[idx]);
					first = 0;
				} else {
					fprintf(out, ", 0x%x", pdata->data16[idx]);
				}
			}
		}
		if (!first)
			fprintf(out, "\n");
	}
	if (interface == SND_INTERFACE_MIXER && type == SND_MIXER_SW_TYPE_BOOLEAN &&
	    !strcmp(name, SND_MIXER_SW_IEC958OUT)) {
		fprintf(out, "%sswitch( \"%s\", ", space, name);
		if (pdata->data32[1] == (('C' << 8) | 'S')) {	/* Cirrus Crystal */
			switchok = 0;
			fprintf(out, "iec958ocs( %s", pdata->enable ? "enable" : "disable");
			if (pdata->data16[4] & 0x2000)
				fprintf(out, " 3d");
			if (pdata->data16[4] & 0x0040)
				fprintf(out, " reset");
			if (pdata->data16[4] & 0x0020)
				fprintf(out, " user");
			if (pdata->data16[4] & 0x0010)
				fprintf(out, " valid");
			if (pdata->data16[5] & 0x0002)
				fprintf(out, " data");
			if (!(pdata->data16[5] & 0x0004))
				fprintf(out, " protect");
			switch (pdata->data16[5] & 0x0018) {
			case 0x0008:
				fprintf(out, " pre2");
				break;
			default:
				break;
			}
			if (pdata->data16[5] & 0x0020)
				fprintf(out, " fsunlock");
			fprintf(out, " type( 0x%x )", (pdata->data16[5] >> 6) & 0x7f);
			if (pdata->data16[5] & 0x2000)
				fprintf(out, " gstatus");
			fprintf(out, " )");
			goto __end;
		}
	}
	fprintf(out, "%sswitch( \"%s\", ", space, name);
	if (!switchok) {
		fprintf(out, v);
		if (type < 0 || type > SND_CTL_SW_TYPE_DWORD) {
			/* TODO: some well known types should be verbose */
			fprintf(out, " rawdata( ");
			for (idx = 0; idx < 31; idx++) {
				fprintf(out, "@%02x:", pdata->data8[idx]);
			}
			fprintf(out, "%02x@ )\n", pdata->data8[31]);
		}
	}
      __end:
	fprintf(out, " )\n");
}


static void soundcard_setup_write_mixer_channel(FILE * out, snd_mixer_channel_info_t * info, snd_mixer_channel_t * channel, snd_mixer_channel_t * record_channel)
{
	fprintf(out, "    ; Capabilities:%s%s%s%s%s%s%s%s%s%s%s%s%s.\n",
		info->caps & SND_MIXER_CINFO_CAP_RECORD ? 	" record" : "",
		info->caps & SND_MIXER_CINFO_CAP_JOINRECORD ? 	" join-record" : "",
		info->caps & SND_MIXER_CINFO_CAP_STEREO ? 	" stereo" : "",
		info->caps & SND_MIXER_CINFO_CAP_HWMUTE ? 	" hardware-mute" : "",
		info->caps & SND_MIXER_CINFO_CAP_JOINMUTE ? 	" join-mute" : "",
		info->caps & SND_MIXER_CINFO_CAP_DIGITAL ?	" digital" : "",
		info->caps & SND_MIXER_CINFO_CAP_INPUT ?	" external-input" : "",
		info->caps & SND_MIXER_CINFO_CAP_LTOR_OUT ?	" ltor-out" : "",
		info->caps & SND_MIXER_CINFO_CAP_RTOL_OUT ?	" rtol-out" : "",
		info->caps & SND_MIXER_CINFO_CAP_SWITCH_OUT ?	" switch-out" : "",
		info->caps & SND_MIXER_CINFO_CAP_LTOR_IN ?	" ltor-in" : "",
		info->caps & SND_MIXER_CINFO_CAP_RTOL_IN ?	" rtol-in" : "",
		info->caps & SND_MIXER_CINFO_CAP_RECORDVOLUME ? " record-volume" : "");
	fprintf(out, "    ; Accepted channel range is from %i to %i.\n", info->min, info->max);
	fprintf(out, "    channel( \"%s\", ", info->name);
	if (info->caps & SND_MIXER_CINFO_CAP_STEREO) {
		char bufl[16] = "";
		char bufr[16] = "";
		if (info->caps & SND_MIXER_CINFO_CAP_RECORDVOLUME) {
			sprintf(bufl, " [%i]", record_channel->left);
			sprintf(bufr, " [%i]", record_channel->right);
		}
		fprintf(out, "stereo( %i%s%s%s%s%s, %i%s%s%s%s%s )",
			channel->left,
			channel->flags & SND_MIXER_FLG_MUTE_LEFT ? " mute" : "",
			channel->flags & SND_MIXER_FLG_RECORD_LEFT ? " record" : "",
			bufl,
			channel->flags & SND_MIXER_FLG_LTOR_OUT ? " swout" : "",
			channel->flags & SND_MIXER_FLG_LTOR_IN ? " swin" : "",
			channel->right,
			channel->flags & SND_MIXER_FLG_MUTE_RIGHT ? " mute" : "",
			channel->flags & SND_MIXER_FLG_RECORD_RIGHT ? " record" : "",
			bufr,
			channel->flags & SND_MIXER_FLG_RTOL_OUT ? " swout" : "",
			channel->flags & SND_MIXER_FLG_RTOL_IN ? " swin" : ""
		    );
	} else {
		char buf[16] = "";
		if (info->caps & SND_MIXER_CINFO_CAP_RECORDVOLUME)
			sprintf(buf, " [%i]", (record_channel->left+record_channel->right) /2);
		fprintf(out, "mono( %i%s%s%s )",
			(channel->left + channel->right) / 2,
			channel->flags & SND_MIXER_FLG_MUTE ? " mute" : "",
			channel->flags & SND_MIXER_FLG_RECORD ? " record" : "",
			buf
		    );
	}
	fprintf(out, " )\n");
}

int soundcard_setup_write(const char *cfgfile)
{
	FILE *out;
	struct soundcard *first;
	struct ctl_switch *ctlsw;
	struct mixer *mixer;
	struct mixer_switch *mixersw;
	struct mixer_channel *mixerchannel;
	struct pcm *pcm;
	struct pcm_switch *pcmsw;
	struct rawmidi *rawmidi;
	struct rawmidi_switch *rawmidisw;

	if ((out = fopen(cfgfile, "w+")) == NULL) {
		error("Cannot open file '%s' for writing...\n", cfgfile);
		return 1;
	}
	fprintf(out, "# ALSA driver configuration\n");
	fprintf(out, "# Generated by alsactl\n");
	fprintf(out, "\n");
	for (first = soundcards; first; first = first->next) {
		fprintf(out, "soundcard( \"%s\" ) {\n", first->control.hwinfo.id);
		if (first->control.switches) {
			fprintf(out, "  control {\n");
			for (ctlsw = first->control.switches; ctlsw; ctlsw = ctlsw->next)
				soundcard_setup_write_switch(out, SND_INTERFACE_CONTROL, ctlsw->s.name, ctlsw->s.type, ctlsw->s.low, ctlsw->s.high, (void *) &ctlsw->s.value);
			fprintf(out, "  }\n");
		}
		for (mixer = first->mixers; mixer; mixer = mixer->next) {
			fprintf(out, "  mixer( \"%s\" ) {\n", mixer->info.name);
			for (mixerchannel = mixer->channels; mixerchannel; mixerchannel = mixerchannel->next)
				soundcard_setup_write_mixer_channel(out, &mixerchannel->i, &mixerchannel->c, &mixerchannel->cr);
			for (mixersw = mixer->switches; mixersw; mixersw = mixersw->next)
				soundcard_setup_write_switch(out, SND_INTERFACE_MIXER, mixersw->s.name, mixersw->s.type, mixersw->s.low, mixersw->s.high, (void *) (&mixersw->s.value));
			fprintf(out, "  }\n");
		}
		for (pcm = first->pcms; pcm; pcm = pcm->next) {
			if (!pcm->pswitches && !pcm->rswitches)
				continue;
			fprintf(out, "  pcm( \"%s\" ) {\n", pcm->info.name);
			if (pcm->pswitches) {
				fprintf(out, "    playback {");
				for (pcmsw = pcm->pswitches; pcmsw; pcmsw = pcmsw->next)
					soundcard_setup_write_switch(out, SND_INTERFACE_PCM, pcmsw->s.name, pcmsw->s.type, pcmsw->s.low, pcmsw->s.high, (void *) &pcmsw->s.value);
				fprintf(out, "    }\n");
			}
			if (pcm->rswitches) {
				fprintf(out, "    record {");
				for (pcmsw = pcm->pswitches; pcmsw; pcmsw = pcmsw->next)
					soundcard_setup_write_switch(out, SND_INTERFACE_PCM, pcmsw->s.name, pcmsw->s.type, pcmsw->s.low, pcmsw->s.high, (void *) &pcmsw->s.value);
				fprintf(out, "    }\n");
			}
			fprintf(out, "  }\n");
		}
		for (rawmidi = first->rawmidis; rawmidi; rawmidi = rawmidi->next) {
			if (!rawmidi->oswitches && !rawmidi->iswitches)
				continue;
			fprintf(out, "  rawmidi( \"%s\" ) {\n", rawmidi->info.name);
			if (rawmidi->oswitches) {
				fprintf(out, "    output {");
				for (rawmidisw = rawmidi->oswitches; rawmidisw; rawmidisw = rawmidisw->next)
					soundcard_setup_write_switch(out, SND_INTERFACE_RAWMIDI, rawmidisw->s.name, rawmidisw->s.type, rawmidisw->s.low, rawmidisw->s.high, (void *) &rawmidisw->s.value);
				fprintf(out, "    }\n");
			}
			if (rawmidi->iswitches) {
				fprintf(out, "    input {");
				for (rawmidisw = rawmidi->iswitches; rawmidisw; rawmidisw = rawmidisw->next)
					soundcard_setup_write_switch(out, SND_INTERFACE_RAWMIDI, rawmidisw->s.name, rawmidisw->s.type, rawmidisw->s.low, rawmidisw->s.high, (void *) &rawmidisw->s.value);
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
	struct mixer_switch *mixersw;
	struct pcm *pcm;
	struct pcm_switch *pcmsw;
	struct rawmidi *rawmidi;
	struct rawmidi_switch *rawmidisw;

	for (soundcard = soundcards; soundcard; soundcard = soundcard->next) {
		if (cardno >= 0 && soundcard->no != cardno)
			continue;
		for (ctlsw = soundcard->control.switches; ctlsw; ctlsw = ctlsw->next) {
			if (ctlsw->change)
				if (!soundcard_open_ctl(&ctlhandle, soundcard)) {
					if ((err = snd_ctl_switch_write(ctlhandle, ctlsw->no, &ctlsw->s)) < 0)
						error("Control switch '%s' write error: %s", ctlsw->s.name, snd_strerror(err));
				}
		}
		for (mixer = soundcard->mixers; mixer; mixer = mixer->next) {
			for (channel = mixer->channels; channel; channel = channel->next)
				if (channel->change)
					if (!soundcard_open_mix(&mixhandle, soundcard, mixer)) {
						if ((err = snd_mixer_channel_write(mixhandle, channel->no, &channel->c)) < 0)
							error("Mixer channel '%s' write error: %s", channel->i.name, snd_strerror(err));
						if ((channel->i.caps & SND_MIXER_CINFO_CAP_RECORDVOLUME) &&
						    (err = snd_mixer_channel_record_write(mixhandle, channel->no, &channel->cr)) < 0)
							error("Mixer channel '%s' record write error: %s", channel->i.name, snd_strerror(err));
					}
			if (mixhandle) {
				snd_mixer_close(mixhandle);
				mixhandle = NULL;
			}
			for (mixersw = mixer->switches; mixersw; mixersw = mixersw->next)
				if (mixersw->change)
					if (!soundcard_open_ctl(&ctlhandle, soundcard)) {
						if ((err = snd_ctl_mixer_switch_write(ctlhandle, mixer->no, mixersw->no, &mixersw->s)) < 0)
							error("Mixer switch '%s' write error: %s", mixersw->s.name, snd_strerror(err));
					}
		}
		for (pcm = soundcard->pcms; pcm; pcm = pcm->next) {
			for (pcmsw = pcm->pswitches; pcmsw; pcmsw = pcmsw->next) {
				if (pcmsw->change)
					if (!soundcard_open_ctl(&ctlhandle, soundcard)) {
						if ((err = snd_ctl_pcm_playback_switch_write(ctlhandle, pcm->no, pcmsw->no, &pcmsw->s)) < 0)
							error("PCM playback switch '%s' write error: %s", pcmsw->s.name, snd_strerror(err));
					}
			}
			for (pcmsw = pcm->rswitches; pcmsw; pcmsw = pcmsw->next) {
				if (pcmsw->change)
					if (!soundcard_open_ctl(&ctlhandle, soundcard)) {
						if ((err = snd_ctl_pcm_playback_switch_write(ctlhandle, pcm->no, pcmsw->no, &pcmsw->s)) < 0)
							error("PCM record switch '%s' write error: %s", pcmsw->s.name, snd_strerror(err));
					}
			}
		}
		for (rawmidi = soundcard->rawmidis; rawmidi; rawmidi = rawmidi->next) {
			for (rawmidisw = rawmidi->oswitches; rawmidisw; rawmidisw = rawmidisw->next) {
				if (rawmidisw->change)
					if (!soundcard_open_ctl(&ctlhandle, soundcard)) {
						if ((err = snd_ctl_rawmidi_output_switch_write(ctlhandle, rawmidi->no, rawmidisw->no, &rawmidisw->s)) < 0)
							error("RAWMIDI output switch '%s' write error: %s", rawmidisw->s.name, snd_strerror(err));
					}
			}
			for (rawmidisw = rawmidi->iswitches; rawmidisw; rawmidisw = rawmidisw->next) {
				if (rawmidisw->change)
					if (!soundcard_open_ctl(&ctlhandle, soundcard)) {
						if ((err = snd_ctl_rawmidi_output_switch_write(ctlhandle, rawmidi->no, rawmidisw->no, &rawmidisw->s)) < 0)
							error("RAWMIDI input switch '%s' write error: %s", rawmidisw->s.name, snd_strerror(err));
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
