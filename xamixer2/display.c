/*****************************************************************************
   xamixer.c - an Alsa based gtk mixer
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

/*****************************************************************************/ 
/* Begin function prototypes */

GtkWidget *group_elements(int card, int mixer, Group *group);
GtkWidget *display_volume1(Group *group, int element, void *handle, char *route);
GtkWidget *display_switch2(Group *group, int element, void *handle, char *route);
GtkWidget *display_switch1(Group *group, int element, void *handle, char *route);
GtkWidget *display_3deffect1(Group *group, int element, void *handle, char *route);

/* End function protoypes */
/*****************************************************************************/ 

GtkWidget *create_mixer_page(int card_num, int mixer_num) 
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *frame;
	int i=card_num, j=mixer_num, k=0, l, m;
	int w=1, col;
	
	/* Compute the number of culumns to use */
	//	w = (int)sqrt((double)card[i].mixer[j].info.elements);
	w = (int)(1.5 * 
		  (float)card[i].mixer[j].info.elements / 
		  (float)card[i].mixer[j].info.groups);
	if (w == 0)
		col = 0;
	else
		/* Compute the number of groups in a column */
		col = (card[i].mixer[j].info.groups + w - 1)/ w;

	/* Create the main bounding box */
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(hbox);


	/* Make a vertical box for each column, then put that column's worth
	   of mixer groups into the column */
	for(l = 0; l < w; l++) {
		/* Make the vertical box to pack it in */
		vbox = gtk_vbox_new(FALSE, 0);
		gtk_widget_show(vbox);
		gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

		for(m = 0; m < col && k < card[i].mixer[j].info.groups; m++) {
			/* Make the group frame */
			frame = gtk_frame_new(card[i].mixer[j].group[k].group.gid.name);
			gtk_widget_show(frame);
			gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
			
			gtk_container_add(GTK_CONTAINER(frame),
					  group_elements(card_num, 
							 mixer_num,
							 &card[i].mixer[j].group[k]));


			/* Now increment the count of which mixer group we're on */
			k++;
		}
	}

	return hbox;
}


GtkWidget *group_elements(int card_num, int mixer, Group *group)
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *box;
	GtkWidget *widget;
	char thor[128];
	int i, j;
	snd_mixer_element_t test;

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox);

	for(i = 0; i < group->group.elements; i++) {
		/* Each element gets its own horizontal box */
		hbox=gtk_hbox_new(FALSE, 0);
		gtk_widget_show(hbox);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
		
		snprintf(thor, 128, "%s routed to the %s", 
			 group->group.pelements[i].name, 
			 group->routes[i].proutes[0].name);

/* 		label = gtk_label_new(thor); */
/* 		gtk_widget_show(label); */
/* 		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0); */


		switch(group->group.pelements[i].type){

		case SND_MIXER_ETYPE_VOLUME1:
			gtk_box_pack_end(GTK_BOX(hbox), 
					 display_volume1(group, i, 
							 card[card_num].mixer[mixer].handle,
							 thor), 
					 FALSE, FALSE, 0);
			break;

		case SND_MIXER_ETYPE_SWITCH1:
			gtk_box_pack_end(GTK_BOX(hbox),
					 display_switch1(group, i, 
							 card[card_num].mixer[mixer].handle,
							 thor),
					 FALSE, FALSE, 0);
			break;

		case SND_MIXER_ETYPE_SWITCH2:
			gtk_box_pack_end(GTK_BOX(hbox),
					 display_switch2(group, i, 
							 card[card_num].mixer[mixer].handle,
							 thor),
					 FALSE, FALSE, 0);
			break;

		case SND_MIXER_ETYPE_3D_EFFECT1:
			gtk_box_pack_end(GTK_BOX(hbox),
					 display_3deffect1(group, i, 
							 card[card_num].mixer[mixer].handle,
							 thor),
					 FALSE, FALSE, 0);
			break;	
		}
	}



	return vbox;
}

