#!/bin/sh

SCRIPT_VERSION=0.4.64
CHANGELOG="http://www.alsa-project.org/alsa-info.sh.changelog"

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
PATH=$PATH:/bin:/sbin:/usr/bin:/usr/sbin
BGTITLE="ALSA-Info v $SCRIPT_VERSION"
PASTEBINKEY="C9cRIO8m/9y8Cs0nVs0FraRx7U0pHsuc"

WGET=$(command -v wget)
REQUIRES="mktemp grep pgrep ping date uname cat dmesg amixer alsactl"

#
# Define some simple functions
#

pbcheck() {
	[ "$UPLOAD" = "no" ] && return

	if [ -z "$PASTEBIN" ]; then
		ping -qc1 www.alsa-project.org || KEEP_FILES="yes" UPLOAD="no" PBERROR="yes"
	else
		ping -qc1 www.pastebin.ca || KEEP_FILES="yes" UPLOAD="no" PBERROR="yes"
	fi
}

update() {
	[ -x "$WGET" ] || return

	SHFILE="$(mktemp -t alsa-info.XXXXXXXXXX)" || exit 1
	wget -O "$SHFILE" "http://www.alsa-project.org/alsa-info.sh" >/dev/null 2>&1
	REMOTE_VERSION="$(sed -n 's/^SCRIPT_VERSION=//p' "$SHFILE")"
	if [ -s "$SHFILE" ] && [ "$REMOTE_VERSION" != "$SCRIPT_VERSION" ]; then
		if [ -n "$DIALOG" ]; then
			OVERWRITE=
			if [ -w "$0" ]; then
				dialog --yesno "Newer version of ALSA-Info has been found\n\nDo you wish to install it?\nNOTICE: The original file $0 will be overwritten!" 0 0
				DIALOG_EXIT_CODE=$?
				[ "$DIALOG_EXIT_CODE" = 0 ] && OVERWRITE="yes"
			fi
			if [ -z "$OVERWRITE" ]; then
				dialog --yesno "Newer version of ALSA-Info has been found\n\nDo you wish to download it?" 0 0
				DIALOG_EXIT_CODE=$?
			fi
			if [ "$DIALOG_EXIT_CODE" = 0 ]; then
				echo "Newer version detected: $REMOTE_VERSION"
				echo "To view the ChangeLog, please visit $CHANGELOG"
				if [ "$OVERWRITE" = "yes" ]; then
					cp "$SHFILE" "$0"
					echo "ALSA-Info script has been updated to v $REMOTE_VERSION"
					echo "Please re-run the script"
					rm -f "$SHFILE"
				else
					echo "ALSA-Info script has been downloaded as $SHFILE."
					echo "Please re-run the script from new location."
				fi
				exit
			else
				rm -f "$SHFILE"
			fi
		else
			echo "Newer version detected: $REMOTE_VERSION"
			echo "To view the ChangeLog, please visit $CHANGELOG"
			if [ -w "$0" ]; then
				echo "The original file $0 will be overwritten!"
				printf "If you do not like to proceed, press Ctrl-C now.."; read -r _
				cp "$SHFILE" "$0"
				echo "ALSA-Info script has been updated. Please re-run it."
				rm -f "$SHFILE"
			else
				echo "ALSA-Info script has been downloaded $SHFILE."
				echo "Please, re-run it from new location."
			fi
			exit
		fi
	else
		rm -f "$SHFILE"
	fi
}

cleanup() {
	if [ -n "$TEMPDIR" ] && [ "$KEEP_FILES" != "yes" ]; then
		rm -rf "$TEMPDIR"
	fi
	[ -n "$KEEP_OUTPUT" ] || rm -f "$NFILE"
}

withaplay() {
	echo "!!Aplay/Arecord output"
	echo "!!--------------------"
	echo ""
	echo "APLAY"
	echo ""
	aplay -l 2>&1
	echo ""
	echo "ARECORD"
	echo ""
	arecord -l 2>&1
	echo ""
} >> "$FILE"

withlsmod() {
	echo "!!All Loaded Modules"
	echo "!!------------------"
	echo ""
	lsmod | while read -r mod _; do
		echo "$mod"
	done
	echo ""
	echo ""
} >> "$FILE"

