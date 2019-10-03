#ifndef MAINLOOP_H_INCLUDED
#define MAINLOOP_H_INCLUDED

#include CURSESINC

void initialize_curses(bool use_color, bool use_mouse);
void mainloop(void);
void app_shutdown(void);

#endif
