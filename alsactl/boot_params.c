/*
 *  Advanced Linux Sound Architecture Control Program - Boot Parameters
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
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include "alsactl.h"

/*
 * Validate boot time
 * Returns: true if boot_time is valid and within synchronization time, false otherwise
 */
bool validate_boot_time(long long boot_time, long long current_time, long long synctime)
{
	long long diff;

	if (boot_time <= 0)
		return false;

	diff = current_time - boot_time;
	if (diff < 0) {
		/* boot_time is in the future - invalid */
		return false;
	}

	if (synctime > 0 && diff >= synctime) {
		/* boot_time has exceeded timeout - invalid */
		return false;
	}

	return true;
}

/*
 * Read boot parameters from the '.Boot' control element
 * Returns: 0 on success, negative error code on failure
 */
int read_boot_params(snd_ctl_t *handle, long long *boot_time, long long *sync_time,
		     long long *restore_time, long long *primary_card)
{
	snd_ctl_elem_id_t *id;
	snd_ctl_elem_value_t *value;
	snd_ctl_elem_info_t *info;
	int err;

	if (boot_time)
		*boot_time = -1;
	if (sync_time)
		*sync_time = -1;
	if (restore_time)
		*restore_time = -1;
	if (primary_card)
		*primary_card = -1;

	snd_ctl_elem_id_alloca(&id);
	snd_ctl_elem_value_alloca(&value);
	snd_ctl_elem_info_alloca(&info);

	snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_CARD);
	snd_ctl_elem_id_set_name(id, ".Boot");
	snd_ctl_elem_id_set_index(id, 0);

	snd_ctl_elem_info_set_id(info, id);
	err = snd_ctl_elem_info(handle, info);
	if (err < 0) {
		if (err == -ENOENT)
			return 0;
		error("Cannot read '.Boot' control info: %s", snd_strerror(err));
		return err;
	}

	if (snd_ctl_elem_info_get_type(info) != SND_CTL_ELEM_TYPE_INTEGER64) {
		error("'.Boot' control element is not of type INTEGER64");
		return -EINVAL;
	}

	if (snd_ctl_elem_info_get_count(info) != 4) {
		error("'.Boot' control element does not have 3 values");
		return -EINVAL;
	}

	snd_ctl_elem_value_set_id(value, id);
	err = snd_ctl_elem_read(handle, value);
	if (err < 0) {
		error("Cannot read '.Boot' control: %s", snd_strerror(err));
		return err;
	}

	dbg("Read boot params: boot_time=%lld sync_time=%lld restore_time=%lld primary_card=%lld",
			snd_ctl_elem_value_get_integer64(value, 0),
			snd_ctl_elem_value_get_integer64(value, 1),
			snd_ctl_elem_value_get_integer64(value, 2),
			snd_ctl_elem_value_get_integer64(value, 3));

	if (boot_time)
		*boot_time = snd_ctl_elem_value_get_integer64(value, 0);
	if (sync_time)
		*sync_time = snd_ctl_elem_value_get_integer64(value, 1);
	if (restore_time)
		*restore_time = snd_ctl_elem_value_get_integer64(value, 2);
	if (primary_card)
		*primary_card = snd_ctl_elem_value_get_integer64(value, 3);

	return 0;
}

/*
 * Write boot parameters to the '.Boot' control element
 * Returns: 0 on success, negative error code on failure
 */
