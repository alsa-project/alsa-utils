#ifndef CURSKEY_H_INCLUDED
#define CURSKEY_H_INCLUDED

#include CURSESINC

/* Additional KEY_ constants */
#define KEY_SPACE ' '
#define KEY_TAB '\t'
#define KEY_DEL 127
#define KEY_ESCAPE 27
#define KEY_INSERT KEY_IC
#define KEY_DELETE KEY_DC
#define KEY_PAGEUP KEY_PPAGE
#define KEY_PAGEDOWN KEY_NPAGE

/* Modifiers */
#define CURSKEY_MOD_CNTRL	1U
#define CURSKEY_MOD_META	2U
#define CURSKEY_MOD_ALT		CURSKEY_MOD_META

/* Defines the range of characters which should be "meta-able" */
#define CURSKEY_MAX_META_CHAR 127

int curskey_init();
void curskey_destroy();
int curskey_define_meta_keys(unsigned int keycode_start);

int curskey_parse(const char *keydef);
int curskey_mod_key(int key, unsigned int modifiers);

#define curskey_meta_key(KEY) \
	curskey_mod_key(KEY, CURSKEY_MOD_META)

#define curskey_cntrl_key(KEY) \
	curskey_mod_key(KEY, CURSKEY_MOD_CNTRL)

#endif
