#include "bindings.h"

#define CNTRL(C) C%32

command_enum textbox_bindings[KEY_MAX] = {
	['\n'] =			CMD_TEXTBOX_CLOSE,
	['\r'] =			CMD_TEXTBOX_CLOSE,
	[27] =				CMD_TEXTBOX_CLOSE,
	[KEY_CANCEL] =		CMD_TEXTBOX_CLOSE,
	[KEY_ENTER] =		CMD_TEXTBOX_CLOSE,
	[KEY_CLOSE] =		CMD_TEXTBOX_CLOSE,
	[KEY_EXIT] =		CMD_TEXTBOX_CLOSE,
	[KEY_DOWN] =		CMD_TEXTBOX_DOWN,
	[KEY_SF] =			CMD_TEXTBOX_DOWN,
	['J'] =				CMD_TEXTBOX_DOWN,
	['j'] =				CMD_TEXTBOX_DOWN,
	['X'] =				CMD_TEXTBOX_DOWN,
	['x'] =				CMD_TEXTBOX_DOWN,
	[KEY_UP] =			CMD_TEXTBOX_UP,
	[KEY_SR] =			CMD_TEXTBOX_UP,
	['K'] =				CMD_TEXTBOX_UP,
	['k'] =				CMD_TEXTBOX_UP,
	['W'] =				CMD_TEXTBOX_UP,
	['w'] =				CMD_TEXTBOX_UP,
	[KEY_LEFT] =		CMD_TEXTBOX_LEFT,
	['H'] =				CMD_TEXTBOX_LEFT,
	['h'] =				CMD_TEXTBOX_LEFT,
	['P'] =				CMD_TEXTBOX_LEFT,
	['p'] =				CMD_TEXTBOX_LEFT,
	[KEY_RIGHT] =		CMD_TEXTBOX_RIGHT,
	['L'] =				CMD_TEXTBOX_RIGHT,
	['l'] =				CMD_TEXTBOX_RIGHT,
	['N'] =				CMD_TEXTBOX_RIGHT,
	['n'] =				CMD_TEXTBOX_RIGHT,
	[KEY_NPAGE] =		CMD_TEXTBOX_PAGE_DOWN,
	[' '] =				CMD_TEXTBOX_PAGE_DOWN,
	[KEY_PPAGE] =		CMD_TEXTBOX_PAGE_UP,
	[KEY_BACKSPACE] =	CMD_TEXTBOX_PAGE_UP,
	['B'] =				CMD_TEXTBOX_PAGE_UP,
	['b'] =				CMD_TEXTBOX_PAGE_UP,
	[KEY_HOME] =		CMD_TEXTBOX_TOP,
	[KEY_BEG] =			CMD_TEXTBOX_TOP,
	[KEY_LL] =			CMD_TEXTBOX_BOTTOM,
	[KEY_END] =			CMD_TEXTBOX_BOTTOM,
	['\t'] =			CMD_TEXTBOX_PAGE_RIGHT,
	[KEY_BTAB] =		CMD_TEXTBOX_PAGE_LEFT,
	// New introduced bindings
	['g'] =				CMD_TEXTBOX_TOP,
	['G'] =				CMD_TEXTBOX_BOTTOM,
	[CNTRL('D')] =		CMD_TEXTBOX_PAGE_DOWN,
	[CNTRL('U')] =		CMD_TEXTBOX_PAGE_UP,
	[CNTRL('N')] =		CMD_TEXTBOX_DOWN,
	[CNTRL('P')] =		CMD_TEXTBOX_UP,
	['<'] =				CMD_TEXTBOX_PAGE_LEFT,
	['>'] =				CMD_TEXTBOX_PAGE_RIGHT,
};