GtkWidget *display_3deffect1(Group *group, int element, void *handle, char *route)
{
	GtkWidget *vbox;
	GtkWidget *box;
	GtkTooltips *tooltips;
	GtkWidget *widget;
	GtkWidget *label;
	int i=element;
	GtkObject *adj;

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox);

	group->gtk[i].interface = calloc(10, sizeof(GtkWidget *));
	group->gtk[i].adjust = calloc(10, sizeof(GtkWidget *));

	/* The on/off switch */
	if(group->einfo[i].data.teffect1.effect & SND_MIXER_EFF1_SW) {
		box = gtk_hbox_new(FALSE, 0);
		gtk_widget_show(box);
		gtk_box_pack_start(GTK_BOX(vbox), box, FALSE, FALSE, 0);
		
		label = gtk_label_new("3D Effect");
		gtk_widget_show(label);
		gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
		
		widget = gtk_check_button_new();
		if(group->element[i].data.teffect1.sw)
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(widget), TRUE);
		
		gtk_widget_show(widget);
		gtk_box_pack_end(GTK_BOX(box), widget, FALSE, FALSE, 0);
		/* Connect it to the callback */
		gtk_signal_connect(GTK_OBJECT(widget), "toggled", 
				   GTK_SIGNAL_FUNC(adjust_teffect1),
				   create_cb_data(group, handle, i, TYPE_SW));
	}



	/* The mono switch */
	if(group->einfo[i].data.teffect1.effect & SND_MIXER_EFF1_MONO_SW) {
		box = gtk_hbox_new(FALSE, 0);
		gtk_widget_show(box);
		gtk_box_pack_start(GTK_BOX(vbox), box, FALSE, FALSE, 0);
		
		label = gtk_label_new("3D Effect Mono");
		gtk_widget_show(label);
		gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
		
		widget = gtk_check_button_new();
		if(group->element[i].data.teffect1.sw)
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(widget), TRUE);
		gtk_widget_show(widget);
		gtk_box_pack_end(GTK_BOX(box), widget, FALSE, FALSE, 0);
		/* Connect it to the callback */
		gtk_signal_connect(GTK_OBJECT(widget), "toggled", 
				   GTK_SIGNAL_FUNC(adjust_teffect1),
				   create_cb_data(group, handle, i, TYPE_MONO_SW));
	}


	/* the wide control */
	if(group->einfo[i].data.teffect1.effect & SND_MIXER_EFF1_WIDE) {
		box = gtk_hbox_new(FALSE, 0);
		gtk_widget_show(box);
		gtk_box_pack_start(GTK_BOX(vbox), box, FALSE, FALSE, 0);
		
		label = gtk_label_new("3D Effect Width");
		gtk_widget_show(label);
		gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
		
		
		adj = gtk_adjustment_new(group->element[i].data.teffect1.wide,
					 group->einfo[i].data.teffect1.min_wide,
					 group->einfo[i].data.teffect1.max_wide,
					 1.0,
					 3.0,
					 0.0);
		widget = gtk_hscale_new(GTK_ADJUSTMENT(adj));
		gtk_scale_set_value_pos(GTK_SCALE(widget), 
					GTK_POS_RIGHT);
		gtk_widget_set_usize(widget, 100, -1);
		gtk_widget_show(widget);
		gtk_box_pack_end(GTK_BOX(box), widget, FALSE, FALSE, 0);

		/* connect the signal */
		gtk_signal_connect(GTK_OBJECT(adj),
				   "value_changed", 
				   GTK_SIGNAL_FUNC (adjust_teffect1),
				   create_cb_data(group, handle, i, TYPE_WIDE));
	}

	/* the volume widget */
	if(group->einfo[i].data.teffect1.effect & SND_MIXER_EFF1_VOLUME) {
		box = gtk_hbox_new(FALSE, 0);
		gtk_widget_show(box);
		gtk_box_pack_start(GTK_BOX(vbox), box, FALSE, FALSE, 0);
		
		label = gtk_label_new("3D Effect Volume");
		gtk_widget_show(label);
		gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
		
		
		adj = gtk_adjustment_new(group->element[i].data.teffect1.volume,
					 group->einfo[i].data.teffect1.min_volume,
					 group->einfo[i].data.teffect1.max_volume,
					 1.0,
					 3.0,
					 0.0);
		widget = gtk_hscale_new(GTK_ADJUSTMENT(adj));
		gtk_scale_set_value_pos(GTK_SCALE(widget), 
					GTK_POS_RIGHT);
		gtk_widget_set_usize(widget, 100, -1);
		gtk_widget_show(widget);
		gtk_box_pack_end(GTK_BOX(box), widget, FALSE, FALSE, 0);
		/* connect the signal */
		gtk_signal_connect(GTK_OBJECT(adj),
				   "value_changed", 
				   GTK_SIGNAL_FUNC (adjust_teffect1),
				   create_cb_data(group, handle, i, TYPE_VOLUME));
	}


	/* The center widget */
	if(group->einfo[i].data.teffect1.effect & SND_MIXER_EFF1_CENTER) {
		box = gtk_hbox_new(FALSE, 0);
		gtk_widget_show(box);
		gtk_box_pack_start(GTK_BOX(vbox), box, FALSE, FALSE, 0);
		
		label = gtk_label_new("3D Effect Center");
		gtk_widget_show(label);
		gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
		
		
		adj = gtk_adjustment_new(group->element[i].data.teffect1.center,
					 group->einfo[i].data.teffect1.min_center,
					 group->einfo[i].data.teffect1.max_center,
					 1.0,
					 3.0,
					 0.0);
		widget = gtk_hscale_new(GTK_ADJUSTMENT(adj));
		gtk_scale_set_value_pos(GTK_SCALE(widget), 
					GTK_POS_RIGHT);
		gtk_widget_set_usize(widget, 100, -1);
		gtk_widget_show(widget);
		gtk_box_pack_end(GTK_BOX(box), widget, FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(adj),
				   "value_changed", 
				   GTK_SIGNAL_FUNC (adjust_teffect1),
				   create_cb_data(group, handle, i, TYPE_CENTER));
	}


	/* The Space widget */
	if(group->einfo[i].data.teffect1.effect & SND_MIXER_EFF1_SPACE) {
		box = gtk_hbox_new(FALSE, 0);
		gtk_widget_show(box);
		gtk_box_pack_start(GTK_BOX(vbox), box, FALSE, FALSE, 0);
		
		label = gtk_label_new("3D Effect Space");
		gtk_widget_show(label);
		gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
		
		
		adj = gtk_adjustment_new(group->element[i].data.teffect1.space,
					 group->einfo[i].data.teffect1.min_space,
					 group->einfo[i].data.teffect1.max_space,
					 1.0,
					 3.0,
					 0.0);
		widget = gtk_hscale_new(GTK_ADJUSTMENT(adj));
		gtk_scale_set_value_pos(GTK_SCALE(widget), 
					GTK_POS_RIGHT);
		gtk_widget_set_usize(widget, 100, -1);
		gtk_widget_show(widget);
		gtk_box_pack_end(GTK_BOX(box), widget, FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(adj),
				   "value_changed", 
				   GTK_SIGNAL_FUNC (adjust_teffect1),
				   create_cb_data(group, handle, i, TYPE_SPACE));
	}

	/* The depth widget */
	if(group->einfo[i].data.teffect1.effect & SND_MIXER_EFF1_DEPTH) {
		box = gtk_hbox_new(FALSE, 0);
		gtk_widget_show(box);
		gtk_box_pack_start(GTK_BOX(vbox), box, FALSE, FALSE, 0);
		
		label = gtk_label_new("3D Effect Depth");
		gtk_widget_show(label);
		gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
		
		
		adj = gtk_adjustment_new(group->element[i].data.teffect1.depth,
					 group->einfo[i].data.teffect1.min_depth,
					 group->einfo[i].data.teffect1.max_depth,
					 1.0,
					 3.0,
					 0.0);
		widget = gtk_hscale_new(GTK_ADJUSTMENT(adj));
		gtk_scale_set_value_pos(GTK_SCALE(widget), 
					GTK_POS_RIGHT);
		gtk_widget_set_usize(widget, 100, -1);
		gtk_widget_show(widget);
		gtk_box_pack_end(GTK_BOX(box), widget, FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(adj),
				   "value_changed", 
				   GTK_SIGNAL_FUNC (adjust_teffect1),
				   create_cb_data(group, handle, i, TYPE_DEPTH));
	}

	/* The delay widget */
	if(group->einfo[i].data.teffect1.effect & SND_MIXER_EFF1_DELAY) {
		box = gtk_hbox_new(FALSE, 0);
		gtk_widget_show(box);
		gtk_box_pack_start(GTK_BOX(vbox), box, FALSE, FALSE, 0);
		
		label = gtk_label_new("3D Effect Delay");
		gtk_widget_show(label);
		gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
		
		
		adj = gtk_adjustment_new(group->element[i].data.teffect1.delay,
					 group->einfo[i].data.teffect1.min_delay,
					 group->einfo[i].data.teffect1.max_delay,
					 1.0,
					 3.0,
					 0.0);
		widget = gtk_hscale_new(GTK_ADJUSTMENT(adj));
		gtk_scale_set_value_pos(GTK_SCALE(widget), 
					GTK_POS_RIGHT);
		gtk_widget_set_usize(widget, 100, -1);
		gtk_widget_show(widget);
		gtk_box_pack_end(GTK_BOX(box), widget, FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(adj),
				   "value_changed", 
				   GTK_SIGNAL_FUNC (adjust_teffect1),
				   create_cb_data(group, handle, i, TYPE_DELAY));
	}


	/* The feedback widget */
	if(group->einfo[i].data.teffect1.effect & SND_MIXER_EFF1_FEEDBACK) {
		box = gtk_hbox_new(FALSE, 0);
		gtk_widget_show(box);
		gtk_box_pack_start(GTK_BOX(vbox), box, FALSE, FALSE, 0);
		
		label = gtk_label_new("3D Effect Feedback");
		gtk_widget_show(label);
		gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
		
		
		adj = gtk_adjustment_new(group->element[i].data.teffect1.feedback,
					 group->einfo[i].data.teffect1.min_feedback,
					 group->einfo[i].data.teffect1.max_feedback,
					 1.0,
					 3.0,
					 0.0);
		widget = gtk_hscale_new(GTK_ADJUSTMENT(adj));
		gtk_scale_set_value_pos(GTK_SCALE(widget), 
					GTK_POS_RIGHT);
		gtk_widget_set_usize(widget, 100, -1);
		gtk_widget_show(widget);
		gtk_box_pack_end(GTK_BOX(box), widget, FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(adj),
				   "value_changed", 
				   GTK_SIGNAL_FUNC (adjust_teffect1),
				   create_cb_data(group, handle, i, TYPE_FEEDBACK));
	}



	return vbox;
}

