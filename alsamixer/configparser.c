#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include CURSESINC
#include "colors.h"
#include "mem.h"
#include "die.h"
#include "gettext_curses.h"
#include "utils.h"
#include "curskey.h"
#include "bindings.h"

#define ERROR_CONFIG -1
#define ERROR_MISSING_ARGUMENTS -2
#define ERROR_TOO_MUCH_ARGUMENTS -3

static const char *error_message;
static const char *error_cause;

extern int mouse_wheel_step;

static int strlist_index(const char *haystack, int padlen, int padchar, const char *needle) {
	int needle_len;
	int pos;
	const char *found;

	needle_len = strlen(needle);
	if (needle_len < padlen && needle[needle_len - 1] != padchar) {
		found = strstr(haystack, needle);
		if (found) {
			pos = (found - haystack);
			if (pos % padlen == 0 && haystack[pos+needle_len] == padchar)
				return pos / padlen;
		}
	}

	return -1;
}

static int color_by_name(const char *name) {
	return strlist_index(
		"default "
		"black   "
		"red     "
		"green   "
		"yellow  "
		"blue    "
		"magenta "
		"cyan    "
		"white   ", 8, ' ', name) - 1;
};

static int attr_by_name(const char *name) {
	int idx = strlist_index(
		"bold      "
		"reverse   "
		"standout  "
		"dim       "
		"underline "
#ifdef A_ITALIC
		"italic    "
#endif
		"normal    "
		"blink     ", 10, ' ', name);

	if (idx < 0)
		return -1;

	return (int[]) {
		A_BOLD,
		A_REVERSE,
		A_STANDOUT,
		A_DIM,
		A_UNDERLINE,
#ifdef A_ITALIC
		A_ITALIC,
#endif
		A_NORMAL,
		A_BLINK,
	}[idx];
};

enum command_word {
	/* $ perl -e '$i=0; printf "W_%s = 0x%X,\n", uc, 1<<$i++ for sort @ARGV' \
	   bottom top page up down left right next previous toggle \
	   close help \
	   control playback capture all refresh set focus mode balance mute
	   */
	W_ALL = 0x1,
	W_BALANCE = 0x2,
	W_BOTTOM = 0x4,
	W_CAPTURE = 0x8,
	W_CLOSE = 0x10,
	W_CONTROL = 0x20,
	W_DOWN = 0x40,
	W_FOCUS = 0x80,
	W_HELP = 0x100,
	W_LEFT = 0x200,
	W_MODE = 0x400,
	W_MUTE = 0x800,
	W_NEXT = 0x1000,
	W_PAGE = 0x2000,
	W_PLAYBACK = 0x4000,
	W_PREVIOUS = 0x8000,
	W_REFRESH = 0x10000,
	W_RIGHT = 0x20000,
	W_SET = 0x40000,
	W_TOGGLE = 0x80000,
	W_TOP = 0x100000,
	W_UP = 0x200000,
	// ---
	W_NUMBER = 0x4000000,
};

static const char *command_words =
	/* $ perl -e 'printf "\"%-9s\"\n", lc for sort @ARGV' \
	   bottom top page up down left right next previous toggle \
	   close help \
	   control playback capture all refresh set focus mode balance mute
	   */
	"all      "
	"balance  "
	"bottom   "
	"capture  "
	"close    "
	"control  "
	"down     "
	"focus    "
	"help     "
	"left     "
	"mode     "
	"mute     "
	"next     "
	"page     "
	"playback "
	"previous "
	"refresh  "
	"right    "
	"set      "
	"toggle   "
	"top      "
	"up       ";

static unsigned int parse_words(const char *name, int *number) {
	unsigned words = 0;
	unsigned word;
	char buf[16];
	int i;

	while (*name) {
		for (i = 0; i < sizeof(buf) - 1; ++i) {
			if (*name == '\0')
				break;
			if (*name == '_') {
				++name;
				break;
			}
			buf[i] = *name;
			++name;
		}
		buf[i] = '\0';

		if (buf[0] >= '0' && buf[0] <= '9') {
			if (number)
				*number = atoi(buf);
			word = W_NUMBER;
		}
		else if ((i = strlist_index(command_words, 9, ' ', buf)) >= 0)
			word = 1 << i;
		else
			return 0;
		
		if (words & word) // Every word only once
			return 0;
		words |= word;
	}

	return words;
}

