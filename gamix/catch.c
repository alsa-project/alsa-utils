
#include "gamix.h"

typedef struct e_q_cell e_q_cell_t;
struct e_q_cell {
	snd_mixer_eid_t eid;
	e_q_cell_t *next;
};
typedef struct g_q_cell g_q_cell_t;
struct g_q_cell {
	snd_mixer_gid_t gid;
	g_q_cell_t *next;
};

static snd_mixer_callbacks_t cb_mix;
static char *cmd_name[]={"rebuild","element value","element change",
						 "element route","element add","element remove",
						 "group value","group change","group add",
						 "group remove"
};
static struct {
	e_q_cell_t *q;
	int q_n;
} e_q;
static struct {
	g_q_cell_t *q;
	int q_n;
} g_q;

static void cb_rb(void *);
static void element_callback(void *,int,snd_mixer_eid_t *);
static void cb_gp(void *,int,snd_mixer_gid_t *);
static void search_obj_elem(s_obj_t **,s_element_t **,snd_mixer_eid_t *);
static void s_e_chk(s_element_t *);
static void rmw_elem(s_element_t *);
static int que_e( snd_mixer_eid_t * );
static int que_g( snd_mixer_gid_t * );
static int chk_group( s_mixer_t * );
static void element_free(s_element_t *);
static gboolean chk_eid(s_obj_t *,snd_mixer_eid_t * );

extern GtkWidget *main_vbox;
extern GtkWidget *mixer_container;

static void cb_rb(void *pd ) {
	printf("cb rb hoe\n");
}

static void element_callback(void *pd,int cmd,snd_mixer_eid_t *eid) {
	int i,j;
	gint ccc;
	s_group_t *group;
	s_element_t *e;
	s_eelements_t *ee;
	s_mixer_t *mixer=(s_mixer_t *)pd;
	s_obj_t *obj=mixer->obj;

	if( !is_etype(eid->type) ) return;

	search_obj_elem(&obj,&e,eid);
	switch( cmd ) {
	case SND_MIXER_READ_ELEMENT_VALUE:
		if( obj == NULL || e == NULL ) break;
		snd_mixer_element_read(mixer->handle,&e->e);
		if( obj->enabled ) {
			ccc=obj->chain;
			obj->chain=FALSE;
			s_e_chk(e);
			obj->chain=ccc;
		}
		return;
		break;
	case SND_MIXER_READ_ELEMENT_REMOVE:
		if( obj == NULL || e == NULL ) break;
		if( obj->enabled ) rmw_elem(e);
		return;
		break;
	case SND_MIXER_READ_ELEMENT_ADD:
		if( obj && e ) {
			return;
		} else {
			if( que_e(eid) == 0 ) return;
		}
		break;
	case SND_MIXER_READ_ELEMENT_CHANGE:
	case SND_MIXER_READ_ELEMENT_ROUTE:
	default:
		printf("eb el cmd %s eid '%s',%d,%d\n",cmd_name[cmd],eid->name,eid->index,eid->type);
		return;
	}

	printf("cb_el cmd %s %s %d %d\n",cmd_name[cmd],eid->name,eid->index,
		   eid->type);
}

static void search_obj_elem( s_obj_t **objs,s_element_t **e_r,
							 snd_mixer_eid_t *eid) {
	s_element_t *e;
	s_eelements_t *ee;
	s_group_t *group;
	s_obj_t *obj;
	int j;

	for( obj=*objs ; obj != NULL ; obj=obj->next ) {
		if( obj->e ) {
			ee=obj->e;
			if( strcmp(ee->e.e.eid.name,eid->name)==0 &&
				ee->e.e.eid.index==eid->index ) {
				*objs=obj;
				*e_r=&ee->e;
				return;
			}
		}
		if( obj->g ) {
			group=obj->g;
			for( j=0 ; j<group->g.elements ; j++ ) {
				e=&group->e[j];
				if( strcmp(e->e.eid.name,eid->name) == 0 &&
					e->e.eid.index == eid->index && e->e.eid.type != 0 ) {
					*objs=obj;
					*e_r=e;
					return;
				}
			}
		}
	}
	*objs=NULL;
	*e_r=NULL;
}