withamixer() {
	echo "!!Amixer output"
	echo "!!-------------"
	echo ""
	sed -n '/]: /s/^[[:space:]]\([0-9]\+\).*/\1/p' /proc/asound/cards | while read -r i; do
		CARD_NAME=$(sed -n "s/^ *$i \([^ ]\+\).*/\1/p" "$TEMPDIR/alsacards.tmp")
		echo "!!-------Mixer controls for card $i $CARD_NAME]"
		echo ""
		amixer -c"$i" info 2>&1
		amixer -c"$i" 2>&1
		echo ""
	done
	echo ""
} >> "$FILE"

withalsactl() {
	echo "!!Alsactl output"
	echo "!!--------------"
	echo ""
	alsactl -f "$TEMPDIR/alsactl.tmp" store
	echo "--startcollapse--"
	cat "$TEMPDIR/alsactl.tmp"
	echo "--endcollapse--"
	echo ""
	echo ""
} >> "$FILE"

withdevices() {
	echo "!!ALSA Device nodes"
	echo "!!-----------------"
	echo ""
	ls -la /dev/snd/*
	echo ""
	echo ""
} >> "$FILE"

withconfigs() {
	[ -e "$HOME/.asoundrc" ] || [ -e /etc/asound.conf ] || [ -e "$HOME/.asoundrc.asoundconf" ] || return

	echo "!!ALSA configuration files"
	echo "!!------------------------"
	echo ""

	#Check for ~/.asoundrc
	if [ -e "$HOME/.asoundrc" ]; then
		echo "!!User specific config file (~/.asoundrc)"
		echo ""
		cat "$HOME/.asoundrc"
		echo ""
		echo ""
	fi

	#Check for .asoundrc.asoundconf (seems to be Ubuntu specific)
	if [ -e "$HOME/.asoundrc.asoundconf" ]; then
		echo "!!asoundconf-generated config file"
		echo ""
		cat "$HOME/.asoundrc.asoundconf"
		echo ""
		echo ""
	fi

	#Check for /etc/asound.conf
	if [ -e /etc/asound.conf ]; then
		echo "!!System wide config file (/etc/asound.conf)"
		echo ""
		cat /etc/asound.conf
		echo ""
		echo ""
	fi
} >> "$FILE"

withsysfs() {
	printed=''

	for i in /sys/class/sound/hwC?D?; do
		[ -f "$i/init_pin_configs" ] || continue
		if [ -z "$printed" ]; then
			echo "!!Sysfs Files"
			echo "!!-----------"
			echo ""
			printed="yes"
		fi
		for f in init_pin_configs driver_pin_configs user_pin_configs init_verbs hints; do
			echo "$i/$f:"
			cat "$i/$f"
			echo
		done
	done
	if [ -n "$printed" ]; then
		echo ""
	fi
} >> "$FILE"

withdmesg() {
	echo "!!ALSA/HDA dmesg"
	echo "!!--------------"
	echo ""
	dmesg | grep -C1 -E 'ALSA|HDA|HDMI|snd[_-]|sound|hda.codec|hda.intel'
	echo ""
	echo ""
} >> "$FILE"

withall() {
	withdevices
	withconfigs
	withaplay
	withamixer
	withalsactl
	withlsmod
	withsysfs
	withdmesg
	WITHALL="no"
}

get_alsa_library_version() {
	ALSA_LIB_VERSION="$(sed -n 's/.*VERSION_STR[[:space:]]*"\(.*\)"/\1/p' /usr/include/alsa/version.h)"

	if [ -z "$ALSA_LIB_VERSION" ]; then
		if [ -f /etc/lsb-release ]; then
			. /etc/lsb-release
		fi
		if [ -f /etc/debian_version ] || [ "$DISTRIB_ID" = "Ubuntu" ]; then
			if command -v dpkg >/dev/null ; then
				ALSA_LIB_VERSION=$(dpkg -l libasound2 | sed -n '$s/[^[:space:]]*[[:space:]]*[^[:space:]]*[[:space:]]*\([^[:space:]]*\).*/\1/p')
			fi

			if [ "$ALSA_LIB_VERSION" = "<none>" ]; then
				ALSA_LIB_VERSION=""
			fi
			return
		fi
	fi
}

