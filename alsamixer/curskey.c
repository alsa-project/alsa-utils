/*
 * curskey.c - parse keybindings in ncurses based applications
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

/*
 * Small library for handling/parsing keybindings in ncurses based
 * terminal applications.
 *
 * It allows you to
 *	 - Parse key definitions into ncurses keycodes returned by getch()
 *	 - Get the string representation of a ncurses keycode
 *
 * Following keys are supported:
 *	 - Ncurses special keys (HOME, END, LEFT, F1, ...)
 *	 - Bindings with control-key (C-x, ^x)
 *	 - Bindings with meta/alt-key (M-x, A-x)
 *
 * Usage:
 *   initscr(); // Has to be called!
 *   if (curskey_init() == OK) {
 *   	...
 *   	curskey_destroy();
 *  }
 *
 *  NOTES:
 *  	The variable `int KEY_RETURN` holds the character that should be
 *  	interpreted as RETURN. Depending on whether nl() or nonl() was called
 *  	this may be either '\n' or '\r'.
 *  	It defaults to '\n'.
 */

#include "curskey.h"
#include <stdlib.h>
#include <string.h>

struct curskey_key_s {
	char *keyname;
	int keycode;
};

int KEY_RETURN = '\n';
unsigned int curskey_keynames_size = 0; 
struct curskey_key_s *curskey_keynames = NULL;
// The starting keycode for enumerating meta/alt key combinations
unsigned int CURSKEY_META_START = 0;
// By default, curskey does not introduce new keybindings.
unsigned int CURSKEY_KEY_MAX = KEY_MAX;

// Names for non-printable/whitespace characters
// and aliases for existing keys
static const struct curskey_key_s curskey_aliases[] = {
	// Keep this sorted by `keyname`
	{ "DEL",	KEY_DEL },
	{ "DELETE",	KEY_DC },
	{ "ESCAPE",	KEY_ESCAPE },
	{ "INSERT",	KEY_IC },
	{ "PAGEDOWN", 	KEY_NPAGE },
	{ "PAGEUP",	KEY_PPAGE },
	{ "SPACE",	KEY_SPACE },
	{ "TAB",	KEY_TAB }
};
#define ALIASES_SIZE (sizeof(curskey_aliases)/sizeof(curskey_aliases[0]))

// MACROS {{{
#define STARTSWITH_KEY(S) \
	((name[0] == 'K' || name[0] == 'k') && \
	(name[1] == 'E' || name[1] == 'e') && \
	(name[2] == 'Y' || name[2] == 'y') && \
	(name[3] == '_'))

#define IS_CONTROL(S) \
	((S[0] == '^') || ((S[0] == 'C' || S[0] == 'c') && S[1] == '-'))

#define IS_META(S) \
	((S[0] == 'M' || S[0] == 'm' || S[0] == 'A' || S[0] == 'a') && S[1] == '-')

#define IS_SHIFT(S) \
	((S[0] == 'S' || S[0] == 's') && S[1] == '-')
// }}} MACROS

// curskey_find {{{
static int curskey_key_cmp(const void *a, const void *b) {
	return strcmp(((struct curskey_key_s*) a)->keyname,
			((struct curskey_key_s*) b)->keyname);
}

static int curskey_find(const struct curskey_key_s *table, unsigned int size, const char *name) {
	unsigned int start = 0;
	unsigned int end = size;
	unsigned int i;
	int cmp;

	while (1) {
		i = (start+end) / 2;
		cmp = strcasecmp(name, table[i].keyname);

		if (cmp == 0)
			return table[i].keycode;
		else if (end == start + 1)
			return ERR;
		else if (cmp > 0)
			start = i;
		else
			end = i;
	}
}
// }}}

/* Like ncurses keyname(), translates the value of a KEY_ constant to its name,
 * but strips leading "KEY_" and parentheses ("KEY_F(...)") off.
 *
 * Returns NULL on failure.
 *
 * This function is not thread-safe.
 */
const char* curskey_keyname(int keycode) {
	int i;
	static char buf[4] = "Fxx";

	if (keycode >= KEY_F(1) && keycode <= KEY_F(63)) {
		i = keycode - KEY_F(0);
		if (i <= 9) {
			buf[1] = '0' + i;
			buf[2] = '\0';
		} else {
			buf[1] = '0' + (int) (i/10);
			buf[2] = '0' + (i % 10);
			buf[3] = '\0';
		}
		return buf;
	}

	if (keycode == KEY_RETURN)
		return "RETURN";

	for (i = 0; i < ALIASES_SIZE; ++i)
		if (keycode == curskey_aliases[i].keycode)
			return curskey_aliases[i].keyname;

	for (i = 0; i < curskey_keynames_size; ++i)
		if (keycode == curskey_keynames[i].keycode)
			return curskey_keynames[i].keyname;

	return NULL;
}

