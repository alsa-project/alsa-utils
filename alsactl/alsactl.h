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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/asoundlib.h>

#define ALSACTL_FILE	"/etc/asound.conf"

#define LEFT 1
#define RIGHT 2

#define OUTPUT 0
#define INPUT 1

extern int debugflag;

extern void error(const char *fmt,...);

struct ctl_switch {
	int change;
	snd_switch_t s;
	struct ctl_switch *next;
};

struct ctl {
	snd_ctl_hw_info_t hwinfo;
	struct ctl_switch *switches;
};

struct mixer_element {
	snd_mixer_element_info_t info;
	snd_mixer_element_t element;
	struct mixer_element *next;
};

struct mixer {
	int no;
	snd_mixer_info_t info;
	struct mixer_element *elements;
	struct ctl_switch *switches;
	struct mixer *next;
};

struct pcm {
	int no;
	snd_pcm_info_t info;
	struct ctl_switch *pswitches;
	struct ctl_switch *rswitches;
	struct pcm *next;
};

struct rawmidi {
	int no;
	snd_rawmidi_info_t info;
	struct ctl_switch *iswitches;
	struct ctl_switch *oswitches;
	struct rawmidi *next;
};

struct soundcard {
	int no;			/* card number */
	struct ctl control;
	struct mixer *mixers;
	struct pcm *pcms;
	struct rawmidi *rawmidis;
	struct soundcard *next;
};

extern struct soundcard *soundcards;
extern struct soundcard *rsoundcards;	/* read soundcards */

void soundcard_setup_init(void);
void soundcard_setup_done(void);
int soundcard_setup_load(const char *filename, int skip);
int soundcard_setup_write(const char *filename, int cardno);
int soundcard_setup_collect_switches(int cardno);
int soundcard_setup_collect_data(int cardno);
int soundcard_setup_merge_switches(int cardno);
int soundcard_setup_merge_data(int cardno);
int soundcard_setup_process_switches(int cardno);
int soundcard_setup_process_data(int cardno);

char *mixer_element_id(snd_mixer_eid_t *eid);
