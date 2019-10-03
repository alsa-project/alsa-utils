/*
 * mixer_widget.c - mixer widget and keys handling
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <alsa/asoundlib.h>
#include "gettext_curses.h"
#include "version.h"
#include "utils.h"
#include "die.h"
#include "mem.h"
#include "colors.h"
#include "widget.h"
#include "textbox.h"
#include "proc_files.h"
#include "card_select.h"
#include "volume_mapping.h"
#include "mixer_clickable.h"
#include "mixer_controls.h"
#include "mixer_display.h"
#include "mixer_widget.h"
#include "bindings.h"

snd_mixer_t *mixer;
char *mixer_device_name;
bool unplugged;

struct widget mixer_widget;

enum view_mode view_mode;

int focus_control_index;
snd_mixer_selem_id_t *current_selem_id;
unsigned int current_control_flags;

bool control_values_changed;
bool controls_changed;

unsigned int mouse_wheel_step = 1;
bool mouse_wheel_focuses_control = 1;

static int elem_callback(snd_mixer_elem_t *elem, unsigned int mask)
{
	if (mask == SND_CTL_EVENT_MASK_REMOVE) {
		controls_changed = TRUE;
	} else {
		if (mask & SND_CTL_EVENT_MASK_VALUE)
			control_values_changed = TRUE;

		if (mask & SND_CTL_EVENT_MASK_INFO)
			controls_changed = TRUE;
	}

	return 0;
}

static int mixer_callback(snd_mixer_t *mixer, unsigned int mask, snd_mixer_elem_t *elem)
{
	if (mask & SND_CTL_EVENT_MASK_ADD) {
		snd_mixer_elem_set_callback(elem, elem_callback);
		controls_changed = TRUE;
	}
	return 0;
}

void create_mixer_object(struct snd_mixer_selem_regopt *selem_regopt)
{
	int err;

	err = snd_mixer_open(&mixer, 0);
	if (err < 0)
		fatal_alsa_error(_("cannot open mixer"), err);

	mixer_device_name = cstrdup(selem_regopt->device);
	err = snd_mixer_selem_register(mixer, selem_regopt, NULL);
	if (err < 0)
		fatal_alsa_error(_("cannot open mixer"), err);

	snd_mixer_set_callback(mixer, mixer_callback);

	err = snd_mixer_load(mixer);
	if (err < 0)
		fatal_alsa_error(_("cannot load mixer controls"), err);

	err = snd_mixer_selem_id_malloc(&current_selem_id);
	if (err < 0)
		fatal_error("out of memory");
}

static void set_view_mode(enum view_mode m)
{
	view_mode = m;
	create_controls();
}

static void close_hctl(void)
{
	free_controls();
	if (mixer_device_name) {
		snd_mixer_detach(mixer, mixer_device_name);
		free(mixer_device_name);
		mixer_device_name = NULL;
	}
}

static void check_unplugged(void)
{
	snd_hctl_t *hctl;
	snd_ctl_t *ctl;
	unsigned int state;
	int err;

	unplugged = FALSE;
	if (mixer_device_name) {
		err = snd_mixer_get_hctl(mixer, mixer_device_name, &hctl);
		if (err >= 0) {
			ctl = snd_hctl_ctl(hctl);
			/* just any random function that does an ioctl() */
			err = snd_ctl_get_power_state(ctl, &state);
			if (err == -ENODEV)
				unplugged = TRUE;
		}
	}
}

void close_mixer_device(void)
{
	check_unplugged();
	close_hctl();

	display_card_info();
	set_view_mode(view_mode);
}

bool select_card_by_name(const char *device_name)
{
	int err;
	bool opened;
	char *msg;

	close_hctl();
	unplugged = FALSE;

	opened = FALSE;
	if (device_name) {
		err = snd_mixer_attach(mixer, device_name);
		if (err >= 0)
			opened = TRUE;
		else {
			msg = casprintf(_("Cannot open mixer device '%s'."), device_name);
			show_alsa_error(msg, err);
			free(msg);
		}
	}
	if (opened) {
		mixer_device_name = cstrdup(device_name);

		err = snd_mixer_load(mixer);
		if (err < 0)
			fatal_alsa_error(_("cannot load mixer controls"), err);
	}

	display_card_info();
	set_view_mode(view_mode);
	return opened;
}

