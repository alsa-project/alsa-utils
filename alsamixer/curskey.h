/*
 * curskey.h - parse keybindings in ncurses based applications
 * Copyright (C) 2017 Benjamin Abendroth
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CURSKEY_H_INCLUDED
#define CURSKEY_H_INCLUDED

#include <ncurses.h>

/* Additional KEY_ constants */
#define KEY_SPACE ' '
#define KEY_TAB '\t'
#define KEY_DEL 127
#define KEY_ESCAPE 27
#define KEY_INSERT KEY_IC
#define KEY_DELETE KEY_DC
#define KEY_PAGEUP KEY_PPAGE
#define KEY_PAGEDOWN KEY_NPAGE
extern int KEY_RETURN;

/* Modifiers */
#define CURSKEY_MOD_CNTRL	1U
#define CURSKEY_MOD_META	2U
#define CURSKEY_MOD_ALT		CURSKEY_MOD_META

extern unsigned int CURSKEY_META_START;

/* Defines the range of characters which should be "meta-able" */
#define CURSKEY_META_END_CHARACTERS 127

/* Macro for checking if meta keys are available */
#define CURSKEY_CAN_META (CURSKEY_META_START+0)

// Holds the maximum keycode used by curskey
extern unsigned int CURSKEY_KEY_MAX;

/* Main functions */
int curskey_init();
void curskey_destroy();

int curskey_mod_key(int key, unsigned int modifiers);
int curskey_unmod_key(int key, unsigned int *modifiers);

#define curskey_meta_key(KEY) \
	curskey_mod_key(KEY, CURSKEY_MOD_META)

#define curskey_cntrl_key(KEY) \
	curskey_mod_key(KEY, CURSKEY_MOD_CNTRL)

int curskey_parse(const char *keydef);
const char* curskey_get_keydef(int keycode);
int curskey_define_meta_keys(unsigned int meta_start);

/* Helper functions */
int curskey_keycode(const char *keyname);
const char* curskey_keyname(int keycode);

#endif