static int que_e(snd_mixer_eid_t *eid ) {
	e_q_cell_t *p;

	if( e_q.q == NULL ) {
		e_q.q=(e_q_cell_t *)malloc(sizeof(e_q_cell_t));
		if( e_q.q == NULL ) {
			fprintf(stderr,nomem_msg);
			return -1;
		}
		p=e_q.q;
		p->eid=*eid;
		p->next=NULL;
		e_q.q_n=1;
		return 0;
	}
	p=e_q.q;
	while( p->next != NULL ) {
		if( strcmp(p->eid.name,eid->name) == 0 && p->eid.index == eid->index )
			return 0;
		p=p->next;
	}
	if( strcmp(p->eid.name,eid->name) == 0 && p->eid.index == eid->index )
		return 0;
	p->next=(e_q_cell_t *)malloc(sizeof(e_q_cell_t));
	if( p->next==NULL ) {
		fprintf(stderr,nomem_msg);
		return -1;
	}
	p=p->next;
	p->eid=*eid;
	p->next=NULL;
	e_q.q_n++;
	return 0;
}

static void cb_gp(void *pd,int cmd,snd_mixer_gid_t *gid) {
	s_mixer_t *mixer=(s_mixer_t *)pd;
	s_obj_t *obj=mixer->obj,*o1;
	s_group_t *g;
	int i;

	for( ; obj != NULL ; obj=obj->next ) {
		if( obj->g ) {
			g=obj->g;
			if( strcmp(g->g.gid.name,gid->name) == 0 &&
				g->g.gid.index == gid->index ) {
				break;
			}
		}
	}

	switch(cmd) {
	case SND_MIXER_READ_GROUP_REMOVE:
		if( obj ) {
			if( obj->enabled ) {
				gtk_widget_destroy(obj->v_frame);
			}
			obj->dyn_e=2;
			obj->enabled=FALSE;
			return;
		}
		break;
	case SND_MIXER_READ_GROUP_CHANGE:
		if( que_g(gid)== 0 ) return;
		break;
	case SND_MIXER_READ_GROUP_ADD:
		if( obj ) {
			obj->dyn_e=3;
			return;
		}
		o1=NULL;
		obj_ins_new_g(&mixer->obj,&o1,gid);
		mixer->o_nums++;
		o1->dyn_e=3;
		break;
	default:
	}
	printf("cb_gp cmd %s gid '%s',%d\n",cmd_name[cmd],gid->name,gid->index);
}

static int que_g(snd_mixer_gid_t *gid ) {
	g_q_cell_t *p;

	if( g_q.q == NULL ) {
		g_q.q=(g_q_cell_t *)malloc(sizeof(g_q_cell_t));
		if( g_q.q == NULL ) {
			fprintf(stderr,nomem_msg);
			return -1;
		}
		p=g_q.q;
		p->gid=*gid;
		p->next=NULL;
		g_q.q_n=1;
		return 0;
	}
	p=g_q.q;
	while( p->next != NULL ) {
		if( strcmp(p->gid.name,gid->name) == 0 && p->gid.index == gid->index )
			return 0;
		p=p->next;
	}
	
	if( strcmp(p->gid.name,gid->name) == 0 && p->gid.index == gid->index )
		return 0;
	p->next=(g_q_cell_t *)malloc(sizeof(g_q_cell_t));
	if( p->next==NULL ) {
		fprintf(stderr,nomem_msg);
		return -1;
	}
	p=p->next;
	p->gid=*gid;
	p->next=NULL;
	g_q.q_n++;
	return 0;
}

