/*****************************************************************************
   cinit.c - routines to initialize the mixer devices
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

extern Card *card; /* And array of the cards */
extern int cards; /* The number of cards in the system. */
extern Config config; /* The system config */

/* End Global Variables */
/*****************************************************************************/

/*****************************************************************************/
/* Begin function prototypes */

int init_group(void *handle, Group *group);
int init_element_route(void *handle, snd_mixer_routes_t *routes, snd_mixer_eid_t *eid);
int misc_group_hack(Mixer *mixer, int index);

/* End function prototypes */
/*****************************************************************************/

int init_cards()
{
	int i,j,k;
	int err;
	snd_ctl_t *handle;

	cards = snd_cards();
	card = calloc(cards, sizeof(Card));
	for(i = 0; i < cards; i++) {
		/* Open the hardware */
		if((err = snd_ctl_open(&handle, i)) < 0) {
			printf("Unable to open card #%i!\nError: %s\n", i, snd_strerror(err));
			card[i].mixer = NULL;
			card[i].number = -1;
			continue;
		} else {
			card[i].number = i;
		}


		/* Get the hardware info - the primary use of this is to find out how many
		 mixer devices the card has, but it's also useful to find out the human-readable
		name of the card. */
		if((err = snd_ctl_hw_info(handle, &card[i].hw_info)) < 0) {
			printf("Unable to get hardware information about card #%i!\nError: %s\n", 
			       i, snd_strerror(err));
			printf("Trying to guess the appropriate values.\n");
			card[i].hw_info.mixerdevs = 1;
		}

		
		/* Allocate out the mixer array */
		card[i].mixer = calloc(card[i].hw_info.mixerdevs, sizeof(Mixer));

		for(j = 0; j < card[i].hw_info.mixerdevs; j++) {
			/* Open the mixer to begin with.  Isn't it funny how there's all this
			   nice generalized code that can handle nearly any situation, and it
			   will be necessary in only a very small percentage of the situations. 
			   Oh well, I guess that that's what distinguishes us from windows. :-) */
			if((err = snd_mixer_open(&card[i].mixer[j].handle, i, j)) < 0) {
				printf("Unable to open mixer #%i on card #%i~\nError: %s\n",
				       j, i, snd_strerror(err));
				card[i].mixer[j].number = -1;
			} else {
				card[i].mixer[j].number = j;
			}

			/* Get the mixer info */
			if((err = snd_mixer_info(card[i].mixer[j].handle, &card[i].mixer[j].info)) < 0) {
				printf("Unable to get the info for mixer #%i on card %i!  Error: %s\n", j, i, snd_strerror(err));
				printf("There's not much more I can do on this mixer.");
				printf("  Shutting it down.\n");
				if((err = snd_mixer_close(card[i].mixer[j].handle)) < 0) {
					printf("Oh well.  I couldn't even close the mixer.  I suspect that something is seriously wrong here.  Good luck.\n");
				}
				card[i].mixer[j].number = -1;
				continue;
			}

			bzero(&card[i].mixer[j].groups, sizeof(snd_mixer_groups_t));
			
			if ((err = snd_mixer_groups(card[i].mixer[j].handle, 
						    &card[i].mixer[j].groups)) < 0) {
				printf("Mixer %i/%i groups error: %s", 
				      i, j, snd_strerror(err));
				return -1;
			}
			
			/* Allocate the space for the group array */
			card[i].mixer[j].groups.pgroups = (snd_mixer_gid_t *)
				calloc(card[i].mixer[j].groups.groups_over, 
				       sizeof(snd_mixer_gid_t));

			if (!card[i].mixer[j].groups.pgroups) {
				printf("No enough memory");
				return -1;
			}

			card[i].mixer[j].groups.groups_size = card[i].mixer[j].info.groups;
			card[i].mixer[j].groups.groups_over = card[i].mixer[j].groups.groups = 0;
			if ((err = snd_mixer_groups(card[i].mixer[j].handle, 
						    &card[i].mixer[j].groups)) < 0) {
				printf("Mixer %i/%i groups (2) error: %s", 
				      i, j, snd_strerror(err));
				return -1;
			}

			/* Allocate the space for the array of the groups - this is more than
			   just their gid's, it's got group-specific info in it */
			card[i].mixer[j].group = calloc(card[i].mixer[j].info.groups + 1,
							sizeof(Group));
			
			/* get the group structures filled out */
			for(k = 0; k < card[i].mixer[j].info.groups; k++) {
				card[i].mixer[j].group[k].group.gid = 
					card[i].mixer[j].groups.pgroups[k];

				init_group(card[i].mixer[j].handle, 
					   &card[i].mixer[j].group[k]);
				
			}
			misc_group_hack(&card[i].mixer[j], k);

		}

		
		if((err = snd_ctl_close(handle)) < 0) {
			printf("strange, there was an error closing card #%i!\nError: %s\n", 
			       i, snd_strerror(err));
			printf("Oh well.\n");
		}
	}


	/* return a successful execution. */
	return 0;
}

