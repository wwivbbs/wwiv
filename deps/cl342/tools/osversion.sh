#!/bin/sh
# Obtain the OS version.

# Make sure that we've been given a single argument consisting of the OS
# name.

if [ -z "$1" ] ; then
	echo "$0: Missing OS name." >&2 ;
	exit 1 ;
fi
if [ $# -ne 1 ] ; then
	echo "$0: Can only supply 1 arg." >&2 ;
	exit 1 ;
fi

# Juggle the args around to get them the way that we want them.

if [ "$1" = "autodetect" ] ; then
	OSNAME=`uname` ;
else
	OSNAME=$1 ;
fi

# Determine the OS version.  The means of doing this varies wildly across
# OSes.  We also check for the various cross-compile environments and return 
# either an appropriate version number (defaulting to '1' if there's no real
# distinction between versions) or an error code if there's no useful
# default available, in which case use requires manual editing of the config
# as we can't automatically detect the OS version.

case $OSNAME in
	# Aches has a broken uname that reports the OS minor version with
	# uname -r instead of the major version.  The alternative command 
	# oslevel reports the full version number, which we can extract in 
	# the standard manner.  
	'AIX')
		echo `oslevel | cut -d'.' -f1` ;;

	'AMX')
		echo 1 ;;

	'Atmel')
		exit 1 ;;

	'BeOS')
		echo `uname -r | sed 's/^[A-Z]//' | cut -b 1` ;;

	'CHORUS')
		echo 5 ;;

	'eCOS')
		echo 1 ;;

	# PHUX returns the version as something like 'B.11.11', so we have to 
	# carefully extract the thing that looks most like a version number from 
	# this.  Coming up with a regex for this (as for the Slowaris version
	# extraction) is a bit too complex, so we use pattern-matching for one-
	# and two-digit version values.
	'HP-UX')
		case `uname -r` in
			*.0?.*)
				echo `uname -r | tr -d '[A-Za-z*]\.' | cut -c 2` ;;
			*.1?.*)
				echo `uname -r | tr -d '[A-Za-z*]\.' | cut -c 1,2` ;;
		esac ;;

	'MinGW')
		echo 5 ;;

	'PalmOS'|'PalmOS-PRC')
		echo 6 ;;

	# QNX uses -v instead of -r for the version, and also has a broken 'cut'.
	'QNX')
		echo `uname -v | sed 's/^[A-Z]//' | cut -c 1` ;;

	# Slowaris uses bizarre version numbering, going up to (SunOS) 4.x 
	# normally, then either using (Slowaris) 2.5/2.6/2.7 to mean Solaris
	# 5/6/7, or using (at least) 5.7 to also mean Solaris 7.  After
	# Solaris 8 it's more consistent (although still weird), with the
	# version numbers being 5.8, 5.9, 5.10.  To handle this mess we check
	# for versions below 5.x, which are handled normally, and above that
	# use a complex regex to derive the version number.
	'SunOS')
		if [ `uname -r | cut -b 1` -lt 5 ] ; then
			echo `uname -r | cut -b 1` ;
		else
			echo `uname -r | sed -e 's/[0-9]*\.\([0-9]*\).*/\1/'` ;
		fi ;;

	'ucLinux')
		echo 2 ;;

	'UCOS')
		echo 2 ;;

	'VxWorks')
		echo 1 ;;

	'XMK')
		echo 3 ;;

	*)
		echo `uname -r | cut -c 1` ;;
esac
