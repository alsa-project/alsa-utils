/*
 *  Advanced Linux Sound Architecture Control Program - UCM Initialization
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
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include "alsactl.h"

#ifdef HAVE_ALSA_USE_CASE_H

#include <alsa/use-case.h>

#define DEFAULT_SYNC_TIME 20

/*
 * Helper: Check if card should skip initialization based on boot parameters
 * Returns: 1 if should skip, 2 if should skip other card, 0 if should continue, negative on error
 * If check_restored is true, also checks if card state is already restored
 */
static int should_skip_initialization(snd_ctl_t *ctl, int cardno, int flags,
				      char **boot_card_group, bool *valid,
				      bool *in_sync, bool *restored, int *primary_card,
				      long long *synctime)
{
	int err;

	err = check_boot_params_validity(ctl, cardno, boot_card_group, valid, in_sync, restored, primary_card, synctime);
	if (err < 0) {
		dbg("boot parameters validity failed: %s", snd_strerror(err));
		return err;
	}

	/* do nothing for other cards in group */
	if (*valid && *primary_card != cardno) {
		dbg("Skipping card %d - not primary (primary is %d)", cardno, *primary_card);
		return CARD_STATE_SKIP;
	}

	/* for immediate initialization, caller must set UCM force-restore flag */
	if (*valid && *in_sync && (flags & FLAG_UCM_RESTORE) == 0) {
		dbg("Skipping card %d - in sync and no force-restore flag", cardno);
		return CARD_STATE_WAIT;
	}
	return 0;
}

/*
 * Helper: Get boot card group configuration from UCM
 * Returns: 0 on success, negative on error
 */
static int get_boot_card_group_config(snd_use_case_mgr_t *uc_mgr, char **boot_card_group, long long *synctime)
{
	char *sync_time = NULL;
	int err;

	err = snd_use_case_get(uc_mgr, "=BootCardGroup", (const char **)boot_card_group);
	if (err != 0 || *boot_card_group == NULL) {
		return -ENOENT;
	}

	dbg("BootCardGroup found: %s", *boot_card_group);

	/* Get optional sync time */
	err = snd_use_case_get(uc_mgr, "=BootCardSyncTime", (const char **)&sync_time);
	if (err == 0 && sync_time != NULL) {
		char *endptr;
		errno = 0;
		*synctime = strtoll(sync_time, &endptr, 10);
		if (errno != 0 || *endptr != '\0' || endptr == sync_time) {
			error("Invalid BootCardSyncTime value '%s'", sync_time);
			*synctime = DEFAULT_SYNC_TIME;
		}
		free(sync_time);
	}

	return 0;
}

/*
 * Helper: Open UCM manager with appropriate flags
 * Returns: 0 on success, negative on error
 */
static int open_ucm_manager(snd_use_case_mgr_t **uc_mgr, int cardno, int flags,
			    bool valid, bool fixed_boot)
{
	char id[64], *nodev, *in_boot;
	int err;

	nodev = (flags & FLAG_UCM_NODEV) ? "" : "-";
	in_boot = (valid || !fixed_boot) ? "" : "<<<InBoot=1>>>";
	snprintf(id, sizeof(id), "%s%shw:%d", nodev, in_boot, cardno);

	err = snd_use_case_mgr_open(uc_mgr, id);
	dbg("ucm open '%s': %d", id, err);

	return err;
}

/*
 * Helper: Reopen UCM manager without InBoot flag
 * Returns: 0 on success, negative on error
 */
static int reopen_ucm_manager(snd_use_case_mgr_t **uc_mgr, int cardno, int flags)
{
	char id[64], *nodev;
	int err;

	snd_use_case_mgr_close(*uc_mgr);

	nodev = (flags & FLAG_UCM_NODEV) ? "" : "-";
	snprintf(id, sizeof(id), "%shw:%d", nodev, cardno);

	err = snd_use_case_mgr_open(uc_mgr, id);
	dbg("ucm reopen '%s': %d", id, err);

	return err;
}

/*
 * Helper: Execute boot sequences
 * Returns: 0 on success, negative on error
 */
static int execute_boot_sequences(snd_use_case_mgr_t *uc_mgr, int flags, bool fixed_boot)
{
	int err = 0;

	if (fixed_boot) {
		err = snd_use_case_set(uc_mgr, "_fboot", NULL);
		dbg("ucm _fboot: %d", err);
		if (err == -ENOENT && (flags & FLAG_UCM_BOOT) != 0) {
			/* _fboot not found but _boot requested - continue */
			err = 0;
		} else if (err < 0) {
			return err;
		}
	}

	if (flags & FLAG_UCM_BOOT) {
		err = snd_use_case_set(uc_mgr, "_boot", NULL);
		dbg("ucm _boot: %d", err);
		if (err < 0)
			return err;

		if ((flags & FLAG_UCM_DEFAULTS) != 0)
			err = snd_use_case_set(uc_mgr, "_defaults", NULL);
	}

	return err;
}