# Basic requires
for prg in $REQUIRES; do
	if ! command -v "$prg" >/dev/null; then
		echo "This script requires $prg utility to continue."
		exit 1
	fi
done

# Run checks to make sure the programs we need are installed.
LSPCI="$(command -v lspci)"
TPUT="$(command -v tput)"
DIALOG="$(command -v dialog)"

# Check to see if sysfs is enabled in the kernel. We'll need this later on
SYSFS=$(mount | sed -n 's/^[^ ]* on \([^ ]*\) type sysfs.*/\1/p')

# Check modprobe config files for sound related options
SNDOPTIONS=$(modprobe -c | sed -n 's/^options \(snd[-_][^ ]*\)/\1:/p')

KEEP_OUTPUT=
NFILE=""

PASTEBIN=""
WWWSERVICE="www.alsa-project.org"
WELCOME="yes"
PROCEED="yes"
UPLOAD="ask"
REPEAT=""
while [ -z "$REPEAT" ]; do
	REPEAT="no"
	case "$1" in
		--update|--help|--about)
			WELCOME="no"
			PROCEED="no"
			;;
		--upload)
			UPLOAD="yes"
			WELCOME="no"
			;;
		--no-upload)
			UPLOAD="no"
			WELCOME="no"
			;;
		--pastebin)
			PASTEBIN="yes"
			WWWSERVICE="pastebin"
			;;
		--no-dialog)
			DIALOG=""
			REPEAT=""
			shift
			;;
		--stdout)
			DIALOG=""
			UPLOAD="no"
			WELCOME="no"
			TOSTDOUT="yes"
			;;
	esac
done


#Script header output.
greeting_message="\

This script visits the following commands/files to collect diagnostic
information about your ALSA installation and sound related hardware.

  dmesg
  lspci
  lsmod
  aplay
  amixer
  alsactl
  /proc/asound/
  /sys/class/sound/
  ~/.asoundrc (etc.)

See '$0 --help' for command line options.
"
if [ "$WELCOME" = "yes" ]; then
	if [ -n "$DIALOG" ]; then
		dialog --backtitle "$BGTITLE" --title "ALSA-Info script v $SCRIPT_VERSION" --msgbox "$greeting_message" 20 80
	else
		echo "ALSA Information Script v $SCRIPT_VERSION"
		echo "--------------------------------"
		echo "$greeting_message"
	fi # dialog
fi # WELCOME

# Set the output file
TEMPDIR=$(mktemp -t -d alsa-info.XXXXXXXXXX) || exit 1
FILE="$TEMPDIR/alsa-info.txt"
if [ -z "$NFILE" ]; then
	NFILE=$(mktemp -t alsa-info.txt.XXXXXXXXXX) || exit 1
fi

trap cleanup 0

