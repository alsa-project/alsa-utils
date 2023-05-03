#!/bin/bash

SCRIPT_VERSION=0.5.3
CHANGELOG='https://www.alsa-project.org/alsa-info.sh.changelog'

#################################################################################
#Copyright (C) 2007 Free Software Foundation.

#This program is free software; you can redistribute it and/or modify
#it under the terms of the GNU General Public License as published by
#the Free Software Foundation; either version 2 of the License, or
#(at your option) any later version.

#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#GNU General Public License for more details.

#You should have received a copy of the GNU General Public License
#along with this program; if not, write to the Free Software
#Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

##################################################################################

# The script was written for 2 main reasons:
#  1. Remove the need for the devs/helpers to ask several questions before we can easily help the user.
#  2. Allow newer/inexperienced ALSA users to give us all the info we need to help them.

#Set the locale (this may or may not be a good idea.. let me know)
export LC_ALL=C

# Change the PATH variable, so we can run lspci (needed for some distros)
PATH="$PATH:/bin:/sbin:/usr/bin:/usr/sbin"
BGTITLE="ALSA-Info v $SCRIPT_VERSION"
PASTEBINKEY='C9cRIO8m/9y8Cs0nVs0FraRx7U0pHsuc'

WGET="$(command -v wget)"
REQUIRES=(mktemp grep pgrep awk date uname cat sort dmesg amixer alsactl)

#
# Define some simple functions
#

update() {
	test -z "$WGET" || test ! -x "$WGET" && return

	SHFILE=$(mktemp -t alsa-info.XXXXXXXXXX) || exit 1
	wget -O $SHFILE "https://www.alsa-project.org/alsa-info.sh" >/dev/null 2>&1
	REMOTE_VERSION=$(grep SCRIPT_VERSION $SHFILE | head -n1 | sed 's/.*=//')
	if [ -s "$SHFILE" ] && [ "$REMOTE_VERSION" != "$SCRIPT_VERSION" ]; then
		if [[ -n $DIALOG ]]
		then
			OVERWRITE=
			if [ -w $0 ]; then
				dialog --yesno "Newer version of ALSA-Info has been found\n\nDo you wish to install it?\nNOTICE: The original file $0 will be overwritten!" 0 0
				DIALOG_EXIT_CODE=$?
				if [[ $DIALOG_EXIT_CODE = 0 ]]; then
				  OVERWRITE=yes
				fi
			fi
			if [ -z "$OVERWRITE" ]; then
				dialog --yesno "Newer version of ALSA-Info has been found\n\nDo you wish to download it?" 0 0
				DIALOG_EXIT_CODE=$?
			fi
			if [[ $DIALOG_EXIT_CODE = 0 ]]
			then
				echo "Newer version detected: $REMOTE_VERSION"
				echo "To view the ChangeLog, please visit $CHANGELOG"
				if [ "$OVERWRITE" = "yes" ]; then
					cp $SHFILE $0
					echo "ALSA-Info script has been updated to v $REMOTE_VERSION"
					echo "Please re-run the script"
					rm $SHFILE 2>/dev/null
				else
					echo "ALSA-Info script has been downloaded as $SHFILE."
					echo "Please re-run the script from new location."
				fi
				exit
			else
				rm $SHFILE 2>/dev/null
			fi
		else
			echo "Newer version detected: $REMOTE_VERSION"
			echo "To view the ChangeLog, please visit $CHANGELOG"
			if [ -w $0 ]; then
				echo "The original file $0 will be overwritten!"
				echo -n "If you do not like to proceed, press Ctrl-C now.." ; read inp
				cp $SHFILE $0
				echo "ALSA-Info script has been updated. Please re-run it."
				rm $SHFILE 2>/dev/null
			else
				echo "ALSA-Info script has been downloaded $SHFILE."
				echo "Please, re-run it from new location."
			fi
			exit
		fi
	else
		rm $SHFILE 2>/dev/null
	fi
}

cleanup() {
	if [ -n "$TEMPDIR" ] && [ "$KEEP_FILES" != "yes" ]; then
		rm -rf "$TEMPDIR" 2>/dev/null
	fi
	test -n "$KEEP_OUTPUT" || rm -f "$NFILE"
}


withaplay() {
        echo "!!Aplay/Arecord output" >> "$FILE"
        echo "!!--------------------" >> "$FILE"
        echo "" >> "$FILE"
       	echo "APLAY" >> "$FILE"
	echo "" >> "$FILE"
	aplay -l >> "$FILE" 2>&1
        echo "" >> "$FILE"
       	echo "ARECORD" >> "$FILE"
	echo "" >> "$FILE"
	arecord -l >> "$FILE" 2>&1
	echo "" >> "$FILE"
}

withmodules() {
	echo "!!All Loaded Modules" >> "$FILE"
	echo "!!------------------" >> "$FILE"
	echo "" >> "$FILE"
	awk '{print $1}' < /proc/modules | sort >> "$FILE"
	echo "" >> "$FILE"
	echo "" >> "$FILE"
}