void tc_init( void ) {
	cb_mix.rebuild=*cb_rb;
	cb_mix.element=*element_callback;
	cb_mix.group=*cb_gp;
}
gint time_callback(gpointer data) {
	GtkRequisition rq;
	int i,j,k,err;
	e_q_cell_t *eq;
	g_q_cell_t *gq;

	k=0;
	for( i=0 ; i<card_num ; i++ ) {
		for( j=0 ; j<cards[i].info.mixerdevs ; j++ ) {
			cb_mix.private_data=(void *)&cards[i].mixer[j];
			e_q.q=NULL;
			e_q.q_n=0;
			g_q.q=NULL;
			g_q.q_n=0;
			err=snd_mixer_read(cards[i].mixer[j].handle,&cb_mix);
			//if( err ) printf("count %d\n",err);
			if( g_q.q_n ) k+=chk_group( &cards[i].mixer[j] );
			for( eq=e_q.q ; eq != NULL ; eq=eq->next )
				printf("que eid '%s',%d,%d\n",eq->eid.name,eq->eid.index,eq->eid.type);
			/*
			for( gq=g_q.q ; gq != NULL ; gq=gq->next )
				printf("que gid '%s',%d\n",gq->gid.name,gq->gid.index);
			*/
		}
	}
	if( k ) {
		gtk_container_remove(GTK_CONTAINER(main_vbox),mixer_container);
		disp_mixer();
	}

	return 1;
}

static int chk_group( s_mixer_t *mixer ) {
	s_obj_t *obj;
	s_group_t *group;
	s_element_t *e;
	g_q_cell_t *gq,*gq2;
	snd_mixer_group_t m_g;
	int i,j,k,l,err,rt=0;

	gq=g_q.q;
	while( gq ) {
		for( obj=mixer->obj ; obj != NULL ; obj=obj->next ) {
			if( obj->g ) {
				if( strcmp( obj->g->g.gid.name , gq->gid.name ) == 0 &&
					obj->g->g.gid.index == gq->gid.index ) break;
			}
		}
		if( obj ) {
			group=obj->g;
			bzero(&m_g,sizeof(snd_mixer_group_t));
			m_g.gid=gq->gid;
			if( (err=snd_mixer_group_read(mixer->handle,&m_g))<0 ) {
				fprintf(stderr,_("Mixer group '%s',%d err 1: %s\n"),
						m_g.gid.name,m_g.gid.index,snd_strerror(err));
				goto __next;
			}
			m_g.pelements = (snd_mixer_eid_t *)malloc(m_g.elements_over*
													sizeof(snd_mixer_eid_t));
			if( m_g.pelements == NULL ) {
				fprintf(stderr,nomem_msg);
				goto __next;
			}
			m_g.elements_size=m_g.elements_over;
			m_g.elements=m_g.elements_over=0;
			if( (err=snd_mixer_group_read(mixer->handle,&m_g))<0 ) {
				fprintf(stderr,_("Mixer group '%s',%d err 2: %s\n"),
						m_g.gid.name,m_g.gid.index,snd_strerror(err));
				free(m_g.pelements);
				goto __next;
			}
			j=0;
			if( group->g.elements == 0 ) {
				j=1;
			} else if( group->g.elements != m_g.elements ) {
				j=1;
			} else {
				k=0;
				for( i=0 ; i<m_g.elements ; i++ ) {
					for( l=k ; l<m_g.elements ; l++ ) {
						if( strcmp(m_g.pelements[i].name,
								   group->g.pelements[l].name)==0 &&
							m_g.pelements[i].index ==
							group->g.pelements[l].index ) {
							if( l=k ) k++;
							break;
						}
					}
					if( l==m_g.elements ) {
						j=1;
						break;
					}
				}
			}
			if( j ) {
				for( i=0 ; i<group->g.elements ; i++ ) {
					element_free(&group->e[i]);
				}
				if( group->g.pelements ) free(group->g.pelements);
				if( group->e ) free(group->e);
				group->g=m_g;
				group->e=(s_element_t *)g_malloc0(group->g.elements*
												  sizeof(s_element_t));
				if( group->e == NULL ) {
					fprintf(stderr,nomem_msg);
					goto __next;
				}
				for( i=0 ; i<group->g.elements ; i++ ) {
					if( chk_eid(mixer->obj,&m_g.pelements[i]) ) {
						/*
						printf("%d: build '%s',%d,%d\n",i,
							   m_g.pelements[i].name,m_g.pelements[i].index,
							   m_g.pelements[i].type);
						*/
						s_element_build(mixer->handle,&group->e[i],NULL,
										group->g.pelements[i],mixer->c_dev,
										mixer->m_dev);
					} else {
						group->g.pelements[i].type=0;
						group->e[i].e.eid=group->g.pelements[i];
						group->e[i].info.eid=group->g.pelements[i];
					}
				}
			} else {
				free(m_g.pelements);
			}
			if( obj->enable ) rt=1;
		} else {
			fprintf(stderr,_("not added gid before change.\n"));
		}
	__next:
		gq2 = gq->next;
		free(gq);
		gq=gq2;
	}
	return rt;
}

