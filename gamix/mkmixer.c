
#include "gamix.h"

static gchar *label_3d[]={
	"wide","volume","center","space","depth","delay","feedback","depth rear"};
static gchar *label_tone[]={"B","T"};

static void close_callback(GtkWidget *,s_mixer *);
static void volume1_callback(GtkAdjustment *,s_element *);
static void volume1_sw_callback(GtkToggleButton *,s_element *);
static void switch1_callback(GtkToggleButton *,s_element *);
static void switch2_callback(GtkToggleButton *,s_element *);
static void chain_callback(GtkToggleButton *,s_group *);
static void accu3_callback(GtkAdjustment *,s_element *);
static void mux1_callback(GtkItem *,s_element *);
static void mux2_callback(GtkItem *,s_element *);
static void sw_3d_callback(GtkToggleButton *,s_element *);
static void vol_3d_callback(GtkAdjustment *,s_element *);
static void sw_tone_callback(GtkToggleButton *,s_element *);
static void vol_tone_callback(GtkAdjustment *,s_element *);
static void chain_callback2(GtkToggleButton *,s_eelements *);
static gint mk_element(s_element *,GtkBox *);

static void close_callback(GtkWidget *w,s_mixer *mixer) {
	int i;
	s_group *g;
	s_eelements *ee;

	for( i=0 ; i<mixer->groups.groups ; i++ ) {
		g=&mixer->group[i];
		g->enabled=FALSE;
	}
	for( i=0 ; i<mixer->ee_n ; i++ ) {
		ee=&mixer->ee[i];
		ee->enabled=FALSE;
	}
	snd_mixer_close(mixer->handle);
	mixer->handle=NULL;
}

static void volume1_sw_callback(GtkToggleButton *b,s_element *e) {
	int i,j,value,err;

	for( i=0 ; i<e->e.data.volume1.voices; i++ ) {
		if( b == GTK_TOGGLE_BUTTON(e->w[i]) ) break;
	}
	value=b->active?1:0;
	if( e->e.data.volume1.pvoices[i] == value ) return;
	if( e->e.data.volume1.voices > 1 && *e->chain ) {
		for( j=0 ; j<e->e.data.volume1.voices; j++ ) {
			e->e.data.volume1.pvoices[j]=value;
			if( j!= i ) {
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(e->w[j]),b->active);
			}
		}
	} else {
		e->e.data.volume1.pvoices[i]=value;
	}
	err=snd_mixer_element_write(cards[e->card].mixer[e->mdev].handle,&e->e);
	if( err < 0 ) {
		fprintf(stderr,_("mixer element write error: %s\n"),snd_strerror(err));
	}
}

static void volume1_callback(GtkAdjustment *adj,s_element *e) {
	int i,j,value,err;

	for( i=0 ; i<e->e.data.volume1.voices; i++ ) {
		if( adj == e->adj[i] ) break;
	}
	value=-(int)adj->value;
	if( e->e.data.volume1.pvoices[i] == value ) return;
	if( e->e.data.volume1.voices > 1 && *e->chain ) {
		for( j=0 ; j<e->e.data.volume1.voices; j++ ) {
			e->e.data.volume1.pvoices[j]=value;
			if( j!= i ) {
				e->adj[j]->value=adj->value;
				gtk_signal_emit_by_name(GTK_OBJECT(e->adj[j]),"value_changed");
			}
		}
	} else {
		e->e.data.volume1.pvoices[i]=value;
	}
	err=snd_mixer_element_write(cards[e->card].mixer[e->mdev].handle,&e->e);
	if( err < 0 ) {
		fprintf(stderr,_("mixer element write error: %s\n"),snd_strerror(err));
	}
}

static void switch1_callback(GtkToggleButton *b,s_element *e ) {
	int i,j;

	for( i=0 ; i<e->e.data.switch1.sw; i++ ) {
		if( b == (GtkToggleButton *)e->w[i] ) break;
	}
	if(	(snd_mixer_get_bit(e->e.data.switch1.psw,i)?TRUE:FALSE) == b->active )
		return;
	if( e->e.data.switch1.sw > 1 && *e->chain ) {
		for( j=0 ; j<e->e.data.switch1.sw; j++ ) {
			snd_mixer_set_bit(e->e.data.switch1.psw,j,b->active);
			if( j != i )
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(e->w[j]),b->active);
		}
	} else {
		snd_mixer_set_bit(e->e.data.switch1.psw,i,b->active);
	}
	snd_mixer_element_write(cards[e->card].mixer[e->mdev].handle,&e->e);
}

static void switch2_callback(GtkToggleButton *b,s_element *e ) {
	int err;

	e->e.data.switch2.sw=b->active;
	err=snd_mixer_element_write(cards[e->card].mixer[e->mdev].handle,&e->e);
}

