
#include "gamix.h"

GtkWidget *window;
GtkWidget *main_vbox;
GtkWidget *mixer_container;
GtkWidget *exit_item;
unsigned char *nomem_msg = N_("No enough ememory.\n");

int main(int , char **);
int disp_mixer( void );
void disp_toolbar(void);
static void exit_gtk(GtkWidget *,gpointer);
static void sel_mctype(GtkWidget *,gpointer);
static void conf_callback(GtkWidget *,gpointer);

static void exit_gtk(GtkWidget *w,gpointer data) {
	gtk_main_quit();
}

int main( int argc , char **argv ) {
	int i;
	gchar *dirname,*filename;

	i=probe_mixer();
	if( i < 0 ) {
		fprintf(stderr,_("Can not make mixer.\n"));
		return -1;
	}

#ifdef ENABLE_NLS
	bindtextdomain (PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
#endif
	gtk_set_locale();
	gtk_init( &argc,&argv);

	dirname = g_strconcat(g_get_home_dir(),"/.gamix",NULL);
	filename = g_strconcat(dirname, "/gtkrc", NULL);
	gtk_rc_init();
	gtk_rc_parse(filename);
	g_free(filename);

	conf.scroll=TRUE;
	conf.wmode=1;
	conf.F_save=FALSE;
	conf.Esave=FALSE;
	conf.fna = g_strconcat(dirname,"/Config",NULL);
	conf.width=0;
	conf.height=0;

	g_free(dirname);

	conf_read();

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_signal_connect(GTK_OBJECT(window),"destroy",
					   GTK_SIGNAL_FUNC(gtk_main_quit),NULL);
	gtk_widget_show(window);
	main_vbox=gtk_vbox_new(FALSE,0);
	gtk_container_add(GTK_CONTAINER(window),main_vbox);
	gtk_widget_show(main_vbox);

	disp_toolbar();

	tc_init();
	gtk_timeout_add(100,(GtkFunction)time_callback,NULL);

	if( disp_mixer()<0 ) return 0;

	gtk_main();
	if( conf.F_save || conf.Esave ) {
		conf_write();
	}
	g_free(conf.fna);
	return 0;
}

void disp_toolbar(void) {
	GtkWidget *menu,*sub_menu,*sub_item;
	GtkWidget *frame;

	frame=gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame),GTK_SHADOW_OUT);
	gtk_box_pack_start(GTK_BOX(main_vbox),frame,FALSE,FALSE,0);
	gtk_widget_show(frame);

	menu=gtk_menu_bar_new();
	gtk_container_add(GTK_CONTAINER(frame),menu);
	gtk_widget_show(menu);

	/* Prg menu */
	sub_menu=gtk_menu_new();

	sub_item=gtk_menu_item_new_with_label(_("config"));
	exit_item=sub_item;
	gtk_menu_append(GTK_MENU(sub_menu),sub_item);
	gtk_signal_connect(GTK_OBJECT(sub_item),"activate",
					   GTK_SIGNAL_FUNC(conf_callback),NULL);
	gtk_widget_show(sub_item);

	sub_item=gtk_menu_item_new_with_label(_("exit"));
	exit_item=sub_item;
	gtk_menu_append(GTK_MENU(sub_menu),sub_item);
	gtk_signal_connect(GTK_OBJECT(sub_item),"activate",
					   GTK_SIGNAL_FUNC(exit_gtk),NULL);
	gtk_widget_show(sub_item);

	sub_item=gtk_menu_item_new_with_label(_("Prog"));
	gtk_widget_show(sub_item);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(sub_item),sub_menu);
	gtk_menu_bar_append(GTK_MENU_BAR(menu),sub_item);

	/* mixer container type menu*/
	if( mdev_num > 1 ) {
		sub_menu=gtk_menu_new();

		sub_item=gtk_menu_item_new_with_label(_("Horizontal"));
		gtk_menu_append(GTK_MENU(sub_menu),sub_item);
		gtk_signal_connect(GTK_OBJECT(sub_item),"activate",
						   GTK_SIGNAL_FUNC(sel_mctype),0);
		gtk_widget_show(sub_item);

		sub_item=gtk_menu_item_new_with_label(_("Vertical"));
		gtk_menu_append(GTK_MENU(sub_menu),sub_item);
		gtk_signal_connect(GTK_OBJECT(sub_item),"activate",
						   GTK_SIGNAL_FUNC(sel_mctype),(gpointer)1);
		gtk_widget_show(sub_item);

		sub_item=gtk_menu_item_new_with_label(_("note book"));
		gtk_menu_append(GTK_MENU(sub_menu),sub_item);
		gtk_signal_connect(GTK_OBJECT(sub_item),"activate",
						   GTK_SIGNAL_FUNC(sel_mctype),(gpointer)2);
		gtk_widget_show(sub_item);

		sub_item=gtk_menu_item_new_with_label(_("C-type"));
		gtk_widget_show(sub_item);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(sub_item),sub_menu);
		gtk_menu_bar_append(GTK_MENU_BAR(menu),sub_item);
	}
}

