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

static int merge_one_control(struct ctl_control *cctl, struct ctl_control *uctl, int cardno)
{
	int idx;

	if (!(cctl->info.access & SND_CONTROL_ACCESS_WRITE))
		return 0;
	switch (cctl->info.type) {
	case SND_CONTROL_TYPE_BOOLEAN:
		if (uctl->type != SND_CONTROL_TYPE_BOOLEAN) {
			error("A wrong type %i for the control '%s'. The type integer is expected. Skipping...", uctl->type, control_id(&uctl->c.id));
			return 1;
		}
		for (idx = 0; idx < cctl->info.values_count; idx++) {
			if (cctl->c.value.integer.value[idx] != uctl->c.value.integer.value[idx]) {
				cctl->change = 1;
				cctl->c.value.integer.value[idx] = uctl->c.value.integer.value[idx];
			}
		}
		break;			
	case SND_CONTROL_TYPE_INTEGER:
		if (uctl->type != SND_CONTROL_TYPE_INTEGER) {
			error("A wrong type %i for the control '%s'. The type integer is expected. Skipping...", uctl->type, control_id(&uctl->c.id));
			return 1;
		}
		for (idx = 0; idx < cctl->info.values_count; idx++) {
			if (cctl->info.value.integer.min > uctl->c.value.integer.value[idx] ||
			    cctl->info.value.integer.max < uctl->c.value.integer.value[idx]) {
			    	error("The value %li for the control '%s' is out of range %i-%i.", uctl->c.value.integer.value[idx], control_id(&uctl->c.id), cctl->info.value.integer.min, cctl->info.value.integer.max);
				return 1;
			}
			if (cctl->c.value.integer.value[idx] != uctl->c.value.integer.value[idx]) {
				cctl->change = 1;
				cctl->c.value.integer.value[idx] = uctl->c.value.integer.value[idx];
			}
		}
		break;
	case SND_CONTROL_TYPE_ENUMERATED:
		if (uctl->type != SND_CONTROL_TYPE_ENUMERATED) {
			error("A wrong type %i for the control '%s'. The type integer is expected. Skipping...", uctl->type, control_id(&uctl->c.id));
			return 1;
		}
		for (idx = 0; idx < cctl->info.values_count; idx++) {
			if (cctl->info.value.enumerated.items <= uctl->c.value.enumerated.item[idx]) {
			    	error("The value %u for the control '%s' is out of range 0-%i.", uctl->c.value.integer.value[idx], control_id(&uctl->c.id), cctl->info.value.enumerated.items-1);
				return 1;
			}
			if (cctl->c.value.enumerated.item[idx] != uctl->c.value.enumerated.item[idx]) {
				cctl->change = 1;
				cctl->c.value.enumerated.item[idx] = uctl->c.value.enumerated.item[idx];
			}
		}
		break;
	case SND_CONTROL_TYPE_BYTES:
		if (uctl->type != SND_CONTROL_TYPE_BYTES) {
			error("A wrong type %i for the control %s. The type 'bytes' is expected. Skipping...", uctl->type, control_id(&uctl->c.id));
			return 1;
		}
		if (memcmp(cctl->c.value.bytes.data, uctl->c.value.bytes.data, uctl->info.values_count)) {
			cctl->change = 1;
			memcpy(cctl->c.value.bytes.data, uctl->c.value.bytes.data, uctl->info.values_count);
		}
		break;
	default:
		error("The control type %i is not known.", cctl->type);
	}
	return 0;
}

static int soundcard_setup_merge_control(struct ctl_control *cctl, struct ctl_control *uctl, int cardno)
{
	struct ctl_control *cctl1;
	
	for ( ; uctl; uctl = uctl->next) {
		for (cctl1 = cctl; cctl1; cctl1 = cctl1->next) {
			if (cctl1->c.id.iface == uctl->c.id.iface &&
			    cctl1->c.id.device == uctl->c.id.device &&
			    cctl1->c.id.subdevice == uctl->c.id.subdevice &&
			    !strncmp(cctl1->c.id.name, uctl->c.id.name, sizeof(cctl1->c.id.name))) {
				merge_one_control(cctl1, uctl, cardno);
				break;
			}
		}
		if (!cctl1) {
			error("Cannot find the control %s...", control_id(&uctl->c.id));
		}
	}
	return 0;
}

int soundcard_setup_merge_controls(int cardno)
{
	struct soundcard *soundcard, *rsoundcard;

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
		soundcard_setup_merge_control(soundcard->control.controls, rsoundcard->control.controls, soundcard->no);
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

int soundcard_setup_process_controls(int cardno)
{
	int err;
	snd_ctl_t *ctlhandle = NULL;
	struct soundcard *soundcard;
	struct ctl_control *ctl;

	for (soundcard = soundcards; soundcard; soundcard = soundcard->next) {
		if (cardno >= 0 && soundcard->no != cardno)
			continue;
		for (ctl = soundcard->control.controls; ctl; ctl = ctl->next) {
			if (ctl->change)
				if (!soundcard_open_ctl(&ctlhandle, soundcard)) {
					if ((err = snd_ctl_cwrite(ctlhandle, &ctl->c)) < 0)
						error("Control '%s' write error: %s", control_id(&ctl->c.id), snd_strerror(err));
				}
		}
		if (ctlhandle) {
			snd_ctl_close(ctlhandle);
			ctlhandle = NULL;
		}
	}
	return 0;
}
