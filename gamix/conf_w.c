
#include "gamix.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

s_conf conf;

static GtkWidget *c_win;

typedef struct {
	gboolean m_en;
	gboolean *g_en;
	gboolean *ee_en;
	GSList *gp;
	gboolean p_e;
	gboolean p_f;
} c_mixer;

typedef struct {
	c_mixer *m;
} c_card;

static c_card *ccard;
static gboolean scrolled;
static gboolean ok_pushed;
static gboolean Esaved;
static gboolean Tosave;

static void close_win(GtkWidget *,gpointer);
static void cancel_b(GtkWidget *,gpointer);
static void ok_b(GtkWidget *,gpointer);
static void tb_callback(GtkToggleButton *,gint *);

static void cread_err(gchar *,int );
static void chk_cfile(void);

static void close_win(GtkWidget *w,gpointer data) {
	gtk_grab_remove(c_win);
	gtk_main_quit();
}

static void cancel_b(GtkWidget *w,gpointer data) {
	gtk_widget_destroy(c_win);
}
static void ok_b(GtkWidget *w,gpointer data) {
	int i,j,k;
	GSList *n;
	GtkWidget *b;

	Tosave=(gboolean)data;

	ok_pushed=TRUE;

	for( i=0 ; i<card_num ; i++ ) {
		for( j=0 ; j<cards[i].info.mixerdevs; j++ ) {
			for( k=0 ; (n = g_slist_nth(ccard[i].m[j].gp,k)) != NULL ; k++ ) {
				b=(GtkWidget *)n->data;
				if( GTK_TOGGLE_BUTTON(b)->active ) break;
			}
			switch(k) {
			case 2:
				ccard[i].m[j].p_e=FALSE;
				ccard[i].m[j].p_f=FALSE;
				break;
			case 1:
				ccard[i].m[j].p_e=TRUE;
				ccard[i].m[j].p_f=FALSE;
				break;
			case 0:
				ccard[i].m[j].p_e=TRUE;
				ccard[i].m[j].p_f=TRUE;
				break;
			}
		}
	}
	gtk_widget_destroy(c_win);
}