static void chain_callback(GtkToggleButton *b,s_group *g ) {
	g->chain = b->active;
}

static void accu3_callback(GtkAdjustment *adj,s_element *e) {
	int i,j,value,err;

	for( i=0 ; i<e->e.data.accu3.voices; i++ ) {
		if( adj == e->adj[i] ) break;
	}
	value=-(int)adj->value;
	if( e->e.data.accu3.pvoices[i] == value ) return;
	if( e->e.data.accu3.voices > 1 && *e->chain ) {
		for( j=0 ; j<e->e.data.accu3.voices; j++ ) {
			e->e.data.accu3.pvoices[j]=value;
			if( j!= i ) {
				e->adj[j]->value=adj->value;
				gtk_signal_emit_by_name(GTK_OBJECT(e->adj[j]),"value_changed");
			}
		}
	} else {
		e->e.data.accu3.pvoices[i]=value;
	}
	err=snd_mixer_element_write(cards[e->card].mixer[e->mdev].handle,&e->e);
	if( err < 0 ) {
		fprintf(stderr,_("mixer element write error: %s\n"),snd_strerror(err));
	}
}

static void mux1_callback(GtkItem *item,s_element *e ) {
	int i,ch,no,err;

	ch=(int)gtk_object_get_data(GTK_OBJECT(item),"ch");
	no=(int)gtk_object_get_data(GTK_OBJECT(item),"no");

	if( strcmp(e->mux[no].name,e->e.data.mux1.poutput[ch].name) == 0 &&
		e->mux[no].index == e->e.data.mux1.poutput[ch].index &&
		e->mux[no].type == e->e.data.mux1.poutput[ch].type ) return;

	if( *e->chain ) {
		for( i=0 ; i<e->e.data.mux1.output ; i++ ) {
			e->e.data.mux1.poutput[i]=e->mux[no];
			if( ch != i ) gtk_option_menu_set_history(
											  GTK_OPTION_MENU(e->w[i]),no);
		}
	} else {
		e->e.data.mux1.poutput[ch]=e->mux[no];
	}
	err=snd_mixer_element_write(cards[e->card].mixer[e->mdev].handle,&e->e);
	if( err< 0 ) {
		fprintf(stderr,_("mixer mux1 element write error: %s\n"),snd_strerror(err));
	}
}

static void mux2_callback(GtkItem *item,s_element *e ) {
	int no,err;

	no=(int)gtk_object_get_data(GTK_OBJECT(item),"no");

	if( strcmp(e->mux[no].name,e->e.data.mux2.output.name) == 0 &&
		e->mux[no].index == e->e.data.mux2.output.index &&
		e->mux[no].type == e->e.data.mux2.output.type ) return;

	e->e.data.mux2.output=e->mux[no];
	err=snd_mixer_element_write(cards[e->card].mixer[e->mdev].handle,&e->e);
	if( err< 0 ) {
		fprintf(stderr,_("mixer mux1 element write error: %s\n"),snd_strerror(err));
	}
}

static void sw_3d_callback(GtkToggleButton *b,s_element *e ) {
	int err;

	if( b == (GtkToggleButton *)e->w[0] ) {
		e->e.data.teffect1.sw = b->active;
	} else {
		e->e.data.teffect1.mono_sw = b->active;
	}
	err=snd_mixer_element_write(cards[e->card].mixer[e->mdev].handle,&e->e);
}

static void vol_3d_callback(GtkAdjustment *adj,s_element *e) {
	int i,err,*v,value;

	for( i=0 ; i<7 ; i++ ) {
		if( adj == e->adj[i] ) break;
	}
	v=NULL;
	switch( i ) {
	case 0:
		v=&e->e.data.teffect1.wide;
		break;
	case 1:
		v=&e->e.data.teffect1.volume;
		break;
	case 2:
		v=&e->e.data.teffect1.center;
		break;
	case 3:
		v=&e->e.data.teffect1.space;
		break;
	case 4:
		v=&e->e.data.teffect1.depth;
		break;
	case 5:
		v=&e->e.data.teffect1.delay;
		break;
	case 6:
		v=&e->e.data.teffect1.feedback;
		break;
	case 7:
		v=&e->e.data.teffect1.depth_rear;
		break;
	}
	value=(int)adj->value;
	if( v ) {
		if( value == *v ) return;
		*v=value;
	} else return;
	err=snd_mixer_element_write(cards[e->card].mixer[e->mdev].handle,&e->e);
	if( err<0 ) {
		fprintf(stderr,_("3D effect write error: %s\n"),snd_strerror(err));
	}
}

static void sw_tone_callback(GtkToggleButton *b,s_element *e ) {
	int err;

	e->e.data.tc1.sw = b->active;
	e->e.data.tc1.tc=SND_MIXER_TC1_SW;
	err=snd_mixer_element_write(cards[e->card].mixer[e->mdev].handle,&e->e);
}

