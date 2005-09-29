/*
 *  Advanced Linux Sound Architecture Control Program - ALSA Device Names
 *  Copyright (c) 2005 by Jaroslav Kysela <perex@suse.cz>
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
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include "aconfig.h"
#include "version.h"
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <alsa/asoundlib.h>
#include "alsactl.h"

typedef int (probe_single)(int card, snd_ctl_t *ctl, snd_config_t *config);

static int globidx;

static void dummy_error_handler(const char *file, int line, const char *function, int err, const char *fmt, ...)
{
}

static int for_each_card(probe_single *fcn, snd_config_t *config)
{
	int card = -1, first = 1, err;
	snd_ctl_t *ctl;
	char ctlname[16];

	while (1) {
		if (snd_card_next(&card) < 0)
			break;
		if (card < 0) {
			if (first) {
				error("No soundcards found...");
				return -ENODEV;
			}
			break;
		}
		first = 0;
		sprintf(ctlname, "hw:%i", card);
		err = snd_ctl_open(&ctl, ctlname, 0);
		if (err < 0)
			return err;
		err = (fcn)(card, ctl, config);
		snd_ctl_close(ctl);
		if (err < 0)
			return err;
	}
	return 0;
}

static int add_entry(snd_config_t *cfg, const char *name,
		     const char *cprefix, const char *flag,
		     const char *comment)
{
	int err;
	snd_config_t *c, *d;
	char id[16];
	char xcomment[256];
	char *flag0 = " (", *flag1 = ")";

	if (cprefix == NULL)
		cprefix = "";
	if (flag == NULL) {
		flag0 = "";
		flag = "";
		flag1 = "";
	}
	sprintf(xcomment, "%s - %s%s%s%s", cprefix, comment, flag0, flag, flag1);
	sprintf(id, "alsactl%i", globidx++);
	err = snd_config_make_compound(&c, id, 0);
	if (err < 0)
		return err;
	err = snd_config_add(cfg, c);
	if (err < 0)
		return err;
	err = snd_config_make_string(&d, "name");
	if (err < 0)
		return err;
	err = snd_config_set_string(d, name);
	if (err < 0)
		return err;
	err = snd_config_add(c, d);
	if (err < 0)
		return err;
	err = snd_config_make_string(&d, "comment");
	if (err < 0)
		return err;
	err = snd_config_set_string(d, xcomment);
	if (err < 0)
		return err;
	err = snd_config_add(c, d);
	if (err < 0)
		return err;
	return 0;
}

static int probe_ctl_card(int card, snd_ctl_t *ctl, snd_config_t *config)
{
	int err;
	snd_ctl_card_info_t * info;
	char name[16];
	const char *dname;
	
	snd_ctl_card_info_alloca(&info);
	err = snd_ctl_card_info(ctl, info);
	if (err < 0) {
		error("Unable to get info for card %i: %s\n", card, snd_strerror(err));
		return err;
	}
	sprintf(name, "hw:%i", card);
	dname = snd_ctl_card_info_get_longname(info);
	err = add_entry(config, name, "Physical Device", NULL, dname);
	if (err < 0)
		return err;
	return 0;
}

static int probe_ctl(snd_config_t *config)
{
	int err;
	snd_config_t *c;

	err = snd_config_make_compound(&c, "ctl", 0);
	if (err < 0)
		return err;
	err = snd_config_add(config, c);
	if (err < 0)
		return err;
	err = for_each_card(probe_ctl_card, c);
	if (err < 0)
		return err;
	return 0;
}

static int probe_pcm_virtual(int card, snd_ctl_t *ctl, snd_config_t *config,
			     const char *name, const char *comment)
{
	snd_pcm_t *pcm1, *pcm2;
	int err1, err2, playback, capture, err;
	char name1[32], name2[32], *flag;
	
	if (!debugflag)
		snd_lib_error_set_handler(dummy_error_handler);
	sprintf(name1, name, card);
	sprintf(name2, "plug:%s", name1);
	err1 = snd_pcm_open(&pcm1, name1, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
	err2 = snd_pcm_open(&pcm2, name1, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
	snd_lib_error_set_handler(NULL);
	if (err1 >= 0)
		snd_pcm_close(pcm1);
	if (err2 >= 0)
		snd_pcm_close(pcm2);
	playback = (err1 == 0 || err1 == -EBUSY);
	capture = (err2 == 0 || err2 == -EBUSY);
	if (playback && capture)
		flag = "Duplex";
	else if (playback)
		flag = "Playback";
	else if (capture)
		flag = "Capture";
	else
		return 0;
	err = add_entry(config, name1, "Abstract Device", flag, comment);
	if (err >= 0)
		err = add_entry(config, name2, "Abstract Device With Conversions", flag, comment);
	return err;
}

static int probe_pcm_card(int card, snd_ctl_t *ctl, snd_config_t *config)
{
	int dev = -1, err, err1, err2;
	snd_pcm_info_t * info1, * info2;
	snd_pcm_class_t class;
	char name[16];
	const char *dname;
	char *flag;
	int first = 1, idx;
	static const char *vnames1[] = {
		"default:%i", "Default Device",
		"front:%i", "Front Speakers",
		"rear:%i", "Rear Speakers",
		NULL
	};
	static const char *vnames2[] = {
		"surround40:%i", "Front and Rear Speakers",
		"surround51:%i", "Front, Rear, Center and Woofer",
		"surround71:%i", "Front, Rear, Side, Center and Woofer",
		"spdif:%i", "S/PDIF (IEC958) Optical or Coaxial Wire",
		"phoneline:%i", "Phone Line Interface",
		"modem:%i", "Soft Modem",
		NULL
	};
	
	snd_pcm_info_alloca(&info1);
	snd_pcm_info_alloca(&info2);
	while (1) {
		err = snd_ctl_pcm_next_device(ctl, &dev);
		if (err < 0)
			return err;
		if (dev < 0)
			break;
		memset(info1, 0, snd_pcm_info_sizeof());
		snd_pcm_info_set_device(info1, dev);
		snd_pcm_info_set_stream(info1, SND_PCM_STREAM_PLAYBACK);
		err1 = snd_ctl_pcm_info(ctl, info1);
		memset(info2, 0, snd_pcm_info_sizeof());
		snd_pcm_info_set_device(info2, dev);
		snd_pcm_info_set_stream(info2, SND_PCM_STREAM_CAPTURE);
		err2 = snd_ctl_pcm_info(ctl, info2);
		if (err1 < 0 && err2 < 0) {
			error("Unable to get info for pcm device %i:%i: %s\n", card, dev, snd_strerror(err1));
			continue;
		}
		dname = snd_pcm_info_get_name(info1);
		class = snd_pcm_info_get_class(info1);
		if (err1 == 0 && err2 == 0)
			flag = "Duplex";
		else if (err1 == 0)
			flag = "Playback";
		else {
			flag = "Capture";
			dname = snd_pcm_info_get_name(info2);
			class = snd_pcm_info_get_class(info2);
		}
		if (class != SND_PCM_CLASS_GENERIC &&
		    class != SND_PCM_CLASS_MULTI &&
		    class != SND_PCM_CLASS_MODEM )	/* skip this */
			continue;
		if (first) {
			for (idx = 0; vnames1[idx] != NULL; idx += 2)
				probe_pcm_virtual(card, ctl, config, vnames1[idx], vnames1[idx+1]);
		}
		first = 0;
		sprintf(name, "hw:%i,%i", card, dev);
		err = add_entry(config, name, "Physical Device", flag, dname);
		if (err < 0)
			return err;
		sprintf(name, "plughw:%i,%i", card, dev);
		err = add_entry(config, name, "Physical Device With Conversions", flag, dname);
		if (err < 0)
			return err;
	}
	if (!first) {
		for (idx = 0; vnames2[idx] != NULL; idx += 2)
			probe_pcm_virtual(card, ctl, config, vnames2[idx], vnames2[idx+1]);
	}
	return 0;
}