int write_boot_params(snd_ctl_t *handle, long long boot_time, long long sync_time,
		      long long restore_time, long long primary_card)
{
	snd_ctl_elem_id_t *id;
	snd_ctl_elem_value_t *value;
	snd_ctl_elem_info_t *info;
	int err;

	snd_ctl_elem_id_alloca(&id);
	snd_ctl_elem_value_alloca(&value);
	snd_ctl_elem_info_alloca(&info);

	snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_CARD);
	snd_ctl_elem_id_set_name(id, ".Boot");
	snd_ctl_elem_id_set_index(id, 0);

	snd_ctl_elem_info_set_id(info, id);
	err = snd_ctl_elem_info(handle, info);
	if (err < 0) {
		if (err == -ENOENT) {
			/* Element not found, create a new user element with 3 integer64 values */
			dbg("'.Boot' control not found, creating new user element");
			/* Do not save this element to the state file */
			snd_ctl_elem_info_set_inactive(info, 1);
			snd_ctl_elem_info_set_read_write(info, 1, 1);
			err = snd_ctl_add_integer64_elem_set(handle, info, 1, 4, -1, LLONG_MAX, 0);
			if (err < 0) {
				error("Cannot create '.Boot' user element: %s", snd_strerror(err));
				return err;
			}
			/* Re-read the element info after creation */
			err = snd_ctl_elem_info(handle, info);
			if (err < 0) {
				error("Cannot read '.Boot' control info after creation: %s", snd_strerror(err));
				return err;
			}
		} else {
			error("Cannot read '.Boot' control info: %s", snd_strerror(err));
			return err;
		}
	}

	dbg("Write boot params: boot_time=%lld sync_time=%lld restore_time=%lld primary_card=%lld",
			boot_time, sync_time, restore_time, primary_card);

	if (snd_ctl_elem_info_get_type(info) != SND_CTL_ELEM_TYPE_INTEGER64) {
		error("'.Boot' control element is not of type INTEGER64");
		return -EINVAL;
	}

	if (snd_ctl_elem_info_get_count(info) != 4) {
		error("'.Boot' control element does not have 3 values");
		return -EINVAL;
	}

	snd_ctl_elem_value_set_id(value, id);
	snd_ctl_elem_value_set_integer64(value, 0, boot_time);
	snd_ctl_elem_value_set_integer64(value, 1, sync_time);
	snd_ctl_elem_value_set_integer64(value, 2, restore_time);
	snd_ctl_elem_value_set_integer64(value, 3, primary_card);

	err = snd_ctl_elem_write(handle, value);
	if (err < 0) {
		error("Cannot write '.Boot' control: %s", snd_strerror(err));
		return err;
	}

	return 0;
}

/*
 * Structure for the group configuration file:
 *
 * <GROUP_NAME> {
 *     card.0 <int>             # primary card in group
 *     card.1 <int>             # optional - next card in group
 *     card.2 <int>             # optional - next card in group
 *     boot_realtime <int64>    # boot time (CLOCK_REALTIME) in seconds
 *     boot_last_update <int64> # timestamp of last configuration update (CLOCK_REALTIME) in seconds
 *     boot_monotonic <int64>   # boot time (CLOCK_MONOTONIC_RAW) in seconds
 *     boot_synctime <int64>    # synchronization time window in seconds
 * }
 */

/*
 * Read card group configuration from file
 * Returns: 0 on success, negative error code on failure
 */
int card_group_load(snd_config_t **config)
{
	snd_input_t *in;
	int err;

	if (!config)
		return -EINVAL;

	*config = NULL;

	err = snd_config_top(config);
	if (err < 0) {
		error("Cannot create top config: %s", snd_strerror(err));
		return err;
	}

	err = snd_input_stdio_open(&in, groupfile, "r");
	if (err < 0) {
		if (err == -ENOENT) {
			dbg("Card group file '%s' not found", groupfile);
			return 0;
		}
		error("Cannot open card group file '%s' for reading: %s", groupfile, snd_strerror(err));
		goto _err;
	}

	err = snd_config_load(*config, in);
	snd_input_close(in);
	if (err < 0) {
		error("Cannot load card group file '%s': %s", groupfile, snd_strerror(err));
		goto _err;
	}

	return 0;

_err:
	snd_config_delete(*config);
	*config = NULL;
	return err;
}

/*
 * Write card group configuration to file
 * Returns: 0 on success, negative error code on failure
 */
