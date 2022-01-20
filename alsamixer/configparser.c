#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include CURSESINC
#include "colors.h"
#include "gettext_curses.h"
#include "utils.h"
#include "curskey.h"
#include "bindings.h"
#include "mixer_widget.h"

#define ERROR_CONFIG (-1)
#define ERROR_MISSING_ARGUMENTS (-2)
#define ERROR_TOO_MUCH_ARGUMENTS (-3)

static const char *error_message;
static const char *error_cause;

static int strlist_index(const char *haystack, unsigned int itemlen, const char *needle) {
	unsigned int needle_len;
	unsigned int pos;
	const char *found;

	needle_len = strlen(needle);
	if (needle_len <= itemlen && needle[needle_len - 1] != ' ') {
		found = strstr(haystack, needle);
		if (found) {
			pos = (found - haystack);
			if (pos % itemlen == 0 && (needle_len == itemlen || haystack[pos+needle_len] == ' '))
				return pos / itemlen;
		}
	}

	return -1;
}

static int color_by_name(const char *name) {
	return strlist_index(
		"default"
		"black  "
		"red    "
		"green  "
		"yellow "
		"blue   "
		"magenta"
		"cyan   "
		"white  ", 7, name) - 1;
};

static int attr_by_name(const char *name) {
	return (int[]) {
		-1,
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
	}[strlist_index(
		"bold     "
		"reverse  "
		"standout "
		"dim      "
		"underline"
#ifdef A_ITALIC
		"italic   "
#endif
		"normal   "
		"blink    ", 9, name) + 1];
};

#define W_NUMBER (1U << 0)

enum textbox_word {
	TW_BOTTOM = (1U << 1),
	TW_CLOSE = (1U << 2),
	TW_DOWN = (1U << 3),
	TW_LEFT = (1U << 4),
	TW_PAGE = (1U << 5),
	TW_RIGHT = (1U << 6),
	TW_TOP = (1U << 7),
	TW_UP = (1U << 8),
};

const char *textbox_words =
	"bottom"
	"close "
	"down  "
	"left  "
	"page  "
	"right "
	"top   "
	"up    ";

enum mixer_word {
	MW_ALL = (1U << 1),
	MW_BALANCE = (1U << 2),
	MW_CAPTURE = (1U << 3),
	MW_CARD = (1U << 4),
	MW_CLOSE = (1U << 5),
	MW_CONTROL = (1U << 6),
	MW_DOWN = (1U << 7),
	MW_FOCUS = (1U << 8),
	MW_HELP = (1U << 9),
	MW_INFORMATION = (1U << 10),
	MW_LEFT = (1U << 11),
	MW_MODE = (1U << 12),
	MW_MUTE = (1U << 13),
	MW_NEXT = (1U << 14),
	MW_PLAYBACK = (1U << 15),
	MW_PREVIOUS = (1U << 16),
	MW_REFRESH = (1U << 17),
	MW_RIGHT = (1U << 18),
	MW_SELECT = (1U << 19),
	MW_SET = (1U << 20),
	MW_SYSTEM = (1U << 21),
	MW_TOGGLE = (1U << 22),
	MW_UP = (1U << 23),
};

const char *mixer_words =
	"all        "
	"balance    "
	"capture    "
	"card       "
	"close      "
	"control    "
	"down       "
	"focus      "
	"help       "
	"information"
	"left       "
	"mode       "
	"mute       "
	"next       "
	"playback   "
	"previous   "
	"refresh    "
	"right      "
	"select     "
	"set        "
	"system     "
	"toggle     "
	"up         ";

static unsigned int parse_words(const char *name, const char* wordlist, unsigned int itemlen, unsigned int *number) {
	unsigned int words = 0;
	unsigned int word;
	int i;
	char buf[16];
	char *endptr;

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
			if (number) {
				*number = strtoumax(buf, &endptr, 10);
				if (*endptr != '\0')
					return 0;
			}
			word = W_NUMBER;
		}
		else if ((i = strlist_index(wordlist, itemlen, buf)) >= 0)
			word = i <= 30 ? (2U << i) : 0;
		else
			return 0;

		if (words & word) // no duplicate words
			return 0;
		words |= word;
	}

	return words;
}

static int textbox_command_by_name(const char *name) {
	switch (parse_words(name, textbox_words, 6, NULL)) {
		case TW_TOP: return CMD_TEXTBOX_TOP;
		case TW_BOTTOM: return CMD_TEXTBOX_BOTTOM;
		case TW_CLOSE: return CMD_TEXTBOX_CLOSE;
		case TW_UP: return CMD_TEXTBOX_UP;
		case TW_DOWN: return CMD_TEXTBOX_DOWN;
		case TW_LEFT: return CMD_TEXTBOX_LEFT;
		case TW_RIGHT: return CMD_TEXTBOX_RIGHT;
		case TW_PAGE|TW_UP: return CMD_TEXTBOX_PAGE_UP;
		case TW_PAGE|TW_DOWN: return CMD_TEXTBOX_PAGE_DOWN;
		case TW_PAGE|TW_LEFT: return CMD_TEXTBOX_PAGE_LEFT;
		case TW_PAGE|TW_RIGHT: return CMD_TEXTBOX_PAGE_RIGHT;
		default: return 0;
	}
}

