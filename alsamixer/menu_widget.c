#include "menu_widget.h"
#include "colors.h"
#include "utils.h"
#include "bindings.h"

/* Returns
 * - KEY_CANCEL: close is requested
 * - KEY_ENTER: item is selected
 * - -1: no action
 */
int menu_widget_handle_key(MENU *menu, int key)
{
	MEVENT m;

	switch (key) {
	case 27:
	case KEY_CANCEL:
	case 'q':
	case 'Q':
		return KEY_CANCEL;
	case '\n':
	case '\r':
	case KEY_ENTER:
		return KEY_ENTER;

	case KEY_MOUSE:
		switch (menu_driver(menu, KEY_MOUSE)) {
			case E_UNKNOWN_COMMAND:
				/* If you double-click an item a REQ_TOGGLE_ITEM is generated
				 * and E_UNKNOWN_COMMAND is returned. (man menu_driver) */
				return KEY_ENTER;
			case E_REQUEST_DENIED:
				/* If menu did not handle KEY_MOUSE is has to be removed from
				 * input queue to prevent an infinite loop. */
				key = wgetch(menu_win(menu));
				if (key == KEY_MOUSE) {
					if (getmouse(&m) == ERR)
						return -1;
					if (m.bstate & (BUTTON4_PRESSED|BUTTON4_CLICKED))
						menu_driver(menu, REQ_UP_ITEM);
#if NCURSES_MOUSE_VERSION > 1
					else if (m.bstate & (BUTTON5_PRESSED|BUTTON5_CLICKED))
						menu_driver(menu, REQ_DOWN_ITEM);
#endif
					else
						return KEY_CANCEL;
				}
				else if (key > 0)
					ungetch(key);
		}
		return -1;

	default:
		if (key < ARRAY_SIZE(textbox_bindings)) {
			key = textbox_bindings[key];
			if (key >= CMD_TEXTBOX___MIN_MENU_COMMAND &&
					key <= CMD_TEXTBOX___MAX_MENU_COMMAND)
				menu_driver(menu, key + KEY_MAX);
		}

		return -1;
	}
}