int card_group_save(snd_config_t *config)
{
	snd_output_t *out;
	char temp_file[PATH_MAX];
	int err;

	if (!config)
		return -EINVAL;

	snprintf(temp_file, sizeof(temp_file), "%s.new", groupfile);

	err = snd_output_stdio_open(&out, temp_file, "w");
	if (err < 0) {
		error("Cannot open temporary card group file '%s' for writing: %s", temp_file, snd_strerror(err));
		return err;
	}

	err = snd_config_save(config, out);
	snd_output_close(out);
	if (err < 0) {
		error("Cannot save temporary card group file '%s': %s", temp_file, snd_strerror(err));
		return err;
	}

	err = rename(temp_file, groupfile);
	if (err < 0) {
		err = -errno;
		error("Cannot rename temporary card group file '%s' to '%s': %s", temp_file, groupfile, strerror(errno));
		return err;
	}

	return 0;
}

/*
 * Get int64 value from card group configuration
 * Returns: 0 on success, negative error code on failure
 */
int card_group_get_int64(snd_config_t *config_group, const char *id, long long *val)
{
	snd_config_t *node;
	int err;

	if (!config_group || !id || !val)
		return -EINVAL;

	err = snd_config_search(config_group, id, &node);
	if (err < 0)
		return err;

	err = snd_config_get_integer64(node, val);
	if (err < 0) {
		long ival;
		err = snd_config_get_integer(node, &ival);
		if (err < 0)
			return err;
		*val = ival;
	}

	return 0;
}

/*
 * Set int64 value in card group configuration
 * Returns: 0 on success, negative error code on failure
 */
int card_group_set_int64(snd_config_t *config_group, const char *id, long long val)
{
	snd_config_t *node;
	int err;

	if (!config_group || !id)
		return -EINVAL;

	err = snd_config_search(config_group, id, &node);
	if (err < 0) {
_create:
		err = snd_config_make_integer64(&node, id);
		if (err < 0) {
			error("Cannot create int64 node for id '%s': %s", id, snd_strerror(err));
			return err;
		}
		err = snd_config_add(config_group, node);
		if (err < 0) {
			error("Cannot add int64 node for id '%s': %s", id, snd_strerror(err));
			snd_config_delete(node);
			return err;
		}
	} else {
		/* alsa-lib should implement automatic type conversion */
		if (snd_config_get_type(node) == SND_CONFIG_TYPE_INTEGER) {
			snd_config_delete(node);
			goto _create;
		}
	}

	err = snd_config_set_integer64(node, val);
	if (err < 0) {
		error("Cannot set int64 value for id '%s': %s", id, snd_strerror(err));
		return err;
	}

	return 0;
}

/*
 * Helper: Find or create card compound within a group
 * Returns: 0 on success, negative error code on failure
 */
static int card_group_get_or_create_card_compound(snd_config_t *config_group, snd_config_t **card_compound)
{
	int err;

	if (!config_group || !card_compound)
		return -EINVAL;

	err = snd_config_search(config_group, "card", card_compound);
	if (err < 0) {
		/* Create card compound */
		err = snd_config_make_compound(card_compound, "card", 0);
		if (err < 0) {
			error("Cannot create card compound: %s", snd_strerror(err));
			return err;
		}
		err = snd_config_add(config_group, *card_compound);
		if (err < 0) {
			error("Cannot add card compound: %s", snd_strerror(err));
			snd_config_delete(*card_compound);
			return err;
		}
	}

	return 0;
}

/*
 * Helper: Determine the primary card in the card compound
 * Returns: card number, otherwise error code
 */
static long card_group_find_primary(snd_config_t *card_compound)
{
	snd_config_iterator_t i, next;

	if (!card_compound)
		return -EINVAL;

	if (snd_config_get_type(card_compound) != SND_CONFIG_TYPE_COMPOUND)
		return -EINVAL;

	snd_config_for_each(i, next, card_compound) {
		snd_config_t *card_node = snd_config_iterator_entry(i);
		long card_val;
		int err;

		err = snd_config_get_integer(card_node, &card_val);
		if (err < 0)
			return -EINVAL;

		return card_val;
	}

	return -ENOENT;
}

