#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <glib.h>

#include <sys/asoundlib.h>

#ifdef ENABLE_NLS
# include <libintl.h>
# undef _
# undef N_
# define _(String) dgettext(PACKAGE,String)
# ifdef gettext_noop
#  define N_(String) gettext_noop(String)
# else
#  define N_(String) (String)
# endif
#else
# define textdomain(String) (String)
# define gettext(String) (String)
# define dgettext(Domain,Message) (Message)
# define dcgettext(Domain,Message,Type) (Message)
# define bindtextdomain(Domain,Directory) (Domain)
# define _(String) (String)
# define N_(String) (String)
#endif

#define preid(eid) printf("'%s',%d,%d\n",(eid).name,(eid).index,(eid).type)

typedef struct {
	snd_mixer_element_t e;
	snd_mixer_element_info_t info;
	GtkWidget **w;
	GtkAdjustment **adj;
	gint card,mdev;
	gint *chain_en;
	gint *chain;
	gint mux_n;
	snd_mixer_eid_t *mux;
} s_element;

typedef struct {
	GtkWidget *v_frame;
	s_element e;
	gint enable;
	gint enabled;
	gint chain;
	gint chain_en;
	GtkWidget *cwb;
} s_eelements;

typedef struct {
	snd_mixer_group_t g;
	GtkWidget *v_frame;
	s_element *e;
	gint enable;
	gint enabled;
	gint chain_en;
	gint chain;
	GtkWidget *cwb;
} s_group;

typedef struct {
	snd_mixer_t *handle;
	snd_mixer_groups_t groups;
	snd_mixer_info_t info;
	s_group *group;
	gint ee_n;
	s_eelements *ee;
	GtkWidget *w;
	gint enable;
	gint enabled;
	gboolean p_e;
	gboolean p_f;
} s_mixer;

typedef struct {
	snd_ctl_hw_info_t info;
	s_mixer *mixer;
} s_card;

typedef struct {
	gint wmode;
	gboolean scroll;
	gchar *fna;
	gboolean F_save;
	gboolean Esave;
	gint width;
	gint height;
} s_conf;

extern GtkWidget *window;
extern int card_num,mdev_num;
extern gint card,mdev;
extern s_card *cards;
extern s_conf conf;
extern unsigned char *nomem_msg;

/* probe.c */
gint probe_mixer( void );

/* mkmixer.c */
GtkWidget *make_mixer( gint , gint );

/* catch.c */
void tc_init(void);
gint time_callback(gpointer);

/* conf_w.c */
gint conf_win( void );
void conf_read( void );
void conf_write( void );
