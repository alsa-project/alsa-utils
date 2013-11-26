/*
 *  Advanced Linux Sound Architecture Control Program
 *  Copyright (c) by Takashi Iwai <tiwai@suse.de>
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
 */

#include "aconfig.h"
#include "version.h"
#include <stdio.h>
#include <alsa/asoundlib.h>

int monitor(const char *name)
{
	snd_ctl_t *ctl;
	snd_ctl_event_t *event;
	int err;

	if (!name)
		name = "default";

	err = snd_ctl_open(&ctl, name, SND_CTL_READONLY);
	if (err < 0) {
		fprintf(stderr, "Cannot open ctl %s\n", name);
		return err;
	}
	err = snd_ctl_subscribe_events(ctl, 1);
	if (err < 0) {
		fprintf(stderr, "Cannot open subscribe events to ctl %s\n", name);
		snd_ctl_close(ctl);
		return err;
	}
	snd_ctl_event_alloca(&event);
	while (snd_ctl_read(ctl, event) > 0) {
		unsigned int mask;

		if (snd_ctl_event_get_type(event) != SND_CTL_EVENT_ELEM)
			continue;

		printf("#%d (%i,%i,%i,%s,%i)",
		       snd_ctl_event_elem_get_numid(event),
		       snd_ctl_event_elem_get_interface(event),
		       snd_ctl_event_elem_get_device(event),
		       snd_ctl_event_elem_get_subdevice(event),
		       snd_ctl_event_elem_get_name(event),
		       snd_ctl_event_elem_get_index(event));

		mask = snd_ctl_event_elem_get_mask(event);
		if (mask == SND_CTL_EVENT_MASK_REMOVE) {
			printf(" REMOVE\n");
			continue;
		}
		if (mask & SND_CTL_EVENT_MASK_VALUE)
			printf(" VALUE");
		if (mask & SND_CTL_EVENT_MASK_INFO)
			printf(" INFO");
		if (mask & SND_CTL_EVENT_MASK_ADD)
			printf(" ADD");
		if (mask & SND_CTL_EVENT_MASK_TLV)
			printf(" TLV");
		printf("\n");
	}
	snd_ctl_close(ctl);
	return 0;
}
