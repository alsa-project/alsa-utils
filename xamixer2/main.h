/*****************************************************************************/
/* Begin system #includes */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/asoundlib.h>
#include <gtk/gtk.h>

/* End system #includes */
/*****************************************************************************/ 



/*****************************************************************************/
/* Begin program #includes */

#include "structs.h"
#include "util.h"
#include "xamixer2.h"
#include "cinit.h"
#include "callbacks.h"
#include "display.h"
#include "config.h"
#include "switches.h"
#include "options.h"

/* End program #includes */
/*****************************************************************************/ 



/*****************************************************************************/
/* Begin #defines */

#define CHANNEL_LEFT                  (1 << 0)
#define CHANNEL_RIGHT                 (1 << 1)
#define CHANNEL_MONO                  (1 << 2)
#define CHANNEL_MUTE_RIGHT            (1 << 3)
#define CHANNEL_MUTE_LEFT             (1 << 4)
#define CHANNEL_MUTE_MONO             (1 << 5)
#define CHANNEL_RECORD                (1 << 6)
#define CHANNEL_RECORD_RIGHT          (1 << 7)
#define CHANNEL_RECORD_LEFT           (1 << 8)
#define CHANNEL_SIMULTANEOUS          (1 << 9)
#define CHANNEL_DISPLAYED             (1 << 10)
#define CHANNEL_LTOR                  (1 << 11)
#define CHANNEL_RTOL                  (1 << 12)


#define CONFIG_USE_XPMS               (1 << 0)
#define CONFIG_ICON_XPM               (1 << 1)
#define CONFIG_MUTE_XPM               (1 << 2)
#define CONFIG_MUTE_XPM_L             (1 << 3)
#define CONFIG_UNMUTE_XPM             (1 << 4)
#define CONFIG_UNMUTE_XPM_L           (1 << 5)
#define CONFIG_REC_XPM                (1 << 6)
#define CONFIG_UNREC_XPM              (1 << 7)
#define CONFIG_SIMUL_XPM              (1 << 8)
#define CONFIG_UNSIMUL_XPM            (1 << 9)
#define CONFIG_LTOR_XPM               (1 << 10)
#define CONFIG_UNLTOR_XPM             (1 << 11)
#define CONFIG_RTOL_XPM               (1 << 12)
#define CONFIG_UNRTOL_XPM             (1 << 13)
#define CONFIG_BACKGROUND_XPM         (1 << 14)
#define CONFIG_SHOW_CARD_NAME         (1 << 15)
#define CONFIG_SHOW_MIXER_NUMBER      (1 << 16)
#define CONFIG_SHOW_MIXER_NAME        (1 << 17)
#define CONFIG_SWITCHES_HIDDEN        (1 << 18)

/* End #defines */
/*****************************************************************************/



/*****************************************************************************/
/* Gtk 1.0 compatability */

#ifndef GTK_HAVE_FEATURES_1_1_0
#define gtk_button_set_relief(a,b)
#endif

/* End Gtk 1.0 compatability */
/*****************************************************************************/ 