GtkWidget *display_switch1(Group *group, int element, void *handle, char *route)
{
	GtkWidget *box;
	GtkTooltips *tooltips;
	GtkWidget *button;
	int i, j;

	i = element;

	box = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(box);

	/* Allocate the widget array */
	group->gtk[i].interface = calloc(group->element[i].data.switch1.sw, sizeof(GtkWidget *));

	for(j = 0; j < group->element[i].data.switch1.sw; j++) {
		button = gtk_check_button_new();
		/* looks painful, doesn't it?  It's checking the state of the appropriate bit */
		if(group->element[i].data.switch1.psw[j / sizeof(unsigned int)] & 
		   (1 << (j % sizeof(unsigned int))))
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON (button), TRUE);
		gtk_widget_show(button);

		/* Set up the tooltips */
		tooltips = gtk_tooltips_new();
		gtk_tooltips_set_tip (tooltips, button, route, NULL);


		gtk_box_pack_start(GTK_BOX (box), button, FALSE, FALSE, 0);

		/* Connect it to the callback */
		gtk_signal_connect(GTK_OBJECT(button), "toggled", 
				   GTK_SIGNAL_FUNC(adjust_switch1),
				   create_cb_data(group, handle, i, j));

		/* Store the widget */
		group->gtk[i].interface[j] = button;
	}


	return box;
}