static void element_free(s_element_t *e) {
	if( e->w ) g_free(e->w);
	if( e->adj ) g_free(e->adj);
	if( e->mux ) free(e->mux);
	snd_mixer_element_free(&e->e);
	snd_mixer_element_info_free(&e->info);
}

static gboolean chk_eid(s_obj_t *obj,snd_mixer_eid_t *eid ) {
	e_q_cell_t *eq,*ep;
	s_element_t *e;
	int i;

	if( !is_etype(eid->type) ) return FALSE;
	for( eq=e_q.q; eq != NULL ; eq=eq->next ) {
		if( strcmp(eq->eid.name,eid->name) == 0 &&
			eq->eid.index == eid->index && eq->eid.type == eq->eid.type ) {
			if( eq == e_q.q ) {
				e_q.q=e_q.q->next;
			} else {
				for(ep=e_q.q;ep->next !=eq ; ep=ep->next );
				ep->next=eq->next;
			}
			free(eq);
			return TRUE;
		}
	}
	for( ; obj != NULL ; obj=obj->next ) {
		if(obj->g) {
			for( i=0 ; obj->g->g.elements ; i++ ) {
				e=&obj->g->e[i];
				if( strcmp( e->e.eid.name,eid->name ) == 0 &&
					e->e.eid.index == eid->index &&
					e->e.eid.type == eid->type ) return FALSE;
			}
		}
		if(obj->e) {
			e=&obj->e->e;
			if( strcmp( e->e.eid.name,eid->name ) == 0 &&
				e->e.eid.index == eid->index &&
				e->e.eid.type == eid->type ) return FALSE;
			
		}
	}
	return TRUE;
}