if [ "$PROCEED" = "yes" ]; then
	if [ -z "$LSPCI" ] && [ -d /sys/bus/pci ]; then
		echo "This script requires lspci. Please install it, and re-run this script."
		exit 0
	fi

	# Fetch the info and store in temp files/variables
	TSTAMP=$(LANG=C TZ=UTC date)
	DISTRO=$(grep -Eihs "buntu|SUSE|Fedora|PCLinuxOS|MEPIS|Mandriva|Debian|Damn|Sabayon|Slackware|KNOPPIX|Gentoo|Zenwalk|Mint|Kubuntu|FreeBSD|Puppy|Freespire|Vector|Dreamlinux|CentOS|Arch|Xandros|Elive|SLAX|Red|BSD|KANOTIX|Nexenta|Foresight|GeeXboX|Frugalware|64|SystemRescue|Novell|Solaris|BackTrack|KateOS|Pardus" /etc/issue /etc/*release /etc/*version)
	KERNEL_VERSION=$(uname -r)
	KERNEL_PROCESSOR=$(uname -p 2>/dev/null) # silence error because -p isn't POSIX
	KERNEL_MACHINE=$(uname -m)
	KERNEL_OS=$(uname -o 2>/dev/null) # silence error because -o isn't POSIX
	case "$(uname -v)" in
		*SMP*)  KERNEL_SMP="Yes";;
		*)		KERNEL_SMP="No";;
	esac
	ALSA_DRIVER_VERSION=$(sed -n '1s/.* \([^ ]*\)\.$/\1/p' /proc/asound/version)
	get_alsa_library_version
	ALSA_UTILS_VERSION=$(amixer -v | sed 's/amixer version //')

	ESDINST=$(command -v esd)
	PAINST=$(command -v pulseaudio)
	ARTSINST=$(command -v artsd)
	JACKINST=$(command -v jackd)
	ROARINST=$(command -v roard)
	DMIDECODE=$(command -v dmidecode)

	#Check for DMI data
	if [ -d /sys/class/dmi/id ]; then
		# No root privileges are required when using sysfs method
		read -r DMI_SYSTEM_MANUFACTURER < /sys/class/dmi/id/sys_vendor
		read -r DMI_SYSTEM_PRODUCT_NAME < /sys/class/dmi/id/product_name
		read -r DMI_SYSTEM_PRODUCT_VERSION < /sys/class/dmi/id/product_version
		read -r DMI_SYSTEM_FIRMWARE_VERSION < /sys/class/dmi/id/bios_version
		read -r DMI_BOARD_VENDOR < /sys/class/dmi/id/board_vendor
		read -r DMI_BOARD_NAME < /sys/class/dmi/id/board_name
	elif [ -x "$DMIDECODE" ]; then
		DMI_SYSTEM_MANUFACTURER=$("$DMIDECODE" -s system-manufacturer)
		DMI_SYSTEM_PRODUCT_NAME=$("$DMIDECODE" -s system-product-name)
		DMI_SYSTEM_PRODUCT_VERSION=$("$DMIDECODE" -s system-version)
		DMI_SYSTEM_FIRMWARE_VERSION=$("$DMIDECODE" -s bios-version)
		DMI_BOARD_VENDOR=$("$DMIDECODE" -s baseboard-manufacturer)
		DMI_BOARD_NAME=$("$DMIDECODE" -s baseboard-product-name)
	fi 2>/dev/null

	# Check for ACPI device status
	if [ -d /sys/bus/acpi/devices ]; then
		for f in /sys/bus/acpi/devices/*/status; do
			read -r ACPI_STATUS < "$f"
			if [ "$ACPI_STATUS" -ne 0 ]; then
				printf '%s \t %s\n' "$f" "$ACPI_STATUS"
			fi
		done > "$TEMPDIR/acpidevicestatus.tmp"
	fi

	if [ -e /proc/asound/modules ]; then
		while read -r _ mod; do
			SNDMODULES="$SNDMODULES${SNDMODULES:+ }$mod"
			echo "$mod"
		done </proc/asound/modules >"$TEMPDIR/alsamodules.tmp"
	else
		: > "$TEMPDIR/alsamodules.tmp"
	fi
	cat /proc/asound/cards > "$TEMPDIR/alsacards.tmp"
	lspci | grep -i "multi\|audio" > "$TEMPDIR/lspci.tmp"

	#Check for HDA-Intel cards codec#*
	cat /proc/asound/card*/codec\#* > "$TEMPDIR/alsa-hda-intel.tmp" 2>/dev/null

	#Check for AC97 cards codec
	cat /proc/asound/card*/codec97\#0/ac97\#0-0 > "$TEMPDIR/alsa-ac97.tmp" 2>/dev/null
	cat /proc/asound/card*/codec97\#0/ac97\#0-0+regs > "$TEMPDIR/alsa-ac97-regs.tmp" 2>/dev/null

	#Check for USB mixer setup
	cat /proc/asound/card*/usbmixer > "$TEMPDIR/alsa-usbmixer.tmp" 2>/dev/null

	#Fetch the info, and put it in $FILE in a nice readable format.
	{
		if [ -z "$PASTEBIN" ]; then
			echo "upload=true&script=true&cardinfo="
		else
			echo "name=$USER&type=33&description=/tmp/alsa-info.txt&expiry=&s=Submit+Post&content="
		fi
		echo "!!################################"
		echo "!!ALSA Information Script v $SCRIPT_VERSION"
		echo "!!################################"
		echo ""
		echo "!!Script ran on: $TSTAMP"
		echo ""
		echo ""
		echo "!!Linux Distribution"
		echo "!!------------------"
		echo ""
		echo "$DISTRO"
		echo ""
		echo ""
		echo "!!DMI Information"
		echo "!!---------------"
		echo ""
		echo "Manufacturer:      $DMI_SYSTEM_MANUFACTURER"
		echo "Product Name:      $DMI_SYSTEM_PRODUCT_NAME"
		echo "Product Version:   $DMI_SYSTEM_PRODUCT_VERSION"
		echo "Firmware Version:  $DMI_SYSTEM_FIRMWARE_VERSION"
		echo "Board Vendor:      $DMI_BOARD_VENDOR"
		echo "Board Name:        $DMI_BOARD_NAME"
		echo ""
		echo ""
		echo "!!ACPI Device Status Information"
		echo "!!---------------"
		echo ""
		cat "$TEMPDIR/acpidevicestatus.tmp"
		echo ""
		echo ""
		echo "!!Kernel Information"
		echo "!!------------------"
		echo ""
		echo "Kernel release:    $KERNEL_VERSION"
		echo "Operating System:  $KERNEL_OS"
		echo "Architecture:      $KERNEL_MACHINE"
		echo "Processor:         $KERNEL_PROCESSOR"
		echo "SMP Enabled:       $KERNEL_SMP"
		echo ""
		echo ""
		echo "!!ALSA Version"
		echo "!!------------"
		echo ""
		echo "Driver version:     $ALSA_DRIVER_VERSION"
		echo "Library version:    $ALSA_LIB_VERSION"
		echo "Utilities version:  $ALSA_UTILS_VERSION"
		echo ""
		echo ""
		echo "!!Loaded ALSA modules"
		echo "!!-------------------"
		echo ""
		cat "$TEMPDIR/alsamodules.tmp"
		echo ""
		echo ""
		echo "!!Sound Servers on this system"
		echo "!!----------------------------"
		echo ""
		if [ -n "$PAINST" ];then
			[ -n "$(pgrep '^(.*/)?pulseaudio$')" ] && PARUNNING="Yes" || PARUNNING="No"
			echo "Pulseaudio:"
			echo "      Installed - Yes ($PAINST)"
			echo "      Running - $PARUNNING"
			echo ""
		fi
		if [ -n "$ESDINST" ];then
			[ -n "$(pgrep '^(.*/)?esd$')" ] && ESDRUNNING="Yes" || ESDRUNNING="No"
			echo "ESound Daemon:"
			echo "      Installed - Yes ($ESDINST)"
			echo "      Running - $ESDRUNNING"
			echo ""
		fi
		if [ -n "$ARTSINST" ];then
			[ -n "$(pgrep '^(.*/)?artsd$')" ] && ARTSRUNNING="Yes" || ARTSRUNNING="No"
			echo "aRts:"
			echo "      Installed - Yes ($ARTSINST)"
			echo "      Running - $ARTSRUNNING"
			echo ""
		fi
		if [ -n "$JACKINST" ];then
			[ -n "$(pgrep '^(.*/)?jackd$')" ] && JACKRUNNING="Yes" || JACKRUNNING="No"
			echo "Jack:"
			echo "      Installed - Yes ($JACKINST)"
			echo "      Running - $JACKRUNNING"
			echo ""
		fi
		if [ -n "$ROARINST" ];then
			[ -n "$(pgrep '^(.*/)?roard$')" ] && ROARRUNNING="Yes" || ROARRUNNING="No"
			echo "RoarAudio:"
			echo "      Installed - Yes ($ROARINST)"
			echo "      Running - $ROARRUNNING"
			echo ""
		fi
		if [ -z "${PAINST}${ESDINST}${ARTSINST}${JACKINST}${ROARINST}" ];then
			echo "No sound servers found."
			echo ""
		fi
		echo ""
		echo "!!Soundcards recognised by ALSA"
		echo "!!-----------------------------"
		echo ""
		cat "$TEMPDIR/alsacards.tmp"
		echo ""
		echo ""

		if [ -n "$LSPCI" ]; then
			echo "!!PCI Soundcards installed in the system"
			echo "!!--------------------------------------"
			echo ""
			cat "$TEMPDIR/lspci.tmp"
			echo ""
			echo ""
			echo "!!Advanced information - PCI Vendor/Device/Subsystem ID's"
			echo "!!-------------------------------------------------------"
			echo ""
			lspci -vvn | grep -A1 '040[1-3]'
			echo ""
			echo ""
		fi

		if [ -n "$SNDOPTIONS" ]; then
			echo "!!Modprobe options (Sound related)"
			echo "!!--------------------------------"
			echo ""
			modprobe -c | sed -n 's/^options \(snd[-_][^ ]*\)/\1:/p'
			echo ""
			echo ""
		fi

		if [ -d "$SYSFS" ] && [ -n "$SNDMODULES" ]; then
			echo "!!Loaded sound module options"
			echo "!!---------------------------"
			echo ""
			for mod in $SNDMODULES; do
				[ -d "$SYSFS/module/$mod/parameters/" ] || continue
				echo "!!Module: $mod"
				for f in "$SYSFS/module/$mod/parameters/"*; do
					read -r value < "$f"
					printf '\t%s : %s\n' "${f##*/}" "$value"
				done
				echo ""
			done
			echo ""
		fi

		if [ -s "$TEMPDIR/alsa-hda-intel.tmp" ]; then
			echo "!!HDA-Intel Codec information"
			echo "!!---------------------------"
			echo "--startcollapse--"
			echo ""
			cat "$TEMPDIR/alsa-hda-intel.tmp"
			echo "--endcollapse--"
			echo ""
			echo ""
		fi

		if [ -s "$TEMPDIR/alsa-ac97.tmp" ]; then
			echo "!!AC97 Codec information"
			echo "!!----------------------"
			echo "--startcollapse--"
			echo ""
			cat "$TEMPDIR/alsa-ac97.tmp"
			echo ""
			cat "$TEMPDIR/alsa-ac97-regs.tmp"
			echo "--endcollapse--"
			echo ""
			echo ""
		fi

		if [ -s "$TEMPDIR/alsa-usbmixer.tmp" ]; then
			echo "!!USB Mixer information"
			echo "!!---------------------"
			echo "--startcollapse--"
			echo ""
			cat "$TEMPDIR/alsa-usbmixer.tmp"
			echo "--endcollapse--"
			echo ""
			echo ""
		fi
	} > "$FILE"

	#If no command line options are specified, then run as though --with-all was specified
	if [ -z "$1" ]; then
		update
		pbcheck
	fi
fi # PROCEED

#loop through command line arguments, until none are left.
while [ -n "$1" ]; do
	case "$1" in
		--pastebin)
			update
			pbcheck
			;;
		--update)
			update
			exit
			;;
		--upload)
			UPLOAD="yes"
			;;
		--no-upload)
			UPLOAD="no"
			;;
		--output)
			shift
			NFILE="$1"
			KEEP_OUTPUT="yes"
			;;
		--debug)
			echo "Debugging enabled. $FILE and $TEMPDIR will not be deleted"
			KEEP_FILES="yes"
			echo ""
			;;
		--with-all)
			withall
			;;
		--with-aplay)
			withaplay
			WITHALL="no"
			;;
		--with-amixer)
			withamixer
			WITHALL="no"
			;;
		--with-alsactl)
			withalsactl
			WITHALL="no"
			;;
		--with-devices)
			withdevices
			WITHALL="no"
			;;
		--with-dmesg)
			withdmesg
			WITHALL="no"
			;;
		--with-configs)
			withconfigs
			WITHALL="no"
			;;
		--stdout)
			UPLOAD="no"
			if [ -z "$WITHALL" ]; then
				withall
			fi
			cat "$FILE"
			rm "$FILE"
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
			echo ""
			echo "	--output FILE (specify the file to output for no-upload mode)"
			echo "	--update (check server for script updates)"
			echo "	--upload (upload contents to remote server)"
			echo "	--no-upload (do not upload contents to remote server)"
			echo "	--pastebin (use http://pastebin.ca) as remote server"
			echo "	    instead www.alsa-project.org"
			echo "	--stdout (print alsa information to standard output"
			echo "	    instead of a file)"
			echo "	--about (show some information about the script)"
			echo "	--debug (will run the script as normal, but will not"
			echo "	     delete $FILE)"
			exit 0
			;;
	esac
	shift