/*
 * Helper: Find card node in card compound
 * Returns: card node if found, NULL otherwise
 */
static snd_config_t *card_group_find_card_node(snd_config_t *card_compound, int cardno)
{
	snd_config_iterator_t i, next;

	if (!card_compound)
		return NULL;

	if (snd_config_get_type(card_compound) != SND_CONFIG_TYPE_COMPOUND)
		return NULL;

	snd_config_for_each(i, next, card_compound) {
		snd_config_t *card_node = snd_config_iterator_entry(i);
		long card_val;
		int err;

		err = snd_config_get_integer(card_node, &card_val);
		if (err < 0)
			continue;

		if ((int)card_val == cardno)
			return card_node;
	}

	return NULL;
}

/*
 * Helper: Add card to card compound
 * Returns: 0 on success, negative error code on failure
 */
static int card_group_add_card(snd_config_t *card_compound, int cardno)
{
	snd_config_t *new_card_node;
	char card_id[16];
	int card_index = 0;
	int err;

	if (!card_compound)
		return -EINVAL;

	/* Find next available card index */
	while (card_index < 100) {
		snprintf(card_id, sizeof(card_id), "%d", card_index);
		if (snd_config_search(card_compound, card_id, &new_card_node) < 0)
			break;
		card_index++;
	}

	err = snd_config_make_integer(&new_card_node, card_id);
	if (err < 0) {
		error("Cannot create card node: %s", snd_strerror(err));
		return err;
	}

	err = snd_config_set_integer(new_card_node, cardno);
	if (err < 0) {
		error("Cannot set card number: %s", snd_strerror(err));
		snd_config_delete(new_card_node);
		return err;
	}

	err = snd_config_add(card_compound, new_card_node);
	if (err < 0) {
		error("Cannot add card node: %s", snd_strerror(err));
		snd_config_delete(new_card_node);
		return err;
	}

	return 0;
}

/*
 * Check boot parameters validity
 * Returns: 0 on success, negative error code on failure
 */
