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

struct ctl_control {
	int change;
	snd_control_type_t type;
	snd_control_info_t info;
	snd_control_t c;
	struct ctl_control *next;
};

struct ctl {
	snd_ctl_hw_info_t hwinfo;
	struct ctl_control *controls;
};

struct soundcard {
	int no;			/* card number */
	struct ctl control;
	struct soundcard *next;
};

extern struct soundcard *soundcards;
extern struct soundcard *rsoundcards;	/* read soundcards */

void soundcard_setup_init(void);
void soundcard_setup_done(void);
int soundcard_setup_load(const char *filename, int skip);
int soundcard_setup_write(const char *filename, int cardno);
int soundcard_setup_collect_controls(int cardno);
int soundcard_setup_merge_controls(int cardno);
int soundcard_setup_process_controls(int cardno);

char *control_id(snd_control_id_t *id);
