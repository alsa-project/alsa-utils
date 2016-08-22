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

#set test freq table (HZ)
freq_table="10 31 73 155 380 977 1932 4119 8197 16197"

#set test number of channels
test_channel=2

#get device number for DP
DP_device_num=0
$ABAT_TEST_PATH/dp_audio_subdevice_number.sh
DP_device_num=$?
if [ $DP_device_num = 77 ]; then
	echo "Prompt: Can not get device with DP audio or \
show the wrong connection type as HDMI in ELD info"
	exit 1
fi

#To get DP audio device number
DP_card_number=$(aplay -l | grep "HDMI 0" | cut -b 6)
if [ "$DP_card_number" = "" ]; then
	echo "Error: Can not get Display audio card."
	exit 1
fi

DP_device="hw:$DP_card_number,$DP_device_num"
echo $device
sleep 2

#get Analog audio record card number
record_card_number=$(arecord -l | grep "Analog" | cut -b 6)
if [ "$record_card_number" = "" ]; then
	echo "Can not get record card number."
	exit 1
fi

#get Analog audio record device number
record_device_number=$(arecord -l | grep "Analog"| cut -d " " -f 8 |cut -b 1)
echo $record_device_number
if [ "$record_device_number" = "" ]; then
        echo "Can not get record device number"
        exit 1
fi

#Notice: to loopback the DP audio output to the analog audio input
record_device="hw:$record_card_number,$record_device_number"
test_flag=0

echo -e "\e[31m Notice: to loopback the DP audio \
output to the analog audio input"
echo -e "\e[0m"
read -p "Press enter to continue"

#call alsabat to do the test for each frequency in the freq_table
for freq in $freq_table
	do
		alsabat -P $DP_device -C plug$record_device -c $test_channel -F $freq
		if [ $? = 0 ]; then
			echo "Test target frequency:$freq for DP audio playback--Passed" \
>> $ABAT_TEST_LOG_FILE
		else
			echo "Test target frequency:$freq for DP audio playback--Failed" \
>> $ABAT_TEST_LOG_FILE
			test_flag=1
		fi
	done

exit $test_flag
