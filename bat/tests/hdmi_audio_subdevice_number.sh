#!/bin/bash

#/*
# * Copyright (C) 2013-2016 Intel Corporation
# *
# * This program is free software; you can redistribute it and/or modify
# * it under the terms of the GNU General Public License as published by
# * the Free Software Foundation; either version 2 of the License, or
# * (at your option) any later version.
# *
# * This program is distributed in the hope that it will be useful,
# * but WITHOUT ANY WARRANTY; without even the implied warranty of
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# * GNU General Public License for more details.
# *
# */
#set -x

subdevice_number=0
get_subdevice=0

#make sure the HDMI monitor is connected and active ########

# To get HDMI audio device number
card_number=$(aplay -l | grep "HDMI 0" | cut -b 6)
if [ "$card_number" = "" ]; then
	echo "Can not get Display audio card."
	#failed to get Display audio card.
	exit 1
fi

audio_card_dir="/proc/asound/card$card_number/"

cd $audio_card_dir
for file in `ls`
	do
		#To get the ELD information according to the connented monitor with HDMI
		if [[ $file == eld* ]]; then
			let subdevice_number+=1
			cat $file | grep connection_type | grep HDMI > /dev/null
			if [ $? = 0 ]; then
				get_subdevice=1
				break
			fi
		fi
	done

#failed to get the subdevice number of HDMI audio.
if [ $get_subdevice == 0 ]; then
	exit 77
fi

#the subdevice number of HDMI audio is 3.
if [ $subdevice_number == 1 ]; then
	exit 3
#the subdevice number of HDMI audio is 7.
elif [ $subdevice_number == 2 ]; then
	exit 7
#the subdevice number of HDMI audio is 8.
elif [ $subdevice_number == 3 ]; then
	exit 8
#default: failed to get the subdevice number of HDMI audio.
else
	exit 77
fi