static int mixer_command_by_name(const char *name) {
	unsigned int channel = 0;
	unsigned int number = 1; // default numeric arg
	unsigned int words = parse_words(name, mixer_words, 11, &number);

	switch (words) {
		case MW_HELP: return CMD_MIXER_HELP;
		case MW_CLOSE: return CMD_MIXER_CLOSE;
		case MW_REFRESH: return CMD_MIXER_REFRESH;
		case MW_SELECT|MW_CARD: return CMD_MIXER_SELECT_CARD;
		case MW_SYSTEM|MW_INFORMATION: return CMD_MIXER_SYSTEM_INFORMATION;
		case MW_MODE|MW_ALL: return CMD_WITH_ARG(CMD_MIXER_SET_VIEW_MODE, VIEW_MODE_ALL);
		case MW_MODE|MW_CAPTURE: return CMD_WITH_ARG(CMD_MIXER_SET_VIEW_MODE, VIEW_MODE_CAPTURE);
		case MW_MODE|MW_PLAYBACK: return CMD_WITH_ARG(CMD_MIXER_SET_VIEW_MODE, VIEW_MODE_PLAYBACK);
		case MW_MODE|MW_TOGGLE: return CMD_MIXER_TOGGLE_VIEW_MODE;
		case MW_CONTROL|MW_BALANCE: return CMD_MIXER_BALANCE_CONTROL;
		case MW_NEXT:
		case MW_NEXT|W_NUMBER:
		case MW_PREVIOUS:
		case MW_PREVIOUS|W_NUMBER:
			return ((number < 1 || number > 511) ? 0 :
					CMD_WITH_ARG((words & MW_NEXT
							? CMD_MIXER_NEXT
							: CMD_MIXER_PREVIOUS), number));
		case MW_CONTROL|MW_FOCUS|W_NUMBER:
			return ((number < 1 || number > 512) ? 0 :
					CMD_WITH_ARG(CMD_MIXER_FOCUS_CONTROL, number - 1));
	}

	if (words & MW_LEFT)
		channel |= LEFT;
	if (words & MW_RIGHT)
		channel |= RIGHT;
	if (!channel)
		channel = LEFT|RIGHT;

	switch (words & ~(MW_LEFT|MW_RIGHT)) {
		case MW_CONTROL|MW_UP:
		case MW_CONTROL|MW_UP|W_NUMBER:
		case MW_CONTROL|MW_DOWN:
		case MW_CONTROL|MW_DOWN|W_NUMBER:
			return ((number < 1 || number > 100) ? 0 :
					CMD_WITH_ARG((words & MW_UP
						 ? CMD_MIXER_CONTROL_UP_LEFT
						 : CMD_MIXER_CONTROL_DOWN_LEFT) + channel - 1, number));
		case MW_CONTROL|MW_SET|W_NUMBER:
			return (number > 100 ? 0 :
					CMD_WITH_ARG(CMD_MIXER_CONTROL_SET_PERCENT_LEFT + channel - 1, number));
		case MW_TOGGLE|MW_MUTE:
			return CMD_WITH_ARG(CMD_MIXER_TOGGLE_MUTE, channel);
		case MW_TOGGLE|MW_CAPTURE:
			return CMD_WITH_ARG(CMD_MIXER_TOGGLE_CAPTURE, channel);
	}

	return 0;
}

static int* element_by_name(const char *name) {
	int idx = strlist_index(
#ifdef TRICOLOR_VOLUME_BAR
		"ctl_bar_hi        "
#endif
		"ctl_bar_lo        "
#ifdef TRICOLOR_VOLUME_BAR
		"ctl_bar_mi        "
#endif
		"ctl_capture       "
		"ctl_frame         "
		"ctl_inactive      "
		"ctl_label         "
		"ctl_label_focus   "
		"ctl_label_inactive"
		"ctl_mark_focus    "
		"ctl_mute          "
		"ctl_nocapture     "
		"ctl_nomute        "
		"errormsg          "
		"infomsg           "
		"menu              "
		"menu_selected     "
		"mixer_active      "
		"mixer_frame       "
		"mixer_text        "
		"textbox           "
		"textfield         ", 18, name);

	if (idx < 0) {
#ifndef TRICOLOR_VOLUME_BAR
		if (strlist_index(
			"ctl_bar_hi"
			"ctl_bar_mi", 10, name) >= 0)
			return &errno; // dummy element
#endif
		return NULL;
	}

	return &( ((int*) &attrs)[idx] );
}

