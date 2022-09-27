/*
 *  Advanced Linux Sound Architecture Control Program - General info
 *  Copyright (c) by Jaroslav Kysela <perex@perex.cz>
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
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "aconfig.h"
#include "alsactl.h"

static int pcm_device_list(snd_ctl_t *ctl, snd_pcm_stream_t stream, bool *first)
{
#ifdef __ALSA_PCM_H
	int err, dev, idx;
	unsigned int count;
	snd_pcm_info_t *pcminfo;
	snd_pcm_info_alloca(&pcminfo);
	bool streamfirst, subfirst;

	dev = -1;
	streamfirst = true;
	while (1) {
		if ((err = snd_ctl_pcm_next_device(ctl, &dev)) < 0) {
			error("snd_ctl_pcm_next_device");
			return err;
		}
		if (dev < 0)
			break;
		snd_pcm_info_set_device(pcminfo, dev);
		snd_pcm_info_set_subdevice(pcminfo, 0);
		snd_pcm_info_set_stream(pcminfo, stream);
		if ((err = snd_ctl_pcm_info(ctl, pcminfo)) < 0) {
			if (err != -ENOENT)
				return err;
			continue;
		}
		if (*first) {
			printf("  pcm:\n");
			*first = false;
		}
		if (streamfirst) {
			printf("    - stream: %s\n      devices:\n", snd_pcm_stream_name(stream));
			streamfirst = false;
		}
		printf("        - device: %d\n          id: %s\n          name: %s\n",
				dev,
				snd_pcm_info_get_id(pcminfo),
				snd_pcm_info_get_name(pcminfo));
		count = snd_pcm_info_get_subdevices_count(pcminfo);
		subfirst = true;
		for (idx = 0; idx < (int)count; idx++) {
			snd_pcm_info_set_subdevice(pcminfo, idx);
			if ((err = snd_ctl_pcm_info(ctl, pcminfo)) < 0) {
				error("control digital audio playback info (%s): %s", snd_ctl_name(ctl), snd_strerror(err));
				return err;
			}
			if (subfirst) {
				printf("          subdevices:\n");
				subfirst = false;
			}
			printf("            - subdevice: %d\n              name: %s\n",
						idx, snd_pcm_info_get_subdevice_name(pcminfo));
		}
	}
#endif
	return 0;
}

static const char *snd_rawmidi_stream_name(snd_rawmidi_stream_t stream)
{
	if (stream == SND_RAWMIDI_STREAM_INPUT)
		return "INPUT";
	if (stream == SND_RAWMIDI_STREAM_OUTPUT)
		return "OUTPUT";
	return "???";
}

static int rawmidi_device_list(snd_ctl_t *ctl, snd_rawmidi_stream_t stream, bool *first)
{
#ifdef __ALSA_RAWMIDI_H
	int err, dev, idx;
	unsigned int count;
	snd_rawmidi_info_t *info;
	snd_rawmidi_info_alloca(&info);
	bool streamfirst, subfirst;

	dev = -1;
	streamfirst = true;
	while (1) {
		if ((err = snd_ctl_rawmidi_next_device(ctl, &dev)) < 0) {
			error("snd_ctl_rawmidi_next_device");
			return err;
		}
		if (dev < 0)
			break;
		snd_rawmidi_info_set_device(info, dev);
		snd_rawmidi_info_set_subdevice(info, 0);
		snd_rawmidi_info_set_stream(info, stream);
		if ((err = snd_ctl_rawmidi_info(ctl, info)) < 0) {
			if (err != -ENOENT)
				return err;
			continue;
		}
		if (*first) {
			printf("  rawmidi:\n");
			*first = false;
		}
		if (streamfirst) {
			printf("    - stream: %s\n      devices:\n", snd_rawmidi_stream_name(stream));
			streamfirst = false;
		}
		printf("        - device: %d\n          id: %s\n          name: %s\n",
				dev,
				snd_rawmidi_info_get_id(info),
				snd_rawmidi_info_get_name(info));
		count = snd_rawmidi_info_get_subdevices_count(info);
		subfirst = true;
		for (idx = 0; idx < (int)count; idx++) {
			snd_rawmidi_info_set_subdevice(info, idx);
			if ((err = snd_ctl_rawmidi_info(ctl, info)) < 0) {
				error("control digital audio playback info (%s): %s", snd_ctl_name(ctl), snd_strerror(err));
				return err;
			}
			if (subfirst) {
				printf("          subdevices:\n");
				subfirst = false;
			}
			printf("            - subdevice: %d\n              name: %s\n",
						idx, snd_rawmidi_info_get_subdevice_name(info));
		}
	}
#endif
	return 0;
}

static int hwdep_device_list(snd_ctl_t *ctl)
{
#ifdef __ALSA_HWDEP_H
	int err, dev;
	snd_hwdep_info_t *info;
	snd_hwdep_info_alloca(&info);
	bool first;

	dev = -1;
	first = true;
	while (1) {
		if ((err = snd_ctl_hwdep_next_device(ctl, &dev)) < 0) {
			error("snd_ctl_pcm_next_device");
			return err;
		}
		if (dev < 0)
			break;
		snd_hwdep_info_set_device(info, dev);
		if ((err = snd_ctl_hwdep_info(ctl, info)) < 0) {
			if (err != -ENOENT)
				return err;
			continue;
		}
		if (first) {
			printf("  hwdep:\n");
			first = false;
		}
		printf("    - device: %d\n      id: %s\n      name: %s\n      iface: %d\n",
				dev,
				snd_hwdep_info_get_id(info),
				snd_hwdep_info_get_name(info),
				snd_hwdep_info_get_iface(info));
	}
#endif
	return 0;
}

static int card_info(snd_ctl_t *ctl)
{
	snd_ctl_card_info_t *info;
	snd_ctl_elem_list_t *clist;
	int err;

	snd_ctl_card_info_alloca(&info);
	snd_ctl_elem_list_alloca(&clist);

	if ((err = snd_ctl_card_info(ctl, info)) < 0) {
		error("Control device %s hw info error: %s", snd_ctl_name(ctl), snd_strerror(err));
		return err;
	}
	printf("#\n# Sound card\n#\n");
	printf("- card: %i\n  id: %s\n  name: %s\n  longname: %s\n",
		snd_ctl_card_info_get_card(info),
		snd_ctl_card_info_get_id(info),
		snd_ctl_card_info_get_name(info),
		snd_ctl_card_info_get_longname(info));
	printf("  driver_name: %s\n", snd_ctl_card_info_get_driver(info));
	printf("  mixer_name: %s\n", snd_ctl_card_info_get_mixername(info));
	printf("  components: %s\n", snd_ctl_card_info_get_components(info));
	if ((err = snd_ctl_elem_list(ctl, clist)) < 0) {
		error("snd_ctl_elem_list failure: %s", snd_strerror(err));
	} else {
		printf("  controls_count: %i\n", snd_ctl_elem_list_get_count(clist));
	}
	return err;
}

int general_card_info(int cardno)
{
	snd_ctl_t *ctl;
	char dev[16];
	bool first;
	int err;

	snprintf(dev, sizeof(dev), "hw:%i", cardno);
	if ((err = snd_ctl_open(&ctl, dev, 0)) < 0) {
		error("Control device %s open error: %s", dev, snd_strerror(err));
		return err;
	}
	err = card_info(ctl);

	first = true;
	if (err >= 0)
		err = pcm_device_list(ctl, SND_PCM_STREAM_PLAYBACK, &first);
	if (err >= 0)
		err = pcm_device_list(ctl, SND_PCM_STREAM_CAPTURE, &first);

	first = true;
	if (err >= 0)
		err = rawmidi_device_list(ctl, SND_PCM_STREAM_PLAYBACK, &first);
	if (err >= 0)
		err = rawmidi_device_list(ctl, SND_PCM_STREAM_CAPTURE, &first);

	if (err >= 0)
		err = hwdep_device_list(ctl);
	snd_ctl_close(ctl);
	return err;
}

int general_info(const char *cardname)
{
	struct snd_card_iterator iter;
	int err;

	err = snd_card_iterator_sinit(&iter, cardname);
	if (err < 0)
		return err;
	while (snd_card_iterator_next(&iter)) {
		if ((err = general_card_info(iter.card)))
			return err;
	}
	return snd_card_iterator_error(&iter);
}