command_enum mixer_bindings[] = {
	[27] =				CMD_MIXER_CLOSE,
	[KEY_CANCEL] =		CMD_MIXER_CLOSE,
	[KEY_F(10)] =		CMD_MIXER_CLOSE,
	[KEY_F(1)] =		CMD_MIXER_HELP,
	[KEY_HELP] =		CMD_MIXER_HELP,
	['H'] =				CMD_MIXER_HELP,
	['h'] =				CMD_MIXER_HELP,
	['?'] =				CMD_MIXER_HELP,
	[KEY_F(2)] =		CMD_MIXER_SYSTEM_INFORMATION,
	['/'] =				CMD_MIXER_SYSTEM_INFORMATION,
	[KEY_F(3)] =		CMD_MIXER_MODE_PLAYBACK,
	[KEY_F(4)] =		CMD_MIXER_MODE_CAPTURE,
	[KEY_F(5)] =		CMD_MIXER_MODE_ALL,
	['\t'] =			CMD_MIXER_MODE_TOGGLE,
	[KEY_F(6)] =		CMD_MIXER_SELECT_CARD,
	['S'] =				CMD_MIXER_SELECT_CARD,
	['s'] =				CMD_MIXER_SELECT_CARD,
	[KEY_REFRESH] =		CMD_MIXER_REFRESH,
	[CNTRL('L')] =		CMD_MIXER_REFRESH,
	['L'] =				CMD_MIXER_REFRESH,
	['l'] =				CMD_MIXER_REFRESH,
	[KEY_LEFT] =		CMD_MIXER_PREVIOUS,
	['p'] =				CMD_MIXER_PREVIOUS,
	['P'] =				CMD_MIXER_PREVIOUS,
	[KEY_RIGHT] =		CMD_MIXER_NEXT,
	['N'] =				CMD_MIXER_NEXT,
	['n'] =				CMD_MIXER_NEXT,
	[KEY_PPAGE] =		CMD_WITH_ARG(CMD_MIXER_CONTROL_UP_N, 5),
	[KEY_NPAGE] =		CMD_WITH_ARG(CMD_MIXER_CONTROL_DOWN_N, 5),
	[KEY_LL] =			CMD_WITH_ARG(CMD_MIXER_CONTROL_N_PERCENT, 0),
	[KEY_END] =			CMD_WITH_ARG(CMD_MIXER_CONTROL_N_PERCENT, 0),
	['0'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_N_PERCENT, 0),
	['1'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_N_PERCENT, 10),
	['2'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_N_PERCENT, 20),
	['3'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_N_PERCENT, 30),
	['4'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_N_PERCENT, 40),
	['5'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_N_PERCENT, 50),
	['6'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_N_PERCENT, 60),
	['7'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_N_PERCENT, 70),
	['8'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_N_PERCENT, 80),
	['9'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_N_PERCENT, 90),
#if 0
	[KEY_BEG] =			CMD_WITH_ARG(CMD_MIXER_CONTROL_N_PERCENT, 100),
	[KEY_HELP] =		CMD_WITH_ARG(CMD_MIXER_CONTROL_N_PERCENT, 100),
#endif
	[KEY_UP] =			CMD_WITH_ARG(CMD_MIXER_CONTROL_UP_N, 1),
	['+'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_UP_N, 1),
	['K'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_UP_N, 1),
	['k'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_UP_N, 1),
	['W'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_UP_N, 1),
	['w'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_UP_N, 1),
	[KEY_DOWN] =		CMD_WITH_ARG(CMD_MIXER_CONTROL_DOWN_N, 1),
	['-'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_DOWN_N, 1),
	['J'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_DOWN_N, 1),
	['j'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_DOWN_N, 1),
	['X'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_DOWN_N, 1),
	['x'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_DOWN_N, 1),
	['Q'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_UP_LEFT_N, 1),
	['q'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_UP_LEFT_N, 1),
	['Y'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_DOWN_LEFT_N, 1),
	['y'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_DOWN_LEFT_N, 1),
	['Z'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_DOWN_LEFT_N, 1),
	['z'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_DOWN_LEFT_N, 1),
	['E'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_UP_RIGHT_N, 1),
	['e'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_UP_RIGHT_N, 1),
	['C'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_DOWN_RIGHT_N, 1),
	['c'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_DOWN_RIGHT_N, 1),
	['M'] =				CMD_MIXER_TOGGLE_MUTE,
	['m'] =				CMD_MIXER_TOGGLE_MUTE,
	['<'] =				CMD_MIXER_TOGGLE_MUTE_LEFT,
	[','] =				CMD_MIXER_TOGGLE_MUTE_LEFT,
	['>'] =				CMD_MIXER_TOGGLE_MUTE_RIGHT,
	['.'] =				CMD_MIXER_TOGGLE_MUTE_RIGHT,
	[' '] =				CMD_MIXER_TOGGLE_CAPTURE,
	[KEY_IC] =			CMD_MIXER_TOGGLE_CAPTURE_LEFT,
	[';'] =				CMD_MIXER_TOGGLE_CAPTURE_LEFT,
	[KEY_DC] =			CMD_MIXER_TOGGLE_CAPTURE_RIGHT,
	['\''] =			CMD_MIXER_TOGGLE_CAPTURE_RIGHT,
	['B'] =				CMD_MIXER_BALANCE_CONTROL,
	['b'] =				CMD_MIXER_BALANCE_CONTROL,
	['='] =				CMD_MIXER_BALANCE_CONTROL,
	// New introduced bindings
	[']'] =				CMD_MIXER_NEXT,
	['}'] =				CMD_MIXER_NEXT,
	['['] =				CMD_MIXER_PREVIOUS,
	['{'] =				CMD_MIXER_PREVIOUS,
	['!'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_FOCUS_N, 1),
	['@'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_FOCUS_N, 2),
	['#'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_FOCUS_N, 3),
	['$'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_FOCUS_N, 4),
	['%'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_FOCUS_N, 5),
	['^'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_FOCUS_N, 6),
	['&'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_FOCUS_N, 7),
	['*'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_FOCUS_N, 8),
	['('] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_FOCUS_N, 9),
	[')'] =				CMD_WITH_ARG(CMD_MIXER_CONTROL_FOCUS_N, 10),
};
