
#include "gamix.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

s_conf conf;

static GtkWidget *c_win;

typedef struct {
	gboolean m_en;
	gboolean *obj_en;
	GSList *gp;
	gboolean p_e;
	gboolean p_f;
	GtkCList *cl;
	int *ord_l;
	int o_nums;
} c_mixer;

typedef struct {
	c_mixer *m;
} c_card;

static c_card *ccard;
static gboolean scrolled;
static gboolean ok_pushed;
static gboolean Esaved;
static gboolean Tosave;
static gboolean sv_wsized;

static void close_win(GtkWidget *,gpointer);
static void cancel_b(GtkWidget *,gpointer);
static void ok_b(GtkWidget *,gpointer);
static void tb_callback(GtkToggleButton *,gint *);
static void sl1_callback(GtkWidget *,gint,gint,GdkEventButton *,gpointer);
static void sl2_callback(GtkWidget *,gint,gint,GdkEventButton *,gpointer);
static int sel_num(GtkCList *,gint);
static void cread_err(gchar *,int );
static void chk_cfile(void);
static void swap_obj(s_obj_t **,s_obj_t *,s_obj_t *);
static s_obj_t *obj_new( s_obj_t **,s_obj_t *);

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
	gchar *s;

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
			for( k=0 ; k<ccard[i].m[j].o_nums ; k++ ) {
				gtk_clist_get_text(ccard[i].m[j].cl,k,0,&s);
				ccard[i].m[j].ord_l[k]=atoi(s)-1;
			}
		}
	}
	gtk_widget_destroy(c_win);
}