static void sel_mctype(GtkWidget *w,gpointer n) {
	int i;
	GtkRequisition rq;

	i=(int)n;
	if( i == conf.wmode ) return;
	conf.wmode=i;
	gtk_container_remove(GTK_CONTAINER(main_vbox),mixer_container);
	if( (i=disp_mixer()) < 0 ) gtk_signal_emit_by_name(GTK_OBJECT(exit_item),
								   "activate");
	gtk_widget_size_request(window,&rq);
	gdk_window_resize(window->window,rq.width,rq.height);
}

int disp_mixer( void ) {
	int i,j;
	GtkWidget *n_label;
	GtkWidget *frame;
	GtkRequisition rq;

	switch( conf.wmode ) {
	case 0: /* H */
		if( conf.scroll ) {
			mixer_container=gtk_hbox_new(TRUE,0);
		} else {
			mixer_container=gtk_hbox_new(FALSE,0);
		}
		for( i=0 ; i<card_num ; i++ ) {
			for( j=0 ; j<cards[i].info.mixerdevs ; j++ ) {
				if( cards[i].mixer[j].enable ) {
					cards[i].mixer[j].w=gtk_frame_new(NULL);
					gtk_frame_set_shadow_type(GTK_FRAME(cards[i].mixer[j].w),
											  GTK_SHADOW_ETCHED_IN);
					gtk_widget_show(cards[i].mixer[j].w);
					frame=make_mixer(i,j);
					if( !frame ) return -1;
					gtk_container_add(GTK_CONTAINER(cards[i].mixer[j].w),
									  frame);
					gtk_box_pack_start(GTK_BOX(mixer_container),
									   cards[i].mixer[j].w,TRUE,TRUE,2);
				}
			}
		}
		break;
	case 1: /* V */
		mixer_container=gtk_vbox_new(FALSE,0);
		for( i=0 ; i<card_num ; i++ ) {
			for( j=0 ; j<cards[i].info.mixerdevs ; j++ ) {
				if( cards[i].mixer[j].enable ) {
					cards[i].mixer[j].w=make_mixer(i,j);
					if( !cards[i].mixer[j].w ) return -1;
					gtk_box_pack_start(GTK_BOX(mixer_container),
									   cards[i].mixer[j].w,TRUE,TRUE,0);
				}
			}
		}
		break;
	case 2: /* NoteBook */
		mixer_container=gtk_notebook_new();
		gtk_notebook_set_tab_pos(GTK_NOTEBOOK(mixer_container),GTK_POS_TOP);
		for( i=0 ; i<card_num ; i++ )
			for( j=0 ; j<cards[i].info.mixerdevs ; j++ ) {
				if( cards[i].mixer[j].enable ) {
					cards[i].mixer[j].w=make_mixer(i,j);
					if( !cards[i].mixer[j].w ) return -1;
					n_label=gtk_label_new(cards[i].mixer[j].info.name);
					gtk_notebook_append_page(GTK_NOTEBOOK(mixer_container),
											 cards[i].mixer[j].w,n_label);
				}
			}
		break;
	}

	gtk_box_pack_start(GTK_BOX(main_vbox),mixer_container,TRUE,TRUE,0);
	gtk_widget_show(mixer_container);

	if( conf.width>0 && conf.height >0 && !conf.F_save ) {
		gtk_widget_size_request(window,&rq);
		gdk_window_resize(window->window,conf.width,conf.height);
		conf.width=0;
		conf.height=0;
	}
	return 0;
}

static void conf_callback(GtkWidget *w ,gpointer data) {
	gint err;
	GtkRequisition rq;

	err=conf_win();
	if( err < 0 )  gtk_signal_emit_by_name(GTK_OBJECT(exit_item),"activate");
	if( err ) {
		gtk_container_remove(GTK_CONTAINER(main_vbox),mixer_container);
		if( disp_mixer() < 0 ) gtk_signal_emit_by_name(
										GTK_OBJECT(exit_item),"activate");
		gtk_widget_size_request(window,&rq);
		gdk_window_resize(window->window,rq.width,rq.height);
	}
}