withamixer() {
        echo "!!Amixer output" >> "$FILE"
        echo "!!-------------" >> "$FILE"
        echo "" >> "$FILE"
	for f in /proc/asound/card*/id; do
		[ -f "$f" ] && read -r CARD_NAME < "$f" || continue
		echo "!!-------Mixer controls for card $CARD_NAME" >> "$FILE"
		echo "" >> "$FILE"
		amixer -c "$CARD_NAME" info >> "$FILE" 2>&1
		amixer -c "$CARD_NAME" >> "$FILE" 2>&1
		echo "" >> "$FILE"
	done
	echo "" >> "$FILE"
}

withalsactl() {
	echo "!!Alsactl output" >> "$FILE"
        echo "!!--------------" >> "$FILE"
        echo "" >> "$FILE"
	alsactl -f "$TEMPDIR/alsactl.tmp" store
	echo "--startcollapse--" >> "$FILE"
	cat "$TEMPDIR/alsactl.tmp" >> "$FILE"
	echo "--endcollapse--" >> "$FILE"
	echo "" >> "$FILE"
	echo "" >> "$FILE"
}

withdevices() {
        echo "!!ALSA Device nodes" >> "$FILE"
        echo "!!-----------------" >> "$FILE"
        echo "" >> "$FILE"
        ls -la /dev/snd/* >> "$FILE"
        echo "" >> "$FILE"
        echo "" >> "$FILE"
}

withconfigs() {
if [[ -e "$HOME/.asoundrc" ]] || [[ -e "/etc/asound.conf" ]] || [[ -e "$HOME/.asoundrc.asoundconf" ]]; then
        echo "!!ALSA configuration files" >> "$FILE"
        echo "!!------------------------" >> "$FILE"
        echo "" >> "$FILE"

        #Check for ~/.asoundrc
        if [[ -e "$HOME/.asoundrc" ]]
        then
                echo "!!User specific config file (~/.asoundrc)" >> "$FILE"
                echo "" >> "$FILE"
                cat "$HOME/.asoundrc" >> "$FILE"
                echo "" >> "$FILE"
                echo "" >> "$FILE"
        fi
	#Check for .asoundrc.asoundconf (seems to be Ubuntu specific)
	if [[ -e "$HOME/.asoundrc.asoundconf" ]]
	then
		echo "!!asoundconf-generated config file" >> "$FILE"
		echo "" >> "$FILE"
		cat "$HOME/.asoundrc.asoundconf" >> "$FILE"
		echo "" >> "$FILE"
		echo "" >> "$FILE"
	fi
        #Check for /etc/asound.conf
        if [[ -e /etc/asound.conf ]]
        then
                echo "!!System wide config file (/etc/asound.conf)" >> "$FILE"
                echo "" >> "$FILE"
                cat /etc/asound.conf >> "$FILE"
                echo "" >> "$FILE"
                echo "" >> "$FILE"
        fi
fi
}

withsysfs() {
    local i f
    local printed=""
    for i in /sys/class/sound/*; do
	case "$i" in
	    */hwC?D?)
		if [ -f "$i/init_pin_configs" ]; then
		    if [ -z "$printed" ]; then
			echo "!!Sysfs Files" >> "$FILE"
			echo "!!-----------" >> "$FILE"
			echo "" >> "$FILE"
		    fi
		    for f in init_pin_configs driver_pin_configs user_pin_configs init_verbs hints; do
			echo "$i/$f:" >> "$FILE"
			cat "$i/$f" >> "$FILE"
			echo >> "$FILE"
		    done
		    printed=yes
		fi
		;;
	    esac
    done
    if [ -n "$printed" ]; then
	echo "" >> "$FILE"
    fi
}

withdmesg() {
	echo "!!ALSA/HDA dmesg" >> "$FILE"
	echo "!!--------------" >> "$FILE"
	echo "" >> "$FILE"
	dmesg | grep -C1 -E 'ALSA|HDA|HDMI|snd[_-]|sound|audio|hda.codec|hda.intel' >> "$FILE"
	echo "" >> "$FILE"
	echo "" >> "$FILE"
}

withpackages() {
	local RPM
	local DPKG
	RPM="$(command -v rpmquery)"
	DPKG="$(command -v dpkg)"
	[ -n "$RPM$DPKG" ] || return
	local PATTERN='(alsa-(lib|oss|plugins|tools|(topology|ucm)-conf|utils|sof-firmware)|libalsa|tinycompress|sof-firmware)'
	{
        echo "!!Packages installed"
        echo "!!--------------------"
        echo ""
	{
		if [ -x "$RPM" ]; then "$RPM" -a; fi
		if [ -x "$DPKG" ]; then "$DPKG" -l; fi
	} | grep -E "$PATTERN"
	echo ""
	} >> "$FILE"
}

withall() {
	withdevices
	withconfigs
	withaplay
	withamixer
	withalsactl
	withmodules
	withsysfs
	withdmesg
	withpackages
	WITHALL=no
}