static int textbox_command_by_name(const char *name) {
	switch (parse_words(name, NULL)) {
		case W_TOP: return CMD_TEXTBOX_TOP;
		case W_BOTTOM: return CMD_TEXTBOX_BOTTOM;
		case W_CLOSE: return CMD_TEXTBOX_CLOSE;
		case W_UP: return CMD_TEXTBOX_UP;
		case W_DOWN: return CMD_TEXTBOX_DOWN;
		case W_LEFT: return CMD_TEXTBOX_LEFT;
		case W_RIGHT: return CMD_TEXTBOX_RIGHT;
		case W_PAGE|W_UP: return CMD_TEXTBOX_PAGE_UP;
		case W_PAGE|W_DOWN: return CMD_TEXTBOX_PAGE_DOWN;
		case W_PAGE|W_LEFT: return CMD_TEXTBOX_PAGE_LEFT;
		case W_PAGE|W_RIGHT: return CMD_TEXTBOX_PAGE_RIGHT;
		default: return 0;
	}
}

static int mixer_command_by_name(const char *name) {
	int channel = 0;
	int number = 1; // W_CONTROL|{W_UP,W_DOWN} default
	unsigned int words = parse_words(name, &number);

	switch (words) {
		case W_CLOSE: return CMD_MIXER_CLOSE;
		case W_HELP: return CMD_MIXER_HELP;
		case W_NEXT: return CMD_MIXER_NEXT;
		case W_PREVIOUS: return CMD_MIXER_PREVIOUS;
		case W_REFRESH: return CMD_MIXER_REFRESH;
		case W_MODE|W_ALL: return CMD_MIXER_MODE_ALL;
		case W_MODE|W_CAPTURE: return CMD_MIXER_MODE_CAPTURE;
		case W_MODE|W_PLAYBACK: return CMD_MIXER_MODE_PLAYBACK;
		case W_MODE|W_TOGGLE: return CMD_MIXER_MODE_TOGGLE;
		case W_CONTROL|W_BALANCE: return CMD_MIXER_BALANCE_CONTROL;
		case W_CONTROL|W_SET|W_NUMBER:
			return (number > 100 ? 0 :
					CMD_WITH_ARG(CMD_MIXER_CONTROL_N_PERCENT, number));
		case W_CONTROL|W_FOCUS|W_NUMBER:
			return ((number < 1 || number > 100) ? 0 :
					CMD_WITH_ARG(CMD_MIXER_CONTROL_FOCUS_N, number));
	}

	if (words & W_LEFT)
		channel |= 1;
	if (words & W_RIGHT)
		channel |= 2;
	if (!channel)
		channel = 3;

	words &= ~(W_LEFT|W_RIGHT);
	switch (words) {
		case W_CONTROL|W_UP:
		case W_CONTROL|W_UP|W_NUMBER:
		case W_CONTROL|W_DOWN:
		case W_CONTROL|W_DOWN|W_NUMBER:
			return ((number < 1 || number > 100) ? 0 :
				CMD_WITH_ARG(
					(words & W_UP ? CMD_MIXER_CONTROL_UP_LEFT_N : CMD_MIXER_CONTROL_DOWN_LEFT_N)
					+ channel - 1, number));
		case W_CONTROL|W_MUTE:
			return CMD_MIXER_TOGGLE_MUTE_LEFT + channel - 1;
		case W_CONTROL|W_CAPTURE:
			return CMD_MIXER_TOGGLE_CAPTURE_LEFT + channel - 1;
	}

	// Other commands...
	if (! strcmp(name, "select_card"))
		return CMD_MIXER_SELECT_CARD;
	if (! strcmp(name, "system_information"))
		return CMD_MIXER_SYSTEM_INFORMATION;

	return 0;
}

