#ifndef COLORS_H_INCLUDED
#define COLORS_H_INCLUDED

#define TRICOLOR_VOLUME_BAR

struct attributes {
	// Alphabetically sorted
#ifdef TRICOLOR_VOLUME_BAR
	int ctl_bar_hi;
#endif
	int ctl_bar_lo;
#ifdef TRICOLOR_VOLUME_BAR
	int ctl_bar_mi;
#endif
	int ctl_capture;
	int ctl_frame;
	int ctl_inactive;
	int ctl_label;
	int ctl_label_focus;
	int ctl_label_inactive;
	int ctl_mark_focus;
	int ctl_mute;
	int ctl_nocapture;
	int ctl_nomute;
	int errormsg;
	int infomsg;
	int menu;
	int menu_selected;
	int mixer_active;
	int mixer_frame;
	int mixer_text;
	int textbox;
	int textfield;
};

extern struct attributes attrs;

void init_colors(int use_color);
void reinit_colors(short bg);
int get_color_pair(short fg, short bg);

#endif