GtkWidget *display_switch2(Group *group, int element, void *handle, char *route)
{
	GtkWidget *button;
	GtkTooltips *tooltips;
	int i, j=0;

	i = element;

	if(!group) {
		printf("Group isn't initialized!\n");
		return NULL;
	}

	button = gtk_check_button_new();

	if(group->element)
		if(group->element[i].data.switch2.sw) {
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), TRUE);
		}

	gtk_widget_show(button);

	/* Set up the tooltip */
	tooltips = gtk_tooltips_new();
	gtk_tooltips_set_tip (tooltips, button, route, NULL);

	if(group->gtk) {
		group->gtk[i].interface = calloc(1, sizeof(GtkWidget *));
		group->gtk[i].interface[j] = button;
	} else {
		printf("Something wasn't initialized properly.\n");
	}

	/* Connect it to the callback */
	gtk_signal_connect(GTK_OBJECT(group->gtk[i].interface[j]),
			   "toggled", 
			   GTK_SIGNAL_FUNC (adjust_switch2),
			   create_cb_data(group, handle, element, j));
	
	return button;
}

GtkWidget *display_volume1(Group *group, int element, void *handle, char *route)
{
	GtkWidget *box;
	GtkTooltips *tooltips;
	int i,j;

	i = element;

	box = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(box);

	group->gtk[i].adjust = calloc(group->element[i].data.volume1.voices, 
				      sizeof(GtkObject *));
	group->gtk[i].interface = calloc(group->element[i].data.volume1.voices, 
					 sizeof(GtkWidget *));
	
	for(j=0; j < group->element[i].data.volume1.voices; j++) {
		group->gtk[i].adjust[j] = 
			gtk_adjustment_new(group->element[i].data.volume1.pvoices[j],
					   group->einfo[i].data.volume1.prange[0].min,
					   group->einfo[i].data.volume1.prange[0].max,
					   1.0,
					   3.0,
					   0.0);
		
		group->gtk[i].interface[j] = 
			gtk_hscale_new(GTK_ADJUSTMENT(group->gtk[i].adjust[j]));

		gtk_signal_connect(GTK_OBJECT(group->gtk[i].adjust[j]),
				   "value_changed", 
				   GTK_SIGNAL_FUNC (adjust_volume1),
				   create_cb_data(group, handle, element, j));

/* 		gtk_scale_set_draw_value(GTK_SCALE(group->gtk[i].interface[j]), */
/* 					 FALSE); */

		gtk_scale_set_value_pos(GTK_SCALE(group->gtk[i].interface[j]), 
					GTK_POS_RIGHT);

 		gtk_widget_set_usize(group->gtk[i].interface[j], 100, -1);

		gtk_widget_show(group->gtk[i].interface[j]);
		gtk_box_pack_start(GTK_BOX(box), 
				   group->gtk[i].interface[j], 
				   FALSE, FALSE, 0);

		/* Set up the tooltip */
		tooltips = gtk_tooltips_new();
		gtk_tooltips_set_tip (tooltips, group->gtk[i].interface[j], route, NULL);

	}
	


	return box;
}
