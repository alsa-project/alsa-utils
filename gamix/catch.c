
#include "gamix.h"

static snd_mixer_callbacks_t cb_mix;

static void cb_rb(void *);
static void element_callback(void *,int,snd_mixer_eid_t *);
static void cb_gp(void *,int,snd_mixer_gid_t *);
static void s_e_chk(s_element *);

static void cb_rb(void *pd ) {
	printf("cb rb hoe\n");
}

static void element_callback(void *pd,int cmd,snd_mixer_eid_t *eid) {
	int i,j;
	gint ccc;
	s_group *group;
	s_element *e;
	s_eelements *ee;
	s_mixer *mixer=(s_mixer *)pd;


	if( cmd != SND_MIXER_READ_ELEMENT_VALUE ) return;
	/*
	if( eid->type != SND_MIXER_ETYPE_VOLUME1 ||
		eid->type != SND_MIXER_ETYPE_SWITCH1 ||
		eid->type != SND_MIXER_ETYPE_SWITCH1 ) {
		return;
	}
	*/
	//printf("hoe '%s',%d,%d\n",eid->name,eid->index,eid->type);
	for( i=0 ; i < mixer->ee_n ; i++ ) {
		ee=&mixer->ee[i];
		if( strcmp(ee->e.e.eid.name,eid->name)==0 &&
			ee->e.e.eid.index==eid->index ) {
			snd_mixer_element_read(mixer->handle,&ee->e.e);
			if( ee->enabled ) {
				ccc=ee->chain;
				ee->chain=FALSE;
				s_e_chk(&ee->e);
				ee->chain=ccc;
			}
			return;
		}
	}
	for( i=0 ; i<mixer->groups.groups ; i++ ) {
		group=&mixer->group[i];
		for( j=0 ; j<group->g.elements ; j++ ) {
			e=&group->e[j];
			if( strcmp(e->e.eid.name,eid->name) == 0 &&
				e->e.eid.index == eid->index) {
				snd_mixer_element_read(mixer->handle,&e->e);
				if( group->enabled ) {
					ccc=group->chain;
					s_e_chk(e);
					group->chain=FALSE;
					group->chain=ccc;
				}
				return;
			}
		}
	}


	printf("elem hoe %d %s %d %d\n",cmd,eid->name,eid->index,eid->type);
}
static void cb_gp(void *pd,int cmd,snd_mixer_gid_t *gid) {
	printf("cb_gp hoe\n");
}

void tc_init( void ) {
	cb_mix.rebuild=*cb_rb;
	cb_mix.element=*element_callback;
	cb_mix.group=*cb_gp;
}
gint time_callback(gpointer data) {
	int i,j,err;

	for( i=0 ; i<card_num ; i++ ) {
		for( j=0 ; j<cards[i].info.mixerdevs ; j++ ) {
			cb_mix.private_data=(void *)&cards[i].mixer[j];
			err=snd_mixer_read(cards[i].mixer[j].handle,&cb_mix);
		}
	}
	return 1;
}

void s_e_chk( s_element *e ) {
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
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(e->w[0]),
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
	}
}