get_alsa_library_version() {
	ALSA_LIB_VERSION="$(grep VERSION_STR /usr/include/alsa/version.h 2>/dev/null | awk '{ print $3 }' | sed 's/"//g')"

	if [ -z "$ALSA_LIB_VERSION" ]; then
		if [ -f /etc/lsb-release ]; then
			. /etc/lsb-release
			case "$DISTRIB_ID" in
				Ubuntu)
					if command -v dpkg > /dev/null ; then
						ALSA_LIB_VERSION="$(dpkg -l libasound2 | tail -1 | awk '{ print $3 }' | cut -f 1 -d -)"
					fi

					if [ "$ALSA_LIB_VERSION" = '<none>' ]; then
						ALSA_LIB_VERSION=""
					fi
					return
					;;
				*)
					return
					;;
			esac
		elif [ -f /etc/debian_version ]; then
			if command -v dpkg > /dev/null ; then
				ALSA_LIB_VERSION="$(dpkg -l libasound2 | tail -1 | awk '{ print $3 }' | cut -f 1 -d -)"
			fi

			if [ "$ALSA_LIB_VERSION" = '<none>' ]; then
				ALSA_LIB_VERSION=""
			fi
			return
		fi
	fi
}

# Basic requires
for prg in "${REQUIRES[@]}"; do
  t="$(command -v "$prg")"
  if test -z "$t"; then
    echo "This script requires $prg utility to continue."
    exit 1
  fi
done

# Run checks to make sure the programs we need are installed.
LSPCI="$(command -v lspci)"
TPUT="$(command -v tput)"
DIALOG="$(command -v dialog)"

# Check to see if sysfs is enabled in the kernel. We'll need this later on
SYSFS="$(mount | grep sysfs | awk '{ print $3 }')"

# Check modprobe config files for sound related options
SNDOPTIONS="$(modprobe -c | sed -n 's/^options \(snd[-_][^ ]*\)/\1:/p')"

KEEP_OUTPUT=
NFILE=""

PASTEBIN=""
WWWSERVICE='www.alsa-project.org'
WELCOME=yes
PROCEED=yes
UPLOAD=ask
REPEAT=""
while [ -z "$REPEAT" ]; do
REPEAT=no
case "$1" in
	--update|--help|--about)
		WELCOME=no
		PROCEED=no
		;;
	--upload)
		UPLOAD=yes
		WELCOME=no
		;;
	--no-upload)
		UPLOAD=no
		WELCOME=no
		;;
	--pastebin)
		PASTEBIN=yes
		WWWSERVICE=pastebin
		;;
	--no-dialog)
		DIALOG=""
		REPEAT=""
		shift
		;;
	--stdout)
		DIALOG=""
		WELCOME=no
		;;
esac
done


#Script header output.
if [ "$WELCOME" = yes ]; then
greeting_message="\

This script visits the following commands/files to collect diagnostic
information about your ALSA installation and sound related hardware.

  dmesg
  lspci
  aplay
  amixer
  alsactl
  rpm, dpkg
  /proc/asound/
  /sys/class/sound/
  ~/.asoundrc (etc.)

See '$0 --help' for command line options.
"
if [ -n "$DIALOG" ]; then
	dialog  --backtitle "$BGTITLE" \
		--title "ALSA-Info script v $SCRIPT_VERSION" \
		--msgbox "$greeting_message" 20 80
else
	echo "ALSA Information Script v $SCRIPT_VERSION"
	echo "--------------------------------"
	echo "$greeting_message"
fi # dialog
fi # WELCOME

# Set the output file
TEMPDIR="$(mktemp -t -d alsa-info.XXXXXXXXXX)" || exit 1
FILE="$TEMPDIR/alsa-info.txt"
if [ -z "$NFILE" ]; then
	NFILE="$(mktemp -t alsa-info.txt.XXXXXXXXXX)" || exit 1
fi

trap cleanup 0

if [ "$PROCEED" = yes ]; then

if [ -z "$LSPCI" ]; then
	if [ -d /sys/bus/pci ]; then
		echo "This script requires lspci. Please install it, and re-run this script."
	fi
fi

# Fetch the info and store in temp files/variables
TSTAMP=$(LANG=C TZ=UTC date)
DISTRO=$(grep -ihs "buntu\|SUSE\|Fedora\|PCLinuxOS\|MEPIS\|Mandriva\|Debian\|Damn\|Sabayon\|Slackware\|KNOPPIX\|Gentoo\|Zenwalk\|Mint\|Kubuntu\|FreeBSD\|Puppy\|Freespire\|Vector\|Dreamlinux\|CentOS\|Arch\|Xandros\|Elive\|SLAX\|Red\|BSD\|KANOTIX\|Nexenta\|Foresight\|GeeXboX\|Frugalware\|64\|SystemRescue\|Novell\|Solaris\|BackTrack\|KateOS\|Pardus\|ALT" /etc/{issue,*release,*version})
read -r KERNEL_RELEASE KERNEL_MACHINE KERNEL_PROCESSOR KERNEL_OS < <(uname -rpmo)
read -r KERNEL_VERSION < <(uname -v)
if [[ "$KERNEL_VERSION" = *SMP* ]]; then KERNEL_SMP=Yes; else KERNEL_SMP=No; fi
ALSA_DRIVER_VERSION=$(cat /proc/asound/version | head -n1 | awk '{ print $7 }' | sed 's/\.$//')
get_alsa_library_version
ALSA_UTILS_VERSION=$(amixer -v | awk '{ print $3 }')

