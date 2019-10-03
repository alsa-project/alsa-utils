#ifndef MIXER_WIDGET_H_INCLUDED
#define MIXER_WIDGET_H_INCLUDED

#include CURSESINC
#include <alsa/asoundlib.h>
#include "widget.h"

enum view_mode {
	VIEW_MODE_PLAYBACK,
	VIEW_MODE_CAPTURE,
	VIEW_MODE_ALL,
	VIEW_MODE_COUNT,
};

enum channel_mask {
	LEFT = 1,
	RIGHT = 2,
};

extern snd_mixer_t *mixer;
extern char *mixer_device_name;
extern bool unplugged;

extern struct widget mixer_widget;

extern enum view_mode view_mode;

extern int focus_control_index;
extern snd_mixer_selem_id_t *current_selem_id;
extern unsigned int current_control_flags;

extern bool control_values_changed;
extern bool controls_changed;

extern unsigned int mouse_wheel_step;
extern bool mouse_wheel_focuses_control;

void create_mixer_object(struct snd_mixer_selem_regopt *selem_regopt);
void create_mixer_widget(void);
void mixer_shutdown(void);
void close_mixer_device(void);
bool select_card_by_name(const char *device_name);
void refocus_control(void);

#endif
