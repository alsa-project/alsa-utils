
#include "gamix.h"

int card_num,mdev_num;
s_card_t *cards;

static int search_es(snd_mixer_eid_t *,snd_mixer_elements_t *);
static gint ab_chk( s_mixer_t *,snd_mixer_eid_t * );
/*
static int s_element_build(snd_mixer_t *,s_element_t *,snd_mixer_elements_t *,
						   snd_mixer_eid_t ,int , int);
*/
static gint mk_mux_lst(snd_mixer_t *,snd_mixer_elements_t *,snd_mixer_element_info_t *,snd_mixer_eid_t **);

gint probe_mixer( void ) {
	int err,i,j,k,l,m;
	snd_ctl_t *p_handle;
	snd_mixer_t *m_handle;
	snd_mixer_groups_t groups;
	snd_mixer_elements_t es;
	s_mixer_t *mixer;
	s_group_t *group;
	s_eelements_t *ee;
	s_obj_t *obj;
	int *es_nums;

	card_num=snd_cards();
	cards=(s_card_t *)g_malloc(sizeof(s_card_t)*card_num);
	if( cards == NULL ) {
		fprintf(stderr,nomem_msg);
		return -1;
	}
	mdev_num=0;
	for( i = 0 ; i<card_num ; i++ ) {
		if((err=snd_ctl_open(&p_handle,i))<0 ) {
			fprintf(stderr,_("open failed: %s\n"),snd_strerror(err));
			return -1;
		}
		err=snd_ctl_hw_info(p_handle, &cards[i].info);
		if(err<0) {
			fprintf(stderr,_("hw info failed: %s\n"),snd_strerror(err));
			snd_ctl_close(p_handle);
			return -1;
		}
		cards[i].mixer=(s_mixer_t *)g_malloc0(sizeof(s_mixer_t)*
										   cards[i].info.mixerdevs);
		if( cards[i].mixer == NULL ) {
			fprintf(stderr,nomem_msg);
			snd_ctl_close(p_handle);
			return -1;
		}
		mdev_num+=cards[i].info.mixerdevs;
		for( j=0 ; j<cards[i].info.mixerdevs ; j++) {
			mixer=&cards[i].mixer[j];
			//mixer->handle=NULL;
			//mixer->obj=NULL;
			mixer->c_dev=i;
			mixer->m_dev=j;
			mixer->o_nums=0;
			mixer->enable=TRUE;
			mixer->enabled=FALSE;
			mixer->p_e=TRUE;
			mixer->p_f=TRUE;
			if((err=snd_mixer_open(&m_handle,i,j))<0 ) {
				fprintf(stderr,_("mixer %d/%d open error: %s\n"),i,j,
						snd_strerror(err));
				snd_ctl_close(p_handle);
				return -1;
			}
			if((err=snd_ctl_mixer_info(p_handle,j,&mixer->info))<0) {
				fprintf(stderr,_("Mixer info failed: %s\n"),snd_strerror(err));
				snd_ctl_close(p_handle);
				snd_mixer_close(m_handle);
				return -1;
			}
			bzero(&groups,sizeof(snd_mixer_groups_t));
			if((err=snd_mixer_groups(m_handle,&groups))<0 ) {
				fprintf(stderr,_("Mixer %d/%d groups error: %s\n"),i,i,
						snd_strerror(err));
				snd_mixer_close(m_handle);
				snd_ctl_close(p_handle);
				return -1;
			}
			groups.pgroups = (snd_mixer_gid_t *)g_malloc(groups.groups_over
													*sizeof(snd_mixer_eid_t));
			if( groups.pgroups == NULL ) {
				fprintf(stderr,nomem_msg);
				snd_ctl_close(p_handle);
				snd_mixer_close(m_handle);
			}
			groups.groups_size =	groups.groups_over;
			groups.groups_over =	groups.groups = 0;
			if((err=snd_mixer_groups(m_handle,&groups))<0 ) {
				fprintf(stderr,_("Mixer %d/%d groups (2) error: %s\n"),
						i,j,snd_strerror(err));
				snd_ctl_close(p_handle);
				snd_mixer_close(m_handle);
				return -1;
			}
			bzero(&es, sizeof(snd_mixer_elements_t));
			if( (err=snd_mixer_elements(m_handle,&es))<0 ) {
				fprintf(stderr,_("mixer elements read failed: %s"),
						snd_strerror(err));
				snd_ctl_close(p_handle);
				snd_mixer_close(m_handle);
				return -1;
			}
			es.pelements = (snd_mixer_eid_t *)g_malloc(
								es.elements_over * sizeof(snd_mixer_eid_t));
			if( es.pelements == NULL ) {
				fprintf(stderr,nomem_msg);
				snd_ctl_close(p_handle);
				snd_mixer_close(m_handle);
				return -1;
			}
			es.elements_size = es.elements_over;
			es.elements_over = es.elements = 0;
			if( (err=snd_mixer_elements(m_handle,&es))<0 ) {
				fprintf(stderr,_("mixer elements read failed(2): %s"),
						snd_strerror(err));
				snd_ctl_close(p_handle);
				snd_mixer_close(m_handle);
				return -1;
			}
			es_nums = (int *)g_malloc(es.elements * sizeof(int));
			if( es_nums == NULL ) {
				fprintf(stderr,nomem_msg);
				snd_ctl_close(p_handle);
				snd_mixer_close(m_handle);
				return -1;
			}
			bzero(es_nums,es.elements * sizeof(int));
			//printf("Card %d mixer %d\n",i,j);
			mixer->o_nums=groups.groups;
			obj=NULL;
			for( k=0 ; k<groups.groups ; k++ ) {
				if( obj ) {
					obj->next=(s_obj_t *)malloc(sizeof(s_obj_t));
					if( obj->next == NULL ) {
						fprintf(stderr,nomem_msg);
						snd_ctl_close(p_handle);
						snd_mixer_close(m_handle);
						return -1;
					}
					obj=obj->next;
				} else {
					mixer->obj=(s_obj_t *)malloc(sizeof(s_obj_t));
					if( mixer->obj == NULL ) {
						fprintf(stderr,nomem_msg);
						snd_ctl_close(p_handle);
						snd_mixer_close(m_handle);
						return -1;
					}
					obj=mixer->obj;

				}
				bzero(obj,sizeof(s_obj_t));
				obj->enable=TRUE;
				obj->enabled=FALSE;
				obj->g=(s_group_t *)malloc(sizeof(s_group_t));
				if( obj->g == NULL ) {
					fprintf(stderr,nomem_msg);
					snd_ctl_close(p_handle);
					snd_mixer_close(m_handle);
					return -1;
				}
				group=obj->g;
				bzero(group,sizeof(s_group_t));
				group->g.gid=groups.pgroups[k];
				if((err=snd_mixer_group_read(m_handle,&group->g)) <0 ) {
					fprintf(stderr,_("Mixer %d/%d group error: %s\n"),i,j,
							snd_strerror(err));
					snd_ctl_close(p_handle);
					snd_mixer_close(m_handle);
					return -1;
				}
				group->g.pelements = (snd_mixer_eid_t *)g_malloc(
							group->g.elements_over*sizeof(snd_mixer_eid_t));
				if( group->g.pelements == NULL ) {
					snd_ctl_close(p_handle);
					fprintf(stderr,nomem_msg);
					snd_mixer_close(m_handle);
					return -1;
				}
				group->g.elements_size = group->g.elements_over;
				group->g.elements = group->g.elements_over = 0;
				if((err=snd_mixer_group_read(m_handle,&group->g)) <0 ) {
					fprintf(stderr,_("Mixer %d/%d group error (2): %s\n"),i,j,
							snd_strerror(err));
					snd_ctl_close(p_handle);
					snd_mixer_close(m_handle);
					return -1;
				}
				group->e=(s_element_t *)g_malloc(group->g.elements_size*
											   sizeof(s_element_t));
				if( group->e == NULL ) {
					fprintf(stderr,nomem_msg);
					snd_ctl_close(p_handle);
					snd_mixer_close(m_handle);
					return -1;
				}
				for( l=0 ; l<group->g.elements ; l++ ) {
					m=search_es( &group->g.pelements[l],&es );
					if( m>-1 ) {
						if( es_nums[m] ) group->g.pelements[l].type=0;
						es_nums[m]++;
					}
					err=s_element_build(m_handle,&group->e[l],&es,
									   group->g.pelements[l],i,j);
					if( err<0 ) {
						snd_ctl_close(p_handle);
						snd_mixer_close(m_handle);
						return -1;
					}
				}
			}
			for( k=0 ; k<es.elements ; k++ ) {
				if( es_nums[k] > 1 ) {
					//for( l=0 ; l<groups.groups ; l++ ) {
					l=0;
					for( obj=mixer->obj ; obj != NULL && l==0 ;
						 obj=obj->next ) {
						group=obj->g;
						for( m=0 ; m<group->g.elements; m++ ) {
							if( strcmp( es.pelements[k].name,group->g.pelements[m].name)==0 &&
								es.pelements[k].index == group->g.pelements[m].index &&
								es.pelements[k].type == group->g.pelements[m].type ) {
								group->e[m].e.eid.type=0;
								group->e[m].info.eid.type=0;
								if( group->e[m].mux ) free(group->e[m].mux);
								l=1;
								break;
							}
						}
					}
				}
			}
			/*
			  delete null object?
			*/
			l=0;
			for( k=0 ; k<es.elements ; k++ ) {
				if( es_nums[k] == 0 || es_nums[k]>1 ) {
					if( ab_chk(mixer,&es.pelements[k]) ) {
						l++;
					} else 	es_nums[k]=1;
				}
			}
			mixer->o_nums+=l;
			if( l>0 ) {
				obj=mixer->obj;
				if( obj ) while( obj->next ) obj=obj->next;
				k=0;
				while(l>0) {
					l--;
					while( es_nums[k]==1 ) k++;
					if( obj ) {
						obj->next=(s_obj_t *)g_malloc0(sizeof(s_obj_t));
						if( obj == NULL ) {
							fprintf(stderr,nomem_msg);
							snd_ctl_close(p_handle);
							snd_mixer_close(m_handle);
							return -1;
						}
						obj=obj->next;
					} else {
						obj=(s_obj_t *)g_malloc0(sizeof(s_obj_t));
						if( obj == NULL ) {
							fprintf(stderr,nomem_msg);
							snd_ctl_close(p_handle);
							snd_mixer_close(m_handle);
							return -1;
						}
						mixer->obj=obj;
					}
					bzero(obj,sizeof(s_obj_t));
					obj->e=(s_eelements_t *)malloc(sizeof(s_eelements_t));
					if( obj->e == NULL ) {
						fprintf(stderr,nomem_msg);
						snd_ctl_close(p_handle);
						snd_mixer_close(m_handle);
						return -1;
					}
					ee=obj->e;
					bzero(ee,sizeof(s_eelements_t));

					err=s_element_build(m_handle,&ee->e,&es,es.pelements[k],
										i,j);
					obj->enable=TRUE;
					obj->enabled=FALSE;
					k++;
				}
			}
			g_free(groups.pgroups);
			g_free(es.pelements);
			g_free(es_nums);
			snd_mixer_close(m_handle);
		}
		snd_ctl_close(p_handle);
	}
	return 0;
}