gint conf_win( void ) {
	int i,j,k,l,m,sf;
	gint changed,*o_l;
	GtkWidget *b;
	GtkWidget *vbox,*box,*frame,*hbox,*hhbox;
	GtkWidget *clist;
	GtkWidget *nb,*n_label;
	GtkStyle *style;
	unsigned char gname[40];
	GSList *gp;
	//s_group_t *group;
	//s_eelements_t *ee;
	s_obj_t *obj,*obj_b,*obj2,*obj2_b;
	gchar *cl_data[3],cl_num[6];
	GtkRequisition rq;

	ok_pushed=FALSE;


	c_win=gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_signal_connect(GTK_OBJECT(c_win),"destroy",GTK_SIGNAL_FUNC(close_win),
					   NULL);
	//gtk_widget_show(c_win);
	style=gtk_widget_get_style(c_win);

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

	hbox=gtk_hbox_new(FALSE,4);
	gtk_box_pack_start(GTK_BOX(box),hbox,FALSE,FALSE,0);
	sv_wsized=conf.sv_wsize;
	b=gtk_toggle_button_new();
	gtk_widget_set_usize(b,10,10);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b),conf.sv_wsize);
	gtk_box_pack_start(GTK_BOX(hbox),b,FALSE,FALSE,0);
	gtk_signal_connect(GTK_OBJECT(b),"toggled",GTK_SIGNAL_FUNC(tb_callback),
					   (gpointer)&sv_wsized);
	gtk_widget_show(b);
	n_label=gtk_label_new(_("Save window size"));
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

			clist=gtk_clist_new(3);
			gtk_clist_freeze(GTK_CLIST(clist));
			gtk_clist_set_selection_mode(GTK_CLIST(clist),
										 GTK_SELECTION_MULTIPLE);
			gtk_clist_set_column_width(GTK_CLIST(clist),0,20);
			gtk_clist_set_column_width(GTK_CLIST(clist),1,6);
			gtk_clist_set_column_width(GTK_CLIST(clist),2,18);
			gtk_clist_set_column_justification(GTK_CLIST(clist),
											   0,GTK_JUSTIFY_RIGHT);
			gtk_clist_set_column_justification(GTK_CLIST(clist),
											   3,GTK_JUSTIFY_LEFT);

			hhbox=gtk_scrolled_window_new(NULL,NULL);
			gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(hhbox),
										   GTK_POLICY_NEVER,
										   GTK_POLICY_AUTOMATIC);
			gtk_widget_show(hhbox);

			ccard[i].m[j].o_nums=cards[i].mixer[j].o_nums;
			ccard[i].m[j].obj_en=(gboolean *)g_malloc(ccard[i].m[j].o_nums
												  * sizeof(gboolean));
			ccard[i].m[j].ord_l=(gint *)g_malloc(ccard[i].m[j].o_nums
												  * sizeof(gint));
			if( ccard[i].m[j].obj_en == NULL || ccard[i].m[j].ord_l == NULL ) {
				fprintf(stderr,nomem_msg);
				g_free(ccard);
				return -1;
			}
			cl_data[0]=" ";
			cl_data[1]=" ";
			cl_data[2]=" ";
			obj=cards[i].mixer[j].obj;
			for( k=0 ; k<ccard[i].m[j].o_nums ; k++ ) {

				sprintf(cl_num,"%d",k+1);
				if( obj->g ) {
					if( obj->g->g.gid.index > 0 ) {
					sprintf(gname,"%s %d",
							obj->g->g.gid.name,
							obj->g->g.gid.index);
					} else {
						strcpy(gname,obj->g->g.gid.name);
					}
				}
				if( obj->e ) {
					if( obj->e->e.e.eid.index > 0 ) {
						sprintf(gname,"%s %d",
								obj->e->e.e.eid.name,
								obj->e->e.e.eid.index);
					} else {
						strcpy(gname,obj->e->e.e.eid.name);
					}
				}
				cl_data[0]=cl_num;
				if( obj->dyn_e ) {
					cl_data[1]="D";
				} else {
					cl_data[1]=" ";
				}
				cl_data[2]=gname;
				gtk_clist_append(GTK_CLIST(clist),cl_data);

				ccard[i].m[j].obj_en[k]=obj->enable;
				if( obj->enable ) {
					gtk_clist_select_row(GTK_CLIST(clist),k,0);
				} else {
					gtk_clist_unselect_row(GTK_CLIST(clist),k,0);
				}

				obj=obj->next;
			}
			ccard[i].m[j].cl=GTK_CLIST(clist);
			gtk_clist_set_reorderable(GTK_CLIST(clist),TRUE);
			gtk_signal_connect(GTK_OBJECT(clist),"select_row",
							   GTK_SIGNAL_FUNC(sl1_callback),
							   (gpointer)&ccard[i].m[j]);
			gtk_signal_connect(GTK_OBJECT(clist),"unselect_row",
							   GTK_SIGNAL_FUNC(sl2_callback),
							   (gpointer)&ccard[i].m[j]);
			gtk_widget_show(clist);
			gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(hhbox),
												  clist);
			gtk_container_set_border_width(GTK_CONTAINER(hhbox),8);
			gtk_widget_size_request(hhbox,&rq);
			gtk_widget_set_usize(hhbox,rq.width,rq.height*2);
			gtk_box_pack_start(GTK_BOX(box),hhbox,FALSE,FALSE,0);

			gtk_widget_show(hhbox);
			gtk_widget_show(box);
			gtk_widget_show(frame);
			gtk_clist_thaw(GTK_CLIST(clist));
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
		conf.sv_wsize=sv_wsized;
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
				k=0;
				sf=0;
				for( obj=cards[i].mixer[j].obj ; obj != NULL ;
					 obj=obj->next ) {
					if( !changed ) {
						if( ccard[i].m[j].ord_l[k] != k ) changed=TRUE;
						if( obj->enable != ccard[i].m[j].obj_en[ccard[i].m[j].ord_l[k]] ) {
							changed = TRUE;
						}
					}
					if( ccard[i].m[j].ord_l[k] != k ) sf=1;
					obj->enable=ccard[i].m[j].obj_en[k];
					obj->enabled=FALSE;
					k++;
				}
				if( sf ) {
					o_l=(gint *)g_malloc(sizeof(gint)*ccard[i].m[j].o_nums);
					if( o_l != NULL ) {
						for( k=0 ; k<ccard[i].m[j].o_nums ; k++ ) o_l[k]=k;
						obj_b=NULL;
						obj=cards[i].mixer[j].obj;
						for( k=0 ; k<ccard[i].m[j].o_nums ; k++ ) {
							if( ccard[i].m[j].ord_l[k] != o_l[k] ) {
								obj2=obj;
								for( l=k ; ccard[i].m[j].ord_l[k]!=o_l[l] ; l++ ) {
									obj2_b=obj2;
									obj2=obj2->next;
								}
								for( m=l ; m>k ; m-- ) o_l[m]=o_l[m-1];
								o_l[m]=k;
								if( obj_b == NULL ) {
									cards[i].mixer[j].obj=obj2;
								} else {
									obj_b->next=obj2;
								}
								obj2_b->next=obj2->next;
								obj2->next=obj;
								obj=obj2;
							}
							obj_b=obj;
							obj=obj->next;
						}
						g_free(o_l);
					}
				}
				/*
				for( obj=cards[i].mixer[j].obj ; obj != NULL ; obj=obj->next) {
					if( obj->g ) {
						printf("G %s %d\n",obj->g->g.gid.name,obj->g->g.gid.index);
					}
					if( obj->e ) {
						printf("E '$s',%d,%d\n",obj->e->e.e.eid.name,
							   obj->e->e.e.eid.index,obj->e->e.e.eid.type);
					}
				}
				*/
			}
			g_free(ccard[i].m[j].obj_en);
			g_free(ccard[i].m[j].ord_l);
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
static void sl1_callback(GtkWidget *w,gint row,gint col,
						 GdkEventButton *ev,gpointer data) {
	int i;

	c_mixer *m=(c_mixer *)data;
	i=sel_num(GTK_CLIST(w),row);
	m->obj_en[i]=TRUE;
}
static void sl2_callback(GtkWidget *w,gint row,gint col,
						 GdkEventButton *ev,gpointer data) {
	int i;

	c_mixer *m=(c_mixer *)data;
	i=sel_num(GTK_CLIST(w),row);
	m->obj_en[i]=FALSE;
}

