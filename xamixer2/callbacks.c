/*****************************************************************************
   callbacks.c - an Alsa based gtk mixer
   Written by Raistlinn (lansdoct@cs.alfred.edu)
   Copyright (C) 1998 by Christopher Lansdown
   
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
******************************************************************************/

/*****************************************************************************/
/* Begin #include's */

#include "main.h"

/* End #include's */
/*****************************************************************************/

/*****************************************************************************/
/* Begin Global Variables */

extern GtkWidget *window;
extern Card *card; /* And array of the cards */
extern int cards; /* The number of cards in the system. */
extern Config config; /* The system config */

/* End Global Variables */
/*****************************************************************************/

void mixer_rebuild_cb(void *data)
{

/* 	printf("A rebuild event happened.\n"); */
/* 	fflush(NULL); */

	return;
}

void mixer_element_cb(void *data, int cmd, snd_mixer_eid_t *eid)
{

/* 	printf("An element event happened.\n"); */
/* 	fflush(NULL); */

	return;
}

void mixer_group_cb(void *data, int cmd, snd_mixer_gid_t *gid)
{

/* 	printf("A group event happened.\n"); */
/* 	fflush(NULL); */

	return;
}

void mixer_change_cb(gpointer data, gint source, GdkInputCondition condition)
{
	snd_mixer_callbacks_t callbacks;  

	/* Set up the callback structure */
	callbacks.private_data = data;
	callbacks.rebuild = mixer_rebuild_cb;
	callbacks.element = mixer_element_cb;
	callbacks.group = mixer_group_cb;
	bzero(callbacks.reserved, sizeof(void *) * 28);
			
	
	/* Actually deal with the event. */
	snd_mixer_read(MIXER(data)->handle, &callbacks);

	return;
}

void adjust_teffect1(GtkWidget *widget, CBData *data)
{
	int i, j, err;
	Group *group;

	i = data->element;
	j = data->index;
	group = data->group;

	switch(j) {
	case TYPE_SW:
		if(GTK_TOGGLE_BUTTON(widget)->active)
			group->element[i].data.teffect1.sw = 1;
		else
			group->element[i].data.teffect1.sw = 0;
		break;
	case TYPE_MONO_SW:
		if(GTK_TOGGLE_BUTTON(widget)->active)
			group->element[i].data.teffect1.mono_sw = 1;
		else
			group->element[i].data.teffect1.mono_sw = 0;
		break;
	case TYPE_WIDE:
		group->element[i].data.teffect1.wide = 
			(int)GTK_ADJUSTMENT(widget)->value;
		break;
	case TYPE_VOLUME:
		group->element[i].data.teffect1.volume = 
			(int)GTK_ADJUSTMENT(widget)->value;
		break;
	case TYPE_CENTER:
		group->element[i].data.teffect1.center = 
			(int)GTK_ADJUSTMENT(widget)->value;
		break;
	case TYPE_SPACE:
		group->element[i].data.teffect1.space = 
			(int)GTK_ADJUSTMENT(widget)->value;
		break;
	case TYPE_DEPTH:
		group->element[i].data.teffect1.depth = 
			(int)GTK_ADJUSTMENT(widget)->value;
		break;
	case TYPE_DELAY:
		group->element[i].data.teffect1.delay = 
			(int)GTK_ADJUSTMENT(widget)->value;
		break;
	case TYPE_FEEDBACK:
		group->element[i].data.teffect1.feedback = 
			(int)GTK_ADJUSTMENT(widget)->value;
		break;
	default:
		printf("Hit the default in adjust_teffect1 - this is bad.\n");
		break;
	}

	/* Now let's write the new value to the card */
	if ((err = snd_mixer_element_write(data->handle, &group->element[i])) < 0) {
		printf("3D Effect Mixer element write error: %s\n", snd_strerror(err));
	}

	return;
}
void adjust_switch1(GtkWidget *widget, CBData *data)
{
	int i, j, err;

	i = data->element;
	j = data->index;

	if(GTK_TOGGLE_BUTTON(widget)->active)
		data->group->element[i].data.switch1.psw[j / sizeof(unsigned int)] |=
			(1 << (j % sizeof(unsigned int)));
	else
		data->group->element[i].data.switch1.psw[j / sizeof(unsigned int)] &=
			~(1 << (j % sizeof(unsigned int)));

	/* Now let's write the new value to the card */
	if ((err = snd_mixer_element_write(data->handle, &data->group->element[i])) < 0) {
		printf("Mixer element write error: %s\n", snd_strerror(err));
	}
						      
	return;
}

void adjust_volume1(GtkWidget *widget, CBData *data)
{
	register int volume;
	int i, j, err;

	i = data->element;
	j = data->index;

	volume = (int)GTK_ADJUSTMENT(data->group->gtk[i].adjust[j])->value;
	data->group->element[i].data.volume1.pvoices[j] = volume;

	/* Now let's write the new value to the card */
	if ((err = snd_mixer_element_write(data->handle, &data->group->element[i])) < 0) {
		printf("Mixer element write error: %s\n", snd_strerror(err));
	}


	return;
}


void adjust_switch2(GtkWidget *widget, CBData *data)
{
	int i, j, err;

	i = data->element;
	j = data->index;

	if(GTK_TOGGLE_BUTTON(data->group->gtk[i].interface[j])->active) {
		data->group->element[i].data.switch2.sw = 1;
	} else {
		data->group->element[i].data.switch2.sw = 0;
	}

	/* Now let's write the new value to the card */
	if ((err = snd_mixer_element_write(data->handle, &data->group->element[i])) < 0) {
		printf("Mixer element write error: %s\n", snd_strerror(err));
	}

	return;
}
