int check_boot_params_validity(snd_ctl_t *handle, int cardno, char **boot_card_group, bool *valid, bool *in_sync, bool *restored, int *primary_card, long long *synctime)
{
	long long boot_time = -1;
	long long restore_time = -1;
	long long primary_card_val = -1;
	long long boot_synctime = -1;
	snd_config_t *config = NULL;
	snd_config_t *config_group = NULL;
	const char *card_group_name = NULL;
	long long group_boot_realtime = -1;
	long long group_boot_monotonic = -1;
	long long group_boot_synctime = -1;
	struct timespec ts_realtime, ts_monotonic;
	long long diff_realtime, diff_monotonic, diff;
	snd_config_iterator_t i, next;
	int err = 0;
	bool is_valid = false;

	if (valid)
		*valid = false;
	if (in_sync)
		*in_sync = false;
	if (restored)
		*restored = false;
	if (primary_card)
		*primary_card = -1;
	if (boot_card_group)
		*boot_card_group = NULL;

	err = read_boot_params(handle, &boot_time, &boot_synctime, &restore_time, &primary_card_val);
	if (err < 0) {
		dbg("Boot element not present or error reading: %s", snd_strerror(err));
		err = 0;
		goto out;
	}

	if (boot_time <= 0) {
		dbg("boot_time is not greater than zero: %lld", boot_time);
		goto out;
	}

	err = card_group_load(&config);
	if (err < 0) {
		dbg("Error loading card group configuration: %s", snd_strerror(err));
		err = 0;
		goto out;
	}

	if (!config) {
		dbg("No group configuration found");
		goto out;
	}

	/* Find the card number in card groups - prefer group with newest boot_realtime */
	snd_config_for_each(i, next, config) {
		snd_config_t *n = snd_config_iterator_entry(i);
		snd_config_t *card_compound;
		const char *group_id;
		long long current_boot_realtime;

		if (snd_config_get_id(n, &group_id) < 0)
			continue;

		if (snd_config_get_type(n) != SND_CONFIG_TYPE_COMPOUND)
			continue;

		err = snd_config_search(n, "card", &card_compound);
		if (err < 0)
			continue;

		if (!card_group_find_card_node(card_compound, cardno))
			continue;

		err = card_group_get_int64(n, "boot_realtime", &current_boot_realtime);
		if (err < 0) {
			dbg("boot_realtime not found in group '%s', skipping", group_id);
			continue;
		}

		if (current_boot_realtime > group_boot_realtime) {
			group_boot_realtime = current_boot_realtime;
			config_group = n;
			card_group_name = group_id;
		}
	}

	if (!card_group_name) {
		dbg("Card %d not found in any group configuration", cardno);
		err = 0;
		goto out;
	}

	err = card_group_get_int64(config_group, "boot_monotonic", &group_boot_monotonic);
	if (err < 0) {
		dbg("boot_monotonic not found in group '%s'", card_group_name);
		err = 0;
		goto out;
	}

	err = card_group_get_int64(config_group, "boot_synctime", &group_boot_synctime);
	if (err < 0) {
		dbg("boot_synctime not found in group '%s'", card_group_name);
		err = 0;
		goto out;
	}

	if (*synctime > 0 && group_boot_synctime != *synctime) {
		err = -EINVAL;
		error("Synchronization time window does not match (%lld != %lld)", *synctime, group_boot_synctime);
		goto out;
	}

	if (boot_synctime > 0 && group_boot_synctime != boot_synctime) {
		err = -EINVAL;
		error("Element synchronization time window does not match (%lld != %lld)", boot_synctime, group_boot_synctime);
		goto out;
	}

	if (clock_gettime(CLOCK_REALTIME, &ts_realtime) < 0) {
		err = -errno;
		error("Failed to get CLOCK_REALTIME: %s", strerror(errno));
		goto out;
	}

	if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts_monotonic) < 0) {
		err = -errno;
		error("Failed to get CLOCK_MONOTONIC_RAW: %s", strerror(errno));
		goto out;
	}

	diff_monotonic = ts_monotonic.tv_sec - group_boot_monotonic;
	diff_realtime = ts_realtime.tv_sec - group_boot_realtime;
	diff = diff_realtime - diff_monotonic;
	dbg("Card group '%s' sync diffs - %lld, %lld, %lld",
		card_group_name, (long long)diff_monotonic, (long long)diff_realtime, (long long)diff);
	/* if the time difference is too big (30 seconds) - obsolete configuration */
	is_valid = diff < 30 || diff > -30;

	if (valid)
		*valid = is_valid;

	if (is_valid) {
		if (boot_card_group) {
			*boot_card_group = strdup(card_group_name);
			if (!*boot_card_group) {
				err = -ENOMEM;
				goto out;
			}
		}
		if (primary_card) {
			*primary_card = (int)primary_card_val;
			dbg("Card group '%s' primary_card %d", card_group_name, *primary_card);
		}
		if (restored) {
			*restored = restore_time > 0;
			dbg("Card group '%s' restored %d", card_group_name, *restored);
		}
		if (in_sync) {
			*in_sync = ts_realtime.tv_sec - group_boot_realtime < group_boot_synctime;
			dbg("Card group '%s' in_sync %d - %lld, %lld, %lld",
				card_group_name, *in_sync, (long long)ts_realtime.tv_sec,
				(long long)group_boot_realtime, (long long)group_boot_synctime);
		}

		if (synctime)
			*synctime = group_boot_synctime;
	}

out:
	if (config)
		snd_config_delete(config);
	return err;
}

/*
 * Remove card from boot parameters - scans all groups and removes all invalid
 * cards in group containing the card.
 * Returns: 0 = no change, 1 = change (card(s) removed), negative error code on failure
 */
