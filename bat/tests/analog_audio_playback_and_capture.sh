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

#get Analog audio card number
card_number=$(aplay -l | grep "Analog" | cut -b 6)
if [ "$card_number" = "" ]; then
        echo "Can not get Analog card number."
        exit 1
fi

#get Analog audio device number
device_number=$(aplay -l | grep "Analog"| cut -d " " -f 8 |cut -b 1)
if [ "$device_number" = "" ]; then
        echo "Can not get Analog device number"
        exit 1
fi


device="hw:$card_number,$device_number"
echo $device

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

#Notice: to loopback the analog audio output to the analog audio input
record_device="hw:$record_card_number,$record_device_number"
test_flag=0

echo  -e "\e[31m Notice: to loopback the analog audio output to \
the analog audio input"
echo -e "\e[0m"
read -p "Press enter to continue"

#call alsabat to do the test for each frequency in the freq_table
for freq in $freq_table
	do
	alsabat -P $device -C plug$record_device -c $test_channel -F $freq
		if [ $? = 0 ]; then
			echo "Test target frequency:$freq for Analog playback -- Passed \
" >> $ABAT_TEST_LOG_FILE
			echo "Test target frequency:$freq for Analog capture -- Passed \
" >> $ABAT_TEST_LOG_FILE
		else
			echo "Test target frequency:$freq for Analog playback -- Failed \
" >> $ABAT_TEST_LOG_FILE
			echo "Test target frequency:$freq for Analog capture -- Failed \
" >> $ABAT_TEST_LOG_FILE
			test_flag=1
		fi
	done

exit $test_flag