static void vol_tone_callback(GtkAdjustment *adj,s_element *e) {
	int i,err,*v,value;

	for( i=0 ; i<2 ; i++ ) {
		if( adj == e->adj[i] ) break;
	}
	v=NULL;
	switch( i ) {
	case 0:
		v=&e->e.data.tc1.bass;
		e->e.data.tc1.tc=SND_MIXER_TC1_BASS;
		break;
	case 1:
		v=&e->e.data.tc1.treble;
		e->e.data.tc1.tc=SND_MIXER_TC1_TREBLE;
		break;
	}
	value=-(int)adj->value;
	if( v ) {
		if( value == *v ) return;
		*v=value;
	} else return;
	err=snd_mixer_element_write(cards[e->card].mixer[e->mdev].handle,&e->e);
	if( err<0 ) {
		fprintf(stderr,_("Tone controll write error: %s\n"),snd_strerror(err));
	}
}

static void chain_callback2(GtkToggleButton *b,s_eelements *ee ) {
	ee->chain = b->active;
}

GtkWidget *make_mixer( gint c_n , gint m_n ) {
	int i,j,k,err;
	GtkWidget *mv_box,*m_name;
	GtkWidget *s_win;
	GtkWidget *mh_box;
	GtkWidget *frame;
	GtkWidget *iv_box;
	GtkWidget *ih_box;
	GtkWidget *c_l;
	char gname[40];
	s_mixer *mixer;
	s_group *group=NULL;
	s_element *e;
	s_eelements *ee;

	if( cards[c_n].mixer[m_n].handle ) {
		snd_mixer_close(cards[c_n].mixer[m_n].handle);
	}
	if( (err=snd_mixer_open(&cards[c_n].mixer[m_n].handle,c_n,m_n)) < 0 ) {
		return NULL;
	}

	mixer = &cards[c_n].mixer[m_n];

	mv_box=gtk_vbox_new(FALSE,0);
	gtk_widget_show(mv_box);

	sprintf(gname,"%s:%s",cards[c_n].info.name,
			cards[c_n].mixer[m_n].info.name);
	m_name=gtk_label_new(gname);
	gtk_box_pack_start(GTK_BOX(mv_box),m_name,FALSE,FALSE,0);
	gtk_widget_show(m_name);

	mh_box=gtk_hbox_new(FALSE,2);
	if( conf.scroll ) {
		s_win=gtk_scrolled_window_new(NULL,NULL);
		gtk_box_pack_start(GTK_BOX(mv_box),s_win,TRUE,TRUE,0);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(s_win),
									   GTK_POLICY_AUTOMATIC,
									   GTK_POLICY_NEVER);
		gtk_widget_show(s_win);
		gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(s_win),
											  mh_box);
		//gtk_container_add(GTK_CONTAINER(s_win),mh_box);
	} else {
		gtk_box_pack_start(GTK_BOX(mv_box),mh_box,TRUE,TRUE,4);
	}
	gtk_widget_show(mh_box);

	for( i=0 ; i<mixer->groups.groups ; i++ ) {
		group = &mixer->group[i];
		k=0;
		for( j=0 ; j<group->g.elements ; j++ ) {
			if( group->e[j].e.eid.type ) k++;
		}
		if( k==0 ) mixer->group[i].enable=FALSE;
		if( mixer->group[i].enable ) {
			group->v_frame=frame=gtk_frame_new(NULL);
			gtk_frame_set_shadow_type(GTK_FRAME(frame),GTK_SHADOW_OUT);
			gtk_box_pack_start(GTK_BOX(mh_box),frame,
							   mixer->p_e,mixer->p_f,0);
			iv_box=gtk_vbox_new(FALSE,0);
			gtk_container_add(GTK_CONTAINER(frame),iv_box);
			group->chain_en=FALSE;
			for( j=0 ; j<group->g.elements ; j++ ) {
				e=&group->e[j];
				e->chain = &group->chain;
				e->chain_en = &group->chain_en;
				if( mk_element(e,GTK_BOX(iv_box))<0 ) return NULL;
			}
			if( mixer->group[i].g.gid.index > 0 ) {
				sprintf(gname,"%s%d",mixer->group[i].g.gid.name,
						mixer->group[i].g.gid.index);
			} else {
				sprintf(gname,"%s",mixer->group[i].g.gid.name);
			}
			ih_box=gtk_hbox_new(FALSE,2);
			gtk_box_pack_start(GTK_BOX(iv_box),ih_box,FALSE,FALSE,0);
			if( group->chain_en ) {
				group->cwb=gtk_toggle_button_new();
				gtk_box_pack_start(GTK_BOX(ih_box),group->cwb,
								   FALSE,FALSE,4);
				gtk_widget_set_usize(group->cwb,10,10);
				gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(group->cwb)
											,group->chain);
				gtk_widget_show(group->cwb);
				gtk_signal_connect(GTK_OBJECT(group->cwb),"toggled",
								   GTK_SIGNAL_FUNC(chain_callback),
								   (gpointer)group);
				c_l=gtk_label_new(_("Lock"));
				gtk_box_pack_start(GTK_BOX(ih_box),c_l,FALSE,FALSE,0);
				gtk_widget_show(c_l);
				gtk_widget_show(ih_box);
				if( strlen(gname) > 10 ) {
					j=0;
					while( gname[j]!=' ' && gname[j]!=0 ) j++;
					if( gname[j]!=0 ) {
						gname[j+3]=0;
					}
				}
			} else {
				c_l=gtk_label_new(" ");
				gtk_box_pack_start(GTK_BOX(ih_box),c_l,FALSE,FALSE,0);
				gtk_widget_show(c_l);
				if( strlen(gname) > 5 ) {
					j=0;
					while( gname[j]!=' ' && gname[j]!=0 ) j++;
					if( gname[j]!=0 ) {
						gname[j+3]=0;
					}
				}
			}
			gtk_frame_set_label(GTK_FRAME(frame),gname);
			gtk_widget_show(ih_box);
			gtk_widget_show(iv_box);
			gtk_widget_show(frame);
			group->enabled=TRUE;
		} else {
			mixer->group[i].enabled=FALSE;
		}
	}
	for( i=0 ; i<mixer->ee_n ; i++ ) {
		if( mixer->ee[i].enable ) {
			ee=&mixer->ee[i];
			e=&ee->e;
			ee->v_frame=frame=gtk_frame_new(NULL);
			gtk_frame_set_shadow_type(GTK_FRAME(frame),GTK_SHADOW_OUT);
			gtk_box_pack_start(GTK_BOX(mh_box),frame,
							   mixer->p_e,mixer->p_f,0);
			iv_box=gtk_vbox_new(FALSE,0);
			gtk_container_add(GTK_CONTAINER(frame),iv_box);
			ee->chain_en=FALSE;
			e->chain=&ee->chain;
			e->chain_en=&ee->chain_en;
			if( mk_element(e,GTK_BOX(iv_box))<0 ) return NULL;
			ih_box=gtk_hbox_new(FALSE,2);
			gtk_box_pack_start(GTK_BOX(iv_box),ih_box,FALSE,FALSE,0);
			if( e->e.eid.index > 0 ) {
				sprintf(gname,"%s%d",e->e.eid.name,e->e.eid.index);
			} else {
				sprintf(gname,"%s",e->e.eid.name);
			}
			if( ee->chain_en ) {
				ee->cwb=gtk_toggle_button_new();
				gtk_box_pack_start(GTK_BOX(ih_box),ee->cwb,FALSE,FALSE,4);
				gtk_widget_set_usize(ee->cwb,10,10);
				gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ee->cwb)
											,group->chain);
				gtk_widget_show(ee->cwb);
				gtk_signal_connect(GTK_OBJECT(ee->cwb),"toggled",
								   GTK_SIGNAL_FUNC(chain_callback2),
								   (gpointer)ee);
				c_l=gtk_label_new(_("Lock"));
				gtk_box_pack_start(GTK_BOX(ih_box),c_l,FALSE,FALSE,0);
				gtk_widget_show(c_l);
				gtk_widget_show(ih_box);
				if( strlen(gname) > 10 ) {
					j=0;
					while( gname[j]!=' ' && gname[j]!=0 ) j++;
					if( gname[j]!=0 ) {
						gname[j+3]=0;
					}
				}
			} else {
				c_l=gtk_label_new(" ");
				gtk_box_pack_start(GTK_BOX(ih_box),c_l,FALSE,FALSE,0);
				gtk_widget_show(c_l);
			}
			gtk_frame_set_label(GTK_FRAME(frame),gname);
			gtk_widget_show(ih_box);
			gtk_widget_show(iv_box);
			gtk_widget_show(frame);
			ee->enabled=TRUE;
		} else {
			mixer->ee[i].enabled=FALSE;
		}
	}
	gtk_signal_connect(GTK_OBJECT(mv_box),"destroy",
					   GTK_SIGNAL_FUNC(close_callback),(gpointer)mixer);
	mixer->enabled=TRUE;
	return mv_box;
}