ESDINST=$(command -v esd)
PWINST=$(command -v pipewire)
PAINST=$(command -v pulseaudio)
ARTSINST=$(command -v artsd)
JACKINST=$(command -v jackd)
JACK2INST=$(command -v jackdbus)
ROARINST=$(command -v roard)
DMIDECODE=$(command -v dmidecode)

#Check for DMI data
if [ -d /sys/class/dmi/id ]; then
    # No root privileges are required when using sysfs method
    DMI_SYSTEM_MANUFACTURER=$(cat /sys/class/dmi/id/sys_vendor 2>/dev/null)
    DMI_SYSTEM_PRODUCT_NAME=$(cat /sys/class/dmi/id/product_name 2>/dev/null)
    DMI_SYSTEM_PRODUCT_VERSION=$(cat /sys/class/dmi/id/product_version 2>/dev/null)
    DMI_SYSTEM_FIRMWARE_VERSION=$(cat /sys/class/dmi/id/bios_version 2>/dev/null)
    DMI_SYSTEM_SKU=$(cat /sys/class/dmi/id/product_sku 2>/dev/null)
    DMI_BOARD_VENDOR=$(cat /sys/class/dmi/id/board_vendor 2>/dev/null)
    DMI_BOARD_NAME=$(cat /sys/class/dmi/id/board_name 2>/dev/null)
elif [ -x $DMIDECODE ]; then
    DMI_SYSTEM_MANUFACTURER=$($DMIDECODE -s system-manufacturer 2>/dev/null)
    DMI_SYSTEM_PRODUCT_NAME=$($DMIDECODE -s system-product-name 2>/dev/null)
    DMI_SYSTEM_PRODUCT_VERSION=$($DMIDECODE -s system-version 2>/dev/null)
    DMI_SYSTEM_FIRMWARE_VERSION=$($DMIDECODE -s bios-version 2>/dev/null)
    DMI_SYSTEM_SKU=$($DMIDECODE -s system-sku-number 2>/dev/null)
    DMI_BOARD_VENDOR=$($DMIDECODE -s baseboard-manufacturer 2>/dev/null)
    DMI_BOARD_NAME=$($DMIDECODE -s baseboard-product-name 2>/dev/null)
fi

# Check for ACPI device status
if [ -d /sys/bus/acpi/devices ]; then
    for f in /sys/bus/acpi/devices/*/status; do
	ACPI_STATUS=$(cat $f 2>/dev/null);
	if [[ "$ACPI_STATUS" -ne 0 ]]; then
	    echo $f $'\t' $ACPI_STATUS >>$TEMPDIR/acpidevicestatus.tmp;
	fi
    done
fi

awk '{ print $2 " (card " $1 ")" }' < /proc/asound/modules > $TEMPDIR/alsamodules.tmp 2> /dev/null
cat /proc/asound/cards > $TEMPDIR/alsacards.tmp
if [[ ! -z "$LSPCI" ]]; then
	for class in 0401 0402 0403; do
		lspci -vvnn -d "::$class" | sed -n '/^[^\t]/,+1p'
	done > $TEMPDIR/lspci.tmp
fi

#Check for HDA-Intel cards codec#*
cat /proc/asound/card*/codec\#* > $TEMPDIR/alsa-hda-intel.tmp 2> /dev/null

#Check for AC97 cards codec
cat /proc/asound/card*/codec97\#0/ac97\#0-0 > $TEMPDIR/alsa-ac97.tmp 2> /dev/null
cat /proc/asound/card*/codec97\#0/ac97\#0-0+regs > $TEMPDIR/alsa-ac97-regs.tmp 2> /dev/null

#Check for USB descriptors
if [ -x /usr/bin/lsusb ]; then
    for f in /proc/asound/card[0-9]*/usbbus; do
	test -f "$f" || continue
	id=$(sed 's@/@:@' $f)
	lsusb -v -s $id >> $TEMPDIR/lsusb.tmp 2> /dev/null
    done
fi

#Check for USB stream setup
cat /proc/asound/card*/stream[0-9]* > $TEMPDIR/alsa-usbstream.tmp 2> /dev/null

#Check for USB mixer setup
cat /proc/asound/card*/usbmixer > $TEMPDIR/alsa-usbmixer.tmp 2> /dev/null

