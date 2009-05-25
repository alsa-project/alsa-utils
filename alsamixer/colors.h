#ifndef COLORS_H_INCLUDED
#define COLORS_H_INCLUDED

extern int attr_mixer_frame;
extern int attr_mixer_text;
extern int attr_mixer_active;
extern int attr_ctl_frame;
extern int attr_ctl_mute;
extern int attr_ctl_nomute;
extern int attr_ctl_capture;
extern int attr_ctl_nocapture;
extern int attr_ctl_label;
extern int attr_ctl_label_focus;
extern int attr_ctl_mark_focus;
extern int attr_ctl_bar;
extern int attr_ctl_inactive;
extern int attr_ctl_label_inactive;
extern int attr_errormsg;
extern int attr_infomsg;
extern int attr_textbox;
extern int attr_textfield;
extern int attr_menu;
extern int attr_menu_selected;

void init_colors(int use_color);

#endif