#define MIX_3D_VOL(NO,name,min_name,max_name,sname) \
  if( e->info.data.teffect1.effect & sname ) { \
    ih_box=gtk_hbox_new(FALSE,2); \
    gtk_box_pack_start(iv_box,ih_box,FALSE,FALSE,0); \
    c_l=gtk_label_new(label_3d[NO]); \
	gtk_box_pack_start(GTK_BOX(ih_box),c_l,FALSE,FALSE,0); \
	gtk_widget_show(c_l); \
	gtk_widget_show(ih_box); \
	e->adj[NO]=(GtkAdjustment *)gtk_adjustment_new( \
	  (gfloat)e->e.data.teffect1.name, \
	  (gfloat)e->info.data.teffect1.min_name-0.5, \
	  (gfloat)e->info.data.teffect1.max_name+1.0, \
	  1.0,1.0,1.0); \
	gtk_signal_connect(GTK_OBJECT(e->adj[NO]), \
	  "value_changed",GTK_SIGNAL_FUNC(vol_3d_callback),(gpointer)e);\
	e->w[NO+2]=gtk_hscale_new(GTK_ADJUSTMENT(e->adj[NO])); \
	gtk_scale_set_draw_value(GTK_SCALE(e->w[NO+2]),FALSE); \
	gtk_box_pack_start(GTK_BOX(iv_box), e->w[NO+2],FALSE,FALSE,4); \
	gtk_widget_show(e->w[NO+2]); \
  } else { ;\
	e->w[NO+2]=NULL; \
	e->adj[NO]=NULL; \
  }