static void show_help(void)
{
	const char *help[] = {
		_("Esc     Exit"),
		_("F1 ? H  Help"),
		_("F2 /    System information"),
		_("F3      Show playback controls"),
		_("F4      Show capture controls"),
		_("F5      Show all controls"),
		_("Tab     Toggle view mode (F3/F4/F5)"),
		_("F6 S    Select sound card"),
		_("L       Redraw screen"),
		"",
		_("Left    Move to the previous control"),
		_("Right   Move to the next control"),
		"",
		_("Up/Down    Change volume"),
		_("+ -        Change volume"),
		_("Page Up/Dn Change volume in big steps"),
		_("End        Set volume to 0%"),
		_("0-9        Set volume to 0%-90%"),
		_("Q W E      Increase left/both/right volumes"),
		/* TRANSLATORS: or Y instead of Z */
		_("Z X C      Decrease left/both/right volumes"),
		_("B          Balance left and right volumes"),
		"",
		_("M          Toggle mute"),
		/* TRANSLATORS: or , . */
		_("< >        Toggle left/right mute"),
		"",
		_("Space      Toggle capture"),
		/* TRANSLATORS: or Insert Delete */
		_("; '        Toggle left/right capture"),
		"",
		_("Authors:"),
		_("  Tim Janik"),
		_("  Jaroslav Kysela <perex@perex.cz>"),
		_("  Clemens Ladisch <clemens@ladisch.de>"),
	};
	show_text(help, ARRAY_SIZE(help), _("Help"));
}

void refocus_control(void)
{
	if (focus_control_index < controls_count) {
		snd_mixer_selem_get_id(controls[focus_control_index].elem, current_selem_id);
		current_control_flags = controls[focus_control_index].flags;
	}

	display_controls();
}

static struct control *get_focus_control(unsigned int type)
{
	if (focus_control_index >= 0 &&
	    focus_control_index < controls_count &&
	    (controls[focus_control_index].flags & IS_ACTIVE) &&
	    (controls[focus_control_index].flags & type))
		return &controls[focus_control_index];
	else
		return NULL;
}

static void change_enum_to_percent(struct control *control, int value)
{
	unsigned int i;
	unsigned int index;
	unsigned int new_index;
	int items;
	int err;

	i = ffs(control->enum_channel_bits) - 1;
	err = snd_mixer_selem_get_enum_item(control->elem, i, &index);
	if (err < 0)
		return;
	new_index = index;
	if (value == 0) {
		new_index = 0;
	} else if (value == 100) {
		items = snd_mixer_selem_get_enum_items(control->elem);
		if (items < 1)
			return;
		new_index = items - 1;
	}
	if (new_index == index)
		return;
	for (i = 0; i <= SND_MIXER_SCHN_LAST; ++i)
		if (control->enum_channel_bits & (1 << i))
			snd_mixer_selem_set_enum_item(control->elem, i, new_index);
}

static void change_enum_relative(struct control *control, int delta)
{
	int items;
	unsigned int i;
	unsigned int index;
	int new_index;
	int err;

	items = snd_mixer_selem_get_enum_items(control->elem);
	if (items < 1)
		return;
	err = snd_mixer_selem_get_enum_item(control->elem, 0, &index);
	if (err < 0)
		return;
	new_index = (int)index + delta;
	if (new_index < 0)
		new_index = 0;
	else if (new_index >= items)
		new_index = items - 1;
	if (new_index == index)
		return;
	for (i = 0; i <= SND_MIXER_SCHN_LAST; ++i)
		if (control->enum_channel_bits & (1 << i))
			snd_mixer_selem_set_enum_item(control->elem, i, new_index);
}

static void change_volume_to_percent(struct control *control, int value, unsigned int channels)
{
	int (*set_func)(snd_mixer_elem_t *, snd_mixer_selem_channel_id_t, double, int);

	if (!(control->flags & HAS_VOLUME_1))
		channels = LEFT;
	if (control->flags & TYPE_PVOLUME)
		set_func = set_normalized_playback_volume;
	else
		set_func = set_normalized_capture_volume;
	if (channels & LEFT)
		set_func(control->elem, control->volume_channels[0], value / 100.0, 0);
	if (channels & RIGHT)
		set_func(control->elem, control->volume_channels[1], value / 100.0, 0);
}

