/*
 *  Advanced Linux Sound Architecture Control Program - Wait for Boot
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
#include "version.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <alsa/asoundlib.h>
#include "alsactl.h"

/**
 * \brief Wait for card boot synchronization using Boot control element
 * \param timeout Maximum wait time in seconds
 * \param cardno Card number
 * \return 0 on success, negative error code on failure
 *
 * This function waits until the card releases the 'waiting' state (UCM).
 * It monitors the '.Boot' control element and uses snd_ctl_wait() and
 * snd_ctl_read() for event-based waiting, similar to boot_wait() in
 * ../alsa-lib/alsa-lib/src/ucm/main.c
 */
int wait_for_card(long long timeout, int cardno)
{
	snd_ctl_t *handle;
	snd_ctl_event_t *event;
	snd_ctl_elem_id_t *id;
	snd_ctl_elem_info_t *info;
	snd_ctl_elem_value_t *value;
	long long boot_time_val, restore_time_val;
	long long synctime = -1;
	struct timespec start_time, now;
	char name[32];
	int err;

	sprintf(name, "hw:%d", cardno);
	err = snd_ctl_open(&handle, name, SND_CTL_READONLY);
	if (err < 0) {
		error("snd_ctl_open error for %s: %s", name, snd_strerror(err));
		return err;
	}

	/* Try to get synctime from boot_synctime in group configuration */
	if (timeout <= 0) {
		bool valid = false, restored = false;
		err = check_boot_params_validity(handle, cardno, NULL, &valid, NULL, &restored, NULL, &synctime);
		if (err == 0 && synctime > 0) {
			timeout = synctime;
			dbg("Using boot_synctime value: %lld seconds", timeout);
		} else {
			timeout = DEFAULT_SYNC_TIME;
		}
		/* Break early if boot params are invalid or already restored */
		if (!valid || restored) {
			dbg("Boot params check: valid=%d, restored=%d - skipping wait", valid, restored);
			snd_ctl_close(handle);
			return 0;
		}
	}

	snd_ctl_event_alloca(&event);
	snd_ctl_elem_id_alloca(&id);
	snd_ctl_elem_info_alloca(&info);
	snd_ctl_elem_value_alloca(&value);

	snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_CARD);
	snd_ctl_elem_id_set_name(id, ".Boot");

	snd_ctl_elem_info_set_id(info, id);
	err = snd_ctl_elem_info(handle, info);
	if (err < 0) {
		dbg("Boot control element not present on card %d, skipping wait", cardno);
		snd_ctl_close(handle);
		return 0;	/* No Boot element, no wait needed */
	}

	if (snd_ctl_elem_info_get_type(info) != SND_CTL_ELEM_TYPE_INTEGER64) {
		error("Boot control element is not INTEGER64 type on card %d", cardno);
		snd_ctl_close(handle);
		return -EINVAL;
	}

	if (snd_ctl_elem_info_get_count(info) < 3) {
		error("Boot control element does not have count >= 3 on card %d", cardno);
		snd_ctl_close(handle);
		return -EINVAL;
	}

	err = snd_ctl_subscribe_events(handle, 1);
	if (err < 0) {
		error("Cannot subscribe to control events: %s", snd_strerror(err));
		snd_ctl_close(handle);
		return err;
	}

	clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);

	dbg("Waiting for card %d to become ready (timeout=%lld seconds)", cardno, timeout);

	while (1) {
		long long diff, remaining = 0;
		long long sync_time_val = -1;

		clock_gettime(CLOCK_MONOTONIC_RAW, &now);

		/* Read current Boot control values */
		err = read_boot_params(handle, &boot_time_val, &sync_time_val, &restore_time_val, NULL);
		if (err < 0) {
			error("Failed to read Boot control element: %s", snd_strerror(err));
			goto _fin;
		}

		dbg("Boot info: boot_time=%lld, sync_time=%lld, restore_time=%lld", boot_time_val, sync_time_val, restore_time_val);

		if (restore_time_val > 0) {
			diff = now.tv_sec - restore_time_val;
			dbg("Controls already restored (diff=%lld seconds), card is ready", diff);
			err = 0;
			goto _fin;
		}

		/* note that realtime may differ from monotonic time, add one second for safety */
		diff = now.tv_sec - start_time.tv_sec;
		if (diff > timeout + 1) {
			dbg("Maximum wait time exceeded (%lld >= %lld seconds), proceeding", diff, timeout);
			break;
		}

		remaining = timeout - diff;

		/* Use synctime from element to limit timeout if available */
		if (sync_time_val > 0 && sync_time_val < timeout) {
			dbg("Limiting timeout from %lld to sync_time %lld seconds", timeout, sync_time_val);
			timeout = sync_time_val;
		}

		if (!validate_boot_time(boot_time_val, now.tv_sec, timeout)) {
			if (boot_time_val > 0) {
				diff = now.tv_sec - boot_time_val;
				dbg("Boot timeout reached (%lld >= %lld seconds), proceeding", diff, timeout);
			}
			break;
		}

		diff = now.tv_sec - boot_time_val;
		if (timeout - diff < remaining)
			remaining = timeout - diff;
		if (remaining <= 0)
			remaining = 1;

		dbg("Waiting %lld seconds", remaining);
		err = snd_ctl_wait(handle, remaining * 1000);
		if (err < 0) {
			error("snd_ctl_wait failed: %s", snd_strerror(err));
			goto _fin;
		}

		if (err == 0)
			continue;	/* Timeout, no events */

		/* Read and check events */
		while (snd_ctl_read(handle, event) > 0) {
			if (!(snd_ctl_event_elem_get_mask(event) & SND_CTL_EVENT_MASK_VALUE))
				continue;	/* Not a value change event */

			if (snd_ctl_event_elem_get_interface(event) != SND_CTL_ELEM_IFACE_CARD ||
			    snd_ctl_event_elem_get_index(event) != 0 ||
			    strcmp(snd_ctl_event_elem_get_name(event), ".Boot") != 0)
				continue;

			dbg("Boot control element value changed");
			break;
		}
	}

	err = 0;
_fin:
	snd_ctl_subscribe_events(handle, 0);
	snd_ctl_close(handle);
	return err;
}