/* Translate the name of a ncurses KEY_ constant to its value.
 * 	"KEY_DOWN" -> 258
 *
 * Return ERR on failure.
 */
int curskey_keycode(const char *name)
{
	int i;

	if (! name)
		return ERR;

	if (STARTSWITH_KEY(name))
		name += 4;

	if (name[0] == 'F' || name[0] == 'f') {
		i = (name[1] == '(' ? 2 : 1);

		if (name[i] >= '0' && name[i] <= '9') {
			i = atoi(name + i);
			if (i >= 1 && i <= 63)
				return KEY_F(i);
		}
	}

	if (! strcasecmp(name, "RETURN"))
		return KEY_RETURN;

	i = curskey_find(curskey_aliases, ALIASES_SIZE, name);
	if (i != ERR)
		return i;

	return curskey_find(curskey_keynames, curskey_keynames_size, name);
}

static void free_ncurses_keynames() {
	if (curskey_keynames) {
		while (curskey_keynames_size)
			free(curskey_keynames[--curskey_keynames_size].keyname);
		free(curskey_keynames);
		curskey_keynames = NULL;
	}
}

/* Create the list of ncurses KEY_ constants.
 * Returns OK on success, ERR on failure.
 */
int create_ncurses_keynames() {
	char	*name;
	struct curskey_key_s *tmp;

	free_ncurses_keynames();
	curskey_keynames = malloc((KEY_MAX - KEY_MIN) * sizeof(struct curskey_key_s));
	if (!curskey_keynames)
		return ERR;

	for (int key = KEY_MIN; key != KEY_MAX; ++key) {
		name = (char*) keyname(key);

		if (!name || !STARTSWITH_KEY(name))
			continue;

		name += 4;
		if (name[0] == 'F' && name[1] == '(')
			continue; // ignore KEY_F(1),...

		name = strdup(name);
		if (! name)
			goto ERROR;
		curskey_keynames[curskey_keynames_size].keycode = key;
		curskey_keynames[curskey_keynames_size].keyname = name;
		++curskey_keynames_size;
	}

	tmp = realloc(curskey_keynames, curskey_keynames_size * sizeof(struct curskey_key_s));
	if (!tmp)
		goto ERROR;
	curskey_keynames = tmp;

	qsort(curskey_keynames, curskey_keynames_size, sizeof(struct curskey_key_s), curskey_key_cmp);

	return OK;
ERROR:
	free_ncurses_keynames();
	return ERR;
}

/* Defines meta escape sequences in ncurses.
 *
 * Returns 0 if meta keys are available, ERR otherwise.
 */
int curskey_define_meta_keys(unsigned int meta_start) {
#ifdef NCURSES_VERSION
	CURSKEY_META_START = meta_start;

	int 	ch;
	int	curs_keycode = CURSKEY_META_START;
	char 	key_sequence[3] = "\e ";

	for (ch = 0; ch <= CURSKEY_META_END_CHARACTERS; ++ch) {
		key_sequence[1] = ch;
		define_key(key_sequence, curs_keycode);
		++curs_keycode;
	}

	CURSKEY_KEY_MAX = CURSKEY_META_START + CURSKEY_META_END_CHARACTERS;
  return ERR;
#endif
  return 0;
}

/* Return the keycode for a key with modifiers applied.
 *
 * Available modifiers are:
 * 	- CURSKEY_MOD_META / CURSKEY_MOD_ALT
 * 	- CURSKEY_MOD_CNTRL
 *
 * See also the macros curskey_meta_key(), curskey_cntrl_key().
 *
 * Returns ERR if the modifiers cannot be applied to this key.
 */
int curskey_mod_key(int key, unsigned int modifiers) {
	if (modifiers & CURSKEY_MOD_CNTRL) {
		/**/ if (key >= 'A' && key <= '_')
			key -= 'A' - 1;
		else if (key >= 'a' && key <= 'z')
			key -= 'a' - 1;
		else if (key == ' ')
			key = 0;
		else
			return ERR;
	}

	if (modifiers & CURSKEY_MOD_META) {
		if (CURSKEY_META_START && (
				key >= 0 && key <= CURSKEY_META_END_CHARACTERS))
			key = CURSKEY_META_START + key;
		else
			return ERR;
	}

	return key;
}

/* The opposite of curskey_mod_key.
 *
 * Return the keycode with modifiers stripped of.
 *
 * Stores modifier mask in `modifiers` if it is not NULL.
 *
 * Returns ERR if the key is invalid.
 */