static int boot_params_remove_card_config(snd_config_t *group_config, int cardno)
{
	snd_config_t *card_compound, *card_node;
	snd_config_iterator_t i, next;
	const char *group_id;
	struct timespec ts_monotonic = {0};
	long primary_card, card_val;
	int err, changes;
	bool valid;

	if (snd_config_get_id(group_config, &group_id) < 0)
		return -EINVAL;

	err = snd_config_search(group_config, "card", &card_compound);
	if (err < 0)
		return 0;

	primary_card = card_group_find_primary(card_compound);
	if (primary_card == cardno) {
_primary:
		dbg("Removing group '%s' (primary card %d)", group_id, cardno);
		snd_config_delete(group_config);
		return 1;
	}

	card_node = card_group_find_card_node(card_compound, cardno);
	if (card_node == NULL)
		return 0;

	dbg("Removing card %d in group '%s'", cardno, group_id);

	changes = 1;
	snd_config_delete(card_node);

_retry:
	snd_config_for_each(i, next, card_compound) {
		snd_config_t *card_node = snd_config_iterator_entry(i);
		snd_ctl_t *handle = NULL;
		char name[32];
		long long boot_time = -1;

		err = snd_config_get_integer(card_node, &card_val);
		if (err < 0)
			continue;

		valid = false;

		sprintf(name, "hw:%ld", card_val);
		err = snd_ctl_open(&handle, name, SND_CTL_READONLY);
		if (err >= 0) {
			err = read_boot_params(handle, &boot_time, NULL, NULL, NULL);
			snd_ctl_close(handle);
			if (err < 0) {
				dbg("Unable to read boot params for card %ld: %s", card_val, snd_strerror(err));
				continue;
			}

			if (ts_monotonic.tv_sec == 0) {
				if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts_monotonic) < 0) {
					dbg("Failed to get CLOCK_MONOTONIC_RAW: %s", strerror(errno));
					return changes;
				}
			}
			valid = validate_boot_time(boot_time, ts_monotonic.tv_sec, 0);
		} else {
			dbg("Unable to open ctl handle for card %ld: %s", card_val, snd_strerror(err));
		}

		if (!valid) {
			if (card_val == primary_card) {
				dbg("Primary card %ld is invalid in group '%s'", card_val, group_id);
				goto _primary;
			}
			changes++;
			dbg("Removing another card %ld in group '%s'", card_val, group_id);
			snd_config_delete(card_node);
			goto _retry;
		}
	}
	if (snd_config_is_empty(card_compound)) {
		dbg("No other cards in group '%s', removing", group_id);
		snd_config_delete(group_config);
	}

	return changes > 0;
}

/*
 * Remove card from boot parameters - scans all groups and removes all invalid
 * cards in group containing the card.
 * Returns: 0 on success, negative error code on failure
 */
int boot_params_remove_card(int cardno)
{
	snd_config_t *config = NULL;
	snd_config_iterator_t i, next;
	const char *group_id = NULL;
	int groups_changed = 0;
	int err = 0;

	/* Load the group configuration */
	err = card_group_load(&config);
	if (err < 0) {
		error("Error loading card group configuration: %s", snd_strerror(err));
		goto out;
	}

	if (!config) {
		dbg("No group configuration found");
		err = 0;
		goto out;
	}

	/* Scan all groups and remove any that contain this card */
restart_scan:
	snd_config_for_each(i, next, config) {
		snd_config_t *group = snd_config_iterator_entry(i);

		if (snd_config_get_id(group, &group_id) < 0)
			continue;

		if (snd_config_get_type(group) != SND_CONFIG_TYPE_COMPOUND)
			continue;

		err = boot_params_remove_card_config(group, cardno);
		if (err < 0) {
			error("Unable to remove card %d from group '%s': %s", group_id, cardno, snd_strerror(err));
			continue;
		}
		if (err > 0)
			groups_changed++;
		goto restart_scan;
	}

	if (groups_changed == 0) {
		dbg("Card %d not found in any group", cardno);
		err = 0;
		goto out;
	}

	dbg("Update %d group(s) containing card %d", groups_changed, cardno);

	/* Save the updated configuration */
	err = card_group_save(config);
	if (err < 0) {
		error("Cannot save card group configuration: %s", snd_strerror(err));
		goto out;
	}

out:
	if (config)
		snd_config_delete(config);
	return err;
}

