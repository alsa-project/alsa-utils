/*
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/asoundlib.h>
#include "amixer.h"


Mixer::Mixer(int card, int device)
{
	mixer_handle = NULL;	

	if (snd_mixer_open(&mixer_handle, card, device) < 0) {
		fprintf(stderr, "Can't access mixer %i/%i\n", card+1, device);
		mixer_status = ~E_MIXER_SUCCESS;
		return;
	}
	mixer_status = E_MIXER_SUCCESS;
	mixer_status |= E_MIXER_NEED_CLOSE;			
}


bool Mixer::Open(int card, int device)
{
	Close();
	if (snd_mixer_open(&mixer_handle, card, device) < 0) {
		fprintf(stderr, "Can't access mixer %i/%i\n", card + 1, device);
		mixer_status = ~E_MIXER_SUCCESS;
	} else{
		mixer_status = E_MIXER_SUCCESS;
		mixer_status |= E_MIXER_NEED_CLOSE;			
	}
	return Init();
}


void Mixer::Close()
{
	if (mixer_handle != NULL && mixer_status & E_MIXER_NEED_CLOSE) {
		snd_mixer_close(mixer_handle);
	}
	mixer_handle = NULL;
	mixer_status = ~E_MIXER_SUCCESS;
}


Mixer::~Mixer()
{
	Close();
}


bool Mixer::Init()
{
	if (!(mixer_status & E_MIXER_SUCCESS)) 
		return false;
	if ((nr_devices = snd_mixer_channels(mixer_handle)) < 0)
		return false;

	return true;
}
 


char* Mixer::Name()
{
	return Name(current_device);
}

char* Mixer::Name(int32 device)
{
	if (snd_mixer_channel_info(mixer_handle,device,&ch_info) < 0)
		return "Unknown";
	return (char *)ch_info.name;
}

void Mixer::Update()
{
	if(snd_mixer_channel_output_read(mixer_handle, current_device, &ch_data) < 0) {
		fprintf(stderr, "Can't read data from channel %i\n", current_device);
		return;		/* No fail code? */
	}
}

void Mixer::DeviceRead(int32 device, int32 *left, int32 *right, int32 *flags)
{
	current_device = device;
	Update();
	*left = ch_data.left;
	*right = ch_data.right;
	*flags = ch_data.flags;
}


void Mixer::DeviceWrite(int32 device, int32 left, int32 right, int32 flags)
{
	current_device = device;
	ch_data.channel = device;
	ch_data.left = left;
	ch_data.right = right;
	ch_data.flags = flags;
	if(snd_mixer_channel_output_write(mixer_handle, device, &ch_data) < 0) {
		fprintf(stderr, "Can't write data to channel %i\n", device);
		return;		/* No fail code? */
	}
}