int curskey_unmod_key(int key, unsigned int* modifiers)
{
	unsigned int null_store;
	if (!modifiers)
		modifiers = &null_store;

	*modifiers = 0;

	if (key < 0)
		return ERR;

	/*
	if (key >= KEY_MIN && key <= KEY_MAX)
		return key;
	*/

	if (CURSKEY_CAN_META &&
			key >= CURSKEY_META_START &&
			key <= CURSKEY_META_START + CURSKEY_META_END_CHARACTERS)
	{
		key = key - CURSKEY_META_START;
		*modifiers |= CURSKEY_MOD_META;
	}

	if (key < ' ' &&
		// We do not want C-I for TAB, etc...
		key != KEY_ESCAPE &&
		key != KEY_TAB &&
		key != KEY_RETURN) {
		if (key == 0)
			key = ' ';
		else
			key += 'A' - 1;
		*modifiers |= CURSKEY_MOD_CNTRL;
	}

	return key;
}

/* Return key definition for a ncurses keycode.
 *
 * The returned string is of the format "[C-][M-]KEY".
 *
 * Returns NULL on failure.
 *
 * This function is not thread-safe.
 */
const char *curskey_get_keydef(int keycode)
{
	unsigned int mod;
	static char buffer[256];
	char *s = buffer;

	keycode = curskey_unmod_key(keycode, &mod);

	/*
	if (keycode == ERR)
		return NULL;

	if (keycode >= KEY_MIN && keycode <= KEY_MAX)
		return curskey_keyname(keycode);
	*/

	if (mod & CURSKEY_MOD_CNTRL) {
		*s++ = 'C';
		*s++ = '-';
	}

	if (mod & CURSKEY_MOD_META) {
		*s++ = 'M';
		*s++ = '-';
	}

	const char* name = curskey_keyname(keycode);
	if (name)
		strcpy(s, name);
	else if (keycode > 32 && keycode < 127) {
		*s++ = keycode;
		*s = '\0';
	}
	else
		return NULL;

	return buffer;
}

/* Return the ncurses keycode for a key definition.
 *
 * Key definition may be:
 *	- Single character (a, z, ...)
 *	- Character with control-modifier (^x, C-x, c-x, ...)
 *	- Character with meta/alt-modifier (M-x, m-x, A-x, a-x, ...)
 *	- Character with both modifiers (C-M-x, M-C-x, M-^x, ...)
 *	- Curses keyname, no modifiers allowed (KEY_HOME, HOME, F1, F(1), ...)
 *
 * Returns ERR if either
 * 	- The key definition is NULL or empty
 * 	- The key could not be found ("KEY_FOO")
 * 	- The key combination is generally invalid ("C-TAB", "C-RETURN")
 * 	- The key is invalid because of compile time options (the
 * 		`define_key()` function was not available.)
 */
int curskey_parse(const char *def) {
	int c;
	unsigned int mod = 0;

	if (! def)
		return ERR;

	for (;;) {
		if (def[0] == '^' && def[1] != '\0') {
			++def;
			mod |= CURSKEY_MOD_CNTRL;
		}
		else if ((def[0] == 'C' || def[0] == 'c') && def[1] == '-') {
			def += 2;
			mod |= CURSKEY_MOD_CNTRL;
		}
		else if (IS_META(def)) {
			if (! CURSKEY_CAN_META)
				return ERR;
			def += 2;
			mod |= CURSKEY_MOD_ALT;
		}
		else
			break;
	}

	if (*def == '\0')
		return ERR;
	else if (*(def+1) == '\0')
		c = *def;
	else
		c = curskey_keycode(def);

	return curskey_mod_key(c, mod);
}

/* Initialize curskey.
 * Returns OK on success, ERR on failure.
 */
int curskey_init() {
	keypad(stdscr, TRUE);
	return create_ncurses_keynames();
}

/* Destroy curskey.
 */
void curskey_destroy() {
	free_ncurses_keynames();
}

// UNUSED CODE {{{
#if 0
/*
 * Return the "normalized" keyname, without preceding "KEY_"
 * and parentheses of function keys removed:
 *	 KEY_HOME -> HOME
 *	 HOME	  -> HOME
 *	 KEY_F(1) -> F1
 *
 * String will be truncated to 64 characters.
 * Returned string must be free()d.
 */
char *curskey_normalize(const char *name)
{
	char normalized[64];
	int i = 0;

	if (! name)
		return NULL;

	if (STARTSWITH_KEY(name))
		name += 4;

	if (name[0] == 'F' || name[0] == 'f') {
		for (; *name; ++name) {
			if (*name == '(' || *name == ')') {
				// ignore
			}
			else if (i < sizeof(normalized) - 1) {
				normalized[i++] = *name;
			}
		}
		normalized[i] = '\0';
		return strdup(normalized);
	}
	else {
		return strdup(name);
	}
}
#endif
// }}} UNUSED CODE
