#!/bin/sh
# Build the static library.

# Make sure that we've been given sufficient arguments.

if [ -z "$1" ] ; then
	echo "$0: Missing OS name." >&2 ;
	exit 1 ;
fi
if [ -z "$2" ] ; then
	echo "$0: Missing library name." >&2 ;
	exit 1 ;
fi
if [ -z "$3" ] ; then
	echo "$0: Missing 'ar' command name." >&2 ;
	exit 1 ;
fi
if [ -z "$4" ] ; then
	echo "$0: Missing object filenames." >&2 ;
	exit 1 ;
fi

# Juggle the args around to get them the way that we want them.

OSNAME=$1
LIBNAME=$2
AR=$3
shift
shift
shift

# The use of ar and ranlib is rather system-dependant.  Some ar's (e.g.OSF1)
# create the .SYMDEF file by default, some require the 's' option, and some
# require the use of ranlib altogether because ar doesn't recognise the 's'
# option.  If we know what's required we use the appropriate form, otherwise
# we first try 'ar rcs' (which works on most systems) and if that fails fall
# back to 'ar rc' followed by ranlib.  QNX doesn't have either ranlib or the
# 's' option to ar, so the best we can do is use 'ar rc'.  Finally, Unicos
# has a weird ar that takes args in a nonstandard form.

case $OSNAME in
	'AIX'|'HP-UX'|'Linux'|'OSF1'|'UNIX_SV')
		$AR rcs $LIBNAME $* ;;

	'Atmel')
		echo "Need to set up Atmel link command" ;;

	'BSD/OS'|'FreeBSD'|'iBSD'|'NetBSD'|'OpenBSD')
		$AR rc $LIBNAME $* ;
		ranlib $LIBNAME ;;

	'CRAY')
		$AR -rc $LIBNAME $* ;;

	'PalmOS')
		palib -add $LIBNAME $* ;
		palink -nodebug -o palmcl.dll $LIBNAME ./static-obj/cryptsld.o \
				-libpath "d:/Palm\\\ SDK/sdk-6/libraries/ARM_4T/Release/Default" ;;

	'PalmOS-PRC')
		arm-palmos-ar rc $LIBNAME $* ;
		arm-palmos-ranlib $LIBNAME ;;

	'QNX')
		$AR rc $LIBNAME $* ;;

	'SunOS')
		if [ `which ar | grep -c "no ar"` = '1' ] ; then
			/usr/ccs/bin/ar rcs $LIBNAME $* ;
		else
			$AR rcs $LIBNAME $* ;
		fi ;;

	'ucLinux')
		echo "Need to set up ucLinux link command" ;;

	*)
		$AR rcs $LIBNAME $* || \
		( $AR rc $LIBNAME $* && ranlib $LIBNAME )

esac
