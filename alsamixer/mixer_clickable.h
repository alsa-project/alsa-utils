#ifndef MIXER_CLICKABLE_H
#define MIXER_CLICKABLE_H

#include CURSESINC
#include "bindings.h"

struct clickable_rect {
	short y1;
	short x1;
	short y2;
	short x2;
	command_enum command;
	int arg1;
};

void clickable_set(int y1, int x1, int y2, int x2, command_enum command, int arg1);
void clickable_set_relative(WINDOW *win, int y1, int x1, int y2, int x2, command_enum command, int arg1);
void clickable_clear(int y1, int x1, int y2, int x2);
struct clickable_rect* clickable_find(int y, int x);

#endif