/*
 * Update restored time for all cards in boot group
 * cards in group containing the card.
 */
static void boot_params_update_restored(snd_config_t *card_compound, int skip_cardno,
					long long boot_time, long long restored, long long primary_cardno)
{
	snd_config_iterator_t i, next;
	int err;

	/* Scan all groups and remove any that contain this card */
	snd_config_for_each(i, next, card_compound) {
		snd_config_t *card_node = snd_config_iterator_entry(i);
		long long boot_time_val, boot_synctime, boot_primary;
		snd_ctl_t *handle;
		char name[32];
		long card_val;

		err = snd_config_get_integer(card_node, &card_val);
		if (err < 0)
			continue;

		if (skip_cardno == (long)card_val)
			continue;

		sprintf(name, "hw:%ld", card_val);
		err = snd_ctl_open(&handle, name, SND_CTL_READONLY);
		if (err < 0) {
			dbg("Unable to open ctl handle for card %ld: %s", card_val, snd_strerror(err));
			continue;
		}

		err = read_boot_params(handle, &boot_time_val, &boot_synctime, NULL, &boot_primary);
		if (err < 0) {
			dbg("Unable to read boot params for card %ld: %s", card_val, snd_strerror(err));
			goto _next;
		}

		if (boot_time_val != boot_time) {
			dbg("Boot time mismatch (%lld != %lld)", boot_time, boot_time_val);
			goto _next;
		}

		if (boot_primary != primary_cardno) {
			dbg("Primary card mismatch (%lld != %lld)", boot_primary, primary_cardno);
			goto _next;
		}

		err = write_boot_params(handle, boot_time_val, boot_synctime, restored, boot_primary);
		if (err < 0) {
			dbg("Unable to save boot params: %s", snd_strerror(err));
			goto _next;
		}

		add_linked_card(card_val);

_next:
		snd_ctl_close(handle);
	}
}

/*
 * Update boot parameters
 * Returns: 0 on success, negative error code on failure
 */