void s_e_chk( s_element_t *e ) {
	int i,j;
	switch( e->e.eid.type ) {
	case SND_MIXER_ETYPE_VOLUME1:
		for( i=0 ; i<e->e.data.volume1.voices; i++ ) {
			if( (e->info.data.volume1.prange[i].max-
				 e->info.data.volume1.prange[i].min) == 1 ) {
				gtk_toggle_button_set_active(
											 GTK_TOGGLE_BUTTON(e->w[i]),
											 e->e.data.volume1.pvoices[i]);
			} else {
				e->adj[i]->value=(gfloat)
					-e->e.data.volume1.pvoices[i];
				gtk_signal_emit_by_name(GTK_OBJECT(e->adj[i]),"value_changed");
			}
		}
		break;
	case SND_MIXER_ETYPE_SWITCH1:
		for( i=0 ; i<e->e.data.switch1.sw; i++) {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(e->w[i]),
									 snd_mixer_get_bit(e->e.data.switch1.psw,i)
										 );
		}
		break;
	case SND_MIXER_ETYPE_SWITCH2:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(e->w[0]),
									 e->e.data.switch2.sw);
		break;
	case SND_MIXER_ETYPE_ACCU3:
		for( i=0 ; i<e->e.data.accu3.voices ; i++ ) {
			e->adj[i]->value=(gfloat)
				-e->e.data.volume1.pvoices[i];
			gtk_signal_emit_by_name(GTK_OBJECT(e->adj[i]),"value_changed");
		}
		break;
	case SND_MIXER_ETYPE_MUX1:
		for( i=0 ; i<e->e.data.mux1.output ; i++ ) {
			for( j=0; j<e->mux_n ; j++ ) {
				if( strcmp(e->mux[j].name,e->e.data.mux1.poutput[i].name)==0 &&
					e->mux[j].index == e->e.data.mux1.poutput[i].index &&
					e->mux[j].type == e->e.data.mux1.poutput[i].type ) break;
			}
			if( j < e->mux_n )
				gtk_option_menu_set_history(GTK_OPTION_MENU(e->w[i]),j);
		}
		break;
	case SND_MIXER_ETYPE_MUX2:
		for( i=0; i<e->mux_n ; i++ ) {
			if( strcmp(e->mux[i].name,e->e.data.mux2.output.name)==0 &&
				e->mux[i].index == e->e.data.mux2.output.index &&
				e->mux[i].type == e->e.data.mux2.output.type )
				break;
		}
		if( i < e->mux_n )
			gtk_option_menu_set_history(GTK_OPTION_MENU(e->w[0]),i);
		break;
	case SND_MIXER_ETYPE_3D_EFFECT1:
		if( e->info.data.teffect1.effect & SND_MIXER_EFF1_SW ) {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(e->w[0]),
										 e->e.data.teffect1.sw);
		}
		if( e->info.data.teffect1.effect & SND_MIXER_EFF1_MONO_SW ) {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(e->w[1]),
										 e->e.data.teffect1.sw);
		}
		if( e->info.data.teffect1.effect & SND_MIXER_EFF1_WIDE ) {
			e->adj[0]->value=(gfloat)e->e.data.teffect1.wide;
			gtk_signal_emit_by_name(GTK_OBJECT(e->adj[0]),"value_changed");
		}
		if( e->info.data.teffect1.effect & SND_MIXER_EFF1_VOLUME ) {
			e->adj[1]->value=(gfloat)e->e.data.teffect1.volume;
			gtk_signal_emit_by_name(GTK_OBJECT(e->adj[1]),"value_changed");
		}
		if( e->info.data.teffect1.effect & SND_MIXER_EFF1_CENTER ) {
			e->adj[2]->value=(gfloat)e->e.data.teffect1.center;
			gtk_signal_emit_by_name(GTK_OBJECT(e->adj[2]),
									"value_changed");
		}
		if( e->info.data.teffect1.effect & SND_MIXER_EFF1_SPACE ) {
			e->adj[3]->value=(gfloat)e->e.data.teffect1.space;
			gtk_signal_emit_by_name(GTK_OBJECT(e->adj[3]),"value_changed");
		}
		if( e->info.data.teffect1.effect & SND_MIXER_EFF1_DEPTH ) {
			e->adj[4]->value=(gfloat)e->e.data.teffect1.depth;
			gtk_signal_emit_by_name(GTK_OBJECT(e->adj[4]),"value_changed");
		}
		if( e->info.data.teffect1.effect & SND_MIXER_EFF1_DELAY ) {
			e->adj[5]->value=(gfloat)e->e.data.teffect1.delay;
			gtk_signal_emit_by_name(GTK_OBJECT(e->adj[5]),"value_changed");
		}
		if( e->info.data.teffect1.effect & SND_MIXER_EFF1_FEEDBACK ) {
			e->adj[6]->value=(gfloat)e->e.data.teffect1.feedback;
			gtk_signal_emit_by_name(GTK_OBJECT(e->adj[6]),"value_changed");
		}
		if( e->info.data.teffect1.effect & SND_MIXER_EFF1_DEPTH_REAR ) {
			e->adj[7]->value=(gfloat)e->e.data.teffect1.depth_rear;
			gtk_signal_emit_by_name(GTK_OBJECT(e->adj[7]),"value_changed");
		}
		break;
	case SND_MIXER_ETYPE_TONE_CONTROL1:
		if( e->info.data.tc1.tc & SND_MIXER_TC1_SW ) {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(e->w[0]),
										 e->e.data.tc1.sw);
		}
		if( e->info.data.tc1.tc & SND_MIXER_TC1_BASS ) {
			e->adj[0]->value=-(gfloat)e->e.data.tc1.bass;
			gtk_signal_emit_by_name(GTK_OBJECT(e->adj[0]),"value_changed");
		}
		if( e->info.data.tc1.tc & SND_MIXER_TC1_TREBLE ) {
			e->adj[1]->value=-(gfloat)e->e.data.tc1.treble;
			gtk_signal_emit_by_name(GTK_OBJECT(e->adj[1]),"value_changed");
		}
		break;
	case SND_MIXER_ETYPE_PAN_CONTROL1:
		printf("catch pan ");
		for( i=0 ; i<e->e.data.pc1.pan ; i++ ) {
			printf(" %d",e->e.data.pc1.ppan[i]);
			e->adj[i]->value=(gfloat)e->e.data.pc1.ppan[i];
			gtk_signal_emit_by_name(GTK_OBJECT(e->adj[i]),"value_changed");
		}
		printf("\n");
		break;
	}
}

