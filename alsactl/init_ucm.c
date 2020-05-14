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

#include <alsa/use-case.h>

/*
 * Keep it as simple as possible. Execute commands from the SectionOnce only.
 */
int init_ucm(int cardno)
{
	snd_use_case_mgr_t *uc_mgr;
	char id[32];
	int err;

	snprintf(id, sizeof(id), "hw:%d", cardno);
	err = snd_use_case_mgr_open(&uc_mgr, id);
	if (err < 0)
		return err;
	err = snd_use_case_set(uc_mgr, "_once", NULL);
	snd_use_case_mgr_close(uc_mgr);
	return err;
}