static double clamp_volume(double v)
{
	if (v < 0)
		return 0;
	if (v > 1)
		return 1;
	return v;
}

static void change_volume_relative(struct control *control, int delta, unsigned int channels)
{
	double (*get_func)(snd_mixer_elem_t *, snd_mixer_selem_channel_id_t);
	int (*set_func)(snd_mixer_elem_t *, snd_mixer_selem_channel_id_t, double, int);
	double left, right;
	int dir;

	if (!(control->flags & HAS_VOLUME_1))
		channels = LEFT;
	if (control->flags & TYPE_PVOLUME) {
		get_func = get_normalized_playback_volume;
		set_func = set_normalized_playback_volume;
	} else {
		get_func = get_normalized_capture_volume;
		set_func = set_normalized_capture_volume;
	}
	if (channels & LEFT)
		left = get_func(control->elem, control->volume_channels[0]);
	if (channels & RIGHT)
		right = get_func(control->elem, control->volume_channels[1]);
	dir = delta > 0 ? 1 : -1;
	if (channels & LEFT) {
		left = clamp_volume(left + delta / 100.0);
		set_func(control->elem, control->volume_channels[0], left, dir);
	}
	if (channels & RIGHT) {
		right = clamp_volume(right + delta / 100.0);
		set_func(control->elem, control->volume_channels[1], right, dir);
	}
}

static void change_control_to_percent(int value, unsigned int channels)
{
	struct control *control;

	control = get_focus_control(TYPE_PVOLUME | TYPE_CVOLUME | TYPE_ENUM);
	if (!control)
		return;
	if (control->flags & TYPE_ENUM)
		change_enum_to_percent(control, value);
	else
		change_volume_to_percent(control, value, channels);
	display_controls();
}

static void change_control_relative(int delta, unsigned int channels)
{
	struct control *control;

	control = get_focus_control(TYPE_PVOLUME | TYPE_CVOLUME | TYPE_ENUM);
	if (!control)
		return;
	if (control->flags & TYPE_ENUM)
		change_enum_relative(control, delta);
	else
		change_volume_relative(control, delta, channels);
	display_controls();
}

static void toggle_switches(unsigned int type, unsigned int channels)
{
	struct control *control;
	unsigned int switch_1_mask;
	int (*get_func)(snd_mixer_elem_t *, snd_mixer_selem_channel_id_t, int *);
	int (*set_func)(snd_mixer_elem_t *, snd_mixer_selem_channel_id_t, int);
	snd_mixer_selem_channel_id_t channel_ids[2];
	int left, right;
	int err;

	control = get_focus_control(type);
	if (!control)
		return;
	if (type == TYPE_PSWITCH) {
		switch_1_mask = HAS_PSWITCH_1;
		get_func = snd_mixer_selem_get_playback_switch;
		set_func = snd_mixer_selem_set_playback_switch;
		channel_ids[0] = control->pswitch_channels[0];
		channel_ids[1] = control->pswitch_channels[1];
	} else {
		switch_1_mask = HAS_CSWITCH_1;
		get_func = snd_mixer_selem_get_capture_switch;
		set_func = snd_mixer_selem_set_capture_switch;
		channel_ids[0] = control->cswitch_channels[0];
		channel_ids[1] = control->cswitch_channels[1];
	}
	if (!(control->flags & switch_1_mask))
		channels = LEFT;
	if (channels & LEFT) {
		err = get_func(control->elem, channel_ids[0], &left);
		if (err < 0)
			return;
	}
	if (channels & RIGHT) {
		err = get_func(control->elem, channel_ids[1], &right);
		if (err < 0)
			return;
	}
	if (channels & LEFT)
		set_func(control->elem, channel_ids[0], !left);
	if (channels & RIGHT)
		set_func(control->elem, channel_ids[1], !right);
	display_controls();
}

static void toggle_mute(unsigned int channels)
{
	toggle_switches(TYPE_PSWITCH, channels);
}

