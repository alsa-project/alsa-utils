/*****************************************************************************
   config.c - parses the config file
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
/* Begin #define's */

#define CHANNEL_SIZE 64
#define LABEL_SIZE 1024

/* End #define's */
/*****************************************************************************/

/*****************************************************************************/
/* Begin Global Variables */

extern Card *card; /* And array of the cards */
extern int cards; /* The number of cards in the system. */
Config config; /* The global config */

/* End Global Variables */
/*****************************************************************************/

int config_init()
{
	/* Initialize the values to some reasonable defaults */
	config.flags &= 0;
	config.labels = NULL;
	config.xpm = NULL;
	config.icon = NULL;
	config.mute = "M";
	config.unmute = NULL;
	config.simul = "|-|";
	config.unsimul = NULL;
	config.rec = "Rec";
	config.unrec = NULL;
	config.scale = 100;
	config.x_pos = -1;
	config.y_pos = -1;
	config.padding = 5;
	config.cdisplay = NULL;

	return 1;
}

int config_read(const char *file)
{
	char *home_dir, *home_env;
	FILE *stream;
	char line[1025], *chr;
	int state = 0; /* 0 = general config; 1 = history */
	unsigned int i = 0;
	int linelen = 0;
	char channel[CHANNEL_SIZE]; /* The name of the channel */
	char label[LABEL_SIZE]; /* The label or xpm name */
	int linenum = 0;

	stream = fopen(file, "r");

	/* If there is no initialized value */
	if(stream == NULL)
		return TRUE;
  
	while(fgets(line, 1024, stream)){
		linenum++;
		/* Get wrid of comments */
		if(is_comment(line))
			continue;
		strip_comment(line);
    
		/* Convert the line to upper  case so that matches aren't case 
		   sensitive (if not in history)*/
		linelen = strlen(line);

		if(strstr(line, "Position")) {
			if(sscanf(line, "Position %i %i", &config.x_pos, &config.y_pos) < 2)
				config.x_pos = config.y_pos = -1;
		}
		else if(strstr(line, "ShowCardName"))
			config.flags |= CONFIG_SHOW_CARD_NAME;
		else if(strstr(line, "ShowMixerNumber"))
			config.flags |= CONFIG_SHOW_MIXER_NUMBER;
		else if(strstr(line, "ShowMixerName"))
			config.flags |= CONFIG_SHOW_MIXER_NAME;
		else if(strstr(line, "IconXpm"))
			if(sscanf(line, "IconXpm %s", label) < 1)
				printf("Bad IconXpm entry at line %i.\n", linenum);
			else {
				config.icon = calloc(strlen(label) + 1, sizeof(char));
				strcpy(config.icon, label);
				config.flags |= CONFIG_ICON_XPM;
			}
		else if(strstr(line, "IgnoreXpms"))
			config.flags &= ~CONFIG_USE_XPMS;
		else if(strstr(line, "UseXpms"))
			config.flags |= CONFIG_USE_XPMS;
		else if(strstr(line, "unMuteXpmLeft"))
			if(sscanf(line, "unMuteXpmLeft %s", label) < 1)
				printf("Bad unMuteXpmLeft entry at line %i.\n", linenum);
			else {
				config.unmute_l = calloc(strlen(label) + 1, sizeof(char));
				strcpy(config.unmute_l, label);
				config.flags |= CONFIG_UNMUTE_XPM_L;
			}
		else if(strstr(line, "unMuteXpm"))
			if(sscanf(line, "unMuteXpm %s", label) < 1)
				printf("Bad unMuteXpm entry at line %i.\n", linenum);
			else {
				config.unmute = calloc(strlen(label) + 1, sizeof(char));
				strcpy(config.unmute, label);
				config.flags |= CONFIG_UNMUTE_XPM;
			}
		else if(strstr(line, "unRecXpm"))
			if(sscanf(line, "unRecXpm %s", label) < 1)
				printf("Bad unRecXpm entry at line %i.\n", linenum);
			else {
				config.unrec = calloc(strlen(label) + 1, sizeof(char));
				strcpy(config.unrec, label);
				config.flags |= CONFIG_UNREC_XPM;
			}  
		else if(strstr(line, "unSimulXpm"))
			if(sscanf(line, "unSimulXpm %s", label) < 1)
				printf("Bad unSimulXpm entry at line %i.\n", linenum);
			else {
				config.unsimul = calloc(strlen(label) + 1, sizeof(char));
				strcpy(config.unsimul, label);
				config.flags |= CONFIG_UNSIMUL_XPM;
			}      
		else if(strstr(line, "MuteLabel"))
			if(sscanf(line, "MuteLabel %s", label) < 1)
				printf("Bad MuteLabel entry at line %i.\n", linenum);
			else {
				config.mute = calloc(strlen(label) + 1, sizeof(char));
				strcpy(config.mute, label);
				config.flags &= ~CONFIG_MUTE_XPM;
			}
		else if(strstr(line, "SimulLabel"))
			if(sscanf(line, "SimulLabel %s", label) < 1)
				printf("Bad SimulLabel entry at line %i.\n", linenum);
			else {
				config.simul = calloc(strlen(label) + 1, sizeof(char));
				strcpy(config.simul, label);
				config.flags &= ~CONFIG_SIMUL_XPM;
			}
		else if(strstr(line, "RecLabel"))
			if(sscanf(line, "RecLabel %s", label) < 1)
				printf("Bad RecLabel entry at line %i.\n", linenum);
			else {
				config.rec = calloc(strlen(label) + 1, sizeof(char));
				strcpy(config.rec, label);
				config.flags &= ~CONFIG_REC_XPM;
			}
		else if(strstr(line, "MuteXpmLeft"))
			if(sscanf(line, "MuteXpmLeft %s", label) < 1)
				printf("Bad MuteXpmLeft entry at line %i.\n", linenum);
			else {
				config.mute_l = calloc(strlen(label) + 1, sizeof(char));
				strcpy(config.mute_l, label);
				config.flags |= CONFIG_MUTE_XPM_L;
			}
		else if(strstr(line, "MuteXpm"))
			if(sscanf(line, "MuteXpm %s", label) < 1)
				printf("Bad MuteXpm entry at line %i.\n", linenum);
			else {
				config.mute = calloc(strlen(label) + 1, sizeof(char));
				strcpy(config.mute, label);
				config.flags |= CONFIG_MUTE_XPM;
			}
		else if(strstr(line, "RecXpm"))
			if(sscanf(line, "RecXpm %s", label) < 1)
				printf("Bad RecXpm entry at line %i.\n", linenum);
			else {
				config.rec = calloc(strlen(label) + 1, sizeof(char));
				strcpy(config.rec, label);
				config.flags |= CONFIG_REC_XPM;
			}
		else if(strstr(line, "SimulXpm"))
			if(sscanf(line, "SimulXpm %s", label) < 1)
				printf("Bad SimulXpm entry at line %i.\n", linenum);
			else {
				config.simul = calloc(strlen(label) + 1, sizeof(char));
				strcpy(config.simul, label);
				config.flags |= CONFIG_SIMUL_XPM;
			}
		else if(strstr(line, "BackgroundXpm"))
			if(sscanf(line, "BackgroundXpm %s", label) < 1)
				printf("Bad BackgroundXpm entry at line %i.\n", linenum);
			else {
				config.background = calloc(strlen(label) + 1, sizeof(char));
				strcpy(config.background, label);
				config.flags |= CONFIG_BACKGROUND_XPM;
			}
		else if(strstr(line, "Label")){
			if(get_label(line, "Label", channel, CHANNEL_SIZE, 
				     label, LABEL_SIZE, '[', ']')){
				config.labels = channel_label_append(config.labels, channel, label);
			}
			else
				printf("Bad Label entry found on line %i.\n", linenum);
		}
		else if(strstr(line, "Xpm")){
			if(get_label(line, "Xpm", channel, CHANNEL_SIZE, 
				     label, LABEL_SIZE, '[', ']')){
				config.xpm = channel_label_append(config.xpm, channel, label);
			}
			else
				printf("Bad Xpm entry found on line %i.\n", linenum);
		}
		else if(strstr(line, "ScaleSize"))
			if(sscanf(line, "ScaleSize %i", &i) == 1)
				config.scale = i;
			else
				printf("Bad ScaleSize entry at line %i.\n", linenum);
		if(strstr(line, "ChannelPadding"))
			if(sscanf(line, "ChannelPadding %i", &i) == 1)
				config.padding = i;
			else
				printf("Bad ChannelPadding entry at line %i.\n", linenum);
    
    
	} /* End of config loop */
  
	/* Close the file */
	fclose(stream);

	return TRUE;
}


