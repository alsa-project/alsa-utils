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

GtkWidget *window;
Card *card; /* And array of the cards */
int cards; /* The number of cards in the system. */
extern Config config; /* The system config */

/* End Global Variables */
/*****************************************************************************/

int main(int argc, char **argv)
{

        /* Begin Variable Declarations */
        GtkWidget *mainbox;
        GtkWidget *notebook;
        GtkWidget *frame;
        GtkWidget *label;
        GtkWidget *table;
        GtkWidget *switch_button;
        GtkWidget *tmpbox;
        GtkWidget *tablebox;
        GtkWidget *separator;
        int i,j,k,xpm,found,fd;
        char title[32];
        char name[128];
        ChannelLabel *tmp;
        char labelname[256];
        char *home_env, *home_dir;
        GtkStyle *style;
        GtkWidget *hbox;
        /* End Variable Declarations */

	/* Go through gtk initialization */
        gtk_init(&argc, &argv);

	/* Read the personal config file - these values override the global config */
        home_env = getenv("HOME");
        home_dir = calloc((strlen(home_env) + 2 + strlen(RCFILE)), 1);
        strcpy(home_dir, home_env);
        strcat(home_dir, "/");
        strcat(home_dir, RCFILE);
        gtk_rc_parse(home_dir);
        free(home_dir);

	/* Read in the soundcard info */
        if(init_cards()) {
                printf("Error.  Unable to initialize sound cards.\n");
                return 1;
        }

        /* Read in normal config info */
        config_init();
        config_read("/usr/local/etc/xamixer.conf");
        home_env = getenv("HOME");
        home_dir = calloc((strlen(home_env) + 2 + strlen(HOME_FILE)), 1);
        strcpy(home_dir, home_env);
        strcat(home_dir, "/");
        strcat(home_dir, HOME_FILE);
        config_read(home_dir);
        free(home_dir);
  
        /* Make the title */
        sprintf(title, "XAmixer2 %s", VERSION);

        /* Create the main window */
        window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(window), title);
        gtk_signal_connect(GTK_OBJECT (window), "delete_event", 
                           (GtkSignalFunc) gtk_main_quit, NULL);
        signal(SIGINT, (void *)gtk_main_quit);
        /* Set the policy */
        gtk_window_set_policy(GTK_WINDOW (window), TRUE, TRUE, TRUE);
        /* Set the position, if one has been defined */
        gtk_widget_set_uposition(window, config.x_pos, config.y_pos);
        /* Realize the window so that we can start drawing pixmaps to it */
        gtk_widget_realize(window);

	/* Set up the pixmaps */
        setup_pixmaps(window);


        /* Create the notebook */
        notebook = gtk_notebook_new();
        gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), GTK_POS_TOP);

        gtk_widget_show(notebook);
	gtk_container_add(GTK_CONTAINER(window), notebook);


        /* Create the notebook pages */
        for(i = 0; i < cards; i++) {
		for(j = 0; j < card[i].hw_info.mixerdevs; j++) {
			
			frame = create_mixer_page(i, j);




			/* Create the label and add the page to the notebook */
			bzero(labelname, 256);
			if(config.flags & CONFIG_SHOW_CARD_NAME) {
				strcpy(labelname, card[i].hw_info.name);
				if(config.flags & (CONFIG_SHOW_MIXER_NAME |
						   CONFIG_SHOW_MIXER_NUMBER))
					strcat(labelname, ", ");
			}
			
			if(config.flags & CONFIG_SHOW_MIXER_NUMBER) {
                                /* Do some trickery to get around an additional 
				   variable, plus this may be more efficient, 
                                   since strcat() has to figure out where the end 
				   of the line is anyhow, plus the copying. */
				sprintf(&labelname[strlen(labelname)], "Mixer %i", j);
				if(config.flags & CONFIG_SHOW_MIXER_NAME)
					strcat(labelname, ", ");
			}
			
                        if(config.flags & CONFIG_SHOW_MIXER_NAME)
                                strcat(labelname, card[i].mixer[j].info.name);
			
                        /* Just in case nothing is specified in the config file */
                        if(!(config.flags & (CONFIG_SHOW_CARD_NAME | 
                                             CONFIG_SHOW_MIXER_NAME | 
					     CONFIG_SHOW_MIXER_NUMBER)))
                                sprintf(labelname, "%i", i + j);
			
                        label = gtk_label_new(labelname);
                        gtk_widget_show(label);
                        gtk_notebook_append_page (GTK_NOTEBOOK (notebook), frame, label);
			
			
		}
	}
	
        /* Create the options page */
        frame = gtk_frame_new("");
        gtk_widget_show(frame);
	//        gtk_container_add(GTK_CONTAINER (frame), create_options_page());
        label = gtk_label_new("Options");
        gtk_widget_show(label);
        gtk_notebook_append_page(GTK_NOTEBOOK (notebook), frame, label);


        /* Set up the icon, if one has been defined. */
        if(config.flags & CONFIG_ICON_XPM && config.icon_xpm)
                gdk_window_set_icon(window->window, NULL, 
                                    config.icon_xpm, config.icon_mask);


	/* Show the whole kit and kaboodle */
	gtk_widget_show(window);

	/* And go into the gtk loop - why does this feel like the first 
	   plunge in a roller coaster after the big hill at the beginning? */
	gtk_main();

	return 0;
}
