/*
 *  Advanced Linux Sound Architecture Control Program
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
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include "alsactl.h"

static int clean_one_control(snd_ctl_t *handle, snd_ctl_elem_id_t *elem_id,
			     snd_ctl_elem_id_t **filter)
{
	snd_ctl_elem_info_t *info;
	char *s;
	int err;

	snd_ctl_elem_info_alloca(&info);
	snd_ctl_elem_info_set_id(info, elem_id);
	err = snd_ctl_elem_info(handle, info);
	if (err < 0) {
		s = snd_ctl_ascii_elem_id_get(elem_id);
		error("Cannot read control info '%s': %s", s, snd_strerror(err));
		free(s);
		return err;
	}

	if (!snd_ctl_elem_info_is_user(info))
		return 0;

	s = snd_ctl_ascii_elem_id_get(elem_id);
	dbg("Application control \"%s\" found.", s);
	if (filter) {
		for (; *filter; filter++) {
			if (snd_ctl_elem_id_compare_set(elem_id, *filter) == 0)
				break;
		}
		if (*filter == NULL) {
			free(s);
			return 0;
		}
	}

	err = snd_ctl_elem_remove(handle, elem_id);
	if (err < 0) {
		error("Cannot remove control '%s': %s", s, snd_strerror(err));
		free(s);
		return err;
	}
	dbg("Application control \"%s\" removed.", s);
	free(s);
	return 0;
}

static void filter_controls_free(snd_ctl_elem_id_t **_filter)
{
	snd_ctl_elem_id_t **filter;

	for (filter = _filter; filter; filter++)
		free(*filter);
	free(_filter);
}

static int filter_controls_parse(char *const *controls, snd_ctl_elem_id_t ***_filter)
{
	snd_ctl_elem_id_t **filter = NULL;
	char *const *c;
	char *s;
	unsigned int count, idx;
	int err;

	if (!controls)
		goto fin;
	for (count = 0, c = controls; *c; c++, count++);
	if (count == 0)
		goto fin;
	filter = calloc(count + 1, sizeof(snd_ctl_elem_id_t *));
	if (filter == NULL) {
nomem:
		error("No enough memory...");
		return -ENOMEM;
	}
	filter[count] = NULL;
	for (idx = 0; idx < count; idx++) {
		err = snd_ctl_elem_id_malloc(&filter[idx]);
		if (err < 0) {
			filter_controls_free(filter);
			goto nomem;
		}
		err = snd_ctl_ascii_elem_id_parse(filter[idx], controls[idx]);
		if (err < 0) {
			error("Cannot parse id '%s': %s", controls[idx], snd_strerror(err));
			filter_controls_free(filter);
			return err;
		}
		s = snd_ctl_ascii_elem_id_get(filter[idx]);
		dbg("Add to filter: \"%s\"", s);
		free(s);
	}
fin:
	*_filter = filter;
	return 0;
}

static int clean_controls(int cardno, char *const *controls)
{
	snd_ctl_t *handle;
	snd_ctl_elem_list_t *list;
	snd_ctl_elem_id_t *elem_id;
	snd_ctl_elem_id_t **filter;
	char name[32];
	unsigned int idx, count;
	int err;

	snd_ctl_elem_id_alloca(&elem_id);
	snd_ctl_elem_list_alloca(&list);

	err = filter_controls_parse(controls, &filter);
	if (err < 0)
		return err;

	sprintf(name, "hw:%d", cardno);
	err = snd_ctl_open(&handle, name, 0);
	if (err < 0) {
		error("snd_ctl_open error: %s", snd_strerror(err));
		filter_controls_free(filter);
		return err;
	}
	dbg("Control device '%s' opened.", name);
	err = snd_ctl_elem_list(handle, list);
	if (err < 0) {
		error("Cannot determine controls: %s", snd_strerror(err));
		goto fin_err;
	}
	count = snd_ctl_elem_list_get_count(list);
	if (count == 0)
		goto fin_ok;
	snd_ctl_elem_list_set_offset(list, 0);
	if ((err = snd_ctl_elem_list_alloc_space(list, count)) < 0) {
		error("No enough memory...");
		goto fin_err;
	}
	if ((err = snd_ctl_elem_list(handle, list)) < 0) {
		error("Cannot determine controls (2): %s", snd_strerror(err));
		goto fin_err;
	}
	for (idx = 0; idx < count; idx++) {
		snd_ctl_elem_list_get_id(list, idx, elem_id);
		err = clean_one_control(handle, elem_id, filter);
		if (err < 0)
			goto fin_err;
	}
fin_ok:
	filter_controls_free(filter);
	snd_ctl_close(handle);
	return 0;
fin_err:
	filter_controls_free(filter);
	snd_ctl_close(handle);
	return err;
}

int clean(const char *cardname, char *const *extra_args)
{
	struct snd_card_iterator iter;
	int err;

	err = snd_card_iterator_sinit(&iter, cardname);
	if (err < 0)
		return err;
	while (snd_card_iterator_next(&iter)) {
		if ((err = clean_controls(iter.card, extra_args)))
			return err;
	}
	return snd_card_iterator_error(&iter);
}
