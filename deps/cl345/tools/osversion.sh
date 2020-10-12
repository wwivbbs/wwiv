#!/bin/sh
# Obtain the OS version.
#
# Usage: osversion.sh osname

OSVERSION=""

# Make sure that we've been given sufficient arguments.

if [ $# -ne 1 ] ; then
	echo "Usage: $0 osname" >&2 ;
	exit 1 ;
fi

# Juggle the args around to get them the way that we want them.

OSNAME=$1
shift

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
		OSVERSION="$(oslevel | cut -d'.' -f1)" ;;

	'AMX')
		OSVERSION="1" ;;

	'Atmel')
		OSVERSION="1" ;;

	'BeOS')
		OSVERSION="$(uname -r | sed 's/^[A-Z]//' | cut -b 1)" ;;

	'CHORUS')
		OSVERSION="5" ;;

	'eCOS')
		OSVERSION="1" ;;

	# PHUX returns the version as something like 'B.11.11', so we have to
	# carefully extract the thing that looks most like a version number from
	# this.  Coming up with a regex for this (as for the Slowaris version
	# extraction) is a bit too complex, so we use pattern-matching for one-
	# and two-digit version values.
	'HP-UX')
		case "$(uname -r)" in
			*.0?.*)
				OSVERSION="$(uname -r | tr -d '[A-Za-z*]\.' | cut -c 2)" ;;
			*.1?.*)
				OSVERSION="$(uname -r | tr -d '[A-Za-z*]\.' | cut -c 1,2)" ;;
		esac ;;

	'MinGW')
		OSVERSION="5" ;;

	'PalmOS'|'PalmOS-PRC')
		OSVERSION="6" ;;

	# QNX uses -v instead of -r for the version, and also has a broken 'cut'.
	'QNX')
		OSVERSION="$(uname -v | sed 's/^[A-Z]//' | cut -c 1)" ;;

	# Slowaris uses bizarre version numbering, going up to (SunOS) 4.x
	# normally, then either using (Slowaris) 2.5/2.6/2.7 to mean Solaris
	# 5/6/7, or using (at least) 5.7 to also mean Solaris 7.  After
	# Solaris 8 it's more consistent (although still weird), with the
	# version numbers being 5.8, 5.9, 5.10.  To handle this mess we check
	# for versions below 5.x, which are handled normally, and above that
	# use a complex regex to derive the version number.
	#
	# As with many other aspects of Slowaris, its shell is too antedeluvian
	# to recognise the "$( ... )" syntax, so we have to use the ` ... `
	# syntax instead.
	'SunOS')
		if [ `uname -r | cut -b 1` -lt 5 ] ; then
			OSVERSION=`uname -r | cut -b 1` ;
		else
			OSVERSION=`uname -r | sed -e 's/[0-9]*\.\([0-9]*\).*/\1/'` ;
		fi ;;

	'ucLinux')
		OSVERSION="2" ;;

	'UCOS')
		OSVERSION="2" ;;

	'VxWorks')
		OSVERSION="1" ;;

	'XMK')
		OSVERSION="3" ;;

	# Some Unix variants have version numbers (or pseudo-version-numbers)
	# greater than ten, so instead of the basic $(uname -r | cut -c 1) we
	# have to check for multi-digit groups as the version number.
	*)
		OSVERSION="$(uname -r | grep -E -o '[0-9]+' | head -1)" ;;
esac
echo "$OSVERSION"
