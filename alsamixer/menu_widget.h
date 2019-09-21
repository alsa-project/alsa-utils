#ifndef MENU_WIDGET_H_INCLUDED
#define MENU_WIDGET_H_INCLUDED

#include "widget.h"
#include <menu.h>

void menu_widget_create(struct widget *widget, MENU *menu, const char *title);
int menu_widget_handle_key(MENU *menu, int key);

#endif