static int search_es(snd_mixer_eid_t *eid,snd_mixer_elements_t *es) {
	int i;
	for( i=0 ; i<es->elements ; i++ ) {
		if( strcmp( es->pelements[i].name , eid->name ) == 0 &&
			es->pelements[i].index == eid->index &&
			es->pelements[i].type == eid->type ) return i;
	}
	return -1;
}

gboolean is_etype(int etype ) {
	switch( etype ) {
	case SND_MIXER_ETYPE_SWITCH1:
	case SND_MIXER_ETYPE_SWITCH2:
	case SND_MIXER_ETYPE_SWITCH3:
	case SND_MIXER_ETYPE_MUX1:
	case SND_MIXER_ETYPE_MUX2:
	case SND_MIXER_ETYPE_ACCU3:
	case SND_MIXER_ETYPE_VOLUME1:
	case SND_MIXER_ETYPE_VOLUME2:
	case SND_MIXER_ETYPE_3D_EFFECT1:
	case SND_MIXER_ETYPE_TONE_CONTROL1:
	case SND_MIXER_ETYPE_PAN_CONTROL1:
		return TRUE;
		break;
	}
	return FALSE;
}

static gint ab_chk( s_mixer_t *mixer,snd_mixer_eid_t *eid ) {
	if( !is_etype(eid->type) ) return FALSE;
	return TRUE;
}

