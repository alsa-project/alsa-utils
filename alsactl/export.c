/*
 *  Advanced Linux Sound Architecture Control Program - Export
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

/**
 * Export variable syntax:
 *
 * For single card:
 *
 * ALSA_CARD_NUMBER=<number>
 * ALSA_CARD_STATE=<state>
 *
 * For multiple cards:
 *
 * ALSA_CARD#_STATE=<state>		# where # is replaced with the card number
 *
 * State list:
 *
 * active				# card was initialized and active
 * skip					# card was skipped (other card in group)
 * waiting				# card is waiting for the initial configuration
 */

#include "aconfig.h"
#include "version.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alsactl.h"

/* Global array to store card states (0 = active, 1 = waiting) */
static int export_card_state[32];

/**
 * export_card_state_set - Set card state
 * @card: Card number
 * @state: Card state (0 = active, 1 = waiting)
 *
 * Returns 0 on success, negative error code on failure
 */
int export_card_state_set(int card, int state)
{
	/* Check bounds */
	if (card < 0 || (unsigned long)card >= ARRAY_SIZE(export_card_state))
		return -EINVAL;

	export_card_state[card] = state;
	return 0;
}

/**
 * export_card_state_print - Export and print card state information
 * @iter: Card iterator containing current card information
 *
 * Prints key=value pairs based on iterator's single flag and export_card_state array.
 * Returns 0 on success, negative error code on failure
 */
static int export_card_state_print(struct snd_card_iterator *iter)
{
	const char *state;
	int istate;

	if (!iter)
		return -EINVAL;

	/* Check bounds */
	if (iter->card < 0 || (unsigned long)iter->card >= ARRAY_SIZE(export_card_state))
		return -EINVAL;

	/* Determine state from export_card_state array */
	istate = export_card_state[iter->card];
	switch (istate) {
	case CARD_STATE_WAIT: state = "waiting"; break;
	case CARD_STATE_SKIP: state = "skip"; break;
	default: state = "active"; break;
	}

	if (iter->single) {
		/* Single card export format */
		printf("ALSA_CARD_NUMBER=%d\n", iter->card);
		printf("ALSA_CARD_STATE=%s\n", state);
	} else {
		/* Multiple cards export format */
		printf("ALSA_CARD%d_STATE=%s\n", iter->card, state);
	}

	return 0;
}

/**
 * export_cards - Export state for all cards
 * @cardname: Card name or NULL for all cards
 *
 * Returns 0 on success, negative error code on failure
 */
int export_cards(const char *cardname)
{
	struct snd_card_iterator iter;
	const char *name;
	int ret;

	ret = snd_card_iterator_sinit(&iter, cardname);
	if (ret < 0)
		return ret;

	while ((name = snd_card_iterator_next(&iter)) != NULL) {
		ret = export_card_state_print(&iter);
		if (ret < 0)
			break;
	}

	if (ret == 0)
		ret = snd_card_iterator_error(&iter);

	return ret;
}