#define MIX_TONE_VOL(NO,name,min_name,max_name,sname) \
  if( e->info.data.tc1.tc & sname ) { \
    tv_box = gtk_vbox_new(FALSE,2); \
    gtk_box_pack_start(GTK_BOX(ih_box),tv_box,TRUE,TRUE,0); \
    c_l=gtk_label_new(label_tone[NO]); \
    gtk_box_pack_start(GTK_BOX(tv_box),c_l,FALSE,FALSE,0); \
	gtk_widget_show(c_l); \
	e->adj[NO]=(GtkAdjustment *)gtk_adjustment_new( \
	  -(gfloat)e->e.data.tc1.name, \
	  -(gfloat)e->info.data.tc1.max_name-0.5, \
	  -(gfloat)e->info.data.tc1.min_name+0.5, \
	  1.0,4.0,1.0); \
	gtk_signal_connect(GTK_OBJECT(e->adj[NO]), \
	  "value_changed",GTK_SIGNAL_FUNC(vol_tone_callback),(gpointer)e);\
	e->w[NO+1]=gtk_vscale_new(GTK_ADJUSTMENT(e->adj[NO])); \
	gtk_scale_set_draw_value(GTK_SCALE(e->w[NO+1]),FALSE); \
	gtk_box_pack_start(GTK_BOX(tv_box), e->w[NO+1],FALSE,FALSE,4); \
	gtk_widget_show(e->w[NO+1]); \
	gtk_widget_show(tv_box); \
  } else { ;\
	e->w[NO+1]=NULL; \
	e->adj[NO]=NULL; \
  }

