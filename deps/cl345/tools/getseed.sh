#!/bin/sh
# Get a (non-cryptographic) random seed for compilation
#
# Usage: getseed.sh osname

OSNAME=""
IS64BIT=0

# Make sure that we've been given sufficient arguments.

if [ $# -lt 1 ] ; then
	echo "Usage: $0 osname" >&2 ;
	exit 1 ;
fi

# Juggle the args around to get them the way that we want them.

OSNAME=$1
shift

# Try and determine whether we need to output a 32- or 64-bit value.  This
# is somewhat crude since it gives the kernel architecture and we could be
# building an arch-a application on an arch-b kernel (typically x86 on x64).
# In addition if we fall back to the default uname -m this assumes that the
# hardware name has some indicator of 64-bit-ness in it.  In general this
# check errs on the side of caution, leaving the value at 32 bits if we're
# not sure.

case $OSNAME in
	'AIX')
		if [ "$(getconf KERNEL_BITMODE)" = "64" ] ; then
			IS64BIT=1 ;
		fi ;;

	'HP-UX')
		if [ "$(getconf KERNEL_BITS)" = "64" ] ; then
			IS64BIT=1 ;
		fi ;;

	'SunOS')
		if [ "$(isainfo -b)" = "64" ] ; then
			IS64BIT=1 ;
		fi ;;

	*)
		if [ "$(uname -m | grep '64')" != "" ] ; then
			IS64BIT=1 ;
		fi ;;
esac

# Print a machine-word-sized /dev/random value as a hex string.

formatDevRandomOutput()
	{
	SEEDVALUE=""

	# Get the seed value as a hex string.
	if [ $IS64BIT -gt 0 ] ; then
		SEEDVALUE="$(od -An -N8 -tx1 < /dev/urandom | tr -d ' \t\n')" ;
	else
		SEEDVALUE="$(od -An -N4 -tx1 < /dev/urandom | tr -d ' \t\n')" ;
	fi ;

	# Display the seed value.
	echo "-DFIXED_SEED=0x"$SEEDVALUE ;
	}

if [ -e /dev/urandom ] ; then
	formatDevRandomOutput ;
	exit 0 ;
fi

# There's no /dev/random, fall back to a random-ish alternative.

if [ $(which last) ] ; then
	SOURCE="last -50" ;
else
	SOURCE="uptime" ;
fi
if [ $(which md5sum) ] ; then
	printf -- "-DFIXED_SEED=0x" ;
	if [ $IS64BIT -gt 0 ] ; then
		echo $($SOURCE | md5sum | cut -c1-16) ;
	else
		echo $($SOURCE | md5sum | cut -c1-8) ;
	fi ;
	exit 0 ;
fi