static int probe_pcm(snd_config_t *config)
{
	int err;
	snd_config_t *c;

	err = snd_config_make_compound(&c, "pcm", 0);
	if (err < 0)
		return err;
	err = snd_config_add(config, c);
	if (err < 0)
		return err;
	err = for_each_card(probe_pcm_card, c);
	if (err < 0)
		return err;
	return 0;
}

static int probe_rawmidi_virtual(snd_config_t *config,
				 const char *name, const char *comment)
{
	snd_rawmidi_t *rawmidi1, *rawmidi2;
	int err1, err2, playback, capture, err;
	char *flag;
	
	if (!debugflag)
		snd_lib_error_set_handler(dummy_error_handler);
	err1 = snd_rawmidi_open(NULL, &rawmidi1, name, SND_RAWMIDI_NONBLOCK);
	err2 = snd_rawmidi_open(&rawmidi2, NULL, name, SND_RAWMIDI_NONBLOCK);
	snd_lib_error_set_handler(NULL);
	if (err1 >= 0)
		snd_rawmidi_close(rawmidi1);
	if (err2 >= 0)
		snd_rawmidi_close(rawmidi2);
	playback = (err1 == 0 || err1 == -EBUSY);
	capture = (err2 == 0 || err2 == -EBUSY);
	if (playback && capture)
		flag = "Duplex";
	else if (playback)
		flag = "Playback";
	else if (capture)
		flag = "Capture";
	else
		return 0;
	err = add_entry(config, name, "Abstract Device", flag, comment);
	return err;
}