#Fetch the info, and put it in $FILE in a nice readable format.
if [[ -z $PASTEBIN ]]; then
echo "upload=true&script=true&cardinfo=" > $FILE
else
echo "name=$USER&type=33&description=/tmp/alsa-info.txt&expiry=&s=Submit+Post&content=" > $FILE
fi
echo "!!################################" >> $FILE
echo "!!ALSA Information Script v $SCRIPT_VERSION" >> $FILE
echo "!!################################" >> $FILE
echo "" >> $FILE
echo "!!Script ran on: $TSTAMP" >> $FILE
echo "" >> $FILE
echo "" >> $FILE
echo "!!Linux Distribution" >> $FILE
echo "!!------------------" >> $FILE
echo "" >> $FILE
echo $DISTRO >> $FILE
echo "" >> $FILE
echo "" >> $FILE
echo "!!DMI Information" >> $FILE
echo "!!---------------" >> $FILE
echo "" >> $FILE
echo "Manufacturer:      $DMI_SYSTEM_MANUFACTURER" >> $FILE
echo "Product Name:      $DMI_SYSTEM_PRODUCT_NAME" >> $FILE
echo "Product Version:   $DMI_SYSTEM_PRODUCT_VERSION" >> $FILE
echo "Firmware Version:  $DMI_SYSTEM_FIRMWARE_VERSION" >> $FILE
echo "System SKU:        $DMI_SYSTEM_SKU" >> $FILE
echo "Board Vendor:      $DMI_BOARD_VENDOR" >> $FILE
echo "Board Name:        $DMI_BOARD_NAME" >> $FILE
echo "" >> $FILE
echo "" >> $FILE
echo "!!ACPI Device Status Information" >> $FILE
echo "!!---------------" >> $FILE
echo "" >> $FILE
cat $TEMPDIR/acpidevicestatus.tmp >> $FILE
echo "" >> $FILE
echo "" >> $FILE
echo "!!Kernel Information" >> $FILE
echo "!!------------------" >> $FILE
echo "" >> $FILE
echo "Kernel release:    $KERNEL_VERSION" >> $FILE
echo "Operating System:  $KERNEL_OS" >> $FILE
echo "Architecture:      $KERNEL_MACHINE" >> $FILE
echo "Processor:         $KERNEL_PROCESSOR" >> $FILE
echo "SMP Enabled:       $KERNEL_SMP" >> $FILE
echo "" >> $FILE
echo "" >> $FILE
echo "!!ALSA Version" >> $FILE
echo "!!------------" >> $FILE
echo "" >> $FILE
echo "Driver version:     $ALSA_DRIVER_VERSION" >> $FILE
echo "Library version:    $ALSA_LIB_VERSION" >> $FILE
echo "Utilities version:  $ALSA_UTILS_VERSION" >> $FILE
echo "" >> $FILE
echo "" >> $FILE
echo "!!Loaded ALSA modules" >> $FILE
echo "!!-------------------" >> $FILE
echo "" >> $FILE
cat $TEMPDIR/alsamodules.tmp >> $FILE
echo "" >> $FILE
echo "" >> $FILE
echo "!!Sound Servers on this system" >> $FILE
echo "!!----------------------------" >> $FILE
echo "" >> $FILE
if [[ -n $PWINST ]];then
[[ $(pgrep '^(.*/)?pipewire$') ]] && PWRUNNING="Yes" || PWRUNNING="No"
echo "PipeWire:" >> $FILE
echo "      Installed - Yes ($PWINST)" >> $FILE
echo "      Running - $PWRUNNING" >> $FILE
echo "" >> $FILE
fi
if [[ -n $PAINST ]];then
[[ $(pgrep '^(.*/)?pulseaudio$') ]] && PARUNNING="Yes" || PARUNNING="No"
echo "Pulseaudio:" >> $FILE
echo "      Installed - Yes ($PAINST)" >> $FILE
echo "      Running - $PARUNNING" >> $FILE
echo "" >> $FILE
fi
if [[ -n $ESDINST ]];then
[[ $(pgrep '^(.*/)?esd$') ]] && ESDRUNNING="Yes" || ESDRUNNING="No"
echo "ESound Daemon:" >> $FILE
echo "      Installed - Yes ($ESDINST)" >> $FILE
echo "      Running - $ESDRUNNING" >> $FILE
echo "" >> $FILE
fi
if [[ -n $ARTSINST ]];then
[[ $(pgrep '^(.*/)?artsd$') ]] && ARTSRUNNING="Yes" || ARTSRUNNING="No"
echo "aRts:" >> $FILE
echo "      Installed - Yes ($ARTSINST)" >> $FILE
echo "      Running - $ARTSRUNNING" >> $FILE
echo "" >> $FILE
fi
if [[ -n $JACKINST ]];then
[[ $(pgrep '^(.*/)?jackd$') ]] && JACKRUNNING="Yes" || JACKRUNNING="No"
echo "Jack:" >> $FILE
echo "      Installed - Yes ($JACKINST)" >> $FILE
echo "      Running - $JACKRUNNING" >> $FILE
echo "" >> $FILE
fi
if [[ -n $JACK2INST ]];then
[[ $(pgrep '^(.*/)?jackdbus$') ]] && JACK2RUNNING="Yes" || JACK2RUNNING="No"
echo "Jack2:" >> $FILE
echo "      Installed - Yes ($JACK2INST)" >> $FILE
echo "      Running - $JACK2RUNNING" >> $FILE
echo "" >> $FILE
fi
if [[ -n $ROARINST ]];then
[[ $(pgrep '^(.*/)?roard$') ]] && ROARRUNNING="Yes" || ROARRUNNING="No"
echo "RoarAudio:" >> $FILE
echo "      Installed - Yes ($ROARINST)" >> $FILE
echo "      Running - $ROARRUNNING" >> $FILE
echo "" >> $FILE
fi
if [[ -z "$PAINST" && -z "$ESDINST" && -z "$ARTSINST" && -z "$JACKINST" && -z "$ROARINST" ]];then
echo "No sound servers found." >> $FILE
echo "" >> $FILE
fi
echo "" >> $FILE
echo "!!Soundcards recognised by ALSA" >> $FILE
echo "!!-----------------------------" >> $FILE
echo "" >> $FILE
cat $TEMPDIR/alsacards.tmp >> $FILE
echo "" >> $FILE
echo "" >> $FILE

