#ifndef BINDINGS_H_INCLUDED
#define BINDINGS_H_INCLUDED

#include CURSESINC
#include <menu.h>
#include <stdint.h>

typedef uint16_t command_enum;
extern command_enum mixer_bindings[KEY_MAX];
extern command_enum textbox_bindings[KEY_MAX];

/* Some commands, like `CMD_MIXER_CONTROL_UP_N` take a numeric argument.
 * The argument is stored in the higher bits, the command itself in the lower
 * bits.
 *
 * These macros can be used to bind/extract arguments to/from a command.
 */

#define CMD_WITH_ARG(CMD, ARG) \
	((CMD) + ((ARG) << 9))

#define CMD_GET_CMD(CMD) \
	((CMD) & 0x1FF)

#define CMD_GET_ARG(CMD) \
	((CMD) >> 9)

enum mixer_command {
	// Keep those in the same order as displayed on screen
	CMD_MIXER_HELP = 1,
	CMD_MIXER_SYSTEM_INFORMATION,
	CMD_MIXER_SELECT_CARD,
	CMD_MIXER_CLOSE,

	CMD_MIXER_SET_VIEW_MODE,
	CMD_MIXER_TOGGLE_VIEW_MODE,
	CMD_MIXER_REFRESH,
	CMD_MIXER_PREVIOUS,
	CMD_MIXER_NEXT,
	CMD_MIXER_BALANCE_CONTROL,
	CMD_MIXER_CONTROL_FOCUS_N,

	// Keep order: left, right, both
	CMD_MIXER_CONTROL_N_PERCENT_LEFT,
	CMD_MIXER_CONTROL_N_PERCENT_RIGHT,
	CMD_MIXER_CONTROL_N_PERCENT,

	// Keep order: left, right, both
	CMD_MIXER_CONTROL_UP_LEFT_N,
	CMD_MIXER_CONTROL_UP_RIGHT_N,
	CMD_MIXER_CONTROL_UP_N,

	// Keep order: left, right, both
	CMD_MIXER_CONTROL_DOWN_LEFT_N,
	CMD_MIXER_CONTROL_DOWN_RIGHT_N,
	CMD_MIXER_CONTROL_DOWN_N,

	// Keep order: left, right, both
	CMD_MIXER_TOGGLE_MUTE_LEFT,
	CMD_MIXER_TOGGLE_MUTE_RIGHT,
	CMD_MIXER_TOGGLE_MUTE,

	// Keep order: left, right, both
	CMD_MIXER_TOGGLE_CAPTURE_LEFT,
	CMD_MIXER_TOGGLE_CAPTURE_RIGHT,
	CMD_MIXER_TOGGLE_CAPTURE,

	// Mouse
	CMD_MIXER_MOUSE_CLICK_MUTE,
	CMD_MIXER_MOUSE_CLICK_VOLUME_BAR,
	CMD_MIXER_MOUSE_CLICK_CONTROL_ENUM,
};

enum textbox_command {
	// These commands are also used inside the menu widget.
	// `REQ_* - KEY_MAX` evalutes to something inbetween 1 and 17.
	// KEY_MAX is substracted because we want to fit the value in 8 bits.
	CMD_TEXTBOX___MIN_MENU_COMMAND = MIN_MENU_COMMAND - KEY_MAX,
	CMD_TEXTBOX_TOP = REQ_FIRST_ITEM - KEY_MAX,
	CMD_TEXTBOX_BOTTOM = REQ_LAST_ITEM - KEY_MAX,
	CMD_TEXTBOX_LEFT = REQ_LEFT_ITEM - KEY_MAX,
	CMD_TEXTBOX_RIGHT = REQ_RIGHT_ITEM - KEY_MAX,
	CMD_TEXTBOX_UP = REQ_UP_ITEM - KEY_MAX,
	CMD_TEXTBOX_DOWN = REQ_DOWN_ITEM - KEY_MAX,
	CMD_TEXTBOX_PAGE_DOWN = REQ_SCR_DPAGE - KEY_MAX,
	CMD_TEXTBOX_PAGE_UP = REQ_SCR_UPAGE - KEY_MAX,
	CMD_TEXTBOX___MAX_MENU_COMMAND = MAX_MENU_COMMAND - KEY_MAX,
	CMD_TEXTBOX_PAGE_LEFT,
	CMD_TEXTBOX_PAGE_RIGHT,
	CMD_TEXTBOX_CLOSE,
};

#endif