static int sel_num(GtkCList *cl,gint row) {
	int rt;
	gchar *s;

	gtk_clist_get_text(cl,row,0,&s);
	rt=atoi(s)-1;
	return rt;
}

void conf_read( void ) {
	int i,j,k,err,ln;
	FILE *fp;
	gchar rbuf[256],*s;
	s_mixer_t *m=NULL;
	snd_mixer_gid_t gid;
	snd_mixer_eid_t eid;
	s_group_t *group;
	s_eelements_t *ee;
	s_obj_t *obj,*obj_n;

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
		case 'Y':
			conf.sv_wsize=atoi(s)?TRUE:FALSE;
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
			obj_n=m->obj;
			break;
		case 'X':
			if( m == NULL ) {
				cread_err(_("No mixer selected"),ln);
				err=-1;
			}
			switch(atoi(s)) {
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
				break;
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
			for( obj=m->obj ; obj != NULL ; obj=obj->next ) {
				if( obj->g ) {
					group=obj->g;
					if( strcmp(gid.name,group->g.gid.name) == 0 &&
						gid.index == group->g.gid.index ) {
						obj->enable=i&1?TRUE:FALSE;
						obj->dyn_e=i&2?3:0;
						break;
					}
				}
			}
			if( obj ) {
				if( obj != obj_n ) swap_obj(&m->obj,obj_n,obj);
				obj_n=obj->next;
			} else {
				if( i&2 ) {
					if( obj_ins_new_g(&m->obj,&obj_n,&gid) == 0 ) {
						obj_n->enable=i&1?TRUE:FALSE;
						obj_n->dyn_e=i&2?2:0;
						obj_n=obj_n->next;
						m->o_nums++;
					} else {
						err=-1;
					}
				} else {
					cread_err(_("There is no such mixer group"),ln);
					err=-1;
				}
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
			for( obj=m->obj ; obj != NULL ; obj=obj->next ) {
				if( obj->e ) {
					ee=obj->e;
					if( strcmp(eid.name,ee->e.e.eid.name) == 0 &&
						eid.index == ee->e.e.eid.index &&
						eid.type == ee->e.e.eid.type ) {
						obj->enable=i&1?TRUE:FALSE;
						obj->dyn_e=i&2?TRUE:FALSE;
						break;
					}
				}
			}
			if( obj ) {
				if( obj != obj_n ) swap_obj(&m->obj,obj_n,obj);
				obj_n=obj->next;
			} else {
				if( i&2 ) {
				} else {
					cread_err(_("There is no such mixer element"),ln);
					err=-1;
				}
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

static void swap_obj( s_obj_t **obj,s_obj_t *o1,s_obj_t *o2 ) {
	s_obj_t *p,*q;

	if( o1 == NULL ) return;

	q=o1;
	while( q->next != o2 ) q=q->next;
	q->next=o2->next;
	if( *obj == o1 ) {
		*obj=o2;
		o2->next=o1;
	} else {
		p=*obj;
		while( p->next != o1 ) p=p->next;
		p->next=o2;
		o2->next=o1;
	}
}

void conf_write(void) {
	int i,j,k;
	FILE *fp;
	s_mixer_t *m;
	s_group_t *g;
	s_eelements_t *ee;
	s_obj_t *obj;

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
	fprintf(fp,"Y %d\n",conf.sv_wsize);
	if( conf.sv_wsize ) fprintf(fp,"W %d,%d\n",i,j);
	for( i=0 ; i<card_num ; i++ ) {
		for( j=0 ; j<cards[i].info.mixerdevs ; j++ ) {
			m=&cards[i].mixer[j];
			fprintf(fp,"# Card: %s\n#   Mixer: %s\n",cards[i].info.name,
					m->info.name);
			fprintf(fp,"M %d,%d=%d\n",i,j,m->enable);
			if( m->p_e ) {
				if( m->p_f ) k=2; else k=1;
			} else k=0;
			fprintf(fp,"X %d\n",k);
			for( obj=m->obj ; obj != NULL ; obj=obj->next ) {
				if( obj->g ) {
					g=obj->g;
					fprintf(fp,"G '%s',%d=%d\n",g->g.gid.name,g->g.gid.index,
							(obj->enable?1:0)|(obj->dyn_e?2:0));
				}
				if( obj->e ) {
					ee=obj->e;
					fprintf(fp,"E '%s',%d,%d=%d\n",
						ee->e.e.eid.name,ee->e.e.eid.index,ee->e.e.eid.type,
						obj->enable);
				}
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

static s_obj_t *obj_new( s_obj_t **objs,s_obj_t *o ) {
	s_obj_t *p,*q;

	q=(s_obj_t *)g_malloc0(sizeof(s_obj_t));
	if( q == NULL ) {
		fprintf(stderr,nomem_msg);
		return NULL;
	}
	if( *objs == o ) {
		q->next=*objs;
		*objs=q;
	} else {
		p=*objs;
		while( p->next != o ) p=p->next;
		q->next=p->next;
		p->next=q;
	}
	return q;
}

gint obj_ins_new_g( s_obj_t **objs,s_obj_t **o1,snd_mixer_gid_t *gid ) {
	s_obj_t *p;
	s_group_t *g;

	p=obj_new(objs,*o1);
	if( p == NULL ) return -1;
	g=(s_group_t *)g_malloc0(sizeof(s_group_t));
	if( g == NULL ) {
		fprintf(stderr,nomem_msg);
		return -1;
	}
	g->g.gid=*gid;
	p->g=g;
	*o1=p;
	return 0;
}
	   
gint obj_ins_new_e( s_obj_t **objs,s_obj_t **o1,snd_mixer_eid_t *eid ) {
	s_obj_t *p;
	s_eelements_t *e;

	p=obj_new(objs,*o1);
	if( p == NULL ) return -1;
	e=(s_eelements_t *)g_malloc0(sizeof(s_eelements_t));
	if( e == NULL ) {
		fprintf(stderr,nomem_msg);
		return -1;
	}
	e->e.e.eid=*eid;
	p->e=e;
	*o1=p;
	return 0;
}
	   