if [[ ! -z "$LSPCI" ]]; then
echo "!!PCI Soundcards installed in the system" >> $FILE
echo "!!--------------------------------------" >> $FILE
echo "" >> $FILE
cat $TEMPDIR/lspci.tmp >> $FILE
echo "" >> $FILE
echo "" >> $FILE
fi

if [ "$SNDOPTIONS" ]; then
echo "!!Modprobe options (Sound related)" >> $FILE
echo "!!--------------------------------" >> $FILE
echo "" >> $FILE
modprobe -c|sed -n 's/^options \(snd[-_][^ ]*\)/\1:/p' >> $FILE
echo "" >> $FILE
echo "" >> $FILE
fi

if [ -d "$SYSFS" ]; then
	echo "!!Loaded sound module options" >> $FILE
	echo "!!---------------------------" >> $FILE
	echo "" >> $FILE
	for mod in $(cat /proc/asound/modules | awk '{ print $2 }'); do
		echo "!!Module: $mod" >> $FILE
		for params in $(echo $SYSFS/module/$mod/parameters/*); do
			echo -ne "\t"
			value=$(cat $params)
			echo "$params : $value" | sed 's:.*/::'
		done >> $FILE
		echo "" >> $FILE
	done
	echo "" >> $FILE
	echo "!!Sysfs card info" >> $FILE
	echo "!!---------------" >> $FILE
	echo "" >> $FILE
	for cdir in $(echo $SYSFS/class/sound/card*); do
		echo "!!Card: $cdir" >> $FILE
		driver=$(readlink -f "$cdir/device/driver")
		echo "Driver: $driver" >> $FILE
		echo "Tree:" >> $FILE
		tree --noreport $cdir -L 2 | sed -e 's/^/\t/g' >> $FILE
		echo "" >> $FILE
	done
	echo "" >> $FILE
	if [ -d $SYSFS/class/sound/ctl-led ]; then
		echo "!!Sysfs ctl-led info" >> $FILE
		echo "!!---------------" >> $FILE
		echo "" >> $FILE
		for path in $(echo $SYSFS/class/sound/ctl-led/[ms][ip]*/card*); do
			echo "!!CTL-LED: $path" >> $FILE
			if [ -r "$path/list" ]; then
				list=$(cat "$path/list")
				echo "List: $list" >> $FILE
			fi
			#echo "Tree:" >> $FILE
			#tree --noreport $path -L 2 | sed -e 's/^/\t/g' >> $FILE
			echo "" >> $FILE
		done
	fi
fi

if [ -s "$TEMPDIR/alsa-hda-intel.tmp" ]; then
	echo "!!HDA-Intel Codec information" >> $FILE
	echo "!!---------------------------" >> $FILE
	echo "--startcollapse--" >> $FILE
	echo "" >> $FILE
	cat $TEMPDIR/alsa-hda-intel.tmp >> $FILE
	echo "--endcollapse--" >> $FILE
	echo "" >> $FILE
	echo "" >> $FILE
fi

if [ -s "$TEMPDIR/alsa-ac97.tmp" ]; then
        echo "!!AC97 Codec information" >> $FILE
        echo "!!----------------------" >> $FILE
        echo "--startcollapse--" >> $FILE
        echo "" >> $FILE
        cat $TEMPDIR/alsa-ac97.tmp >> $FILE
        echo "" >> $FILE
        cat $TEMPDIR/alsa-ac97-regs.tmp >> $FILE
        echo "--endcollapse--" >> $FILE
	echo "" >> $FILE
	echo "" >> $FILE
fi

if [ -s "$TEMPDIR/lsusb.tmp" ]; then
        echo "!!USB Descriptors" >> $FILE
        echo "!!---------------" >> $FILE
        echo "--startcollapse--" >> $FILE
        cat $TEMPDIR/lsusb.tmp >> $FILE
        echo "--endcollapse--" >> $FILE
	echo "" >> $FILE
	echo "" >> $FILE
fi

if [ -s "$TEMPDIR/alsa-usbstream.tmp" ]; then
        echo "!!USB Stream information" >> $FILE
        echo "!!----------------------" >> $FILE
        echo "--startcollapse--" >> $FILE
        echo "" >> $FILE
        cat $TEMPDIR/alsa-usbstream.tmp >> $FILE
        echo "--endcollapse--" >> $FILE
	echo "" >> $FILE
	echo "" >> $FILE
fi

if [ -s "$TEMPDIR/alsa-usbmixer.tmp" ]; then
        echo "!!USB Mixer information" >> $FILE
        echo "!!---------------------" >> $FILE
        echo "--startcollapse--" >> $FILE
        echo "" >> $FILE
        cat $TEMPDIR/alsa-usbmixer.tmp >> $FILE
        echo "--endcollapse--" >> $FILE
	echo "" >> $FILE
	echo "" >> $FILE
fi

#If no command line options are specified, then run as though --with-all was specified
if [ -z "$1" ]; then
	update
fi

fi # proceed

#loop through command line arguments, until none are left.
if [ -n "$1" ]; then
	until [ -z "$1" ]
	do
	case "$1" in
		--pastebin)
		        update
			;;
		--update)
			update
			exit
			;;
		--upload)
			UPLOAD=yes
			;;
		--no-upload)
			UPLOAD=no
			;;
		--output)
			shift
			NFILE="$1"
			KEEP_OUTPUT=yes
			;;
		--debug)
			echo "Debugging enabled. $FILE and $TEMPDIR will not be deleted"
			KEEP_FILES=yes
			echo ""
			;;
		--with-all)
			withall
			;;
		--with-aplay)
			withaplay
			WITHALL=no
			;;
		--with-amixer)
			withamixer
			WITHALL=no
			;;
		--with-alsactl)
			withalsactl
			WITHALL=no
			;;
		--with-devices)
			withdevices
			WITHALL=no
			;;
		--with-dmesg)
			withdmesg
			WITHALL=no
			;;
		--with-configs)
			withconfigs
			WITHALL=no
			;;
		--with-packages)
			withpackages
			WITHALL=no
			;;
		--stdout)
			UPLOAD=no
			if [ -z "$WITHALL" ]; then
				withall
			fi
			cat "$FILE"
			rm "$FILE"
			exit
			;;
		--about)
			echo "Written/Tested by the following users of #alsa on irc.freenode.net:"
			echo ""
			echo "	wishie - Script author and developer / Testing"
			echo "	crimsun - Various script ideas / Testing"
			echo "	gnubien - Various script ideas / Testing"
			echo "	GrueMaster - HDA Intel specific items / Testing"
			echo "	olegfink - Script update function"
			echo "  TheMuso - display to stdout functionality"
			exit 0
			;;
		*)
			echo "alsa-info.sh version $SCRIPT_VERSION"
			echo ""
			echo "Available options:"
			echo "	--with-aplay (includes the output of aplay -l)"
			echo "	--with-amixer (includes the output of amixer)"
			echo "	--with-alsactl (includes the output of alsactl)"
			echo "	--with-configs (includes the output of ~/.asoundrc and"
			echo "	    /etc/asound.conf if they exist)" 
			echo "	--with-devices (shows the device nodes in /dev/snd/)"
			echo "	--with-dmesg (shows the ALSA/HDA kernel messages)"
			echo "	--with-packages (includes known packages installed)"
			echo ""
			echo "	--output FILE (specify the file to output for no-upload mode)"
			echo "	--update (check server for script updates)"
			echo "	--upload (upload contents to remote server)"
			echo "	--no-upload (do not upload contents to remote server)"
			echo "	--pastebin (use 'https://pastebin.ca') as remote server"
			echo "	    instead www.alsa-project.org"
			echo "	--stdout (print alsa information to standard output"
			echo "	    instead of a file)"
			echo "	--about (show some information about the script)"
			echo "	--debug (will run the script as normal, but will not"
			echo "	     delete ${FILE})"
			exit 0
			;;
	esac
	shift 1
	done