int s_element_build(snd_mixer_t *handle,s_element_t *e,
						   snd_mixer_elements_t *es, snd_mixer_eid_t eid,
						   int c,int m) {
	int err;
	bzero(&e->info,sizeof(snd_mixer_element_info_t));
	bzero(&e->e,sizeof(snd_mixer_element_t));
	e->e.eid = eid;
	e->info.eid = eid;
	if( eid.type != SND_MIXER_ETYPE_SWITCH1 &&
		eid.type != SND_MIXER_ETYPE_SWITCH2 &&
		eid.type > 0 ) {
		err=snd_mixer_element_info_build(handle,&e->info);
		if( err<0 ) {
			preid(eid);
			fprintf(stderr,_("Mixer element info build failed: %s\n"),
					snd_strerror(err));
		}
	}
	e->w=NULL;
	e->adj=NULL;
	e->card=c;
	e->mdev=m;
	e->mux_n=0;
	e->mux=NULL;
	if( !is_etype(e->e.eid.type) ) return 0;

	err=snd_mixer_element_build(handle,&e->e);
	if( err<0 ) {
		preid(eid);
		fprintf(stderr,_("Mixer element build failed: %s\n"),snd_strerror(err));
	}
	if( e->info.eid.type == SND_MIXER_ETYPE_MUX1 || 
		e->info.eid.type == SND_MIXER_ETYPE_MUX2 ) {
		e->mux_n=mk_mux_lst(handle,es,&e->info,&e->mux);
		if( e->mux_n < 0 ) {
			snd_mixer_close(handle);
			return -1;
		}
	}
	return 0;
}

