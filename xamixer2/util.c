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
extern Config config; /* The system config */


int is_same(char *string1, char *string2)
{
	int i = 0;
	
	if(strlen(string1) != strlen(string2))
		return 0;
	
	while(string1[i] != '\0') 
		if(string1[i] != string2[i])
			return 0;
		else
			i++;
	
	return 1;
	
}

void strip_comment(char *string)
{
  char *place;
  int i = 0, j = 0, size;

  size = strlen(string);
  /* Get wrid of the comments */
  place = string;
  while((place = strchr(place, '#'))){
    if(string[(place - string) -1] != '\\')
      *place = '\0';
    place++;
  }

  /* Replace the escape sequences */
  place = calloc(1, strlen(string));
  while(string[i] != '\0'){
    if(string[i] == '\\')
      place[j] = string[++i];
    else
      place[j] = string[i];

    i++;
    j++;
  }

  EAZERO(string, size);
  strncpy(string, place, size);
  free(place);
  return;
}

int is_comment(char *string)
{
  int i=0;

  while (string[i] != '\0'){
    if (string[i] == '#')
      return 1;
    if (string[i] != ' ' && string[i] != '\t')
      return 0;
    i++;
  }
  return 0;
}

ChannelLabel *channel_label_append(ChannelLabel *head, char *channel, char *label)
{
  ChannelLabel *tmp;

  tmp = calloc(1, sizeof(ChannelLabel));

  tmp->next = head;
  tmp->channel = calloc(strlen(channel) + 1, sizeof(char));
  strcpy(tmp->channel, channel);
  tmp->label = calloc(strlen(label) + 1, sizeof(char));
  strcpy(tmp->label, label);

  return tmp;
}

int get_label(char *line, char *expect, char *value1, size_t value1_len, 
	      char *value2, size_t value2_len, char quote1, char quote2)
{
  char *tmp;
  int len, i;

  len = strlen(line);

  if(expect) {
    tmp = strstr(line, expect);
    if(!tmp)
      return FALSE;
    tmp = &tmp[strlen(expect)];
  }
  else
    tmp = line;


  tmp = strchr(tmp, quote1) + 1;
  if(!tmp)
    return FALSE;
  for(i = 0; i < (value1_len - 1) && tmp[i] != quote2; i++) 
    value1[i] = tmp[i];
  value1[i] = '\0';

  tmp = strchr(tmp, quote1) + 1;
  if(!tmp)
    return FALSE;
  for(i = 0; i < (value2_len - 1) && tmp[i] != quote2; i++)
    value2[i] = tmp[i];
  value2[i] = '\0';



  return TRUE;
}

MixerInfo *create_mixer_info(Mixer *mixer, int num, unsigned int flags)
{
  MixerInfo *info;
  info = calloc(1, sizeof(MixerInfo));

  info->mixer = mixer;
  info->channel = num;
  info->flags = flags;
  info->other = NULL;
  info->mute = NULL;
  info->unmute = NULL;

  return info;
}

CBData *create_cb_data(Group *group, void *handle, int element, int index)
{
	CBData *data;
	
	data = malloc(sizeof(CBData));

	data->group = group;
	data->handle = handle;
	data->element = element;
	data->index = index;

	return data;
}