done

if [ "$PROCEED" = "no" ]; then
	exit 1
fi

if [ -z "$WITHALL" ]; then
	withall
fi

if [ "$UPLOAD" = "ask" ]; then
	if [ -n "$DIALOG" ]; then
		dialog --backtitle "$BGTITLE" --title "Information collected" --yes-label " UPLOAD / SHARE " --no-label " SAVE LOCALLY " --defaultno --yesno "\n\nAutomatically upload ALSA information to $WWWSERVICE?" 10 80
		DIALOG_EXIT_CODE=$?
		if [ "$DIALOG_EXIT_CODE" = 0 ]; then
			UPLOAD="yes"
		else
			UPLOAD="no"
		fi
	else
		printf 'Automatically upload ALSA information to %s? [y/N] : ' "$WWWSERVICE"
		read -r CONFIRM
		if [ "$CONFIRM" = "y" ]; then
			UPLOAD="yes"
		else
			UPLOAD="no"
		fi
	fi

fi

if [ "$UPLOAD" = "no" ]; then
	if [ -z "$TOSTDOUT" ]; then
		mv -f "$FILE" "$NFILE" || exit 1
		KEEP_OUTPUT="yes"
	fi

	if [ -n "$DIALOG" ]; then
		if [ -n "$PBERROR" ]; then
			dialog --backtitle "$BGTITLE" --title "Information collected" --msgbox "An error occurred while contacting the $WWWSERVICE.\n Your information was NOT automatically uploaded.\n\nYour ALSA information is in $NFILE" 10 100
		else
			dialog --backtitle "$BGTITLE" --title "Information collected" --msgbox "\n\nYour ALSA information is in $NFILE" 10 60
		fi
	else
		echo
		if [ -n "$PBERROR" ]; then
			echo "An error occurred while contacting the $WWWSERVICE."
			echo "Your information was NOT automatically uploaded."
			echo ""
			echo "Your ALSA information is in $NFILE"
			echo ""
		elif [ -z "$TOSTDOUT" ]; then
			echo ""
			echo "Your ALSA information is in $NFILE"
			echo ""
		fi
	fi

	exit