static void toggle_capture(unsigned int channels)
{
	toggle_switches(TYPE_CSWITCH, channels);
}

static void balance_volumes(void)
{
	struct control *control;
	double left, right;

	control = get_focus_control(TYPE_PVOLUME | TYPE_CVOLUME);
	if (!control || !(control->flags & HAS_VOLUME_1))
		return;
	if (control->flags & TYPE_PVOLUME) {
		left = get_normalized_playback_volume(control->elem, control->volume_channels[0]);
		right = get_normalized_playback_volume(control->elem, control->volume_channels[1]);
	} else {
		left = get_normalized_capture_volume(control->elem, control->volume_channels[0]);
		right = get_normalized_capture_volume(control->elem, control->volume_channels[1]);
	}
	left = (left + right) / 2;
	if (control->flags & TYPE_PVOLUME) {
		set_normalized_playback_volume(control->elem, control->volume_channels[0], left, 0);
		set_normalized_playback_volume(control->elem, control->volume_channels[1], left, 0);
	} else {
		set_normalized_capture_volume(control->elem, control->volume_channels[0], left, 0);
		set_normalized_capture_volume(control->elem, control->volume_channels[1], left, 0);
	}
	display_controls();
}

static int on_mouse_key() {
	MEVENT m;
	command_enum cmd = 0;
	unsigned int channels = LEFT | RIGHT;
	unsigned int index;
	struct control *control;
	struct clickable_rect *rect;

	if (getmouse(&m) == ERR)
		return 0;

	if (m.bstate & (
				BUTTON1_PRESSED|BUTTON1_RELEASED|
				BUTTON2_PRESSED|BUTTON2_RELEASED|
				BUTTON3_PRESSED|BUTTON3_RELEASED))
		return 0;

	rect = clickable_find(m.y, m.x);
	if (rect)
		cmd = rect->command;

#if NCURSES_MOUSE_VERSION > 1
	if (m.bstate & (BUTTON4_CLICKED|BUTTON4_PRESSED|BUTTON5_CLICKED|BUTTON5_PRESSED)) {
		switch (cmd) {
			case CMD_MIXER_MOUSE_CLICK_CONTROL_ENUM:
				focus_control_index = rect->arg1;
				return CMD_WITH_ARG((
							m.bstate & (BUTTON4_CLICKED|BUTTON4_PRESSED)
							? CMD_MIXER_CONTROL_UP
							: CMD_MIXER_CONTROL_DOWN
						), 1);

			case CMD_MIXER_MOUSE_CLICK_VOLUME_BAR:
				if (mouse_wheel_focuses_control)
					focus_control_index = rect->arg1;

			default:
				return CMD_WITH_ARG((
							m.bstate & (BUTTON4_CLICKED|BUTTON4_PRESSED)
							? CMD_MIXER_CONTROL_UP
							: CMD_MIXER_CONTROL_DOWN
						), mouse_wheel_step);
		}
	}
#endif

	/* If the rectangle has got an argument (value != -1) it is used for
	 * setting `focus_control_index` */
	if (rect && rect->arg1 >= 0)
		focus_control_index = rect->arg1;

	switch (cmd) {
	case CMD_MIXER_MOUSE_CLICK_VOLUME_BAR:
		if (m.bstate & (BUTTON3_CLICKED|BUTTON3_DOUBLE_CLICKED|BUTTON3_TRIPLE_CLICKED))
			channels = m.x - rect->x1 + 1;
		return CMD_WITH_ARG(CMD_MIXER_CONTROL_SET_PERCENT_LEFT + channels - 1,
			(100 * (rect->y2 - m.y) / (rect->y2 - rect->y1)) // volume
		);

	case CMD_MIXER_MOUSE_CLICK_MUTE:
		if (m.bstate & (BUTTON3_CLICKED|BUTTON3_DOUBLE_CLICKED|BUTTON3_TRIPLE_CLICKED))
			channels = m.x - rect->x1 + 1;
		return CMD_WITH_ARG(CMD_MIXER_TOGGLE_MUTE, channels);

	case CMD_MIXER_MOUSE_CLICK_CONTROL_ENUM:
		control = get_focus_control(TYPE_ENUM);
		if (control &&
			(snd_mixer_selem_get_enum_item(control->elem, 0, &index) >= 0)) {
				return (index == 0
					? CMD_WITH_ARG(CMD_MIXER_CONTROL_UP, 100)
					: CMD_WITH_ARG(CMD_MIXER_CONTROL_DOWN, 1));
		}
		break;

	default:
		return cmd; // non-mouse command
	}

	return 0; // failed mouse command
}