int update_boot_params(snd_ctl_t *handle, int cardno, const char *boot_card_group,
		       bool valid, bool restored, long long synctime)
{
	snd_config_t *config = NULL;
	snd_config_t *config_group = NULL;
	snd_config_t *card_compound = NULL;
	struct timespec ts_realtime, ts_monotonic;
	long long value;
	long long boot_time = 0;
	long long restore_time;
	long long primary_card;
	int err = 0;

	if (!boot_card_group) {
		error("boot_card_group parameter is required");
		return -EINVAL;
	}

	if (synctime <= 0) {
		error("synchronization time window is required");
		return -EINVAL;
	}

	err = card_group_load(&config);
	if (err < 0) {
		error("Error loading card group configuration: %s", snd_strerror(err));
		goto out;
	}

	if (!config) {
		err = snd_config_top(&config);
		if (err < 0) {
			error("Cannot create top config: %s", snd_strerror(err));
			goto out;
		}
	}

	/* If valid is false, remove the boot_card_group from configuration */
	if (!valid) {
		err = snd_config_search(config, boot_card_group, &config_group);
		if (err == 0 && config_group) {
			err = boot_params_remove_card_config(config_group, cardno);
			if (err < 0) {
				error("Cannot manage group '%s': %s", boot_card_group, snd_strerror(err));
				goto out;
			}
			dbg("Updated group '%s' in configuration", boot_card_group);
		}
	}

	if (clock_gettime(CLOCK_REALTIME, &ts_realtime) < 0) {
		err = -errno;
		error("Failed to get CLOCK_REALTIME: %s", strerror(errno));
		goto out;
	}
	ts_realtime.tv_nsec = 0;

	if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts_monotonic) < 0) {
		err = -errno;
		error("Failed to get CLOCK_MONOTONIC_RAW: %s", strerror(errno));
		goto out;
	}
	ts_monotonic.tv_nsec = 0;
	restore_time = ts_monotonic.tv_sec;

	err = snd_config_search(config, boot_card_group, &config_group);
	if (err < 0) {
		err = snd_config_make_compound(&config_group, boot_card_group, 0);
		if (err < 0) {
			error("Cannot create group '%s': %s", boot_card_group, snd_strerror(err));
			goto out;
		}
		err = snd_config_add(config, config_group);
		if (err < 0) {
			error("Cannot add group '%s': %s", boot_card_group, snd_strerror(err));
			snd_config_delete(config_group);
			goto out;
		}
	}

	err = card_group_get_or_create_card_compound(config_group, &card_compound);
	if (err < 0)
		goto out;

	primary_card = card_group_find_primary(card_compound);
	if (primary_card < 0) {
		dbg("Primary card not found, using %d", cardno);
		primary_card = cardno;
	}

	if (!card_group_find_card_node(card_compound, cardno)) {
		err = card_group_add_card(card_compound, cardno);
		if (err < 0)
			goto out;
	}

	if (primary_card != cardno || valid) {
		err = card_group_get_int64(config_group, "boot_realtime", &value);
		if (err < 0) {
			err = card_group_set_int64(config_group, "boot_realtime", ts_realtime.tv_sec);
			if (err < 0) {
				error("Cannot set boot_realtime: %s", snd_strerror(err));
				goto out;
			}
			dbg("Set boot_realtime to %lld", (long long)ts_realtime.tv_sec);
		} else {
			dbg("Preserving existing boot_realtime: %lld", value);
			ts_realtime.tv_sec = value;
		}

		err = card_group_get_int64(config_group, "boot_monotonic", &value);
		if (err < 0) {
			err = card_group_set_int64(config_group, "boot_monotonic", ts_monotonic.tv_sec);
			if (err < 0) {
				error("Cannot set boot_monotonic: %s", snd_strerror(err));
				goto out;
			}
			dbg("Set boot_monotonic to %lld", (long long)ts_monotonic.tv_sec);
		} else {
			dbg("Preserving existing boot_monotonic: %lld", value);
			ts_monotonic.tv_sec = value;
		}
	} else {
		err = card_group_set_int64(config_group, "boot_realtime", ts_realtime.tv_sec);
		if (err < 0) {
			error("Cannot set boot_realtime: %s", snd_strerror(err));
			goto out;
		}
		dbg("Set boot_realtime to %lld", (long long)ts_realtime.tv_sec);
		err = card_group_set_int64(config_group, "boot_monotonic", ts_monotonic.tv_sec);
		if (err < 0) {
			error("Cannot set boot_monotonic: %s", snd_strerror(err));
			goto out;
		}
		dbg("Set boot_monotonic to %lld", (long long)ts_monotonic.tv_sec);
	}

	if (synctime > 0) {
		err = card_group_set_int64(config_group, "boot_synctime", synctime);
		if (err < 0) {
			error("Cannot set boot_synctime: %s", snd_strerror(err));
			goto out;
		}
	}

	err = card_group_set_int64(config_group, "boot_last_update", ts_realtime.tv_sec);
	if (err < 0) {
		error("Cannot set boot_last_update: %s", snd_strerror(err));
		goto out;
	}

	err = card_group_save(config);
	if (err < 0) {
		error("Cannot save card group configuration: %s", snd_strerror(err));
		goto out;
	}

	/* Update '.Boot' control element on the card */
	boot_time = ts_monotonic.tv_sec;
	if (!restored)
		restore_time = -1;

	err = write_boot_params(handle, boot_time, synctime, restore_time, primary_card);
	if (err < 0) {
		error("Cannot write boot parameters: %s", snd_strerror(err));
		goto out;
	}

	if (primary_card == cardno)
		boot_params_update_restored(card_compound, primary_card, boot_time, restore_time, primary_card);

	dbg("Updated boot parameters for card %d in group '%s'", cardno, boot_card_group);

out:
	if (config)
		snd_config_delete(config);
	return err;
}