fi # UPLOAD

# Test that wget is installed, and supports --post-file. Upload $FILE if it does, and prompt user to upload file if it does not.
if wget --help 2>&1 | grep -q post-file; then
	if [ -n "$DIALOG" ]; then
		if [ -z "$PASTEBIN" ]; then
			wget -O - --tries=5 --timeout=60 --post-file="$FILE" "http://www.alsa-project.org/cardinfo-db/" >"$TEMPDIR/wget.tmp" 2>&1 || echo "Upload failed; exit"
			{
				for i in 10 20 30 40 50 60 70 80 90; do
					echo $i
					sleep 0.2
				done
				echo
			} | dialog --backtitle "$BGTITLE" --guage "Uploading information to www.alsa-project.org ..." 6 70 0
		else
			wget -O - --tries=5 --timeout=60 --post-file="$FILE" "http://pastebin.ca/quiet-paste.php?api=$PASTEBINKEY&encrypt=t&encryptpw=blahblah" >"$TEMPDIR/wget.tmp" 2>&1 || echo "Upload failed; exit"
			{
				for i in 10 20 30 40 50 60 70 80 90; do
					echo $i
					sleep 0.2
				done
				echo
			} | dialog --backtitle "$BGTITLE" --guage "Uploading information to www.pastebin.ca ..." 6 70 0
		fi

		dialog --backtitle "$BGTITLE" --title "Information uploaded" --yesno "Would you like to see the uploaded information?" 5 100
		DIALOG_EXIT_CODE=$?
		if [ "$DIALOG_EXIT_CODE" = 0 ]; then
			grep -v "alsa-info.txt" "$FILE" > "$TEMPDIR/uploaded.txt"
			dialog --backtitle "$BGTITLE" --textbox "$TEMPDIR/uploaded.txt" 0 0
		fi

		clear
	else # no dialog
		if [ -z "$PASTEBIN" ]; then
			printf 'Uploading information to www.alsa-project.org ... '
			wget -O - --tries=5 --timeout=60 --post-file="$FILE" http://www.alsa-project.org/cardinfo-db/ >"$TEMPDIR/wget.tmp" 2>&1 &
		else
			printf 'Uploading information to www.pastebin.ca ... '
			wget -O - --tries=5 --timeout=60 --post-file="$FILE" "http://pastebin.ca/quiet-paste.php?api=$PASTEBINKEY" >"$TEMPDIR/wget.tmp" 2>&1 &
		fi

		#Progess spinner for wget transfer.
		i=1
		printf ' '
		while kill -0 "$!"; do # run while backgrounded wget is running
			case "$(((i += 1) % 4))" in
				0)  printf '\b/';;
				1)  printf '\b-';;
				2)  printf '\b\\';;
				3)  printf '\b|';;
			esac
		done

		printf '\b Done!\n'
		echo ""
	fi # dialog

	if [ -z "$PASTEBIN" ]; then
		FINAL_URL=$(grep "SUCCESS:" "$TEMPDIR/wget.tmp" | cut -d ' ' -f 2)
	else
		FINAL_URL=$(grep "SUCCESS:" "$TEMPDIR/wget.tmp" | sed -n 's/.*\:\([0-9]\+\).*/http:\/\/pastebin.ca\/\1/p')
	fi

	# See if tput is available, and use it if it is.
	if [ -n "$TPUT" ]; then
		FINAL_URL=$(tput setaf 1; printf '%s' "$FINAL_URL"; tput sgr0)
	fi

	# Output the URL of the uploaded file.
	echo "Your ALSA information is located at $FINAL_URL"
	echo "Please inform the person helping you."
	echo ""