static void rmw_elem(s_element_t *e) {
	int i,j;
	switch( e->e.eid.type ) {
	case SND_MIXER_ETYPE_VOLUME1:
		for( i=0 ; i<e->e.data.volume1.voices; i++ ) {
			if( (e->info.data.volume1.prange[i].max-
				 e->info.data.volume1.prange[i].min) == 1 ) {
				gtk_widget_destroy( e->w[i] );
			} else {
				//gtk_widget_destroy(e->adj[i]);
				gtk_widget_destroy(e->w[i]);
			}
		}
		break;
	case SND_MIXER_ETYPE_SWITCH1:
		for( i=0 ; i<e->e.data.switch1.sw; i++) {
			gtk_widget_destroy(e->w[i]);
		}
		break;
	case SND_MIXER_ETYPE_SWITCH2:
		gtk_widget_destroy(e->w[0]);
		break;
	case SND_MIXER_ETYPE_ACCU3:
		for( i=0 ; i<e->e.data.accu3.voices ; i++ ) {
			gtk_widget_destroy(e->w[i]);
		}
		break;
	case SND_MIXER_ETYPE_MUX1:
		for( i=0 ; i<e->e.data.mux1.output ; i++ ) {
			gtk_widget_destroy(e->w[i]);
		}
		break;
	case SND_MIXER_ETYPE_MUX2:
		gtk_widget_destroy(e->w[0]);
		break;
	case SND_MIXER_ETYPE_3D_EFFECT1:
		if( e->info.data.teffect1.effect & SND_MIXER_EFF1_SW ) {
			gtk_widget_destroy(e->w[0]);
		}
		if( e->info.data.teffect1.effect & SND_MIXER_EFF1_MONO_SW ) {
			gtk_widget_destroy(e->w[1]);
		}
		if( e->info.data.teffect1.effect & SND_MIXER_EFF1_WIDE ) {
			gtk_widget_destroy(e->w[2]);
		}
		if( e->info.data.teffect1.effect & SND_MIXER_EFF1_VOLUME ) {
			gtk_widget_destroy(e->w[3]);
		}
		if( e->info.data.teffect1.effect & SND_MIXER_EFF1_CENTER ) {
			gtk_widget_destroy(e->w[4]);
		}
		if( e->info.data.teffect1.effect & SND_MIXER_EFF1_SPACE ) {
			gtk_widget_destroy(e->w[5]);
		}
		if( e->info.data.teffect1.effect & SND_MIXER_EFF1_DEPTH ) {
			gtk_widget_destroy(e->w[6]);
		}
		if( e->info.data.teffect1.effect & SND_MIXER_EFF1_DELAY ) {
			gtk_widget_destroy(e->w[7]);
		}
		if( e->info.data.teffect1.effect & SND_MIXER_EFF1_FEEDBACK ) {
			gtk_widget_destroy(e->w[8]);
		}
		if( e->info.data.teffect1.effect & SND_MIXER_EFF1_DEPTH_REAR ) {
			gtk_widget_destroy(e->w[9]);
		}
		break;
	case SND_MIXER_ETYPE_TONE_CONTROL1:
		if( e->info.data.tc1.tc & SND_MIXER_TC1_SW ) {
			gtk_widget_destroy(e->w[0]);
		}
		if( e->info.data.tc1.tc & SND_MIXER_TC1_BASS ) {
			gtk_widget_destroy(e->w[1]);
		}
		if( e->info.data.tc1.tc & SND_MIXER_TC1_TREBLE ) {
			gtk_widget_destroy(e->w[2]);
		}
		break;
	case SND_MIXER_ETYPE_PAN_CONTROL1:
		j=0;
		for( i=0 ; i<e->e.data.pc1.pan ; i++ ) {
			gtk_widget_destroy(e->w[j++]);
			gtk_widget_destroy(e->w[j++]);
			gtk_widget_destroy(e->w[j++]);
			gtk_widget_destroy(e->w[j++]);
		}
		break;
	}
}
