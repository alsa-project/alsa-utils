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

#include "atypes.h"

#define		E_MIXER_SUCCESS		1
#define		E_MIXER_NEED_CLOSE	2

/* FIXME */
#define 	E_MIXER_RECORD		0

#define 	E_MIXER_MUTE_LEFT	SND_MIXER_DFLG_MUTE_LEFT
#define		E_MIXER_MUTE_RIGHT	SND_MIXER_DFLG_MUTE_RIGHT
#define		E_MIXER_MUTE		SND_MIXER_DFLG_MUTE


class Mixer 
{
public:	
			Mixer(int card = 0, int device = 0);
			~Mixer();
	bool		Init();
	bool		Open(int card, int device);
	void		Close();
	void		DeviceSet(int32 device) {
				 current_device = device;
				 Update();
			}
	char*		Name(int32 device);
	char*		Name();
	int32		NumDevices() { return nr_devices; }
	void		Update();
	void 		DeviceRead(int32 device, int32 *left, int32 *right, int32 *flag);
	void 		DeviceWrite(int32 device, int32 left, int32 right, int32 flag);
	void 		Read(int32 *left, int32 *right, int32 *flags) {
				DeviceRead(current_device, left, right, flags);
			}
	void		Read_dB(int32 *left_dB, int32 *right_dB) {
				*left_dB = ch_data.left_dB;
				*right_dB = ch_data.right_dB;
			}
	void 		Write(int32 left, int32 right, int32 flags) { 
				DeviceWrite(current_device, left, right, flags);
			}
	int		Left() { return ch_data.left; }
	int		Right() { return ch_data.right; }
	Mixer& 		operator[](int32 device) {
				DeviceSet(device);
				return (*this);
			}
private:
	snd_mixer_info_t	 info;
        snd_mixer_channel_direction_t	 ch_data;
     	snd_mixer_channel_info_t ch_info;
     	
	void *		mixer_handle;
	int32		mixer_status;
	int32		current_device;
	int32		nr_devices;
};
