#!/bin/sh
# Get a list of the system libraries that need to be linked with cryptlib.
#
# Usage: getlibs.sh [special] compiler osname [crosscompile]

LDARGS=""
ISSPECIAL=0
CROSSCOMPILE=0

# Make sure that we've been given sufficient arguments.

if [ "$1" = "special" ] ; then
    ISSPECIAL=1 ;
    shift ;
fi
if [ $# -lt 2 ] ; then
	echo "Usage: $0 getlibs.sh [special] compiler osname [crosscompile]" >&2 ;
	exit 1 ;
fi
if [ ! -z "$3" ] ; then
	CROSSCOMPILE=$3 ;
fi

# Juggle the args around to get them the way that we want them.

CC=$1
OSNAME=$2
shift
shift

# Get the compiler build options, which affect the linker options.

if [ $ISSPECIAL -eq 0 ] ; then
	BUILDOPTS="$(./tools/ccopts.sh $CC $OSNAME 2>&1)" ;
else
	BUILDOPTS="$(./tools/ccopts.sh special $CC $OSNAME 2>&1)" ;
fi

hasSubstring()
	{
	STRING=$1
	SUBSTRING=$2

	# Check whether SUBSTRING is present in STRING by checking whether it
	# matches the pattern ' * $SUBSTRING * '.
    case "$STRING" in
		*"$SUBSTRING"*)
			return 0 ;;
	esac

	return 1
	}

# If we're using a newer version of clang, we'll have been compiled with
# specific compiler options that need to be passed down to the linker.
# See the comment in tools/ccopts.sh on why we need to check the version
# number used to enable this flag rather than just checking for the presence
# of the flag in the compile options.

if hasSubstring "$BUILDOPTS" "sanitize=safe-stack" ; then
	CLANG_VER="$(clang -dumpversion | tr -d  '.' | cut -c 1-2)" ;
	if [ $CLANG_VER -gt 47 ] ; then
		LDARGS="$LDARGS -fsanitize=safe-stack" ;
	fi ;
fi

# Add any libraries needed by optional components.  In the case of zlib use
# this is a real pain because some Linux distros require that you use
# potentially out-of-date system-provided libraries if they're present
# rather than your own, up-to-date code.

if hasSubstring "$BUILDOPTS" "HAS_TPM" ; then
	LDARGS="$LDARGS -ltspi" ;
fi
if hasSubstring "$BUILDOPTS" "HAS_ZLIB" ; then
	LDARGS="$LDARGS -lz" ;
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
#	NCR MP-RAS (!threads):		-K xpg42 -lnsl -lsocket -lc89 -lresolv
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

OSVERSION="$(./tools/osversion.sh $OSNAME)"
case $OSNAME in
	'AIX')
		LDARGS="$LDARGS -lc_r -lpthreads" ;
		if [ $OSVERSION -gt 4 ] ; then
			LDARGS="$LDARGS -lbsd" ;
		fi ;;

	'BeOS')
		if [ -f /system/lib/libbind.so ] ; then
			LDARGS="$LDARGS -lbind -lsocket" ;
		fi ;;

	'BSD/OS')
		LDARGS="$LDARGS -lgcc" ;;

	'CRAY')
		LDARGS="$LDARGS -lpthread" ;;

	'Darwin')
		LDARGS="$LDARGS -lresolv" ;;

	'FreeBSD'|'NetBSD')
		LDARGS="$LDARGS -lpthread" ;;

	'HP-UX')
		if [ $OSVERSION -ge 10 ] ; then
			LDARGS="$LDARGS -lpthread" ;
		fi ;;

	'IRIX'|'IRIX64')
		LDARGS="$LDARGS -lw" ;;

	'Linux')
		LDARGS="$LDARGS -lresolv -lpthread" ;
		if [ -x /sbin/ldconfig ] && \
		   [ "$(/sbin/ldconfig -p | grep -c 'libdl\.so')" -gt 0 ] ; then
			LDARGS="$LDARGS -ldl" ;
		elif [ -f /usr/lib/libdl.so ] || \
			 [ -f /usr/lib32/libdl.so ] || \
			 [ -f /usr/lib64/libdl.so ] || \
			 [ -f /usr/lib/i386-linux-gnu/libdl.so ] ; then
			LDARGS="$LDARGS -ldl" ;
		fi ;;

	'OSF1')
		LDARGS="$LDARGS -lresolv -lpthread" ;;

	'SunOS')
		if [ $OSVERSION -le 4 ] ; then
			LDARGS="$LDARGS -ldl -lnsl -lposix4" ;
		elif [ $OSVERSION -le 6 ] ; then
			LDARGS="$LDARGS -lw -lsocket -lkstat -lnsl -lposix4 -lthread" ;
		else
			LDARGS="$LDARGS -lw -lresolv -lsocket -lkstat -lrt -lnsl -lthread" ;
		fi ;;

	'UNIX_SV')
		LDARGS="$LDARGS -K xpg42 -lnsl -lsocket -lc89 -lresolv" ;;

	'UnixWare')
		LDARGS="$LDARGS -lsocket" ;;
esac

# Finally, report what we've found

echo "$LDARGS"