else
	# We couldn't find a suitable wget, so tell the user to upload manually.
	mv -f "$FILE" "$NFILE" || exit 1
	KEEP_OUTPUT="yes"
	if [ -z "$DIALOG" ]; then
		if [ -z "$PASTEBIN" ]; then
			echo ""
			echo "Could not automatically upload output to http://www.alsa-project.org"
			echo "Possible reasons are:"
			echo "    1. Couldnt find 'wget' in your PATH"
			echo "    2. Your version of wget is less than 1.8.2"
			echo ""
			echo "Please manually upload $NFILE to http://www.alsa-project.org/cardinfo-db/ and submit your post."
			echo ""
		else
			echo ""
			echo "Could not automatically upload output to http://www.pastebin.ca"
			echo "Possible reasons are:"
			echo "    1. Couldnt find 'wget' in your PATH"
			echo "    2. Your version of wget is less than 1.8.2"
			echo ""
			echo "Please manually upload $NFILE to http://www.pastebin.ca/upload.php and submit your post."
			echo ""
		fi
	else
		if [ -z "$PASTEBIN" ]; then
			dialog --backtitle "$BGTITLE" --msgbox "Could not automatically upload output to http://www.alsa-project.org.\nPossible reasons are:\n\n    1. Couldn't find 'wget' in your PATH\n    2. Your version of wget is less than 1.8.2\n\nPlease manually upload $NFILE to http://www.alsa-project,org/cardinfo-db/ and submit your post." 25 100
		else
			dialog --backtitle "$BGTITLE" --msgbox "Could not automatically upload output to http://www.pastebin.ca.\nPossible reasons are:\n\n    1. Couldn't find 'wget' in your PATH\n    2. Your version of wget is less than 1.8.2\n\nPlease manually upload $NFILE to http://www.pastebin.ca/upload.php and submit your post." 25 100
		fi
	fi
fi
