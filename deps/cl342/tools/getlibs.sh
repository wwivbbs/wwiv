#!/bin/sh
# Get a list of the system libraries that need to be linked with cryptlib.

# Make sure that we've been given a single argument consisting of the OS name.

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

# Some OS's require the linking of additional special libraries, either into
# the executable for the static-lib version or into the library itself for
# the shared-lib version.  The OS's and their libraries are:
#
#	AIX 4.x:					-lc_r -lpthreads
#	AIX 5.x:					-lc_r -lpthreads -lbsd
#	BeOS:						None
#	BeOS with BONE:				-lbind -lsocket
#	BSDI:						-lgcc
#	Cray Unicos:				-lpthread
#	Cygwin:						None
#	FreeBSD:					See note for -lc_r, -lpthread
#	Irix:						-lw
#	Linux:						-lresolv -lpthread (-ldl see note)
#	NetBSD:						-lpthread
#	MVS:						None
#	NCR MP-RAS (threads):		-Xdce -lnsl -lsocket -lc89 -lresolv -lpthread
#	NCR MP-RAS (no.threads):	-K xpg42 -lnsl -lsocket -lc89 -lresolv
#	OSF1/DEC Unix:				-lresolv -lpthread
#	OS X (Darwin):				-lresolv 
#	PHUX 9.x, 10.x:				None
#	PHUX 11.x:					-lpthread
#	SunOS 4.x:					-ldl -lnsl -lposix4
#	SunOS 5.5 and 5.6:			-lw -lsocket -lkstat -lnsl -lposix4 -lthread
#	Solaris 7+ (SunOS 5.7+):	-lw -lresolv -lsocket -lkstat -lrt -lnsl -lthread
#	Tandem OSS/NSK:				None
#	UnixWare (SCO):				-lsocket
#
# Comments:
#
#	-lbsd = flock() support via BSD compatibility layer for Aches.
#	-lc_r = libc extended with re-entrant functions needed for threading.
#			This is required by FreeBSD 5.1-RELEASE but not FreeBSD 5.1-
#			CURRENT, which has the standard libc re-entrant.  Because there's
#			no easy way to tell what we're running under (they both have the
#			same version numbers) we'd have to use it for both, however by
#			using -pthread during the compile we can tell the compiler to
#			sort the mess out for us - it'll link against libc_r or libc as
#			appropriate.
#	-ldl = dload() support for dynamically loaded PKCS #11 drivers.  Some
#			Debian releases require that the use of this library be
#			explicitly specified, although there's no discernable pattern
#			for when this might be required (or even any easy way to detect
#			Debian), so we unconditionally include it under Linux if libdl
#			exists.  Note that on some Linux systems it's hidden in
#			/usr/lib32 or /usr/lib64 so we check for multiple locations,
#			and then Ubuntu makes it even worse by hiding it in even more
#			places like /usr/lib/i386-linux-gnu/libdl.so.
#			If this continues then it may be easier to just assume that
#			Linux, or at least Ubuntu, always has libdl present.  As a
#			somewhat safer alternative we first try ldconfig, and only then
#			fall back to hardcoded locations.
#	-lgcc = Extra gcc support lib needed for BSDI, which ships with gcc but
#			not the proper libs for it.
#	-lkstat = kstat functions for Solaris randomness gathering.
#	-lsocket = Resolver functions.
#	-lnsl = Socket support for Slowaris, which doesn't have it in libc.
#   -lposix4 = Solaris 2.5 and 2.6 library for sched_yield.
#	-lresolv = Resolver functions.
# 	-lrt = Solaris 2.7 and above realtime library for sched_yield().
#	-lssp = gcc additional library needed in some cases when
#			-f-stack-protector is used.
#	-lthread/lpthread/lpthreads = pthreads support.  Note that this generally
#			has to be linked as late as possible (and in particular after the
#			implied -lc) because libpthread overrides non-thread-safe and stub
#			functions in libraries linked earlier on with thread-safe
#			alternatives.
#	-lw = Widechar support.

OSVERSION=`./tools/osversion.sh $OSNAME`
case $OSNAME in
	'AIX')
		if [ $OSVERSION -le 4 ] ; then
			echo "-lc_r -lpthreads" ;
		else
			echo "-lc_r -lpthreads -lbsd" ;
		fi ;;

	'BeOS')
		if [ -f /system/lib/libbind.so ] ; then
			echo "-lbind -lsocket" ;
		else
			echo "" ;
		fi ;;

	'BSD/OS')
		echo "-lgcc" ;;

	'CRAY')
		echo "-lpthread" ;;

	'CYGWIN_NT-5.0'|'CYGWIN_NT-5.1')
		echo "" ;;

	'Darwin')
		echo "-lresolv" ;;

	'FreeBSD'|'NetBSD')
		echo "-lpthread" ;;

	'HP-UX')
		if [ $OSVERSION -lt 10 ] ; then
			echo "" ;
		else
			echo "-lpthread" ;
		fi ;;

	'IRIX'|'IRIX64')
		echo "-lw" ;;

	'Linux')
		if [ -x /sbin/ldconfig -a \
			 `/sbin/ldconfig -p | grep -c 'libdl\.so'` -gt 0 ] ; then
			echo "-lresolv -lpthread -ldl" ;
		elif [ -f /usr/lib/libdl.so -o \
			   -f /usr/lib32/libdl.so -o \
			   -f /usr/lib64/libdl.so -o \
			   -f /usr/lib/i386-linux-gnu/libdl.so ] ; then
			echo "-lresolv -lpthread -ldl" ;
		else
			echo "-lresolv -lpthread" ;
		fi ;;

	'OSF1')
		echo "-lresolv -lpthread" ;;

	'SunOS')
		if [ $OSVERSION -le 4 ] ; then
			echo "-ldl -lnsl -lposix4" ;
		elif [ $OSVERSION -le 6 ] ; then
			echo "-lw -lsocket -lkstat -lnsl -lposix4 -lthread" ;
		else
			echo "-lw -lresolv -lsocket -lkstat -lrt -lnsl -lthread" ;
		fi ;;

	'UNIX_SV')
		echo "-K xpg42 -lnsl -lsocket -lc89 -lresolv" ;;

	'UnixWare')
		echo "-lsocket" ;;

	*)
		echo "" ;;

esac