static int probe_rawmidi_card(int card, snd_ctl_t *ctl, snd_config_t *config)
{
	int dev = -1, err, err1, err2;
	snd_rawmidi_info_t * info1, * info2;
	char name[16];
	const char *dname;
	const char *subname;
	char *flag;
	int subdev;
	
	snd_rawmidi_info_alloca(&info1);
	snd_rawmidi_info_alloca(&info2);
	while (1) {
		err = snd_ctl_rawmidi_next_device(ctl, &dev);
		if (err < 0)
			return err;
		if (dev < 0)
			break;
		memset(info1, 0, snd_rawmidi_info_sizeof());
		snd_rawmidi_info_set_device(info1, dev);
		snd_rawmidi_info_set_stream(info1, SND_RAWMIDI_STREAM_OUTPUT);
		err1 = snd_ctl_rawmidi_info(ctl, info1);
		memset(info2, 0, snd_rawmidi_info_sizeof());
		snd_rawmidi_info_set_device(info2, dev);
		snd_rawmidi_info_set_stream(info2, SND_RAWMIDI_STREAM_INPUT);
		err2 = snd_ctl_rawmidi_info(ctl, info2);
		if (err1 < 0 && err2 < 0) {
			error("Unable to get info for rawmidi device %i:%i: %s\n", card, dev, snd_strerror(err1));
			continue;
		}
		dname = snd_rawmidi_info_get_name(info1);
		subname = snd_rawmidi_info_get_subdevice_name(info1);
		if (err1 == 0 && err2 == 0)
			flag = "Duplex";
		else if (err1 == 0)
			flag = "Output";
		else {
			flag = "Input";
			dname = snd_rawmidi_info_get_name(info2);
			subname = snd_rawmidi_info_get_subdevice_name(info2);
		}
		if (subname[0] == '\0') {
			sprintf(name, "hw:%i,%i", card, dev);
			err = add_entry(config, name, "Physical Device", flag, dname);
			if (err < 0)
				return err;
		} else {
			subdev = 0;
			do {
				sprintf(name, "hw:%i,%i,%i", card, dev, subdev);
				if (err1 == 0)
					subname = snd_rawmidi_info_get_subdevice_name(info1);
				else
					subname = snd_rawmidi_info_get_subdevice_name(info2);
				if (err1 == 0 && err2 == 0)
					flag = "Duplex";
				else if (err1 == 0)
					flag = "Output";
				else
					flag = "Input";
				err = add_entry(config, name, "Physical Device", flag, subname);
				if (err < 0)
					return err;
				++subdev;
				snd_rawmidi_info_set_subdevice(info1, subdev);
				snd_rawmidi_info_set_subdevice(info2, subdev);
				err1 = snd_ctl_rawmidi_info(ctl, info1);
				err2 = snd_ctl_rawmidi_info(ctl, info2);
			} while (err1 == 0 || err2 == 0);
		}
	}
	return 0;
}