int setup_pixmaps(GtkWidget *xpmparent)
{
	GtkStyle *style;
	int fd;

	if(!(config.flags & CONFIG_USE_XPMS)) {
		config.mute = "M";
		config.simul = "|-|";
		config.rec = "Rec";
		return;
	}



	if(config.flags & CONFIG_ICON_XPM){
		fd = open(config.icon, O_RDONLY);
		if(fd != -1) {
			close(fd);
			style = gtk_widget_get_style(xpmparent);
			config.icon_xpm = 
				gdk_pixmap_create_from_xpm(xpmparent->window,
							   &config.icon_mask,
							   &style->bg[GTK_STATE_NORMAL],
							   config.icon);
		}
		else {
			printf("Unable to open pixmap %s.\n", config.icon);
			config.flags &= ~CONFIG_ICON_XPM;
			config.icon_xpm = NULL;
			free(config.icon);
			config.icon = NULL;
		}
	}


	if(config.flags & CONFIG_MUTE_XPM_L){
		fd=open(config.mute_l, O_RDONLY);
		if(fd != -1) {
			close(fd);
			style = gtk_widget_get_style(xpmparent);
			config.mute_xpm_l = 
				gdk_pixmap_create_from_xpm(xpmparent->window,
							   &config.mute_mask_l,
							   &style->bg[GTK_STATE_NORMAL],
							   config.mute_l);
		}
		else {
			printf("Unable to open pixmap %s.\n", config.mute_l);
			config.flags &= ~CONFIG_MUTE_XPM_L;
			config.mute_xpm_l = NULL;
			free(config.mute_l);
			config.mute_l = NULL;
		}
	}
  

	if(config.flags & CONFIG_MUTE_XPM){
		fd=open(config.mute, O_RDONLY);
		if(fd != -1) {
			close(fd);
			style = gtk_widget_get_style(xpmparent);
			config.mute_xpm = 
				gdk_pixmap_create_from_xpm(xpmparent->window,
							   &config.mute_mask,
							   &style->bg[GTK_STATE_NORMAL],
							   config.mute);
			if(!(config.flags & CONFIG_MUTE_XPM_L)) {
				config.mute_xpm_l = config.mute_xpm;
				config.mute_mask_l = config.mute_mask;
				config.flags |= CONFIG_MUTE_XPM_L;
			}
		}
		else {
			printf("Unable to open pixmap %s.\n", config.mute);
			config.flags &= ~CONFIG_MUTE_XPM;
			config.mute_xpm = NULL;
			free(config.mute);
			config.mute = "M";
		}
	}
  


	if(config.flags & CONFIG_UNMUTE_XPM_L) {
		fd=open(config.unmute_l, O_RDONLY);
		if(fd != -1) {
			close(fd);
			style = gtk_widget_get_style(xpmparent);
			config.unmute_xpm_l = 
				gdk_pixmap_create_from_xpm(xpmparent->window,
							   &config.unmute_mask_l,
							   &style->bg[GTK_STATE_NORMAL],
							   config.unmute_l);
		}
		else {
			printf("Unable to open pixmap %s.\n", config.unmute_l);
			config.flags &= ~CONFIG_UNMUTE_XPM_L;
			free(config.unmute_l);
			config.unmute_l=NULL;
		}
	}

  

	if(config.flags & CONFIG_UNMUTE_XPM) {
		fd=open(config.unmute, O_RDONLY);
		if(fd != -1) {
			close(fd);
			style = gtk_widget_get_style(xpmparent);
			config.unmute_xpm = 
				gdk_pixmap_create_from_xpm(xpmparent->window,
							   &config.unmute_mask,
							   &style->bg[GTK_STATE_NORMAL],
							   config.unmute);
			if(!(config.flags & CONFIG_UNMUTE_XPM_L)) {
				printf("Invoked!\n");
				config.unmute_xpm_l = config.unmute_xpm; 
				config.unmute_mask_l = config.unmute_mask;
				config.flags |= CONFIG_UNMUTE_XPM_L;
			}
		}
		else {
			printf("Unable to open pixmap %s.\n", config.unmute);
			config.flags &= ~CONFIG_UNMUTE_XPM;
			free(config.unmute);
			config.unmute=NULL;
		}
	}



	if(config.flags & CONFIG_REC_XPM) {
		fd=open(config.rec, O_RDONLY);
		if(fd != -1) {
			close(fd);
			style = gtk_widget_get_style(xpmparent);
			config.rec_xpm = 
				gdk_pixmap_create_from_xpm(xpmparent->window,
							   &config.rec_mask,
							   &style->bg[GTK_STATE_NORMAL],
							   config.rec);
		}
		else {
			printf("Unable to open pixmap %s.\n", config.rec);
			config.flags &= ~CONFIG_REC_XPM;
			free(config.rec);
			config.rec = "Rec";
		}
	}

	if(config.flags & CONFIG_UNREC_XPM) {
		fd=open(config.unrec, O_RDONLY);
		if(fd != -1) {
			close(fd);
			style = gtk_widget_get_style(xpmparent);
			config.unrec_xpm = 
				gdk_pixmap_create_from_xpm(xpmparent->window,
							   &config.unrec_mask,
							   &style->bg[GTK_STATE_NORMAL],
							   config.unrec);
		}
		else {
			printf("Unable to open pixmap %s.\n", config.unrec);
			config.flags &= ~CONFIG_UNREC_XPM;
			free(config.unrec);
			config.unrec=NULL;
		}
	}

	if(config.flags & CONFIG_SIMUL_XPM) {
		fd = open(config.simul, O_RDONLY);
		if(fd != -1) {
			close(fd);
			style = gtk_widget_get_style(xpmparent);
			config.simul_xpm = 
				gdk_pixmap_create_from_xpm(xpmparent->window,
							   &config.simul_mask,
							   &style->bg[GTK_STATE_NORMAL],
							   config.simul);
		}
		else {
			printf("Unable to open pixmap %s.\n", config.simul);
			config.flags &= ~CONFIG_SIMUL_XPM;
			free(config.simul);
			config.simul="|-|";
		}
	}

	if(config.flags & CONFIG_UNSIMUL_XPM) {
		fd = open(config.unsimul, O_RDONLY);
		if(fd != -1) {
			close(fd);
			style = gtk_widget_get_style(xpmparent);
			config.unsimul_xpm = 
				gdk_pixmap_create_from_xpm(xpmparent->window,
							   &config.unsimul_mask,
							   &style->bg[GTK_STATE_NORMAL],
							   config.unsimul);
		}
		else {
			printf("Unable to open pixmap %s.\n", config.unsimul);
			config.flags &= ~CONFIG_UNSIMUL_XPM;
			free(config.unsimul);
			config.unsimul=NULL;
		}
	}

	if(config.flags & CONFIG_BACKGROUND_XPM) {
		fd = open(config.background, O_RDONLY);
		if(fd != -1) {
			close(fd);
			style = gtk_widget_get_style(xpmparent);
			config.background_xpm = 
				gdk_pixmap_create_from_xpm(xpmparent->window,
							   &config.background_mask,
							   &style->bg[GTK_STATE_NORMAL],
							   config.background);
		}
		else {
			printf("Unable to open pixmap %s.\n", config.background);
			config.flags &= ~CONFIG_BACKGROUND_XPM;
			free(config.background);
			config.background=NULL;
		}
	}
  
	return TRUE;
}