static int* element_by_name(const char *name) {
	int idx = strlist_index(
#ifdef TRICOLOR_VOLUME_BAR
		"ctl_bar_hi         "
#endif
		"ctl_bar_lo         "
#ifdef TRICOLOR_VOLUME_BAR
		"ctl_bar_mi         "
#endif
		"ctl_capture        "
		"ctl_frame          "
		"ctl_inactive       "
		"ctl_label          "
		"ctl_label_focus    "
		"ctl_label_inactive "
		"ctl_mark_focus     "
		"ctl_mute           "
		"ctl_nocapture      "
		"ctl_nomute         "
		"errormsg           "
		"infomsg            "
		"menu               "
		"menu_selected      "
		"mixer_active       "
		"mixer_frame        "
		"mixer_text         "
		"textbox            "
		"textfield          ", 19, ' ', name);

	if (idx < 0)
		return NULL;

	return (int*[]) {
#ifdef TRICOLOR_VOLUME_BAR
		&attr_ctl_bar_hi,
#else
		&errno,
#endif
		&attr_ctl_bar_lo,
#ifdef TRICOLOR_VOLUME_BAR
		&attr_ctl_bar_mi,
#else
		&errno,
#endif
		&attr_ctl_capture,
		&attr_ctl_frame,
		&attr_ctl_inactive,
		&attr_ctl_label,
		&attr_ctl_label_focus,
		&attr_ctl_label_inactive,
		&attr_ctl_mark_focus,
		&attr_ctl_mute,
		&attr_ctl_nocapture,
		&attr_ctl_nomute,
		&attr_errormsg,
		&attr_infomsg,
		&attr_menu,
		&attr_menu_selected,
		&attr_mixer_active,
		&attr_mixer_frame,
		&attr_mixer_text,
		&attr_textbox,
		&attr_textfield,
	}[idx];
}

// === Configuration commands ===

static int cfg_bind(char **argv, int argc) {
	const char *command_name;
	command_enum command = 0;
	command_enum *bindings = mixer_bindings;

	if (argc == 2)
		command_name = argv[1];
	else if (argc == 3) {
		command_name = argv[2];

		if (! strcmp(argv[1], "textbox")) {
			bindings = textbox_bindings;
		}
		else if (! strcmp(argv[1], "mixer"))
			; // bindings = mixer_bindings
		else {
			error_message = _("invalid widget");
			error_cause = argv[1];
			return ERROR_CONFIG;
		}
	}
	else {
		return (argc < 2 ? ERROR_MISSING_ARGUMENTS : ERROR_TOO_MUCH_ARGUMENTS);
	}

	int keycode = curskey_parse(argv[0]);
	if (keycode < 0 || keycode >= ARRAY_SIZE(mixer_bindings)) {
		error_message = _("invalid key");
		error_cause = argv[0];
		return ERROR_CONFIG;
	}

	if (bindings == textbox_bindings)
		command = textbox_command_by_name(command_name);
	else
		command = mixer_command_by_name(command_name);

	if (!command) {
		if (!strcmp(command_name, "none"))
			; // command = 0
		else {
			error_message = _("invalid command");
			error_cause = command_name;
			return ERROR_CONFIG;
		}
	}

	bindings[keycode] = command;
	return 0;
}

static int cfg_color(char **argv, int argc)
{
	short fg_color, bg_color;
	int *element;
	int attr, i;

	if (argc < 3)
		return ERROR_MISSING_ARGUMENTS;

	if (NULL == (element = element_by_name(argv[0]))) {
		error_message = _("unknown theme element");
		error_cause = argv[0];
		return ERROR_CONFIG;
	}

	if (-2 == (fg_color = color_by_name(argv[1]))) {
		error_message = _("unknown color");
		error_cause = argv[1];
		return ERROR_CONFIG;
	}

	if (-2 == (bg_color = color_by_name(argv[2]))) {
		error_message = _("unknown color");
		error_cause = argv[2];
		return ERROR_CONFIG;
	}

	*element = get_color_pair(fg_color, bg_color);

	for (i = 3; i < argc; ++i) {
		if (-1 == (attr = attr_by_name(argv[i]))) {
			error_message = _("unknown color attribute");
			error_cause = argv[i];
			return ERROR_CONFIG;
		}
		else
			*element |= attr;
	}
	return 0;
}

static int cfg_set(char **argv, int argc)
{
	if (argc == 2) {
		if (! strcmp(argv[0], "mouse_wheel_step")) {
			mouse_wheel_step = atoi(argv[1]);
			if (!mouse_wheel_step || mouse_wheel_step > 10) {
				mouse_wheel_step = 1;
				error_message = _("invalid value");
				error_cause = argv[1];
				return ERROR_CONFIG;
			}
		}
		else {
			error_message = _("unknown option");
			error_cause = argv[0];
			return ERROR_CONFIG;
		}
	}
	else {
		return (argc < 2 ? ERROR_MISSING_ARGUMENTS : ERROR_TOO_MUCH_ARGUMENTS);
	}

	return 0;
}