static int probe_rawmidi(snd_config_t *config)
{
	int err;
	snd_config_t *c;

	err = snd_config_make_compound(&c, "rawmidi", 0);
	if (err < 0)
		return err;
	err = snd_config_add(config, c);
	if (err < 0)
		return err;
	err = probe_rawmidi_virtual(c, "default", "Default Device");
	if (err < 0)
		return err;
	err = for_each_card(probe_rawmidi_card, c);
	if (err < 0)
		return err;
	err = add_entry(c, "virtual", "Virtual Device", "Duplex", "Sequencer");
	if (err < 0)
		return err;
	err = add_entry(c, "virtual:MERGE=0", "Virtual Device", "Duplex", "Sequencer (No Merge)");
	if (err < 0)
		return err;
	return 0;
}

static int probe_timers(snd_config_t *config)
{
	int err;
	snd_timer_query_t *handle;
	snd_timer_id_t *id;
	snd_timer_ginfo_t *info;
	char name[64];
	const char *dname;

	err = snd_timer_query_open(&handle, "default", 0);
	if (err < 0)
		return err;
	snd_timer_id_alloca(&id);
	snd_timer_ginfo_alloca(&info);
	snd_timer_id_set_class(id, SND_TIMER_CLASS_NONE);
	while (1) {
		err = snd_timer_query_next_device(handle, id);
		if (err < 0)
			goto _err;
		if (snd_timer_id_get_class(id) < 0)
			break;
		if (snd_timer_id_get_class(id) == SND_TIMER_CLASS_PCM)
			continue;
		snd_timer_ginfo_set_tid(info, id);
		err = snd_timer_query_info(handle, info);
		if (err < 0)
			continue;
		sprintf(name, "hw:CLASS=%i,SCLASS=%i,CARD=%i,DEV=%i,SUBDEV=%i",
			snd_timer_id_get_class(id),
			snd_timer_id_get_sclass(id),
			snd_timer_id_get_card(id),
			snd_timer_id_get_device(id),
			snd_timer_id_get_subdevice(id));
		dname = snd_timer_ginfo_get_name(info);
		err = add_entry(config, name, "Physical Device", NULL, dname);
		if (err < 0)
			goto _err;
	}
	err = 0;
       _err:
	snd_timer_query_close(handle);
	return err;
}

static int probe_timer(snd_config_t *config)
{
	int err;
	snd_config_t *c;

	err = snd_config_make_compound(&c, "timer", 0);
	if (err < 0)
		return err;
	err = snd_config_add(config, c);
	if (err < 0)
		return err;
	err = probe_timers(c);
	if (err < 0)
		return err;
	return 0;
}

static int probe_seq(snd_config_t *config)
{
	int err;
	snd_config_t *c;

	err = snd_config_make_compound(&c, "seq", 0);
	if (err < 0)
		return err;
	err = snd_config_add(config, c);
	if (err < 0)
		return err;
	err = add_entry(c, "default", "Default Device", "Duplex", "Sequencer");
	if (err < 0)
		return err;
	err = add_entry(c, "hw", "Physical Device", "Duplex", "Sequencer");
	if (err < 0)
		return err;
	return 0;
}

typedef int (probe_fcn)(snd_config_t *config);

static probe_fcn * probes[] = {
	probe_ctl,
	probe_pcm,
	probe_rawmidi,
	probe_timer,
	probe_seq,
	NULL
};

int generate_names(const char *cfgfile)
{
	int err, idx;
	snd_config_t *config;
	snd_output_t *out;
	int stdio, ok = 0;

	err = snd_config_top(&config);
	if (err < 0) {
		error("snd_config_top error: %s", snd_strerror(err));
		return err;
	}
	for (idx = 0; probes[idx]; idx++) {
		globidx = 1;
		err = (probes[idx])(config);
		if (err < 0) {
			/* ignore -ENOTTY indicating the non-existing component */
			if (err != -ENOTTY)
				error("probe %i failed: %s", idx, snd_strerror(err));
		} else {
			ok++;
		}
	}
	if (ok > 0) {
		stdio = !strcmp(cfgfile, "-");
		if (stdio) {
			err = snd_output_stdio_attach(&out, stdout, 0);
		} else {
			err = snd_output_stdio_open(&out, cfgfile, "w+");
		}
		if (err < 0) {
			error("Cannot open %s for writing", cfgfile);
			return -errno;
		}
		err = snd_config_save(config, out);
		snd_output_close(out);
		if (err < 0)
			error("snd_config_save: %s", snd_strerror(err));
	} else {
		return -ENOENT;
	}
	return 0;
}