fi

if [ "$PROCEED" = no ]; then
	exit 1
fi

if [ -z "$WITHALL" ]; then
	withall
fi

# Check if wget is installed, and supports --post-file.
if ! wget --help 2>/dev/null | grep -q post-file; then
	# We couldn't find a suitable wget. If --upload was passed, tell the user to upload manually.
	if [ "$UPLOAD" != yes ]; then
		:
	elif [ -n "$DIALOG" ]; then
		if [ -z "$PASTEBIN" ]; then
			dialog --backtitle "$BGTITLE" --msgbox "Could not automatically upload output to 'https://www.alsa-project.org'.\nPossible reasons are:\n\n    1. Couldn't find 'wget' in your PATH\n    2. Your version of wget is less than 1.8.2\n\nPlease manually upload $NFILE to 'https://www.alsa-project.org/cardinfo-db' and submit your post." 25 100
		else
			dialog --backtitle "$BGTITLE" --msgbox "Could not automatically upload output to 'https://www.pastebin.ca'.\nPossible reasons are:\n\n    1. Couldn't find 'wget' in your PATH\n    2. Your version of wget is less than 1.8.2\n\nPlease manually upload $NFILE to 'https://www.pastebin.ca/upload.php' and submit your post." 25 100
		fi
	else
		if [ -z "$PASTEBIN" ]; then
			echo ""
			echo "Could not automatically upload output to 'https://www.alsa-project.org'"
			echo "Possible reasons are:"
			echo "    1. Couldn't find 'wget' in your PATH"
			echo "    2. Your version of wget is less than 1.8.2"
			echo ""
			echo "Please manually upload $NFILE to 'https://www.alsa-project.org/cardinfo-db' and submit your post."
			echo ""
		else
			echo ""
			echo "Could not automatically upload output to 'https://www.pastebin.ca'"
			echo "Possible reasons are:"
			echo "    1. Couldn't find 'wget' in your PATH"
			echo "    2. Your version of wget is less than 1.8.2"
			echo ""
			echo "Please manually upload $NFILE to 'https://www.pastebin.ca/upload.php' and submit your post."
			echo ""
		fi
	fi
	UPLOAD=no