gint conf_win( void ) {
	int i,j,k,l;
	gint changed;
	GtkWidget *b;
	GtkWidget *vbox,*box,*frame,*hbox,*hhbox,*box1,*box2;
	GtkWidget *nb,*n_label;
	unsigned char gname[40];
	GSList *gp;

	ok_pushed=FALSE;

	c_win=gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_signal_connect(GTK_OBJECT(c_win),"destroy",GTK_SIGNAL_FUNC(close_win),
					   NULL);

	vbox=gtk_vbox_new(FALSE,10);
	gtk_container_add(GTK_CONTAINER(c_win),vbox);
	
	/* options */
	nb=gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(nb),GTK_POS_TOP);
	gtk_box_pack_start(GTK_BOX(vbox),nb,FALSE,FALSE,0);
	
	/*  OPT */
	frame=gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame),GTK_SHADOW_IN);
	gtk_container_set_border_width(GTK_CONTAINER(frame),20);
	box=gtk_vbox_new(FALSE,10);
	gtk_container_set_border_width(GTK_CONTAINER(box),10);
	gtk_container_add(GTK_CONTAINER(frame),box);

	hbox=gtk_hbox_new(FALSE,4);
	gtk_box_pack_start(GTK_BOX(box),hbox,FALSE,FALSE,0);
	scrolled=conf.scroll;
	b=gtk_toggle_button_new();
	gtk_widget_set_usize(b,10,10);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b),conf.scroll);
	gtk_box_pack_start(GTK_BOX(hbox),b,FALSE,FALSE,0);
	gtk_signal_connect(GTK_OBJECT(b),"toggled",GTK_SIGNAL_FUNC(tb_callback),
					   (gpointer)&scrolled);
	gtk_widget_show(b);
	n_label=gtk_label_new(_("Scroll window enable"));
	gtk_box_pack_start(GTK_BOX(hbox),n_label,FALSE,FALSE,0);
	gtk_widget_show(n_label);
	gtk_widget_show(hbox);

	hbox=gtk_hbox_new(FALSE,4);
	gtk_box_pack_start(GTK_BOX(box),hbox,FALSE,FALSE,0);
	Esaved=conf.Esave;
	b=gtk_toggle_button_new();
	gtk_widget_set_usize(b,10,10);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b),conf.Esave);
	gtk_box_pack_start(GTK_BOX(hbox),b,FALSE,FALSE,0);
	gtk_signal_connect(GTK_OBJECT(b),"toggled",GTK_SIGNAL_FUNC(tb_callback),
					   (gpointer)&Esaved);
	gtk_widget_show(b);
	n_label=gtk_label_new(_("Config save when exit"));
	gtk_box_pack_start(GTK_BOX(hbox),n_label,FALSE,FALSE,0);
	gtk_widget_show(n_label);
	gtk_widget_show(hbox);

	n_label=gtk_label_new("OPT");
	gtk_widget_show(box);
	gtk_widget_show(frame);
	gtk_notebook_append_page(GTK_NOTEBOOK(nb),frame,n_label);
	
	/* Mixer */
	ccard=(c_card *)g_malloc(card_num*sizeof(c_card));
	if( ccard == NULL ) {
			fprintf(stderr,nomem_msg);
			return -1;
	}
	for( i=0 ; i<card_num ; i++ ) {
		ccard[i].m=(c_mixer *)g_malloc(cards[i].info.mixerdevs*sizeof(c_mixer));
		if( ccard[i].m == NULL ) {
			fprintf(stderr,nomem_msg);
			return -1;
		}
		for( j=0 ; j<cards[i].info.mixerdevs; j++ ) {
			n_label=gtk_label_new(cards[i].mixer[j].info.name);
			frame=gtk_frame_new(NULL);
			gtk_frame_set_shadow_type(GTK_FRAME(frame),GTK_SHADOW_IN);
			gtk_container_set_border_width(GTK_CONTAINER(frame),20);
			gtk_notebook_append_page(GTK_NOTEBOOK(nb),frame,n_label);

			box=gtk_vbox_new(FALSE,2);
			gtk_container_set_border_width(GTK_CONTAINER(box),10);
			gtk_container_add(GTK_CONTAINER(frame),box);

			hbox=gtk_hbox_new(FALSE,4);
			gtk_box_pack_start(GTK_BOX(box),hbox,FALSE,FALSE,0);
			
			ccard[i].m[j].m_en=cards[i].mixer[j].enable;
			b=gtk_toggle_button_new();
			gtk_widget_set_usize(b,10,10);
			gtk_box_pack_start(GTK_BOX(hbox),b,FALSE,FALSE,0);
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b),
										 ccard[i].m[j].m_en);
			gtk_signal_connect(GTK_OBJECT(b),"toggled",
							   GTK_SIGNAL_FUNC(tb_callback),
							   (gpointer)&ccard[i].m[j].m_en);
			gtk_widget_show(b);
			n_label=gtk_label_new(cards[i].mixer[j].info.name);
			gtk_box_pack_start(GTK_BOX(hbox),n_label,FALSE,FALSE,0);
			gtk_widget_show(n_label);
			gtk_widget_show(hbox);
			
			if( cards[i].mixer[j].p_e ) {
				if( cards[i].mixer[j].p_f ) k=2; else k=1;
			} else k=0;
			hbox=gtk_hbox_new(FALSE,4);
			gtk_box_pack_start(GTK_BOX(box),hbox,FALSE,FALSE,0);
			n_label=gtk_label_new(_("Spacing: "));
			gtk_box_pack_start(GTK_BOX(hbox),n_label,FALSE,FALSE,0);
			gtk_widget_show(n_label);

			b=gtk_radio_button_new_with_label(NULL,_("NONE"));
			gtk_box_pack_start(GTK_BOX(hbox),b,FALSE,FALSE,0);
			gtk_widget_show(b);
			if( k==0 ) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b),
													TRUE);

			gp=gtk_radio_button_group(GTK_RADIO_BUTTON(b));

			b=gtk_radio_button_new_with_label(gp,_("space"));
			gtk_box_pack_start(GTK_BOX(hbox),b,FALSE,FALSE,0);
			gtk_widget_show(b);
			if( k==1 ) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b),
													TRUE);
			gp=gtk_radio_button_group(GTK_RADIO_BUTTON(b));

			b=gtk_radio_button_new_with_label(gp,_("expand"));
			gtk_box_pack_start(GTK_BOX(hbox),b,FALSE,FALSE,0);
			gtk_widget_show(b);
			if( k==2 ) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b),
													TRUE);
			ccard[i].m[j].gp=gtk_radio_button_group(GTK_RADIO_BUTTON(b));
			gtk_widget_show(hbox);

			hhbox=gtk_hbox_new(TRUE,20);
			gtk_container_set_border_width(GTK_CONTAINER(hhbox),8);
			gtk_box_pack_start(GTK_BOX(box),hhbox,FALSE,FALSE,0);
			box1=gtk_vbox_new(FALSE,4);
			gtk_box_pack_start(GTK_BOX(hhbox),box1,FALSE,FALSE,0);
			box2=gtk_vbox_new(FALSE,4);
			gtk_box_pack_start(GTK_BOX(hhbox),box2,FALSE,FALSE,0);

			ccard[i].m[j].g_en=(gint *)g_malloc(cards[i].mixer[j].groups.groups
											   * sizeof(gint));
			if( ccard[i].m[j].g_en == NULL ) {
				fprintf(stderr,nomem_msg);
				g_free(ccard);
				return -1;
			}
			l=0;
			for( k=0 ; k<cards[i].mixer[j].groups.groups ; k++ ) {
				ccard[i].m[j].g_en[k]=cards[i].mixer[j].group[k].enable;
				hbox=gtk_hbox_new(FALSE,2);
				b=gtk_toggle_button_new();
				gtk_widget_set_usize(b,10,10);
				gtk_box_pack_start(GTK_BOX(hbox),b,FALSE,FALSE,0);
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b),
											ccard[i].m[j].g_en[k]);
				gtk_signal_connect(GTK_OBJECT(b),"toggled",
								   GTK_SIGNAL_FUNC(tb_callback),
								   (gpointer)&ccard[i].m[j].g_en[k]);
				gtk_widget_show(b);
				if( cards[i].mixer[j].groups.pgroups[k].index > 0 ) {
					sprintf(gname,"%s %d",
							cards[i].mixer[j].groups.pgroups[k].name,
							cards[i].mixer[j].groups.pgroups[k].index);
				} else {
					strcpy(gname,cards[i].mixer[j].groups.pgroups[k].name);
				}
				n_label=gtk_label_new(gname);
				gtk_box_pack_start(GTK_BOX(hbox),n_label,FALSE,FALSE,0);
				gtk_widget_show(n_label);
				if( (l&1) ) {
					gtk_box_pack_start(GTK_BOX(box2),hbox,FALSE,FALSE,0);
				} else {
					gtk_box_pack_start(GTK_BOX(box1),hbox,FALSE,FALSE,0);
				}
				l++;
				gtk_widget_show(hbox);
			}
			if( cards[i].mixer[j].ee_n ) {
				ccard[i].m[j].ee_en=(gint *)g_malloc(cards[i].mixer[j].ee_n *
													 sizeof(gint));
				if( ccard[i].m[j].ee_en == NULL ) {
					fprintf(stderr,nomem_msg);
					g_free(ccard[i].m[j].g_en);
					g_free(ccard[i].m);
					g_free(ccard);
					return -1;
				}
				for( k=0 ; k<cards[i].mixer[j].ee_n ; k++ ) {
					ccard[i].m[j].ee_en[k]=cards[i].mixer[j].ee[k].enable;
					hbox=gtk_hbox_new(FALSE,2);
					b=gtk_toggle_button_new();
					gtk_widget_set_usize(b,10,10);
					gtk_box_pack_start(GTK_BOX(hbox),b,FALSE,FALSE,0);
					gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b),
												 ccard[i].m[j].ee_en[k]);
					gtk_signal_connect(GTK_OBJECT(b),"toggled",
									   GTK_SIGNAL_FUNC(tb_callback),
									   (gpointer)&ccard[i].m[j].ee_en[k]);
					gtk_widget_show(b);
					if( cards[i].mixer[j].ee[k].e.e.eid.index > 0 ) {
						sprintf(gname,"%s %d",
								cards[i].mixer[j].ee[k].e.e.eid.name,
								cards[i].mixer[j].ee[k].e.e.eid.index);
					} else {
						strcpy(gname,cards[i].mixer[j].ee[k].e.e.eid.name);
					}
					n_label=gtk_label_new(gname);
					gtk_box_pack_start(GTK_BOX(hbox),n_label,FALSE,FALSE,0);
					gtk_widget_show(n_label);
					if( (l&1) ) {
						gtk_box_pack_start(GTK_BOX(box2),hbox,FALSE,FALSE,0);
					} else {
						gtk_box_pack_start(GTK_BOX(box1),hbox,FALSE,FALSE,0);
					}
					l++;
					gtk_widget_show(hbox);
				}
			}

			gtk_widget_show(box1);
			gtk_widget_show(box2);
			gtk_widget_show(hhbox);
			gtk_widget_show(box);
			gtk_widget_show(frame);
		}
	}
	
	gtk_widget_show(nb);
	/* buttons */
	box=gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(box),GTK_BUTTONBOX_END);
	gtk_button_box_set_spacing(GTK_BUTTON_BOX(box),5);
	gtk_box_pack_end(GTK_BOX(vbox),box,FALSE,FALSE,0);

	b=gtk_button_new_with_label(_("OK"));
	gtk_box_pack_start(GTK_BOX(box),b,TRUE,TRUE,0);
	gtk_signal_connect(GTK_OBJECT(b),"clicked",GTK_SIGNAL_FUNC(ok_b),
					   (gpointer)FALSE);
	GTK_WIDGET_SET_FLAGS(b,GTK_CAN_DEFAULT);
	gtk_widget_show(b);
	gtk_widget_grab_default(b);

	Tosave=FALSE;
	b=gtk_button_new_with_label(_("SAVE"));
	gtk_box_pack_start(GTK_BOX(box),b,TRUE,TRUE,0);
	gtk_signal_connect(GTK_OBJECT(b),"clicked",GTK_SIGNAL_FUNC(ok_b),
					   (gpointer)TRUE);
	GTK_WIDGET_SET_FLAGS(b,GTK_CAN_DEFAULT);
	gtk_widget_show(b);

	b=gtk_button_new_with_label(_("CANCEL"));
	gtk_box_pack_start(GTK_BOX(box),b,TRUE,TRUE,0);
	gtk_signal_connect(GTK_OBJECT(b),"clicked",GTK_SIGNAL_FUNC(cancel_b),NULL);
	GTK_WIDGET_SET_FLAGS(b,GTK_CAN_DEFAULT);
	gtk_widget_show(b);

	gtk_widget_show(box);

	gtk_widget_show(vbox);
	gtk_widget_show(c_win);

	gtk_grab_add(c_win);
	gtk_main();

	changed=FALSE;

	if( ok_pushed ) {
		if ( conf.scroll != scrolled ) changed=TRUE;
		conf.scroll=scrolled;
		conf.Esave = Esaved;
	}
	
	for( i=0 ; i<card_num ; i++ ) {
		for( j=0 ; j<cards[i].info.mixerdevs ; j++ ) {
			if( ok_pushed ) {
				if( !changed ) {
					if( cards[i].mixer[j].enable != ccard[i].m[j].m_en )
						changed = TRUE;
				}
				cards[i].mixer[j].enable=ccard[i].m[j].m_en;
				cards[i].mixer[j].enabled=FALSE;
				if( !changed ) {
					if( cards[i].mixer[j].p_e != ccard[i].m[j].p_e ||
						cards[i].mixer[j].p_f != ccard[i].m[j].p_f )
						changed=TRUE;
				}
				cards[i].mixer[j].p_e=ccard[i].m[j].p_e;
				cards[i].mixer[j].p_f=ccard[i].m[j].p_f;
				for( k=0 ; k<cards[i].mixer[j].groups.groups ; k++ ) {
					if( !changed ) 
						if( cards[i].mixer[j].group[k].enable !=
							ccard[i].m[j].g_en[k] ) {
							changed = TRUE;
						}
					cards[i].mixer[j].group[k].enable = ccard[i].m[j].g_en[k];
					cards[i].mixer[j].group[k].enabled=FALSE;
				}
				for( k=0 ; k<cards[i].mixer[j].ee_n ; k++ ) {
					if( !changed ) 
						if( cards[i].mixer[j].ee[k].enable !=
							ccard[i].m[j].ee_en[k] ) {
							changed = TRUE;
						}
					cards[i].mixer[j].ee[k].enable = ccard[i].m[j].ee_en[k];
					cards[i].mixer[j].ee[k].enabled=FALSE;
				}
			}
			g_free(ccard[i].m[j].g_en);
			g_free(ccard[i].m[j].ee_en);
		}
		g_free(ccard[i].m);
	}
	g_free(ccard);
	if( Tosave ) {
		conf_write();
		conf.F_save=FALSE;
	}

	return changed;
}

