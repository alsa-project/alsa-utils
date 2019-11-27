/*
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __USECASE_H
#define __USECASE_H

struct context {
	snd_use_case_mgr_t *uc_mgr;
	const char *command;
	char *card;
	char **argv;
	int argc;
	int arga;
	char *batch;
	unsigned int interactive:1;
	unsigned int no_open:1;
	unsigned int do_exit:1;
};

void dump(struct context *context, const char *format);

#endif
