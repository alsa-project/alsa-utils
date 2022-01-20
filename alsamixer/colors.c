/*
 * colors.c - color and attribute definitions
 * Copyright (c) 1998,1999 Tim Janik
 *                         Jaroslav Kysela <perex@perex.cz>
 * Copyright (c) 2009      Clemens Ladisch <clemens@ladisch.de>
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
#include CURSESINC
#include "colors.h"

struct attributes attrs;
static short background_color = -1;

int get_color_pair(short fg, short bg)
{
	static int color_pairs_defined = 0;
	short i, pair_fg, pair_bg;

	for (i = 1; i <= color_pairs_defined; ++i) {
		if (OK == pair_content(i, &pair_fg, &pair_bg))
			if (pair_fg == fg && pair_bg == bg)
				return COLOR_PAIR(i);
	}

	if (color_pairs_defined + 1 < COLOR_PAIRS) {
		++color_pairs_defined;
		init_pair(color_pairs_defined, fg, bg);
		return COLOR_PAIR(color_pairs_defined);
	}

	return 0;
}

void reinit_colors(short bg)
{
	if (bg == background_color)
		return;
	init_pair(1, COLOR_CYAN, bg);
	init_pair(2, COLOR_YELLOW, bg);
	init_pair(4, COLOR_RED, bg);
	init_pair(5, COLOR_WHITE, bg);
	background_color = bg;
}

void init_colors(int use_color)
{
	if (!!has_colors() == !!use_color) {
		start_color();
		use_default_colors();

		get_color_pair(COLOR_CYAN, background_color); // COLOR_PAIR(1)
		get_color_pair(COLOR_YELLOW, background_color);
		get_color_pair(COLOR_WHITE, COLOR_GREEN);
		get_color_pair(COLOR_RED, background_color);
		get_color_pair(COLOR_WHITE, background_color);
		get_color_pair(COLOR_WHITE, COLOR_BLUE);
		get_color_pair(COLOR_RED, COLOR_BLUE);
		get_color_pair(COLOR_GREEN, COLOR_GREEN);
		get_color_pair(COLOR_WHITE, COLOR_RED); // COLOR_PAIR(9)
#ifdef TRICOLOR_VOLUME_BAR
		get_color_pair(COLOR_WHITE, COLOR_WHITE);
		get_color_pair(COLOR_RED, COLOR_RED);
#endif

		attrs = (struct attributes) {
			.mixer_frame = COLOR_PAIR(1),
			.mixer_text = COLOR_PAIR(1),
			.mixer_active = A_BOLD | COLOR_PAIR(2),
			.ctl_frame = A_BOLD | COLOR_PAIR(1),
			.ctl_mute = COLOR_PAIR(1),
			.ctl_nomute = A_BOLD | COLOR_PAIR(3),
			.ctl_capture = A_BOLD | COLOR_PAIR(4),
			.ctl_nocapture = COLOR_PAIR(5),
			.ctl_label = A_BOLD | COLOR_PAIR(6),
			.ctl_label_focus = A_BOLD | COLOR_PAIR(7),
			.ctl_mark_focus = A_BOLD | COLOR_PAIR(4),
			.ctl_bar_lo = A_BOLD | COLOR_PAIR(8),
#ifdef TRICOLOR_VOLUME_BAR
			.ctl_bar_mi = A_BOLD | COLOR_PAIR(10),
			.ctl_bar_hi = A_BOLD | COLOR_PAIR(11),
#endif
			.ctl_inactive = COLOR_PAIR(5),
			.ctl_label_inactive = A_REVERSE | COLOR_PAIR(5),
			.errormsg = A_BOLD | COLOR_PAIR(9),
			.infomsg = A_BOLD | COLOR_PAIR(6),
			.textbox = A_BOLD | COLOR_PAIR(6),
			.textfield = A_REVERSE | COLOR_PAIR(5),
			.menu = A_BOLD | COLOR_PAIR(6),
			.menu_selected = A_REVERSE | COLOR_PAIR(6)
		};
	} else {
		attrs = (struct attributes) {
			.mixer_frame = A_NORMAL,
			.mixer_text = A_NORMAL,
			.mixer_active = A_BOLD,
			.ctl_frame = A_BOLD,
			.ctl_mute = A_NORMAL,
			.ctl_nomute = A_BOLD,
			.ctl_capture = A_BOLD,
			.ctl_nocapture = A_NORMAL,
			.ctl_label = A_REVERSE,
			.ctl_label_focus = A_REVERSE | A_BOLD,
			.ctl_mark_focus = A_BOLD,
			.ctl_bar_lo = A_BOLD,
#ifdef TRICOLOR_VOLUME_BAR
			.ctl_bar_mi = A_BOLD,
			.ctl_bar_hi = A_BOLD,
#endif
			.ctl_inactive = A_NORMAL,
			.ctl_label_inactive = A_REVERSE,
			.errormsg = A_STANDOUT,
			.infomsg = A_NORMAL,
			.textbox = A_NORMAL,
			.textfield = A_REVERSE,
			.menu = A_NORMAL,
			.menu_selected = A_REVERSE,
		};
	}
}
