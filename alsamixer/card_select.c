/*
 * card_select.c - select a card by list or device name
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
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <alsa/asoundlib.h>
#include <menu.h>
#include "gettext_curses.h"
#include "die.h"
#include "mem.h"
#include "utils.h"
#include "colors.h"
#include "widget.h"
#include "menu_widget.h"
#include "mixer_widget.h"
#include "device_name.h"
#include "card_select.h"

struct card {
	struct card *next;
	char *indexstr;
	char *name;
	char *device_name;
};

static struct widget list_widget;
static struct card first_card;
static ITEM **items;
static MENU *menu;
static ITEM *initial_item;

static void on_key_enter(void)
{
	ITEM *item = current_item(menu);
	if (item) {
		struct card *card = item_userptr(item);
		if (card->device_name) {
			if (select_card_by_name(card->device_name))
				list_widget.close();
		} else {
			create_device_name_form();
		}
	}
}

static void on_handle_key(int key)
{
	switch (menu_widget_handle_key(menu, key)) {
		case KEY_ENTER:
			on_key_enter();
			break;
		case KEY_CANCEL:
			list_widget.close();
			break;
	}
}

static void create(void)
{
	menu_widget_create(&list_widget, menu, _("Sound Card"));
}

void close_card_select_list(void)
{
	unsigned int i;
	struct card *card, *next_card;

	unpost_menu(menu);
	free_menu(menu);
	for (i = 0; items[i]; ++i)
		free_item(items[i]);
	free(items);
	for (card = first_card.next; card; card = next_card) {
		next_card = card->next;
		free(card->indexstr);
		free(card->name);
		free(card->device_name);
		free(card);
	}
	widget_free(&list_widget);
}

static struct widget list_widget = {
	.handle_key = on_handle_key,
	.window_size_changed = create,
	.close = close_card_select_list,
};

static int get_cards(void)
{
	int count, number, err;
	snd_ctl_t *ctl;
	snd_ctl_card_info_t *info;
	char buf[32];
	struct card *card, *prev_card;

	first_card.indexstr = "-";
	first_card.name = _("(default)");
	first_card.device_name = "default";
	count = 1;

	snd_ctl_card_info_alloca(&info);
	prev_card = &first_card;
	number = -1;
	for (;;) {
		err = snd_card_next(&number);
		if (err < 0)
			fatal_alsa_error(_("cannot enumerate sound cards"), err);
		if (number < 0)
			break;
#if defined(SND_LIB_VER) && SND_LIB_VER(1, 2, 5) <= SND_LIB_VERSION
		sprintf(buf, "sysdefault:%d", number);
#else
		sprintf(buf, "hw:%d", number);
#endif
		err = snd_ctl_open(&ctl, buf, 0);
		if (err < 0)
			continue;
		err = snd_ctl_card_info(ctl, info);
		snd_ctl_close(ctl);
		if (err < 0)
			continue;
		card = ccalloc(1, sizeof *card);
		card->device_name = cstrdup(buf);
		card->indexstr = cstrdup(buf + 3);
		card->name = cstrdup(snd_ctl_card_info_get_name(info));
		prev_card->next = card;
		prev_card = card;
		++count;
	}

	card = ccalloc(1, sizeof *card);
	card->indexstr = cstrdup(" ");
	card->name = cstrdup(_("enter device name..."));
	prev_card->next = card;
	++count;

	return count;
}

static void create_list_items(int cards)
{
	int i;
	struct card *card;
	ITEM *item;

	initial_item = NULL;
	items = ccalloc(cards + 1, sizeof(ITEM*));
	i = 0;
	for (card = &first_card; card; card = card->next) {
		item = new_item(card->indexstr, card->name);
		if (!item)
			fatal_error("cannot create menu item");
		set_item_userptr(item, card);
		items[i++] = item;
		if (!initial_item &&
		    mixer_device_name &&
		    (!card->device_name ||
		     !strcmp(card->device_name, mixer_device_name)))
			initial_item = item;
	}
	assert(i == cards);
}

void create_card_select_list(void)
{
	int cards;

	cards = get_cards();
	create_list_items(cards);

	menu = new_menu(items);
	if (!menu)
		fatal_error("cannot create menu");
	set_menu_fore(menu, attrs.menu_selected);
	set_menu_back(menu, attrs.menu);
	set_menu_mark(menu, NULL);
	if (initial_item)
		set_current_item(menu, initial_item);
	set_menu_spacing(menu, 2, 1, 1);
	menu_opts_on(menu, O_SHOWDESC);

	create();
}