static int cfg_bind(char **argv, unsigned int argc) {
	const char *command_name;
	command_enum command = 0;
	unsigned int i;
	int keys[3] = { -1, -1, -1 };
	union {
		command_enum *mixer_bindings;
		uint8_t *textbox_bindings;
	} bind_to = {
		.mixer_bindings = mixer_bindings
	};

	if (argc == 2)
		command_name = argv[1];
	else if (argc == 3) {
		command_name = argv[2];

		if (! strcmp(argv[1], "textbox")) {
			bind_to.textbox_bindings = textbox_bindings;
		}
		else if (! strcmp(argv[1], "mixer"))
			; // bind_to.mixer_bindings = mixer_bindings
		else {
			error_message = _("invalid widget");
			error_cause = argv[1];
			return ERROR_CONFIG;
		}
	}
	else {
		return (argc < 2 ? ERROR_MISSING_ARGUMENTS : ERROR_TOO_MUCH_ARGUMENTS);
	}

	keys[0] = curskey_parse(argv[0]);
	if (keys[0] < 0 || keys[0] >= ARRAY_SIZE(mixer_bindings)) {
		error_message = _("invalid key");
		error_cause = argv[0];
		return ERROR_CONFIG;
	}

	if (keys[0] == KEY_ENTER || keys[0] == '\n' || keys[0] == '\r') {
		keys[0] = KEY_ENTER;
		keys[1] = '\n';
		keys[2] = '\r';
	}

	if (bind_to.textbox_bindings == textbox_bindings)
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

	for (i = 0; i < ARRAY_SIZE(keys) && keys[i] != -1; ++i) {
		if (bind_to.textbox_bindings == textbox_bindings)
			bind_to.textbox_bindings[keys[i]] = command;
		else
			bind_to.mixer_bindings[keys[i]] = command;
	}

	return 0;
}

static int cfg_color(char **argv, unsigned int argc)
{
	short fg_color, bg_color;
	unsigned int i;
	int *element;
	int attr;

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

static int cfg_set(char **argv, unsigned int argc)
{
	char *endptr;

	if (argc == 2) {
		if (! strcmp(argv[0], "mouse_wheel_step")) {
			mouse_wheel_step = strtoumax(argv[1], &endptr, 10);
			if (mouse_wheel_step > 100 || *endptr != '\0') {
				mouse_wheel_step = 1;
				error_message = _("invalid value");
				error_cause = argv[1];
				return ERROR_CONFIG;
			}
		}
		else if (! strcmp(argv[0], "mouse_wheel_focuses_control")) {
			if ((argv[1][0] == '0' || argv[1][0] == '1') && argv[1][1] == '\0')
				mouse_wheel_focuses_control = argv[1][0] - '0';
			else {
				error_message = _("invalid value");
				error_cause = argv[1];
				return ERROR_CONFIG;
			}
		}
		else if (!strcmp(argv[0], "background")) {
			int bg_color = color_by_name(argv[1]);
			if (bg_color == -2) {
				error_message = _("unknown color");
				error_cause = argv[1];
				return ERROR_CONFIG;
			}
			reinit_colors(bg_color);
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

/* Split $line on whitespace, store it in $args, return the argument count.
 * Return 0 for commented lines ('\s*#').
 *
 * This will modify contents of $line.
 */
static unsigned int parse_line(char *line, char **args, unsigned int args_size)
{
	unsigned int count;

	for (count = 0; count < args_size; ++count) {
		while (*line && isspace(*line))
			++line;

		if (*line == '\0')
			break;

		if (*line == '#' && count == 0)
			break;

		args[count] = line;

		while (*line && !isspace(*line))
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
	unsigned int argc = parse_line(line, args, ARRAY_SIZE(args));
	int ret = 0;

	if (argc >= 1) {
		error_cause = NULL;
		//error_message = _("unknown error");

		if (argc >= ARRAY_SIZE(args))
			ret = ERROR_TOO_MUCH_ARGUMENTS;
		else {
			ret = strlist_index(
				"bind "
				"color"
				"set  ", 5, args[0]);
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

void parse_config_file(const char *file_name)
{
	char *buf;
	unsigned int file_size;
	unsigned int lineno;
	unsigned int i;
	char *line;

	endwin(); // print warnings to stderr

	buf = read_file(file_name, &file_size);
	if (!buf) {
		fprintf(stderr, "%s: %s\n", file_name, strerror(errno));
		return;
	}

	curskey_init();
	curskey_define_meta_keys(128);

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