int misc_group_hack(Mixer *mixer, int index)
{
	/* This code is largely copied straight from amixer. - God I love the GPL. */
	snd_mixer_elements_t elements;
	snd_mixer_eid_t *element;
	snd_mixer_group_t *group;
	int err, idx, gdx, idx2;
	int flag;
	int count=0; /* The count of elements not in any group */
	snd_mixer_eid_t **array;

	bzero(&elements, sizeof(elements));
	if ((err = snd_mixer_elements(mixer->handle, &elements)) < 0) {
		printf("Mixer elements error: %s", snd_strerror(err));
		return -1;
	}
	elements.pelements = (snd_mixer_eid_t *)malloc(elements.elements_over * 
						       sizeof(snd_mixer_eid_t));
	if (!elements.pelements) {
		printf("Not enough memory");
		return -1;
	}
	elements.elements_size = elements.elements_over;
	elements.elements_over = elements.elements = 0;
	if ((err = snd_mixer_elements(mixer->handle, &elements)) < 0) {
		printf("Mixer elements (2) error: %s", snd_strerror(err));
		return -1;
	}

	/* Allocate the temporary array to hold the mixer ID structs */
	array = malloc(elements.elements * sizeof(snd_mixer_eid_t *));
	if(!array)
		printf("Not enough memory.\n");


	for (idx = 0; idx < elements.elements; idx++) {
		element = &elements.pelements[idx];
		flag = 0; /* The flag will be set if the same element name & type 
			     is encountered */
		for(gdx = 0; gdx < mixer->info.groups; gdx++) {
			group = &mixer->group[gdx].group;
			for(idx2 = 0; idx2 < group->elements; idx2++) {
				if(group && element)
					if(group->pelements[idx2].type == element->type && 
					   is_same(group->pelements[idx2].name, element->name))
						flag = 1;
			}
		}
		if(!flag) {
			/* We found a mixer element that's not in a group */
			array[count] = element;
			count++;
			if(count > elements.elements)
				printf("Houston, we have a problem.\n");
		}
	}

	/* Set up the group member */
	strncpy(mixer->group[index].group.gid.name, "Miscellaneous\0", 24);
	mixer->group[index].group.gid.index = 0;
	mixer->group[index].group.elements_size = 0; /* I hope that this doesn't matter */
	mixer->group[index].group.elements = count;
	mixer->group[index].group.elements_over = 0; /* I hope tha this doesn't matter */


	mixer->group[index].group.pelements = (snd_mixer_eid_t *)malloc(count * 
									sizeof(snd_mixer_eid_t));
	mixer->group[index].routes = calloc(mixer->group[index].group.elements, 
					    sizeof(snd_mixer_routes_t));
	mixer->group[index].element = calloc(mixer->group[index].group.elements, 
				      sizeof(snd_mixer_element_t));
	mixer->group[index].einfo = calloc(mixer->group[index].group.elements, 
				    sizeof(snd_mixer_element_info_t));
	mixer->group[index].gtk = calloc(mixer->group[index].group.elements, 
				  sizeof(Gtk_Channel));
	/* Copy the snd_mixer_eid_t structures into the new group structure and init the routes */

	for(idx = 0; idx < count; idx++) {
		mixer->group[index].group.pelements[idx] = *array[idx];

		mixer->group[index].einfo[idx].eid = mixer->group[index].group.pelements[idx];
		if(snd_mixer_element_has_info(&mixer->group[index].group.pelements[idx]) == 1) 
			if((err = 
			    snd_mixer_element_info_build(mixer->handle, 
							 &mixer->group[index].einfo[idx])) < 0) {
				printf("Unable to get element information for element %s!  ",
				       mixer->group[index].group.pelements[idx].name);
				printf("Error: %s.\n", snd_strerror(err));
			}
					
		mixer->group[index].element[idx].eid = mixer->group[index].group.pelements[idx];
		if(snd_mixer_element_has_control(&mixer->group[index].element[idx].eid))
			if((err = snd_mixer_element_build(mixer->handle, 
							&mixer->group[index].element[idx])) < 0) {
				printf("Unable to read element %s!  ",
						mixer->group[index].group.pelements[idx].name);
				printf("Error: %s.\n", snd_strerror(err));
			}

		init_element_route(mixer->handle, 
				   &mixer->group[index].routes[idx], 
				   &mixer->group[index].group.pelements[idx]);
	}



	

	/* Increase the number of groups to include the new group */
	mixer->info.groups++;

	if(elements.pelements)
		free(elements.pelements);
	if(array)
		free(array);

	return 1;
}


