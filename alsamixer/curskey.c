#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "curskey.h"
#include "utils.h"
#include "mem.h"

struct curskey_key {
	char *keyname;
	int keycode;
};

static struct curskey_key *keynames;
static unsigned int keynames_count;

unsigned int meta_keycode_start;
static uint64_t invalid_meta_char_mask[2];

// Names for non-printable/whitespace characters and aliases for existing keys
static const struct curskey_key keyname_aliases[] = {
	// Sorted by `keyname`
	{ "DEL",      KEY_DEL },
	{ "DELETE",   KEY_DC },
	{ "ENTER",    '\n' },
	{ "ENTER",    '\r' },
	{ "ESCAPE",   KEY_ESCAPE },
	{ "INSERT",   KEY_IC },
	{ "PAGEDOWN", KEY_NPAGE },
	{ "PAGEUP",   KEY_PPAGE },
	{ "SPACE",    KEY_SPACE },
	{ "TAB",      KEY_TAB }
};

#define STARTSWITH_KEY(S) \
	((name[0] == 'K' || name[0] == 'k') && \
	(name[1] == 'E' || name[1] == 'e') && \
	(name[2] == 'Y' || name[2] == 'y') && \
	(name[3] == '_'))

#define IS_META(S) \
	((S[0] == 'M' || S[0] == 'm' || S[0] == 'A' || S[0] == 'a') && S[1] == '-')

static int curskey_key_cmp(const void *a, const void *b) {
	return strcmp(((struct curskey_key*) a)->keyname,
			((struct curskey_key*) b)->keyname);
}

static int curskey_find(const struct curskey_key *table, unsigned int size, const char *name) {
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

	i = curskey_find(keyname_aliases, ARRAY_SIZE(keyname_aliases), name);
	if (i != ERR)
		return i;

	return curskey_find(keynames, keynames_count, name);
}

static void free_ncurses_keynames() {
	if (keynames) {
		while (keynames_count)
			free(keynames[--keynames_count].keyname);
		free(keynames);
		keynames = NULL;
	}
}

/* Create the list of ncurses KEY_ constants and their names.
 * Returns OK on success, ERR on failure.
 */
int create_ncurses_keynames() {
	int	key;
	char *name;

	free_ncurses_keynames();
	keynames = ccalloc(sizeof(struct curskey_key), (KEY_MAX - KEY_MIN));

	for (key = KEY_MIN; key != KEY_MAX; ++key) {
		name = (char*) keyname(key);

		if (!name || !STARTSWITH_KEY(name))
			continue;

		name += 4;
		if (name[0] == 'F' && name[1] == '(')
			continue; // ignore KEY_F(1),...

		keynames[keynames_count].keycode = key;
		keynames[keynames_count].keyname = cstrdup(name);
		++keynames_count;
	}

	keynames = crealloc(keynames, keynames_count * sizeof(struct curskey_key));
	qsort(keynames, keynames_count, sizeof(struct curskey_key), curskey_key_cmp);

	return OK;
}

/* Defines meta escape sequences in ncurses.
 *
 * Some combinations with meta/alt may not be available since they collide
 * with the prefix of a pre-defined key.
 * For example, keys F1 - F4 begin with "\eO", so ALT-O cannot be defined.
 *
 * Returns OK if meta keys are available, ERR otherwise.
 */
int curskey_define_meta_keys(unsigned int keycode_start) {
#ifdef NCURSES_VERSION
	int ch;
	int keycode;
	int new_keycode = keycode_start;
	char key_sequence[3] = "\e ";

	invalid_meta_char_mask[0] = 0;
	invalid_meta_char_mask[1] = 0;

	for (ch = 0; ch <= CURSKEY_MAX_META_CHAR; ++ch) {
		key_sequence[1] = ch;
		keycode = key_defined(key_sequence);
		if (! keycode) {
			define_key(key_sequence, new_keycode);
		}
		else if (keycode == new_keycode)
			;
		else
			invalid_meta_char_mask[ch/65] |= (1UL << (ch % 64));

		++new_keycode;
	}

	meta_keycode_start = keycode_start;
	return OK;
#endif
	return ERR;
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
		if ((key >= 'A' && key <= '_') || (key >= 'a' && key <= 'z') || key == ' ')
			key = key % 32;
		else
			return ERR;
	}

	if (modifiers & CURSKEY_MOD_META) {
		if (meta_keycode_start &&
				(key >= 0 && key <= CURSKEY_MAX_META_CHAR) &&
				! (invalid_meta_char_mask[key/65] & (1UL << (key % 64)))) {
			key = meta_keycode_start + key;
		}
		else
			return ERR;
	}

	return key;
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
 * 	- The key combination is invalid in general ("C-TAB", "C-RETURN")
 * 	- The key is invalid because of compile time options (the
 * 		`define_key()` function was not available.)
 * 	- The key is invalid because it could not be defined by
 * 		curskey_define_meta_keys()
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
			if (! meta_keycode_start)
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
 * Returns OK on success, ERR on failure.  */
int curskey_init() {
	keypad(stdscr, TRUE);
	return create_ncurses_keynames();
}

/* Destroy curskey.  */
void curskey_destroy() {
	free_ncurses_keynames();
}