gint mk_element(s_element *e,GtkBox *iv_box) {
	int i,j,k;
	GtkWidget *ih_box,*tv_box;
	GtkWidget *menu,*c_l,*item;

	ih_box=gtk_hbox_new(TRUE,0);
	switch( e->e.eid.type) {
	case SND_MIXER_ETYPE_VOLUME1:
		if( (e->info.data.volume1.prange[0].max-
			 e->info.data.volume1.prange[0].min) == 1 ) {
			gtk_box_pack_start(iv_box,ih_box,FALSE,FALSE,0);
		} else
			gtk_box_pack_start(iv_box,ih_box,TRUE,TRUE,0);
		if( e->e.data.volume1.voices > 1 ) {
			*e->chain_en=TRUE;
			*e->chain=TRUE;
		}
		if( e->w == NULL ) {
			e->w = (GtkWidget **)g_malloc( e->e.data.volume1.voices *
										   sizeof(GtkWidget *));
		}
		if( e->w == NULL ) {
			fprintf(stderr,nomem_msg);
			return -1;
		}
		if( e->adj == NULL ) {
			e->adj=(GtkAdjustment **)g_malloc(e->e.data.volume1.voices*
											  sizeof(GtkAdjustment *));
		}
		if( e->adj==NULL ) {
			printf(nomem_msg);
			return -1;
		}
		for( i=0 ; i<e->e.data.volume1.voices ; i++ ) {
			if( (e->info.data.volume1.prange[i].max-
				 e->info.data.volume1.prange[i].min) == 1 ) {
				e->adj[i]=NULL;
				e->w[i]=gtk_toggle_button_new_with_label("V");
				gtk_box_pack_start(GTK_BOX(ih_box),e->w[i],
								   FALSE,FALSE,0);
				gtk_toggle_button_set_state(
									GTK_TOGGLE_BUTTON(e->w[i]),
									e->e.data.volume1.pvoices[i]);
				gtk_signal_connect(GTK_OBJECT(e->w[i]),"toggled",
								   GTK_SIGNAL_FUNC(volume1_sw_callback),
								   (gpointer)e);
				gtk_widget_show(e->w[i]);
			} else {
				e->adj[i]=(GtkAdjustment *)gtk_adjustment_new(
						-(gfloat)e->e.data.volume1.pvoices[i],
						-(gfloat)e->info.data.volume1.prange[i].max-0.5,
						-(gfloat)e->info.data.volume1.prange[i].min+0.5,
						1.0,4.0,1.0);
				gtk_signal_connect(GTK_OBJECT(e->adj[i]),"value_changed",
								   GTK_SIGNAL_FUNC(volume1_callback),
								   (gpointer)e);
				e->w[i]=gtk_vscale_new(GTK_ADJUSTMENT(e->adj[i]));
				gtk_scale_set_draw_value(GTK_SCALE(e->w[i]),FALSE);
				gtk_box_pack_start(GTK_BOX(ih_box),e->w[i],FALSE,FALSE,4);
				gtk_widget_show(e->w[i]);
			}
		}
		break;
	case SND_MIXER_ETYPE_SWITCH1:
		gtk_box_pack_start(iv_box,ih_box,FALSE,FALSE,4);
		if( e->e.data.switch1.sw > 1 ) {
			*e->chain_en=TRUE;
			*e->chain=TRUE;
		}
		if( e->w == NULL ) {
			e->w = (GtkWidget **)g_malloc( e->e.data.switch1.sw *
										   sizeof(GtkWidget *));
		}
		if( e->w == NULL ) {
			fprintf(stderr,nomem_msg);
			return -1;
		}
		for( i=0 ; i<e->e.data.switch1.sw ; i++ ) {
			e->w[i]=gtk_toggle_button_new();
			gtk_box_pack_start(GTK_BOX(ih_box),e->w[i],FALSE,FALSE,0);
			gtk_widget_set_usize(e->w[i],10,10);
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(e->w[i]),
									snd_mixer_get_bit(e->e.data.switch1.psw,i)
										);
			gtk_signal_connect(GTK_OBJECT(e->w[i]),"toggled",
							   GTK_SIGNAL_FUNC(switch1_callback),(gpointer)e);
			gtk_widget_show(e->w[i]);
		}
		break;
	case SND_MIXER_ETYPE_SWITCH2:
		gtk_box_pack_start(iv_box,ih_box,FALSE,FALSE,4);
		if( e->w == NULL ) {
			e->w = (GtkWidget **)g_malloc(sizeof(GtkWidget *));
		}
		if( e->w == NULL ) {
			fprintf(stderr,nomem_msg);
			return -1;
		}
		e->w[0]=gtk_toggle_button_new();
		gtk_box_pack_start(GTK_BOX(ih_box),e->w[0],FALSE,FALSE,0);
		gtk_widget_set_usize(e->w[0],10,10);
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(e->w[0]),
									e->e.data.switch2.sw);
		gtk_signal_connect(GTK_OBJECT(e->w[0]),"toggled",
						   GTK_SIGNAL_FUNC(switch2_callback),
						   (gpointer)e);
		gtk_widget_show(e->w[0]);
		break;
	case SND_MIXER_ETYPE_ACCU3:
		gtk_box_pack_start(iv_box,ih_box,FALSE,FALSE,0);
		if( e->e.data.accu3.voices > 1 ) {
			*e->chain_en=TRUE;
			*e->chain=TRUE;
		}
		if( e->w == NULL ) {
			e->w = (GtkWidget **)g_malloc(e->e.data.accu3.voices *
										  sizeof(GtkWidget *));
		}
		if( e->w == NULL ) {
			fprintf(stderr,nomem_msg);
			return -1;
		}
		if( e->adj == NULL ) {
			e->adj=(GtkAdjustment **)g_malloc(e->e.data.accu3.voices*
											  sizeof(GtkAdjustment *));
		}
		if( e->adj==NULL ) {
			printf(nomem_msg);
			return -1;
		}
		for( i=0 ; i<e->e.data.accu3.voices ; i++ ) {
			e->adj[i]=(GtkAdjustment *)gtk_adjustment_new(
								-(gfloat)e->e.data.accu3.pvoices[i],
								-(gfloat)e->info.data.accu3.prange[i].max-0.5,
								-(gfloat)e->info.data.accu3.prange[i].min+0.5,
								1.0,1.0,1.0);
			gtk_signal_connect(GTK_OBJECT(e->adj[i]),"value_changed",
							   GTK_SIGNAL_FUNC(accu3_callback),(gpointer)e);
			e->w[i]=gtk_vscale_new(GTK_ADJUSTMENT(e->adj[i]));
			gtk_scale_set_draw_value(GTK_SCALE(e->w[i]),FALSE);
			gtk_box_pack_start(GTK_BOX(ih_box),e->w[i],FALSE,FALSE,4);
			gtk_widget_show(e->w[i]);
		}
		break;
	case SND_MIXER_ETYPE_MUX1:
		if( e->e.data.mux1.output > 1 ) {
			*e->chain_en=TRUE;
			*e->chain=TRUE;
		}
		if( e->w == NULL ) {
			e->w = (GtkWidget **)g_malloc(e->e.data.mux1.output *
										  sizeof(GtkWidget *));
		}
		if( e->w == NULL ) {
			fprintf(stderr,nomem_msg);
			return -1;
		}
		for( i=0 ; i<e->e.data.mux1.output ; i++ ) {
			e->w[i]=gtk_option_menu_new();
			menu=gtk_menu_new();
			k=0;
			for( j=0 ; j<e->mux_n; j++ ) {
				if( strcmp(e->mux[j].name,e->e.data.mux1.poutput[i].name)==0 &&
					e->mux[j].index == e->e.data.mux1.poutput[i].index &&
					e->mux[j].type == e->e.data.mux1.poutput[i].type ) k=j;
				item=gtk_menu_item_new_with_label(e->mux[j].name);
				gtk_object_set_data(GTK_OBJECT(item),"ch",(gpointer)i);
				gtk_object_set_data(GTK_OBJECT(item),"no",(gpointer)j);
				gtk_signal_connect(GTK_OBJECT(item),"activate",
								   GTK_SIGNAL_FUNC(mux1_callback),(gpointer)e);
				gtk_menu_append(GTK_MENU(menu),item);
				gtk_widget_show(item);
			}
			gtk_option_menu_set_menu(GTK_OPTION_MENU(e->w[i]),menu);
			gtk_box_pack_start(iv_box,e->w[i],FALSE,FALSE,4);
			gtk_widget_show(e->w[i]);
			gtk_option_menu_set_history(GTK_OPTION_MENU(e->w[i]),k);
		}
		break;
	case SND_MIXER_ETYPE_MUX2:
		if( e->w == NULL ) {
			e->w = (GtkWidget **)g_malloc(sizeof(GtkWidget *));
		}
		if( e->w == NULL ) {
			fprintf(stderr,nomem_msg);
			return -1;
		}
		e->w[0]=gtk_option_menu_new();
		menu=gtk_menu_new();
		k=0;
		for( j=0 ; j<e->mux_n; j++ ) {
			if( strcmp(e->mux[j].name,e->e.data.mux2.output.name)==0 &&
				e->mux[j].index == e->e.data.mux2.output.index &&
				e->mux[j].type == e->e.data.mux2.output.type ) k=j;
			item=gtk_menu_item_new_with_label(e->mux[j].name);
			gtk_object_set_data(GTK_OBJECT(item),"no",(gpointer)k);
			gtk_signal_connect(GTK_OBJECT(item),"activate",
							   GTK_SIGNAL_FUNC(mux2_callback),(gpointer)e);
			gtk_menu_append(GTK_MENU(menu),item);
			gtk_widget_show(item);
		}
		gtk_option_menu_set_menu(GTK_OPTION_MENU(e->w[0]),menu);
		gtk_box_pack_start(iv_box,e->w[0],FALSE,FALSE,4);
		gtk_widget_show(e->w[0]);
		gtk_option_menu_set_history(GTK_OPTION_MENU(e->w[0]),k);
		break;
	case SND_MIXER_ETYPE_3D_EFFECT1:
		if( e->w == NULL ) {
			e->w = (GtkWidget **)g_malloc(10*sizeof(GtkWidget *));
		}
		if( e->w == NULL ) {
			fprintf(stderr,nomem_msg);
			return -1;
		}
		if( e->adj == NULL ) {
			e->adj=(GtkAdjustment **)g_malloc(8*sizeof(GtkAdjustment *));
		}
		if( e->adj==NULL ) {
			printf(nomem_msg);
			return -1;
		}
		if( e->info.data.teffect1.effect & SND_MIXER_EFF1_SW ) {
			ih_box=gtk_hbox_new(FALSE,2);
			gtk_box_pack_start(iv_box,ih_box,FALSE,FALSE,0);
			e->w[0]=gtk_toggle_button_new();
			gtk_box_pack_start(GTK_BOX(ih_box),e->w[0],FALSE,FALSE,4);
			gtk_widget_set_usize(e->w[0],10,10);
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(e->w[0])
										,e->e.data.teffect1.sw);
			gtk_widget_show(e->w[0]);
			gtk_signal_connect(GTK_OBJECT(e->w[0]),"toggled",
							   GTK_SIGNAL_FUNC(sw_3d_callback),(gpointer)e);
			c_l=gtk_label_new(_("Enable"));
			gtk_box_pack_start(GTK_BOX(ih_box),c_l,FALSE,FALSE,0);
			gtk_widget_show(c_l);
			gtk_widget_show(ih_box);
		} else {
			e->w[0]=NULL;
		}
		if( e->info.data.teffect1.effect & SND_MIXER_EFF1_MONO_SW ) {
			ih_box=gtk_hbox_new(FALSE,2);
			gtk_box_pack_start(iv_box,ih_box,FALSE,FALSE,0);
			e->w[1]=gtk_toggle_button_new();
			gtk_box_pack_start(GTK_BOX(ih_box),e->w[1],FALSE,FALSE,4);
			gtk_widget_set_usize(e->w[1],10,10);
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(e->w[1])
										,e->e.data.teffect1.mono_sw);
			gtk_widget_show(e->w[1]);
			gtk_signal_connect(GTK_OBJECT(e->w[1]),"toggled",
							   GTK_SIGNAL_FUNC(sw_3d_callback),(gpointer)e);
			c_l=gtk_label_new(_("MONO"));
			gtk_box_pack_start(GTK_BOX(ih_box),c_l,FALSE,FALSE,0);
			gtk_widget_show(c_l);
			gtk_widget_show(ih_box);
		} else {
			e->w[1]=NULL;
		}
		MIX_3D_VOL(0,wide,min_wide,max_wide,SND_MIXER_EFF1_WIDE);
		MIX_3D_VOL(1,volume,min_volume,max_volume,SND_MIXER_EFF1_VOLUME);
		MIX_3D_VOL(2,center,min_center,max_center,SND_MIXER_EFF1_CENTER);
		MIX_3D_VOL(3,space,min_space,max_space,SND_MIXER_EFF1_SPACE);
		MIX_3D_VOL(4,depth,min_depth,max_depth,SND_MIXER_EFF1_DEPTH);
		MIX_3D_VOL(5,delay,min_delay,max_delay,SND_MIXER_EFF1_DELAY);
		MIX_3D_VOL(6,feedback,min_feedback,max_feedback,SND_MIXER_EFF1_FEEDBACK);
		MIX_3D_VOL(7,depth_rear,min_depth_rear,max_depth_rear,SND_MIXER_EFF1_DEPTH_REAR);
		break;
	case SND_MIXER_ETYPE_TONE_CONTROL1:
		if( e->w == NULL ) {
			e->w = (GtkWidget **)g_malloc(3*sizeof(GtkWidget *));
		}
		if( e->w == NULL ) {
			fprintf(stderr,nomem_msg);
			return -1;
		}
		if( e->adj == NULL ) {
			e->adj=(GtkAdjustment **)g_malloc(2*sizeof(GtkAdjustment *));
		}
		if( e->adj==NULL ) {
			printf(nomem_msg);
			return -1;
		}
		e->e.data.tc1.tc=e->info.data.tc1.tc;
		snd_mixer_element_read(cards[e->card].mixer[e->mdev].handle,&e->e);
		if( e->info.data.tc1.tc &
			(SND_MIXER_TC1_BASS | SND_MIXER_TC1_TREBLE ) ) {
			gtk_box_pack_start(iv_box,ih_box,TRUE,TRUE,0);
			MIX_TONE_VOL(0,bass,min_bass,max_bass,SND_MIXER_TC1_BASS);
			MIX_TONE_VOL(1,treble,min_treble,max_treble,SND_MIXER_TC1_TREBLE);
		}
		if( e->info.data.tc1.tc & SND_MIXER_TC1_SW ) {
			if( e->info.data.tc1.tc &
				(SND_MIXER_TC1_BASS | SND_MIXER_TC1_TREBLE ) )
				ih_box=gtk_hbox_new(FALSE,2);
			gtk_box_pack_start(iv_box,ih_box,FALSE,FALSE,0);
			e->w[0]=gtk_toggle_button_new();
			gtk_box_pack_start(GTK_BOX(ih_box),e->w[0],FALSE,FALSE,4);
			gtk_widget_set_usize(e->w[0],10,10);
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(e->w[0])
										,e->e.data.tc1.sw);
			gtk_widget_show(e->w[0]);
			gtk_signal_connect(GTK_OBJECT(e->w[0]),"toggled",
							   GTK_SIGNAL_FUNC(sw_tone_callback),(gpointer)e);
			c_l=gtk_label_new(_("Enable"));
			gtk_box_pack_start(GTK_BOX(ih_box),c_l,FALSE,FALSE,0);
			gtk_widget_show(c_l);
			gtk_widget_show(ih_box);
		} else {
			e->w[0]=NULL;
		}
	}
	gtk_widget_show(ih_box);
	return 0;
}
