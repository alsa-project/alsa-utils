/*
 *  Advanced Linux Sound Architecture Control Program
 *  Copyright (c) Jaroslav Kysela <perex@suse.cz>
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


static int get_int_state(const char *str)
{
	if (!strcasecmp(str, "on"))
		return SND_CTL_POWER_D0;
	if (!strcasecmp(str, "off"))
		return SND_CTL_POWER_D3hot;
	if (*str == 'D' || *str == 'd') {
		str++;
		if (!strcmp(str, "0"))
			return SND_CTL_POWER_D0;
		if (!strcmp(str, "1"))
			return SND_CTL_POWER_D1;
		if (!strcmp(str, "2"))
			return SND_CTL_POWER_D2;
		if (!strcmp(str, "3"))
			return SND_CTL_POWER_D3;
		if (!strcmp(str, "3hot"))
			return SND_CTL_POWER_D3hot;
		if (!strcmp(str, "3cold"))
			return SND_CTL_POWER_D3cold;
	}
	return -1;
}

static const char *get_str_state(int power_state)
{
	static char str[16];

	switch (power_state) {
	case SND_CTL_POWER_D0:
		return "D0";
	case SND_CTL_POWER_D1:
		return "D1";
	case SND_CTL_POWER_D2:
		return "D2";
	// return SND_CTL_POWER_D3;	/* it's same as D3hot */
	case SND_CTL_POWER_D3hot:
		return "D3hot";
	case SND_CTL_POWER_D3cold:
		return "D3cold";
	default:
		sprintf(str, "???0x%x", power_state);
		return str;
	}
}

static int show_power(int cardno)
{
	snd_ctl_t *handle;
	char name[16];
	unsigned int power_state;
	int err;

	sprintf(name, "hw:%d", cardno);
	err = snd_ctl_open(&handle, name, 0);
	if (err < 0) {
		error("snd_ctl_open error: %s", snd_strerror(err));
		return err;
	}
	err = snd_ctl_get_power_state(handle, &power_state);
	if (err < 0) {
		error("snd_ctl_get_power_state error: %s", snd_strerror(err));
		snd_ctl_close(handle);
		return err;
	}
	snd_ctl_close(handle);
	printf("Power state for card #%d is %s\n", cardno, get_str_state(power_state));
	return 0;
}

static int set_power(int cardno, unsigned int power_state)
{
	snd_ctl_t *handle;
	char name[16];
	int err;

	sprintf(name, "hw:%d", cardno);
	err = snd_ctl_open(&handle, name, 0);
	if (err < 0) {
		error("snd_ctl_open error: %s", snd_strerror(err));
		return err;
	}
	err = snd_ctl_set_power_state(handle, power_state);
	if (err < 0) {
		error("snd_ctl_set_power_state error: %s", snd_strerror(err));
		snd_ctl_close(handle);
		return err;
	}
	err = snd_ctl_get_power_state(handle, &power_state);
	if (err < 0) {
		error("snd_ctl_get_power_state error: %s", snd_strerror(err));
		snd_ctl_close(handle);
		return err;
	}
	snd_ctl_close(handle);
	printf("Power state for card #%d is %s\n", cardno, get_str_state(power_state));
	return 0;
}


int power(const char *argv[], int argc)
{
	int power_state, err;
	
	if (argc == 0) {		/* show status only */
		int card, first = 1;

		card = -1;
		/* find each installed soundcards */
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
			if ((err = show_power(card)) < 0)
				return err;
		}
		return 0;
	}
	power_state = get_int_state(argv[0]);
	if (power_state >= 0) {
		int card, first = 1;

		card = -1;
		/* find each installed soundcards */
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
			if ((err = set_power(card, power_state)))
				return err;
		}
	} else {
		int cardno;

		cardno = snd_card_get_index(argv[0]);
		if (cardno < 0) {
			error("Cannot find soundcard '%s'...", argv[0]);
			return -ENODEV;
		}
		if (argc > 1) {
			power_state = get_int_state(argv[1]);
			if (power_state < 0) {
				error("Invalid power state '%s'...", argv[1]);
				return -EINVAL;
			}
			if ((err = set_power(cardno, power_state)) < 0)
				return err;
		} else {
			if ((err = show_power(cardno)) < 0)
				return err;
		}
	}
	return 0;
}
