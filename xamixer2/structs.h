/******************************************************************************/
/* Begin Structures */

struct _Gtk_Channel
{
	GtkWidget **interface; /* And array of the interfaces (slider, button, etc.) */
	GtkObject **adjust; /* An array of the adjustments */
};
typedef struct _Gtk_Channel Gtk_Channel;

struct _Group
{
	snd_mixer_group_t group; /* The group structure */
	snd_mixer_element_t *element; /* an array of all the elements in the group */
	snd_mixer_element_info_t *einfo; /* an array of the info about all of the elements */
	snd_mixer_routes_t *routes; /* an array of all the routes for the elements */
	Gtk_Channel *gtk; /* The Gtk+ widgets used for each mixer element */
};
typedef struct _Group Group;

struct _Mixer
{
        int number; /* The number of the mixer device */
        snd_mixer_t *handle;
        snd_mixer_info_t info; /* The info for the mixer */
        int cnum; /* The number of channels present */
        int snum; /* The number of mixer switches present */
	snd_mixer_groups_t groups; /* The mixer groups */
	Group *group; /* An array of the mixer groups */
        char name[80]; /* The name of the mixer */
        GtkWidget *switch_table;
};
typedef struct _Mixer Mixer;



struct _Card
{
	snd_ctl_hw_info_t hw_info; /* The hardware info about the card. */
        int number; /* The card's number */
        void *handle; /* The handle for the mixer */
        char name[80]; /* The name of the card */
        Mixer *mixer; /* A dynamic array of all of the mixers */
        int nmixers; /* The number of mixers on the card */
        int npcms; /* The number of pcm devices */
};
typedef struct _Card Card;


struct _MixerInfo
{
        Mixer *mixer; /* Which card */
        int channel; /* Which channel */
        unsigned int flags; /* flags */
        GtkWidget *other; /* The other range widget */
        GtkWidget *mute; /* The mute pixmap */
        GtkWidget *unmute; /* The unmute pixmap */
};
typedef struct _MixerInfo MixerInfo;


struct _ChannelLabel
{
        struct _ChannelLabel *next; /* pointer to the next node in the list */
        char *channel; /* The channel name */
        char *label; /* The channel label or pixmap */
};
typedef struct _ChannelLabel ChannelLabel;


struct _CBData
{
	Group *group; /* The group */
	void *handle; /* The mixer handle */
	int element; /* The element number to use as an index */
	int index; /* The index such as the voice # or something like that */
};
typedef struct _CBData CBData;



struct _Config
{
        unsigned int flags; /* Flags */
        ChannelLabel *labels; /* The text labels for channels */
        ChannelLabel *xpm; /* The pixmaps (file names) for channels */
        char *icon; /* The Icon pixmap to use */
        char *mute; /* The mute label or pixmap (indicated in a flag) */
        char *mute_l; /* The left mute label or pixmap (indicated in a flag) */
        char *unmute; /* The unmute label or pixmap (indicated in a flag) */
        char *unmute_l; /* The left unmute label or pixmap (indicated in a flag) */
        char *simul; /* The simultaneous label or pixmap (indicated in a flag */
        char *unsimul; /* The unsimultaneous label or pixmap (indicated in a flag */
        char *rec; /* The record label or pixmap (indicated in a flag) */
        char *unrec; /* The unrecord label or pixmap (indicated in a flag) */
        char *background; /* The background xpm */
        unsigned int scale; /* The size in pixels that the scales should be set to */
        unsigned int padding; /* The padding between channels */
        int x_pos, y_pos; /* The position to start out at -1 = default */
        GtkWidget *cdisplay; /* The channel display window */
        GdkPixmap *icon_xpm; /* The icon xpm */
        GdkPixmap *mute_xpm; /* The mute pixmap */
        GdkPixmap *unmute_xpm; /* The unmute pixmap */
        GdkPixmap *mute_xpm_l; /* The left mute pixmap */
        GdkPixmap *unmute_xpm_l; /* The left unmute pixmap */
        GdkPixmap *rec_xpm; /* The record pixmap */
        GdkPixmap *unrec_xpm; /* The record off pixmap */
        GdkPixmap *simul_xpm; /* The sumultaneous pixmap */
        GdkPixmap *unsimul_xpm; /* The independent pixmap */
        GdkPixmap *background_xpm; /* The background pixmap */
        GdkBitmap *icon_mask;
        GdkBitmap *mute_mask;
        GdkBitmap *unmute_mask;
        GdkBitmap *mute_mask_l;
        GdkBitmap *unmute_mask_l;
        GdkBitmap *rec_mask;
        GdkBitmap *unrec_mask;
        GdkBitmap *simul_mask;
        GdkBitmap *unsimul_mask;
        GdkBitmap *background_mask;
};
typedef struct _Config Config;

/* End Structures */
/******************************************************************************/


#if 0
struct _Channel
{
        int num; /* The channel's number */
        snd_mixer_channel_t data; /* the data */
        snd_mixer_channel_info_t info; /* The info */
        unsigned int flags; /* The Channel's flags */
        GtkWidget *lm, *rm, *mm, *rec; /* The associated widgets */
        GtkWidget *lrec, *rrec; /* More associated widgets */
        GtkObject *ladj, *radj, *madj; /* The associated objects */
        GtkTooltips *left_tt, *right_tt, *mono_tt; /* The tooltips */
        GtkWidget *lscal, *rscal, *mscal; /* The scale widgets */
        GtkWidget *label, *lock;
        GtkWidget *ltor_in, *rtol_in;
        void *mixer; /* A pointer to the mixer */
};
typedef struct _Channel Channel;
#endif