/*
 * Execute commands from the FixedBootSequence and BootSequence.
 * Handle also card groups.
 * Returns: 0 = success, 1 = skip this card (e.g. linked or in-sync), negative on error
 */
int init_ucm(int flags, int cardno)
{
	snd_use_case_mgr_t *uc_mgr;
	char id[64];
	char *boot_card_group = NULL, *boot_card_group_verify = NULL;
	bool fixed_boot, valid = false, in_sync = false, restored = false;
	snd_ctl_t *ctl = NULL;
	int err, primary_card = -1, lock_fd = -1;
	long long synctime = -1;

	if (flags & FLAG_UCM_DISABLED) {
		dbg("ucm disabled");
		return -ENXIO;
	}

	fixed_boot = (flags & FLAG_UCM_FBOOT) != 0;

	snprintf(id, sizeof(id), "hw:%d", cardno);
	err = snd_ctl_open(&ctl, id, 0);
	if (err < 0) {
		dbg("UCM: unable to open control device '%s': %s", id, snd_strerror(err));
		return err;
	}

	err = should_skip_initialization(ctl, cardno, flags, &boot_card_group, &valid,
					 &in_sync, &restored, &primary_card, &synctime);
	if (err != 0)
		goto _fin;

	if (valid) {
		if (restored) {
			err = CARD_STATE_RESTORED;
			goto _fin;
		}
		lock_fd = group_state_lock(groupfile, LOCK_TIMEOUT);
		if (lock_fd < 0) {
			err = lock_fd;
			goto _fin;
		}
	}

	err = open_ucm_manager(&uc_mgr, cardno, flags, valid, fixed_boot);
	if (err < 0)
		goto _fin;

	if (!fixed_boot)
		goto _execute_boot;

	if (!valid) {
		err = get_boot_card_group_config(uc_mgr, &boot_card_group, &synctime);
		if (err == -ENOENT) {
			/* No BootCardGroup - remove any existing boot params */
			err = boot_params_remove_card(cardno);
			if (err < 0)
				goto _error;
			goto _execute_boot;
		} else if (err < 0) {
			goto _error;
		}

		if (lock_fd < 0) {
			lock_fd = group_state_lock(groupfile, LOCK_TIMEOUT);
			if (lock_fd < 0) {
				err = lock_fd;
				goto _error;
			}
		}

		err = should_skip_initialization(ctl, cardno, flags, &boot_card_group_verify,
						 &valid, &in_sync, &restored, &primary_card, &synctime);
		if (err != 0)
			goto _error;

		if (valid && (boot_card_group_verify == NULL || strcmp(boot_card_group_verify, boot_card_group) != 0)) {
			dbg("expected different boot card group (got '%s', expected '%s')", boot_card_group_verify, boot_card_group);
			err = -EINVAL;
			goto _error;
		}

		if ((flags & FLAG_UCM_RESTORE) == 0 && (!valid || restored)) {
			dbg("Skipping card %d (group '%s') - %s and no force-restore flag", cardno, boot_card_group,
				!valid ? "validity not passed" : "already restored");
			if (!valid) {
				/* create initial 'Boot' element */
				err = update_boot_params(ctl, cardno, boot_card_group, 0, restored, synctime);
				if (err < 0)
					goto _error;
			}
			err = restored ? CARD_STATE_RESTORED : CARD_STATE_WAIT;
			goto _error;
		}

		err = reopen_ucm_manager(&uc_mgr, cardno, flags);
		if (err < 0)
			goto _fin;
	}

_execute_boot:
	if (flags & FLAG_UCM_FBOOT)
		restored = true;

	if (boot_card_group) {
		err = update_boot_params(ctl, cardno, boot_card_group, valid, restored, synctime);
		if (err < 0)
			goto _error;
	}

	err = execute_boot_sequences(uc_mgr, flags, fixed_boot);
	if (err < 0)
		goto _error;

	err = 0;

_error:
	snd_use_case_mgr_close(uc_mgr);
_fin:
	if (lock_fd >= 0)
		group_state_unlock(lock_fd, groupfile);
	if (ctl)
		snd_ctl_close(ctl);
	free(boot_card_group);
	free(boot_card_group_verify);
	dbg("ucm init complete %d", err);
	return err;
}

#else

int init_ucm(int flags, int cardno)
{
	return -ENXIO;
}

#endif