static void tb_callback(GtkToggleButton *b,gint *c) {
	*c=b->active;
}

void conf_read( void ) {
	int i,j,k,err,ln;
	FILE *fp;
	gchar rbuf[256],*s;
	s_mixer *m=NULL;
	snd_mixer_gid_t gid;
	snd_mixer_eid_t eid;

	fp=fopen(conf.fna,"rt");
	if( fp == NULL ) {
		conf.F_save=TRUE;
		return;
	}
	ln=1;
	err=0;
	while( !feof(fp) && err>-5 ) {
		fgets(rbuf,255,fp);
		rbuf[255]=0;
		s=rbuf+2;
		err=0;
		switch( rbuf[0] ) {
		case 'S':
			conf.scroll=atoi(s)?TRUE:FALSE;
			break;
		case 'C':
			i=atoi(s);
			if( i<0 || i>2 ) {
				err=-1;
			} else conf.wmode=i;
			break;
		case 'W':
			sscanf(s,"%d,%d\n",&conf.width,&conf.height);
			break;
		case 'A':
			conf.Esave=atoi(s)?TRUE:FALSE;
			break;
		case 'M':
			sscanf(s,"%d,%d=%d\n",&i,&j,&k);
			if( i<0 || i>=card_num ) {
				cread_err(_("Invalied card No."),ln);
				err=-10;
				break;
			}
			if( j<0 || j>=cards[i].info.mixerdevs ) {
				cread_err(_("Invalied mixer device No."),ln);
				err=-10;
			}
			m=&cards[i].mixer[j];
			m->enable=k?TRUE:FALSE;
			break;
		case 'X':
			if( m == NULL ) {
				cread_err(_("No mixer selected"),ln);
				err=-1;
			}
			switch(i) {
			case 0:
				m->p_e=FALSE;
				m->p_f=FALSE;
				break;
			case 1:
				m->p_e=TRUE;
				m->p_f=FALSE;
				break;
			case 2:
				m->p_e=TRUE;
				m->p_f=TRUE;
			default:
				cread_err(_("Invalied value for X"),ln);
				err=-1;
				break;
			}
			break;
		case 'G':
			if( m == NULL ) {
				cread_err(_("No mixer selected"),ln);
				err=-1;
			}
			s++;
			for( i=0 ; *s!='\'' && *s>0 ; i++ ) gid.name[i]=*(s++);
			gid.name[i]=0;
			if( *s == 0 ) {
				cread_err(_("Invalied argument"),ln);
				err=-1;
				break;
			}
			s+=2;
			sscanf(s,"%d=%d\n",&gid.index,&i);
			for( j=0; j<m->groups.groups ; j++ ) {
				if( strcmp(gid.name,m->groups.pgroups[j].name) == 0 &&
					gid.index == m->groups.pgroups[j].index ) {
					m->group[j].enable=i?TRUE:FALSE;
					break;
				}
			}
			if( j>=m->groups.groups ) {
				cread_err(_("There is no such mixer group"),ln);
				err=-1;
			}
			break;
		case 'E':
			if( m == NULL ) {
				cread_err(_("No mixer selected"),ln);
				err=-1;
			}
			s++;
			for( i=0 ; *s!='\'' && *s>0 ; i++ ) eid.name[i]=*(s++);
			eid.name[i]=0;
			if( *s == 0 ) {
				cread_err(_("Invalied argument"),ln);
				err=-1;
				break;
			}
			s+=2;
			sscanf(s,"%d,%d=%d\n",&eid.index,&eid.type,&i);
			for( j=0; j<m->ee_n ; j++ ) {
				if( strcmp(eid.name,m->ee[j].e.e.eid.name) == 0 &&
					eid.index == m->ee[j].e.e.eid.index &&
					eid.type == m->ee[j].e.e.eid.type ) {
					m->ee[j].enable=i?TRUE:FALSE;
					break;
				}
			}
			if( j>=m->ee_n ) {
				cread_err(_("There is no such mixer element"),ln);
				err=-1;
			}
			break;
		}
		if( err<0 ) conf.F_save=TRUE;
		ln++;
	}
	fclose(fp);
}
static void cread_err(gchar *s,int n ) {
	fprintf(stderr,_("config %d:%s\n"),n,s);
}

