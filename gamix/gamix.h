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
} s_element_t;

typedef struct s_eelements {
	s_element_t e;
} s_eelements_t;

typedef struct s_group {
	snd_mixer_group_t g;
	s_element_t *e;
} s_group_t;

typedef struct s_obj s_obj_t;
struct s_obj {
	s_group_t *g;
	s_eelements_t *e;
	GtkWidget *v_frame;
	gint enable;
	gint enabled;
	gint chain;
	gint chain_en;
	gint dyn_e;
	GtkWidget *cwb;
	s_obj_t *next;
};

typedef struct {
	snd_mixer_t *handle;
	snd_mixer_info_t info;
	int c_dev,m_dev;
	gint o_nums;
	s_obj_t *obj;
	GtkWidget *w;
	gboolean enable;
	gboolean enabled;
	gboolean p_e;
	gboolean p_f;
} s_mixer_t;

typedef struct {
	snd_ctl_hw_info_t info;
	s_mixer_t *mixer;
} s_card_t;

typedef struct {
	gint wmode;
	gboolean scroll;
	gchar *fna;
	gboolean F_save;
	gboolean Esave;
	gboolean sv_wsize;
	gint width;
	gint height;
} s_conf;

extern GtkWidget *window;
extern int card_num,mdev_num;
extern gint card,mdev;
extern s_card_t *cards;
extern s_conf conf;
extern unsigned char *nomem_msg;

/* probe.c */
gint probe_mixer( void );
gboolean is_etype( int );
int s_element_build(snd_mixer_t *,s_element_t *,snd_mixer_elements_t *,
					snd_mixer_eid_t ,int , int);

/* mkmixer.c */
GtkWidget *make_mixer( gint , gint );

/* catch.c */
void tc_init(void);
gint time_callback(gpointer);

/* conf_w.c */
gint conf_win( void );
void conf_read( void );
void conf_write( void );
gint obj_ins_new_g( s_obj_t **,s_obj_t **,snd_mixer_gid_t *);
gint obj_ins_new_e( s_obj_t **,s_obj_t **,snd_mixer_eid_t *);
