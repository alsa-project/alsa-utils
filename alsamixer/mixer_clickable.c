#include <stdlib.h>
#include <string.h>
#include "mixer_clickable.h"

extern int screen_cols;
extern int screen_lines;

static struct clickable_rect *clickable_rects = NULL;
static unsigned int clickable_rects_count = 0;
static unsigned int last_rect = 0;

/* Using 0 instead of -1 for marking free rectangles allows us to use
 * memset for `freeing` all rectangles at once.
 * Zero is actually a valid coordinate in ncurses, but since we don't have
 * any clickables in the top line this is fine. */
#define FREE_MARKER 0
#define RECT_IS_FREE(RECT) ((RECT).y1 == FREE_MARKER)
#define RECT_FREE(RECT) ((RECT).y1 = FREE_MARKER)

void clickable_set(int y1, int x1, int y2, int x2, command_enum command, int arg1) {
	struct clickable_rect* tmp;
	unsigned int i;

	for (i = last_rect; i < clickable_rects_count; ++i) {
		if (RECT_IS_FREE(clickable_rects[i])) {
			last_rect = i;
			goto SET_CLICKABLE_DATA;
		}
	}

	for (i = 0; i < last_rect; ++i) {
		if (RECT_IS_FREE(clickable_rects[i])) {
			last_rect = i;
			goto SET_CLICKABLE_DATA;
		}
	}

	last_rect = clickable_rects_count;
	tmp = realloc(clickable_rects, (clickable_rects_count + 8) * sizeof(*clickable_rects));
	if (!tmp) {
		free(clickable_rects);
		clickable_rects = NULL;
		clickable_rects_count = 0;
		last_rect = 0;
		return;
	}
	clickable_rects = tmp;
#if FREE_MARKER == 0
	memset(clickable_rects + clickable_rects_count, 0, 8 * sizeof(*clickable_rects));
#else
	for (i = clickable_rects_count; i < clickable_rects_count + 8; ++i)
		RECT_FREE(clickable_rects[i]);
#endif
	clickable_rects_count += 8;

SET_CLICKABLE_DATA:
	clickable_rects[last_rect] = (struct clickable_rect) {
		.y1 = y1,
		.x1 = x1,
		.x2 = x2,
		.y2 = y2,
		.command = command,
		.arg1 = arg1
	};
}

void clickable_set_relative(WINDOW *win, int y1, int x1, int y2, int x2, command_enum command, int arg1) {
	int y, x;
	getyx(win, y, x);
	y1 = y + y1;
	x1 = x + x1;
	y2 = y + y2;
	x2 = x + x2;
	clickable_set(y1, x1, y2, x2, command, arg1);
}

void clickable_clear(int y1, int x1, int y2, int x2) {
#define IS_IN_RECT(Y, X) (Y >= y1 && Y <= y2 && X >= x1 && X <= x2)
	unsigned int i;

	if (x1 == 0 && x2 == -1 && y2 == -1) {
		if (y1 == 0) {
			// Optimize case: clear all
#if FREE_MARKER == 0
			if (clickable_rects)
				memset(clickable_rects, 0,
						clickable_rects_count * sizeof(*clickable_rects));
#else
			for (i = 0; i < clickable_rects_count; ++i)
				RECT_FREE(clickable_rects[i]);
#endif
		}
		else {
			// Optimize case: clear all lines beyond y1
			for (i = 0; i < clickable_rects_count; ++i) {
				if (clickable_rects[i].y2 >= y1)
					RECT_FREE(clickable_rects[i]);
			}
		}
		return;
	}

	if (y2 < 0)
		y2 = screen_lines + y2 + 1;
	if (x2 < 0)
		x2 = screen_cols + x2 + 1;

	for (i = 0; i < clickable_rects_count; ++i) {
		if (!RECT_IS_FREE(clickable_rects[i]) && (
				IS_IN_RECT(clickable_rects[i].y1, clickable_rects[i].x1) ||
				IS_IN_RECT(clickable_rects[i].y2, clickable_rects[i].x2)
			))
		{
			RECT_FREE(clickable_rects[i]);
		}
	}
}

struct clickable_rect* clickable_find(int y, int x) {
	unsigned int i;

	for (i = 0; i < clickable_rects_count; ++i) {
		if (
				!RECT_IS_FREE(clickable_rects[i]) &&
				y >= clickable_rects[i].y1 &&
				x >= clickable_rects[i].x1 &&
				y <= clickable_rects[i].y2 &&
				x <= clickable_rects[i].x2
			)
		{
			return &clickable_rects[i];
		}
	}

	return NULL;
}
