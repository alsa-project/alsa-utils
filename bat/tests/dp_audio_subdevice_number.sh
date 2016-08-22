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

#make sure the DP monitor is connected and active

# To get DisplayPort audio device number
card_number=$(aplay -l | grep "HDMI 1" | cut -b 6)
echo $card_number
if [ "$card_number" = "" ]; then
	echo "Can not get Display audio card."
	exit 254
fi

audio_card_dir="/proc/asound/card$card_number/"

cd $audio_card_dir

for file in `ls`
do
	#To get the ELD info according to the connented monitor with DisplayPort.
	if [[ $file == eld* ]]; then
		let subdevice_number+=1
		cat $file | grep connection_type | grep DisplayPort > /dev/null
		if [ $? = 0 ]; then
			echo "Get the ELD information according to the connented \
monitor with DisplayPort."
			get_subdevice=1
			break
		fi
	fi
done

#failed to get the subdevice number of DisplayPort audio
if [ $get_subdevice == 0 ]; then
        exit 77
fi

#the subdevice number of DisplayPort audio is 3
if [ $subdevice_number == 1 ]; then
	exit 3
#the subdevice number of DisplayPort audio is 7.
elif [ $subdevice_number == 2 ]; then
	exit 7
#the subdevice number of DisplayPort audio is 8
elif [ $subdevice_number == 3 ]; then
	exit 8
#default: failed to get the subdevice number of DisplayPort audio
else
	exit 77
fi