void conf_write(void) {
	int i,j,k;
	FILE *fp;
	s_mixer *m;
	s_group *g;
	s_eelements *ee;

	fp=fopen(conf.fna,"wt");
	if( fp == NULL ) {
		chk_cfile();
		fp=fopen(conf.fna,"wt");
	}
	if( fp == NULL ) {
		fprintf(stderr,_("gamix: config file not saved.\n"));
		return;
	}
	fprintf(fp,"# OPT\n");
	fprintf(fp,"S %d\n",conf.scroll);
	fprintf(fp,"C %d\n",conf.wmode);
	fprintf(fp,"A %d\n",conf.Esave);
	gdk_window_get_size(window->window,&i,&j);
	fprintf(fp,"W %d,%d\n",i,j);
	for( i=0 ; i<card_num ; i++ ) {
		for( j=0 ; j<cards[i].info.mixerdevs ; j++ ) {
			m=&cards[i].mixer[j];
			fprintf(fp,"# Card: %s\n#   Mixer: %s\n",cards[i].info.name,
					m->info.name);
			fprintf(fp,"M %d,%d=%d\n",i,j,m->enable);
			if( m->p_e ) {
				if( m->p_f ) k=2; else k=3;
			} else k=0;
			fprintf(fp,"X %d\n",k);
			for( k=0; k<m->groups.groups ; k++ ) {
				g=&cards[i].mixer[j].group[k];
				fprintf(fp,"G '%s',%d=%d\n",
						m->groups.pgroups[k].name,m->groups.pgroups[k].index,
						g->enable);
			}
			for( k=0; k<m->ee_n ; k++ ) {
				ee=&cards[i].mixer[j].ee[k];
				fprintf(fp,"E '%s',%d,%d=%d\n",
						ee->e.e.eid.name,ee->e.e.eid.index,ee->e.e.eid.type,
						ee->enable);
			}
		}
	}
	fclose(fp);
}

static void chk_cfile( void ) {
	int i,j,k,err;
	gchar *name;

	k=strlen(g_get_home_dir());
	name=g_strdup(conf.fna);
	i=1;
	j=strlen(name)-1;
	err=-1;
	while( i>0 ) {
		if( err<0 ) {
			while( name[j] != '/' ) j--;
			name[j]=0;
			if( j <= k ) {
				fprintf(stderr,"Can not make dir ~/.gamix\n");
				g_free(name);
				return;
			}
		} else {
			while( name[j] != 0 ) j++;
			name[j]='/';
		}
		err=mkdir(name,S_IRUSR|S_IWUSR|S_IXUSR|
				       S_IRGRP|S_IXGRP| S_IROTH|S_IXOTH);
		if( err<0 ) {
			if( errno == ENOENT ) {
				i++;
			} else {
				fprintf(stderr,"Can not make dir %s\n",name);
				g_free(name);
				return;
			}
		} else {
			i--;
		}
	}
}
