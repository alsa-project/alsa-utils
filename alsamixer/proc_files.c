/*
 * proc_files.c - shows ALSA system information files
 * Copyright (c) Clemens Ladisch <clemens@ladisch.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
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

#include "aconfig.h"
#include <assert.h>
#include <menu.h>
#include <unistd.h>
#include "gettext_curses.h"
#include "utils.h"
#include "die.h"
#include "mem.h"
#include "colors.h"
#include "widget.h"
#include "textbox.h"
#include "proc_files.h"
#include "menu_widget.h"

static struct widget proc_widget;
static ITEM *items[7];
static unsigned int items_count;
static MENU *menu;

static void on_handle_key(int key)
{
	ITEM *item;

	switch (menu_widget_handle_key(menu, key)) {
		case KEY_ENTER:
			item = current_item(menu);
			if (item)
				show_textfile(item_name(item));
			break;
		case KEY_CANCEL:
			proc_widget.close();
			break;
	}
}

static void create(void)
{
	menu_widget_create(&proc_widget, menu, _("Select File"));
}

static void on_close(void)
{
	unsigned int i;

	unpost_menu(menu);
	free_menu(menu);
	for (i = 0; i < items_count; ++i)
		free_item(items[i]);
	widget_free(&proc_widget);
}

static void add_item(const char *file_name)
{
	if (access(file_name, F_OK) == 0) {
		items[items_count] = new_item(file_name, NULL);
		if (!items[items_count])
			fatal_error("cannot create menu item");
		++items_count;
		assert(items_count < ARRAY_SIZE(items));
	}
}

static struct widget proc_widget = {
	.handle_key = on_handle_key,
	.window_size_changed = create,
	.close = on_close,
};

void create_proc_files_list(void)
{
	items_count = 0;
	add_item("/proc/asound/version");
	add_item("/proc/asound/cards");
	add_item("/proc/asound/devices");
	add_item("/proc/asound/oss/devices");
	add_item("/proc/asound/timers");
	add_item("/proc/asound/pcm");
	items[items_count] = NULL;

	menu = new_menu(items);
	if (!menu)
		fatal_error("cannot create menu");
	set_menu_fore(menu, attrs.menu_selected);
	set_menu_back(menu, attrs.menu);
	set_menu_mark(menu, NULL);
	menu_opts_off(menu, O_SHOWDESC);

	create();
}
