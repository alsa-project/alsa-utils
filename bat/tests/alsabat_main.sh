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

#alsabat test scripts path
export ABAT_TEST_PATH=`pwd`

#alsabat test log file, path+filename
Day=`date +"%Y-%m-%d-%H-%M"`
Log_FileName="test_result-"${Day}".log"
export ABAT_TEST_LOG_FILE=${ABAT_TEST_PATH}/log/${Log_FileName}

#terminal display colour setting
ESC_GREEN="\033[32m"
ESC_RED="\033[31m"
ESC_YELLOW="\033[33;1m"
ESC_OFF="\033[0m"

#total/pass/fail test cases number
total_case_number=0
suit_number=1
pass_number=0
fail_number=0
# =========================== Public function  ==========================

function get_platform_info()
{
	#to get the audio card number
	Card_Number=$(aplay -l | grep "HDMI 0" | cut -b 6)
	cd /proc/asound/card$Card_Number/
	for file in `ls`
	do
		if [[ $file == codec* ]]; then
			#to get the hardware platform ID, currently Intel skylake,
			#broadwell and haswell hardware platforms are supported
			Platform_ID=`cat $file |grep "Codec:" |cut -d " " -f 3`
			if [ "$Platform_ID" == "Skylake" ] \
				|| [ "$Platform_ID" == "Broadwell" ] \
				|| [ "$Platform_ID" == "Haswell" ]; then
				echo $Platform_ID
				break
				exit 0
			fi
		else
			printf '\033[1;31m %-30s %s \033[1;31m%s\n\033[0m' \
						"Get platform information failed";
			exit 1
		fi
	done
}

#printf the "pass" info in the file
show_pass()
{
	echo -e "$suit_number - [$1]:test ------- PASS" >> $ABAT_TEST_LOG_FILE
	printf '\033[1;33m %-30s %s \033[1;32m%s\n\033[0m' \
"$suit_number -	[$1]" "--------------------------------  " "PASS";
}

#printf the "fail" info in the file
show_fail()
{
	echo -e "$suit_number - [$1]:test ------- FAIL" >> $ABAT_TEST_LOG_FILE
	printf '\033[1;33m %-30s %s \033[1;31m%s\n\033[0m' \
"$suit_number - [$1]" "--------------------------------  " "FAIL";
}


function run_test()
{
	for TestItem in $@
		do
			Date=`date`
			Dot="$Dot".
			echo "Now doing $TestItem test$Dot"

			#map test case to test script
			eval item='$'$TestItem

			#to check the test script existing
			if  [ ! -f "$item" ]; then
				echo -e "\e[31m not found $TestItem script,confirm it firstly"
				echo -e "\e[0m"
				exit 1
			fi

			#to run each test script
			eval "\$$TestItem"
			Result=$?
			#record the test result to the log file
			if [ $Result -eq 0 ]; then
				show_pass "$TestItem"
			else
				show_fail "$TestItem"
			fi
			suit_number=$(($suit_number + 1))

		done
}

function test_suites ( )
{
	#define the test suites/cases need to be run
	TestProgram="verify_Analog_audio_playback_and_capture \
				verify_HDMI_audio_playback verify_DP_audio_playback"

	#run each test suites/test cases
	run_test "$TestProgram"

	# to printf the detailed test results on the screen
	cat $ABAT_TEST_LOG_FILE |grep FAIL
	case_number=$(($case_number - 1))
	total_case_number=`cat $ABAT_TEST_LOG_FILE |grep -c "Test target frequency:"`
	pass_number=`cat $ABAT_TEST_LOG_FILE |grep -c "Passed"`
	fail_number=`cat $ABAT_TEST_LOG_FILE |grep -c "Failed"`
	echo -e "\e[0m"
	echo -e "\e[1;33m *---------------------------------------------------*\n"
	echo -e " * "Total" ${total_case_number} "cases", \
"PASS:" ${pass_number} "cases", "FAIL:" ${fail_number} "cases", \
"Passrate is:" $((pass_number*100/total_case_number)) "%" *\n"
	echo -e " *-------------------------------------------------------*\e[0m\n"

	#the the result also will be saved on the log file
	echo "Total" ${total_case_number} "cases", \
"PASS:" ${pass_number} "cases", "FAIL:" ${fail_number} "cases",  \
"Passrate:" $((pass_number*100/total_case_number)) "%" >> ${ABAT_TEST_LOG_FILE}

	#return 0, if the script finishs normally
	exit 0
}

function main ( )
{
	echo "Test results are as follows:" > ${ABAT_TEST_LOG_FILE}
	get_platform_info # get hardware platform information
	cd $ABAT_TEST_PATH

	# make sure the log folder is exist
	if [ ! -d "$ABAT_TEST_PATH/log/" ]; then
		mkdir "log"
	fi

	#map the test cases to test scripts
	source map_test_case

	#setting the alsa configure environment
	alsactl restore -f $ABAT_TEST_PATH/asound_state/asound.state.$Platform_ID

	#Printf the user interface info
	clear
	echo -e "\e[1;33m"
	date
	echo -e "\e[0m"
	echo -e "\e[1;33m *-------------------------------------------------*\n"
	echo -e " *--Running the audio automated test on $Platform_ID-------*\n"
	echo -e " *------------------------------------------------------*\e[0m\n"
	read -p "Press enter to continue"

	#run the test suites/test cases
	test_suites
}

#the main entrance function
main