// === Configuration parsing ===

/* Split $line on whitespace, store it in $args, ignoring everything after the
 * first comment char ('#').
 *
 * This will modify contents of $line.
 */
static size_t parse_line(char *line, char **args, size_t args_size)
{
	size_t count;

	for (count = 0; count < args_size; ++count) {
		while (*line && isspace(*line))
			++line;

		if (*line == '\0' || *line == '#')
			break;

		args[count] = line;

		while (*line && !isspace(*line) && *line != '#')
			++line;

		if (*line != '\0') {
			*line = '\0';
			++line;
		}
	}

	return count;
}

static int process_line(char *line) {
	char *args[16];
	size_t argc = parse_line(line, args, ARRAY_SIZE(args));
	int ret = 0;

	if (argc >= 1) {
		error_cause = NULL;
		//error_message = _("unknown error");

		if (argc >= ARRAY_SIZE(args))
			ret = ERROR_TOO_MUCH_ARGUMENTS;
		else {
			ret = strlist_index(
				"bind  "
				"color "
				"set   ", 6, ' ', args[0]);
			switch (ret) {
				case 0: ret = cfg_bind(args + 1, argc - 1); break;
				case 1: ret = cfg_color(args + 1, argc - 1); break;
				case 2: ret = cfg_set(args + 1, argc - 1); break;
				default: error_message = _("unknown command");
			}
		}

		if (ret == ERROR_MISSING_ARGUMENTS)
			error_message = _("missing arguments");
		else if (ret == ERROR_TOO_MUCH_ARGUMENTS)
			error_message = _("too much arguments");
	}

	return ret;
}

// Taken and adapted from textbox.c
#define MAX_FILE_SIZE 1048576
static char *read_file(const char *file_name, unsigned int *file_size)
{
	FILE *f;
	int err;
	char *buf;
	unsigned int allocated = 2048;
	unsigned int bytes_read;

	f = fopen(file_name, "r");
	if (!f) {
		fprintf(stderr, _("Cannot open file \"%s\"."), file_name);
		fprintf(stderr, " %s\n", strerror(errno));
		return NULL;
	}
	*file_size = 0;
	buf = NULL;
	do {
		allocated *= 2;
		buf = crealloc(buf, allocated);
		bytes_read = fread(buf + *file_size, 1, allocated - *file_size, f);
		*file_size += bytes_read;
	} while (*file_size == allocated && allocated < MAX_FILE_SIZE);
	fclose(f);
	if (*file_size > 0 && buf[*file_size - 1] != '\n' && *file_size < allocated) {
		buf[*file_size] = '\n';
		++*file_size;
	}
	return buf;
}

void parse_config_file(const char *file_name)
{
	char *buf;
	unsigned int file_size;
	unsigned int lineno;
	unsigned int i;
	char *line;

	buf = read_file(file_name, &file_size);
	if (!buf)
		return;

	curskey_init();
	endwin(); // print warnings to stderr

	lineno = 0;
	line = buf;
	for (i = 0; i < file_size; ++i) {
		if (buf[i] == '\n') {
			buf[i] = '\0';
			++lineno;
			if (process_line(line) < 0) {
				if (error_cause)
					fprintf(stderr, "%s:%d: %s: %s: %s\n", file_name, lineno, line, error_message, error_cause);
				else
					fprintf(stderr, "%s:%d: %s: %s\n", file_name, lineno, line, error_message);
			}
			line = &buf[i + 1];
		}
	}

	free(buf);
	curskey_destroy();
}

void parse_default_config_file() {
	char file[4096];
	const char *home;

	home = getenv("XDG_CONFIG_HOME");
	if (home && *home) {
		snprintf(file, sizeof(file), "%s/alsamixer.rc", home);
		if (! access(file, F_OK))
			return parse_config_file(file);
	}

	home = getenv("HOME");
	if (!home || !*home) {
		struct passwd *pwd = getpwuid(getuid());
		if (pwd)
			home = pwd->pw_dir;
	}

	if (home && *home) {
		snprintf(file, sizeof(file), "%s/.config/alsamixer.rc", home);
		if (! access(file, F_OK))
			return parse_config_file(file);

		snprintf(file, sizeof(file), "%s/.alsamixer.rc", home);
		if (! access(file, F_OK))
			return parse_config_file(file);
	}
}
