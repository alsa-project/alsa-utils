/*
 *  Advanced Linux Sound Architecture Control Program
 *  Copyright (c) 1999 by Jaroslav Kysela <perex@suse.cz>
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

static char *sw_id(const char *name, int cardno, int devno, const char *id)
{
	static char str[256];
	
	sprintf(str, "%s %s card %i", name, id, cardno);
	if (devno >= 0)
		sprintf(str + strlen(str)," device %i", devno);
	return str;
}

static int merge_one_sw(struct ctl_switch *csw, struct ctl_switch *usw, int cardno, int devno, const char *id)
{
	switch (csw->s.type) {
	case SND_SW_TYPE_BOOLEAN:
		if (usw->s.type != SND_SW_TYPE_BOOLEAN) {
			error("A wrong type for the switch %s. The type boolean is expected. Skipping...", sw_id(usw->s.name, cardno, devno, id));
			return 1;
		}
		if (csw->s.value.enable != usw->s.value.enable) {
			csw->change = 1;
			csw->s.value.enable = usw->s.value.enable;
		}
		if (!strncmp(csw->s.name, SND_MIXER_SW_IEC958_OUTPUT, sizeof(csw->s.name))) {
			if (usw->s.value.data32[1] == (('C' << 8) | 'S')) {
				if (csw->s.value.data16[4] != usw->s.value.data16[4] ||
				    csw->s.value.data16[5] != usw->s.value.data16[5]) {
					csw->change = 1;
					csw->s.value.data16[4] = usw->s.value.data16[4];
					csw->s.value.data16[5] = usw->s.value.data16[5];
				}
			}
		}
		break;			
	case SND_SW_TYPE_BYTE:
		if (usw->s.type != SND_SW_TYPE_DWORD) {
			error("A wrong type for the switch %s. The type byte is expected. Skipping...", sw_id(usw->s.name, cardno, devno, id));
			return 1;
		}
		if (csw->s.low > usw->s.value.data32[0] ||
		    csw->s.high < usw->s.value.data32[0]) {
		    	error("The value %u for the switch %s is out of range %i-%i.", usw->s.value.data32[0], sw_id(usw->s.name, cardno, devno, id), csw->s.low, csw->s.high);
			return 1;
		}
		if (csw->s.value.data8[0] != (unsigned char)usw->s.value.data32[0]) {
			csw->change = 1;
			csw->s.value.data8[0] = (unsigned char)usw->s.value.data32[0];
		}
		break;
	case SND_SW_TYPE_WORD:
		if (usw->s.type != SND_SW_TYPE_DWORD) {
			error("A wrong type for the switch %s. The type word is expected. Skipping...", sw_id(usw->s.name, cardno, devno, id));
			return 1;
		}
		if (csw->s.low > usw->s.value.data32[0] ||
		    csw->s.high < usw->s.value.data32[0]) {
		    	error("The value %u for the switch %s is out of range %i-%i.", usw->s.value.data32[0], sw_id(usw->s.name, cardno, devno, id), csw->s.low, csw->s.high);
			return 1;
		}
		if (csw->s.value.data16[0] != (unsigned short)usw->s.value.data32[0]) {
			csw->change = 1;
			csw->s.value.data16[0] = (unsigned short)usw->s.value.data32[0];
		}
		break;
	case SND_SW_TYPE_DWORD:
		if (usw->s.type != SND_SW_TYPE_DWORD) {
			error("A wrong type for the switch %s. The type dword is expected. Skipping...", sw_id(usw->s.name, cardno, devno, id));
			return 1;
		}
		if (csw->s.low > usw->s.value.data32[0] ||
		    csw->s.high < usw->s.value.data32[0]) {
		    	error("The value %u for the switch %s is out of range %i-%i.", usw->s.value.data32[0], sw_id(usw->s.name, cardno, devno, id), csw->s.low, csw->s.high);
			return 1;
		}
		if (csw->s.value.data32[0] != usw->s.value.data32[0]) {
			csw->change = 1;
			csw->s.value.data32[0] = usw->s.value.data32[0];
		}
		break;
	case SND_SW_TYPE_USER_READ_ONLY:
		break;
	case SND_SW_TYPE_USER:
		if (usw->s.type != SND_SW_TYPE_USER) {
			error("A wrong type %i for the switch %s. The type user is expected. Skipping...", usw->s.type, sw_id(usw->s.name, cardno, devno, id));
			return 1;
		}
		if (memcmp(csw->s.value.data8, usw->s.value.data8, 32)) {
			csw->change = 1;
			memcpy(csw->s.value.data8, usw->s.value.data8, 32);
		}
		break;
	default:
		error("The switch type %i is not known.", csw->s.type);
	}
	return 0;
}

static int soundcard_setup_merge_sw(struct ctl_switch *csw, struct ctl_switch *usw, int cardno, int devno, const char *id)
{
	struct ctl_switch *csw1;
	
	for ( ; usw; usw = usw->next) {
		for (csw1 = csw; csw1; csw1 = csw1->next) {
			if (!strncmp(csw1->s.name, usw->s.name, sizeof(csw1->s.name))) {
				merge_one_sw(csw1, usw, cardno, devno, id);
				break;
			}
		}
		if (!csw1) {
			error("Cannot find the switch %s...", sw_id(usw->s.name, cardno, devno, id));
		}
	}
	return 0;
}

int soundcard_setup_merge_switches(int cardno)
{
	struct soundcard *soundcard, *rsoundcard;
	struct mixer *mixer, *rmixer;
	struct pcm *pcm, *rpcm;
	struct rawmidi *rawmidi, *rrawmidi;

	for (rsoundcard = rsoundcards; rsoundcard; rsoundcard = rsoundcard->next) {
		for (soundcard = soundcards; soundcard; soundcard = soundcard->next) {
			if (!strncmp(soundcard->control.hwinfo.id, rsoundcard->control.hwinfo.id, sizeof(soundcard->control.hwinfo.id)))
				break;
		}
		if (!soundcard) {
			error("The soundcard '%s' was not found...\n", rsoundcard->control.hwinfo.id);
			continue;
		}
		if (cardno >= 0 && soundcard->no != cardno)
			continue;
		soundcard_setup_merge_sw(soundcard->control.switches, rsoundcard->control.switches, soundcard->no, -1, "control");
		for (rmixer = rsoundcard->mixers; rmixer; rmixer = rmixer->next) {
			for (mixer = soundcard->mixers; mixer; mixer = mixer->next) {
				if (!strncmp(mixer->info.name, rmixer->info.name, sizeof(mixer->info.name)))
					break;
			}
			if (!mixer) {
				error("The mixer device '%s' from the soundcard %i was not found...\n", rmixer->info.name, soundcard->no);
				continue;
			}
			soundcard_setup_merge_sw(mixer->switches, rmixer->switches, soundcard->no, mixer->no, "mixer");
		}
		for (rpcm = rsoundcard->pcms; rpcm; rpcm = rpcm->next) {
			for (pcm = soundcard->pcms; pcm; pcm = pcm->next) {
				if (!strncmp(pcm->info.name, rpcm->info.name, sizeof(pcm->info.name)))
					break;
			}
			if (!rpcm) {
				error("The PCM device '%s' from the soundcard %i was not found...\n", rpcm->info.name, soundcard->no);
				continue;
			}
			soundcard_setup_merge_sw(pcm->pswitches, rpcm->pswitches, soundcard->no, pcm->no, "PCM playback");
			soundcard_setup_merge_sw(pcm->rswitches, rpcm->rswitches, soundcard->no, pcm->no, "PCM capture");
		}
		for (rrawmidi = rsoundcard->rawmidis; rrawmidi; rrawmidi = rrawmidi->next) {
			for (rawmidi = soundcard->rawmidis; rawmidi; rawmidi = rawmidi->next) {
				if (!strncmp(rawmidi->info.name, rrawmidi->info.name, sizeof(rawmidi->info.name)))
					break;
			}
			if (!rrawmidi) {
				error("The rawmidi device '%s' from the soundcard %i was not found...\n", rrawmidi->info.name, soundcard->no);
				continue;
			}
			soundcard_setup_merge_sw(rawmidi->iswitches, rrawmidi->iswitches, soundcard->no, rawmidi->no, "rawmidi input");
			soundcard_setup_merge_sw(rawmidi->oswitches, rrawmidi->oswitches, soundcard->no, rawmidi->no, "rawmidi output");
		}
	}
	return 0;
}

static char *element_id(snd_mixer_eid_t *eid, int cardno, int devno, const char *id)
{
	static char str[256];

	sprintf(str, "%s %s card %i", mixer_element_id(eid), id, cardno);
	if (devno >= 0)
		sprintf(str + strlen(str)," device %i", devno);
	return str;
}

static int merge_one_element(struct mixer_element *celement, struct mixer_element *uelement, int cardno, int devno, const char *id)
{
	int tmp;

	if (snd_mixer_element_has_control(&celement->element.eid) != 1)
		return 0;
	switch (celement->element.eid.type) {
	case SND_MIXER_ETYPE_SWITCH1:
		if (celement->element.data.switch1.sw != uelement->element.data.switch1.sw) {
			error("Element %s has got a wrong count of voices.", element_id(&celement->element.eid, cardno, devno, id));
			return 1;
		}
		tmp = ((celement->element.data.switch1.sw + 31) / 32) * sizeof(unsigned int);
		memcpy(celement->element.data.switch1.psw, uelement->element.data.switch1.psw, tmp);
		break;
	case SND_MIXER_ETYPE_SWITCH2:
		celement->element.data.switch2.sw = uelement->element.data.switch2.sw;
		break;
	case SND_MIXER_ETYPE_SWITCH3:
		if (celement->element.data.switch3.rsw != uelement->element.data.switch3.rsw) {
			error("Element %s has got a wrong count of switches.", element_id(&celement->element.eid, cardno, devno, id));
			return 1;
		}
		tmp = ((celement->element.data.switch3.rsw + 31) / 32) * sizeof(unsigned int);
 		memcpy(celement->element.data.switch3.prsw, uelement->element.data.switch3.prsw, tmp);
		break;
	case SND_MIXER_ETYPE_VOLUME1:
		if (celement->element.data.volume1.voices != uelement->element.data.volume1.voices) {
			error("Element %s has got a wrong count of voices.", element_id(&celement->element.eid, cardno, devno, id));
			return 1;
		}
		tmp = celement->element.data.volume1.voices * sizeof(int);
		memcpy(celement->element.data.volume1.pvoices, uelement->element.data.volume1.pvoices, tmp);
		break;
	case SND_MIXER_ETYPE_VOLUME2:
		if (celement->element.data.volume2.avoices != uelement->element.data.volume2.avoices) {
			error("Element %s has got a wrong count of voices.", element_id(&celement->element.eid, cardno, devno, id));
			return 1;
		}
		tmp = celement->element.data.volume2.avoices * sizeof(int);
		memcpy(celement->element.data.volume2.pavoices, uelement->element.data.volume2.pavoices, tmp);
		break;
	case SND_MIXER_ETYPE_ACCU3:
		if (celement->element.data.accu3.voices != uelement->element.data.accu3.voices) {
			error("Element %s has got a wrong count of voices.", element_id(&celement->element.eid, cardno, devno, id));
			return 1;
		}
		tmp = celement->element.data.accu3.voices * sizeof(int);
		memcpy(celement->element.data.accu3.pvoices, uelement->element.data.accu3.pvoices, tmp);
		break;
	case SND_MIXER_ETYPE_MUX1:
		if (celement->element.data.mux1.poutput)
			free(celement->element.data.mux1.poutput);
		celement->element.data.mux1.output_size = 0;
		celement->element.data.mux1.output = 0;
		celement->element.data.mux1.output_over = 0;
		tmp = uelement->element.data.mux1.output * sizeof(snd_mixer_eid_t);
		if (tmp > 0) {
			celement->element.data.mux1.poutput = (snd_mixer_eid_t *)malloc(uelement->element.data.mux1.output_size * sizeof(snd_mixer_eid_t));
			if (!celement->element.data.mux1.poutput) {
				error("No enough memory...");
				return 1;
			}
			celement->element.data.mux1.output_size = uelement->element.data.mux1.output_size;
			celement->element.data.mux1.output = uelement->element.data.mux1.output;
			memcpy(celement->element.data.mux1.poutput, uelement->element.data.mux1.poutput, tmp);
		}
		break;
	case SND_MIXER_ETYPE_MUX2:
		celement->element.data.mux2.output = uelement->element.data.mux2.output;
		break;
	case SND_MIXER_ETYPE_TONE_CONTROL1:
		if ((uelement->element.data.tc1.tc & ~celement->info.data.tc1.tc) != 0) {
			error("Wrong (unsupported) input for the element %s.", element_id(&celement->element.eid, cardno, devno, id));
			return 1;
		}
		celement->element.data.tc1.tc = uelement->element.data.tc1.tc;
		celement->element.data.tc1.sw = uelement->element.data.tc1.sw;
		celement->element.data.tc1.bass = uelement->element.data.tc1.bass;
		celement->element.data.tc1.treble = uelement->element.data.tc1.treble;
		break;
	case SND_MIXER_ETYPE_3D_EFFECT1:
		if ((uelement->element.data.teffect1.effect & ~celement->info.data.teffect1.effect) != 0) {
			error("Wrong (unsupported) input for the element %s.", element_id(&celement->element.eid, cardno, devno, id));
			return 1;
		}
		celement->element.data.teffect1.effect = uelement->element.data.teffect1.effect;
		celement->element.data.teffect1.sw = uelement->element.data.teffect1.sw;
		celement->element.data.teffect1.mono_sw = uelement->element.data.teffect1.mono_sw;
		celement->element.data.teffect1.wide = uelement->element.data.teffect1.wide;
		celement->element.data.teffect1.volume = uelement->element.data.teffect1.volume;
		celement->element.data.teffect1.center = uelement->element.data.teffect1.center;
		celement->element.data.teffect1.space = uelement->element.data.teffect1.space;
		celement->element.data.teffect1.depth = uelement->element.data.teffect1.depth;
		celement->element.data.teffect1.delay = uelement->element.data.teffect1.delay;
		celement->element.data.teffect1.feedback = uelement->element.data.teffect1.feedback;
		celement->element.data.teffect1.depth_rear = uelement->element.data.teffect1.depth_rear;
		break;
	case SND_MIXER_ETYPE_PRE_EFFECT1:
		if (celement->element.data.peffect1.pparameters)
			free(celement->element.data.peffect1.pparameters);
		celement->element.data.peffect1.parameters_size = 0;
		celement->element.data.peffect1.parameters = 0;
		celement->element.data.peffect1.parameters_over = 0;
		celement->element.data.peffect1.item = uelement->element.data.peffect1.item;
		if (celement->element.data.peffect1.item < 0) {
			celement->element.data.peffect1.pparameters = (int *)malloc(uelement->element.data.peffect1.parameters_size * sizeof(int));
			if (!celement->element.data.peffect1.pparameters) {
				error("No enough memory..");
				return 1;
			}
			celement->element.data.peffect1.parameters_size = uelement->element.data.peffect1.parameters_size;
			celement->element.data.peffect1.parameters = uelement->element.data.peffect1.parameters;
			tmp = celement->element.data.peffect1.parameters * sizeof(int);
			memcpy(celement->element.data.peffect1.pparameters, uelement->element.data.peffect1.pparameters, tmp);
		}
		break;
	default:
		error("The element type %i for the element %s is not known.", celement->element.eid.type, mixer_element_id(&celement->element.eid));
	}
	return 0;
}

static int soundcard_setup_merge_element(struct mixer_element *celement, struct mixer_element *uelement, int cardno, int devno, const char *id)
{
	struct mixer_element *element;
	
	for ( ; uelement; uelement = uelement->next) {
		for (element = celement; element; element = element->next) {
			if (!strncmp(element->element.eid.name, uelement->element.eid.name, sizeof(element->element.eid.name)) &&
			    element->element.eid.index == uelement->element.eid.index &&
			    element->element.eid.type == uelement->element.eid.type) {
				merge_one_element(element, uelement, cardno, devno, id);
				break;
			}
		}
		if (!element) {
			error("Cannot find the element %s...", element_id(&uelement->element.eid, cardno, devno, id));
		}
	}
	return 0;
}

int soundcard_setup_merge_data(int cardno)
{
	struct soundcard *soundcard, *rsoundcard;
	struct mixer *mixer, *rmixer;

	for (rsoundcard = rsoundcards; rsoundcard; rsoundcard = rsoundcard->next) {
		for (soundcard = soundcards; soundcard; soundcard = soundcard->next) {
			if (!strncmp(soundcard->control.hwinfo.id, rsoundcard->control.hwinfo.id, sizeof(soundcard->control.hwinfo.id)))
				break;
		}
		if (!soundcard) {
			error("The soundcard '%s' was not found...\n", rsoundcard->control.hwinfo.id);
			continue;
		}
		if (cardno >= 0 && soundcard->no != cardno)
			continue;
		for (rmixer = rsoundcard->mixers; rmixer; rmixer = rmixer->next) {
			for (mixer = soundcard->mixers; mixer; mixer = mixer->next) {
				if (!strncmp(mixer->info.name, rmixer->info.name, sizeof(mixer->info.name)))
					break;
			}
			if (!mixer) {
				error("The mixer device '%s' from the soundcard %i was not found...\n", rmixer->info.name, soundcard->no);
				continue;
			}
			soundcard_setup_merge_element(mixer->elements, rmixer->elements, soundcard->no, mixer->no, "mixer");
		}
	}
	return 0;
}

static int soundcard_open_ctl(snd_ctl_t **ctlhandle, struct soundcard *soundcard)
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

static int soundcard_open_mix(snd_mixer_t **mixhandle, struct soundcard *soundcard, struct mixer *mixer)
{
	int err;

	if (*mixhandle)
		return 0;
	if ((err = snd_mixer_open(mixhandle, soundcard->no, mixer->no)) < 0) {
		error("Cannot open mixer interface for soundcard #%i.", soundcard->no + 1);
		return 1;
	}
	return 0;
}

int soundcard_setup_process_switches(int cardno)
{
	int err;
	snd_ctl_t *ctlhandle = NULL;
	struct soundcard *soundcard;
	struct ctl_switch *ctlsw;
	struct mixer *mixer;
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
						if ((err = snd_ctl_pcm_capture_switch_write(ctlhandle, pcm->no, &ctlsw->s)) < 0)
							error("PCM capture switch '%s' write error: %s", ctlsw->s.name, snd_strerror(err));
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
		if (ctlhandle) {
			snd_ctl_close(ctlhandle);
			ctlhandle = NULL;
		}
	}
	return 0;
}

int soundcard_setup_process_data(int cardno)
{
	int err;
	snd_ctl_t *ctlhandle = NULL;
	snd_mixer_t *mixhandle = NULL;
	struct soundcard *soundcard;
	struct mixer *mixer;
	struct mixer_element *element;

	for (soundcard = soundcards; soundcard; soundcard = soundcard->next) {
		if (cardno >= 0 && soundcard->no != cardno)
			continue;
		for (mixer = soundcard->mixers; mixer; mixer = mixer->next) {
			for (element = mixer->elements; element; element = element->next)
				if (snd_mixer_element_has_control(&element->element.eid) == 1) {
					if (!soundcard_open_mix(&mixhandle, soundcard, mixer)) {
						if ((err = snd_mixer_element_write(mixhandle, &element->element)) < 0)
							error("Mixer element %s write error: %s", mixer_element_id(&element->element.eid), snd_strerror(err));
					}
				}
			if (mixhandle) {
				snd_mixer_close(mixhandle);
				mixhandle = NULL;
			}
		}
		if (ctlhandle) {
			snd_ctl_close(ctlhandle);
			ctlhandle = NULL;
		}
	}
	return 0;
}