static void on_handle_key(int key)
{
	int arg;
	command_enum cmd;

	if (key == KEY_MOUSE)
		cmd = on_mouse_key();
	else if (key < ARRAY_SIZE(mixer_bindings))
		cmd = mixer_bindings[key];
	else
		return;

	arg = CMD_GET_ARGUMENT(cmd);
	cmd = CMD_GET_COMMAND(cmd);

	switch (cmd) {
	case CMD_MIXER_CONTROL_DOWN_LEFT:
	case CMD_MIXER_CONTROL_DOWN_RIGHT:
	case CMD_MIXER_CONTROL_DOWN:
		arg = (-arg);
	case CMD_MIXER_CONTROL_UP_LEFT:
	case CMD_MIXER_CONTROL_UP_RIGHT:
	case CMD_MIXER_CONTROL_UP:
		change_control_relative(arg, cmd % 4);
		break;
	case CMD_MIXER_CONTROL_SET_PERCENT_LEFT:
	case CMD_MIXER_CONTROL_SET_PERCENT_RIGHT:
	case CMD_MIXER_CONTROL_SET_PERCENT:
		change_control_to_percent(arg, cmd % 4);
		break;
	case CMD_MIXER_CLOSE:
		mixer_widget.close();
		break;
	case CMD_MIXER_HELP:
		show_help();
		break;
	case CMD_MIXER_SYSTEM_INFORMATION:
		create_proc_files_list();
		break;
	case CMD_MIXER_TOGGLE_VIEW_MODE:
		arg = (view_mode + 1) % VIEW_MODE_COUNT;
	case CMD_MIXER_SET_VIEW_MODE:
		set_view_mode((enum view_mode)(arg));
		break;
	case CMD_MIXER_SELECT_CARD:
		create_card_select_list();
		break;
	case CMD_MIXER_REFRESH:
		clearok(mixer_widget.window, TRUE);
		display_controls();
		break;
	case CMD_MIXER_PREVIOUS:
		arg = (-arg);
	case CMD_MIXER_NEXT:
		arg = focus_control_index + arg;
	case CMD_MIXER_FOCUS_CONTROL:
		focus_control_index = arg;
		if (focus_control_index < 0)
			focus_control_index = 0;
		else if (focus_control_index >= controls_count)
			focus_control_index = controls_count - 1;
		refocus_control();
		break;
	case CMD_MIXER_TOGGLE_MUTE:
		toggle_mute(arg);
		break;
	case CMD_MIXER_TOGGLE_CAPTURE:
		toggle_capture(arg);
		break;
	case CMD_MIXER_BALANCE_CONTROL:
		balance_volumes();
		break;
	}
}

static void create(void)
{
	static const char title[] = " AlsaMixer v" SND_UTIL_VERSION_STR " ";

	widget_init(&mixer_widget, screen_lines, screen_cols, 0, 0,
		    attrs.mixer_frame, WIDGET_BORDER);
	if (screen_cols >= (sizeof(title) - 1) + 2) {
		wattrset(mixer_widget.window, attrs.mixer_active);
		mvwaddstr(mixer_widget.window, 0, (screen_cols - (sizeof(title) - 1)) / 2, title);
	}
	init_mixer_layout();
	display_card_info();
	set_view_mode(view_mode);
}

static void on_window_size_changed(void)
{
	create();
}

static void on_close(void)
{
	widget_free(&mixer_widget);
}

void mixer_shutdown(void)
{
	free_controls();
	if (mixer)
		snd_mixer_close(mixer);
	if (current_selem_id)
		snd_mixer_selem_id_free(current_selem_id);
}

struct widget mixer_widget = {
	.handle_key = on_handle_key,
	.window_size_changed = on_window_size_changed,
	.close = on_close,
};

void create_mixer_widget(void)
{
	create();
}