static gint mk_mux_lst(snd_mixer_t *handle,snd_mixer_elements_t *es,
						snd_mixer_element_info_t *info,
						snd_mixer_eid_t **eids) {
	int i,j,err,n=0;
	snd_mixer_routes_t rt;
	int *ee_lst;
	snd_mixer_eid_t *eid_l;

	ee_lst=(int *)g_malloc(es->elements*sizeof(int));
	if( ee_lst == NULL ) {
		fprintf(stderr,nomem_msg);
		return -1;
	}
	for( i=0 ; i<es->elements ; i++ ) {
		bzero(&rt,sizeof(rt));
		rt.eid=es->pelements[i];
		if(( err=snd_mixer_routes(handle,&rt))<0 ) {
			fprintf(stderr,_("Mixer route error: %s\n"),snd_strerror(err));
			g_free(ee_lst);
			return -1;
		}
		if (!rt.routes_over) continue;
		rt.proutes=(snd_mixer_eid_t *)g_malloc(rt.routes_over *
												sizeof(snd_mixer_eid_t));
		if( rt.proutes == NULL ) {
			fprintf(stderr,nomem_msg);
			g_free(ee_lst);
			return -1;
		}
		rt.routes_size=rt.routes_over;
		rt.routes=rt.routes_over=0;
		if( (err=snd_mixer_routes(handle,&rt)) < 0 ) {
			fprintf(stderr,_("Mixer route (2) error: %s\n"),snd_strerror(err));
			g_free(ee_lst);
			return -1;
		}
		for( j=0 ; j<rt.routes ; j++ ) {
			if( strcmp(rt.proutes[j].name,info->eid.name) == 0 &&
				rt.proutes[j].index == info->eid.index &&
				rt.proutes[j].type == info->eid.type ) {
				ee_lst[n++]=i;
				break;
			}
		}
		g_free(rt.proutes);
	}
	if( n == 0 ) {
		*eids = NULL;
		return 0;
	}
	eid_l = *eids = (snd_mixer_eid_t *)g_malloc(n*sizeof(snd_mixer_eid_t));
	if( *eids == NULL ) {
		fprintf(stderr,nomem_msg);
		g_free(ee_lst);
		return -1;
	}
	for( i=0 ; i<n ; i++ ) {
		eid_l[i]=es->pelements[ee_lst[i]];
	}
	g_free(ee_lst);
	return n;
}