int init_group(void *handle, Group *group)
{
	/* This is largely a mess copied from amixer that gets the group info in a very strange
	   way, I wish that I knew how it really worked.  Anyhow, once we get the group into
	   and the info about the elements in the group, we'll set up the element array. */
	int idx, err;
	
	if((err = snd_mixer_group(handle, 
				  &group->group)) < 0) {
		printf("Unable to get info for group %s!  ", group->group.gid.name);
		printf("Error: %s\n", snd_strerror(err));
		printf("elements_size = %i, elements_over=%i, elements=%i\n",
		       group->group.elements_size, 
		       group->group.elements_over,
		       group->group.elements);
		return 0;
	}
	group->group.pelements = (snd_mixer_eid_t *)calloc(group->group.elements_over, 
							   sizeof(snd_mixer_eid_t));
	if (!group->group.pelements) {
		printf("Not enough memory...");
		return 0;
	}
	group->group.elements_size = group->group.elements_over;
	group->group.elements = group->group.elements_over = 0;
	if ((err = snd_mixer_group(handle, &group->group)) < 0) {
		printf("Unable to get second group info for group %s.  Error: %s\n", 
		       group->group.gid.name, snd_strerror(err));
		printf("elements_size = %i, elements_over=%i, elements=%i\n",
		       group->group.elements_size, 
		       group->group.elements_over,
		       group->group.elements);
		return 0;
	}


	/* Allocate out the arrays for the elements and element info */
	group->element = calloc(group->group.elements, sizeof(snd_mixer_element_t));
	group->einfo = calloc(group->group.elements, sizeof(snd_mixer_element_info_t));
	group->routes = calloc(group->group.elements, sizeof(snd_mixer_routes_t));
	group->gtk = calloc(group->group.elements, sizeof(Gtk_Channel));

	/* Now go through and get that info */
	for (idx = 0; idx < group->group.elements; idx++) {
		group->einfo[idx].eid = group->group.pelements[idx];
		if(snd_mixer_element_has_info(&group->group.pelements[idx]) == 1) 
			if((err = snd_mixer_element_info_build(handle, &group->einfo[idx])) < 0) {
				printf("Unable to get element information for element %s!  ",
				       group->group.pelements[idx].name);
				printf("Error: %s.\n", snd_strerror(err));
			}
					
		group->element[idx].eid = group->group.pelements[idx];
		if((err = snd_mixer_element_build(handle, &group->element[idx])) < 0) {
			printf("Unable to read element %s!  ",
			       group->group.pelements[idx].name);
			printf("Error: %s.\n", snd_strerror(err));
		}

		init_element_route(handle, &group->routes[idx], &group->group.pelements[idx]);

	}


	return 1;
}


int init_element_route(void *handle, snd_mixer_routes_t *routes, snd_mixer_eid_t *eid)
{
	int err, idx;
	/* Most of this code is taken straight from amixer as well. */
	/* This just gets the routes for the mixer element and stores them. */

	bzero(routes, sizeof(snd_mixer_routes_t));
	routes->eid = *eid;
	if ((err = snd_mixer_routes(handle, routes)) < 0) {
		printf("Element %s route error: %s", eid->name, snd_strerror(err));
		return -1;
	}
	if (!routes->routes_over)
		return 0;
	routes->proutes = (snd_mixer_eid_t *)malloc(routes->routes_over * 
						    sizeof(snd_mixer_eid_t));
	if (!routes->proutes) {
		printf("No enough memory...");
		return -1;
	}
	routes->routes_size = routes->routes_over;
	routes->routes = routes->routes_over = 0;
	if ((err = snd_mixer_routes(handle, routes)) < 0) {
		printf("Element (2) %s route error: %s", eid->name, snd_strerror(err));
		return -1;
	}

	return 1;
}