fi

if [ "$UPLOAD" = ask ]; then
	if [ -n "$DIALOG" ]; then
		dialog --backtitle "$BGTITLE" --title "Information collected" --yes-label " UPLOAD / SHARE " --no-label " SAVE LOCALLY " --defaultno --yesno "\n\nAutomatically upload ALSA information to $WWWSERVICE?" 10 80
		DIALOG_EXIT_CODE="$?"
		if [ "$DIALOG_EXIT_CODE" != 0 ]; then
			UPLOAD=no
		else
			UPLOAD=yes
		fi
	else
		echo -n "Automatically upload ALSA information to $WWWSERVICE? [y/N] : "
		read -e CONFIRM
		if [ "$CONFIRM" != y ]; then
			UPLOAD=no
		else
			UPLOAD=yes
		fi
	fi

fi

if [ "$UPLOAD" = no ]; then

	mv -f "$FILE" "$NFILE" || exit 1
	KEEP_OUTPUT=yes

	if [[ -n "$DIALOG" ]]
	then
		dialog --backtitle "$BGTITLE" --title "Information collected" --msgbox "\n\nYour ALSA information is in $NFILE" 10 60
	else
		echo ""
		echo "Your ALSA information is in $NFILE"
		echo ""
	fi

	exit

fi # UPLOAD

if [[ -n "$DIALOG" ]]
then
	dialog --backtitle "$BGTITLE" --infobox "Uploading information to $WWWSERVICE ..." 6 70
else
	echo -n "Uploading information to $WWWSERVICE ..."
fi

if [[ -z "$PASTEBIN" ]]; then
	wget -O - --tries=5 --timeout=60 --post-file="$FILE" 'https://www.alsa-project.org/cardinfo-db/' &> "$TEMPDIR/wget.tmp"
else
	wget -O - --tries=5 --timeout=60 --post-file="$FILE" 'https://pastebin.ca/quiet-paste.php?api='"${PASTEBINKEY}"'&encrypt=t&encryptpw=blahblah' &> "$TEMPDIR/wget.tmp"
fi

if [ "$?" -ne 0 ]; then
	mv -f "$FILE" "$NFILE" || exit 1
	KEEP_OUTPUT=yes

	if [ -n "$DIALOG" ]; then
		dialog --backtitle "$BGTITLE" --title "Information not uploaded" --msgbox "An error occurred while contacting $WWWSERVICE.\n Your information was NOT automatically uploaded.\n\nYour ALSA information is in $NFILE" 10 100
	else
		echo ""
		echo "An error occurred while contacting $WWWSERVICE."
		echo "Your information was NOT automatically uploaded."
		echo ""
		echo "Your ALSA information is in $NFILE"
		echo ""
	fi

	exit
fi

if [ -n "$DIALOG" ]; then

dialog --backtitle "$BGTITLE" --title "Information uploaded" --yesno "Would you like to see the uploaded information?" 5 100 
DIALOG_EXIT_CODE="$?"
if [ "$DIALOG_EXIT_CODE" = 0 ]; then
	grep -v alsa-info.txt "$FILE" > "$TEMPDIR/uploaded.txt"
	dialog --backtitle "$BGTITLE" --textbox "$TEMPDIR/uploaded.txt" 0 0
fi

clear

# no dialog
else

echo -e " Done!"
echo ""

fi # dialog

if [ -z "$PASTEBIN" ]; then
	FINAL_URL="$(grep 'SUCCESS:' "$TEMPDIR/wget.tmp" | cut -d ' ' -f 2)"
else
	FINAL_URL="$(grep 'SUCCESS:' "$TEMPDIR/wget.tmp" | sed -n 's/.*\:\([0-9]\+\).*/https:\/\/pastebin.ca\/\1/p')"
fi

# See if tput is available, and use it if it is.
if [ -x "$TPUT" ]; then
	FINAL_URL="$(tput setaf 1; printf '%s' "$FINAL_URL"; tput sgr0)"
fi

# Output the URL of the uploaded file.	
echo "Your ALSA information is located at $FINAL_URL"
echo "Please inform the person helping you."
echo ""
