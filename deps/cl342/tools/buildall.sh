#!/bin/sh
# Build all cryptlib modules

# Make sure that we've been given sufficient arguments.

if [ "$1" = "shared" ] ; then
    SHARED=1 ;
    shift ;
fi
if [ "$1" = "" ] ; then
	echo "$0: Missing OS name." >&2 ;
	exit 1 ;
fi
if [ "$2" = "" ] ; then
	echo "$0: Missing compiler name." >&2 ;
	exit 1 ;
fi
if [ "$3" = "" ] ; then
	echo "$0: Missing compiler flags." >&2 ;
	exit 1 ;
fi

# Juggle the args around to get them the way that we want them.

OSNAME=$1
CC=$2
shift
shift

# Additional constants from the makefile.

MAJ="3"
MIN="4"
PLV="2"
PROJ="cl"
SHARED_OBJ_PATH="./shared-obj/"
if [ $OSNAME = "Darwin" ] ; then
	SLIBNAME="lib$PROJ.$MAJ.$MIN.dylib" ;
else
	SLIBNAME="lib$PROJ.so.$MAJ.$MIN.$PLV" ;
fi

# More SunOS braindamage, along with broken compiler installs (see comments
# elsewhere) you can't even use 'which' to check for the compiler's
# presence because Sun's version works differently to everyone else's.  To
# compensate for this we use a special-case Sun only check.

if [ $OSNAME = "SunOS" -a `which cc | grep -c "no cc"` = '1' ] ; then
	if [ -x /opt/SUNWspro/bin/cc ] ; then
		echo "Sun Workshop compiler detected but it's not in the path, rerun make with" ;
		echo "the path set to point to the compiler directories, usually" ;
		echo "/opt/SUNWspro/bin." ;
	else
		echo "Couldn't find a C compiler ('cc' is not present), is your path correct?" ;
	fi ;
	exit 1;
fi

# OS X Snow Leopard broke dlopen(), if it's called from a (sub-)thread then it
# dies with a SIGTRAP.  Specifically, if you dlopen() a shared library linked
# with CoreFoundation from a thread and the calling app wasn't linked with
# CoreFoundation then the function CFInitialize() inside dlopen() checks if
# the thread is the main thread and if it isn't it crashes with a SIGTRAP.
#
# This is now handled in cryptlib.c by disabling asynchronous driver binding,
# the following check is left here in case this isn't sufficient.
#
#if [ $OSNAME = "Darwin" -a `sw_vers -productVersion | grep -c "10\.6"` = '1' ] ; then
#	echo "This version of OS X Snow Leopard may have a buggy dlopen() that crashes" ;
#	echo "with a SIGTRAP when called from a thread.  If the cryptlib self-test dies" ;
#	echo "with the message 'Trace/BPT trap' then add:" ;
#	echo "" ;
#	echo "  #undef USE_THREADS" ;
#	echo "" ;
#	echo "at around line 40 of cryptlib.c and rebuild.  Note that this will disable" ;
#	echo "the use of asynchronous driver binding, which may make startups a little" ;
#	echo "slower." ;
#	echo "" ;
#fi

# Unicos has a broken uname so we override detection.

if [ `uname -m | cut -c 1-4` = 'CRAY' ] ; then
	OSNAME="CRAY" ;
fi

# Get the compiler flags for the default compiler and the OS version and
# make sure that we got a valid result.  This check is necessary because
# sometimes problems (typically sh bugs on some OSes) cause the script to
# bail out without producing any output.  Since the resulting CFLAGS string
# is empty, we add an extra character to the comparison string to avoid
# syntax errors.

if [ $SHARED ] ; then
	CFLAGS="`./tools/ccopts.sh $CC $OSNAME`" ;
else
	CFLAGS="`./tools/ccopts.sh $CC`" ;
fi
if [ '$(CFLAGS)x' = 'x' ] ; then
	echo "$0: Couldn't get compiler flags via tools/ccopts.sh." >&2 ;
	exit 1 ;
fi

OSVERSION="`./tools/osversion.sh $OSNAME`"
if [ `echo $OSVERSION | grep -c '[^0-9]'` -gt 0 ] ; then
	echo "$0: Couldn't correctly determine OS version string '$OSVERSION'." >&2 ;
	exit 1 ;
fi

# Check whether we're doing a cross-compile and provide an early-out for
# this special case.  Since $CROSSCOMPILE is usually a null value we add an
# extra character to the comparison string to avoid syntax errors.

if [ '$(CROSSCOMPILE)x' = '1x' ] ; then
	echo "Cross-compiling for OS target $OSNAME" ;
	CFLAGS="$* `./tools/ccopts.sh $CC $OSNAME` \
			-DOSVERSION=`./tools/osversion.sh $OSNAME`" ;
	if [ $SHARED ] ; then
		make TARGET=$SLIBNAME OBJPATH=$SHARED_OBJ_PATH $CFLAGS $OSNAME ;
	else
		make $CFLAGS $OSNAME ;
	fi ;
fi

# Some systems have the development tools as optional components, or the
# tools are included but suck so much that everyone uses gcc instead (mind
# you recent gcc versions seem to be in a race to catch up on vendor-
# compiler suckage).  For these systems we build with gcc if it's present,
# otherwise we fall back to the native development tools.

checkForGcc()
	{
	OSNAME=$1
	SHARED=$2

	# If we're not running gcc, there's nothing to do
	if gcc -v > /dev/null 2>&1 -lt 0 ; then
		return ;
	fi

	# We don't provide a gcc option for AIX because mixing gcc-created and
	# IBM-sourced components isn't a good idea, and it seems that anyone
	# who's using AIX also has the IBM tools installed with it.
	if [ $OSNAME = "AIX" ] ; then
		return ;
	fi

	# gcc is available, use that by default
	echo "Building with gcc instead of the default $OSNAME compiler."
	echo "  (Re-scanning for build options under gcc)."
	BUILDWITHGCC=1
	if [ $SHARED ] ; then
		CFLAGS="`./tools/ccopts.sh gcc $OSNAME`" ;
	else
		CFLAGS="`./tools/ccopts.sh gcc`" ;
	fi
	}

if [ $OSNAME = "AIX" -o $OSNAME = "HP-UX" -o $OSNAME = "SunOS" ] ; then
	checkForGcc $OSNAME $SHARED ;
fi

# Build the appropriate target with the given compiler.  If we're overriding
# the default cc with gcc we have to recreate $CFLAGS based on the use of
# gcc rather than cc.

buildWithGcc()
	{
	OSNAME=$1
	shift

	make CC=gcc LD=gcc CFLAGS="$* $CFLAGS -DOSVERSION=$OSVERSION" $OSNAME
	}

buildWithNativeTools()
	{
	OSNAME=$1
	CC=$2
	shift
	shift

	make CC=$CC CFLAGS="$* $CFLAGS -DOSVERSION=$OSVERSION" $OSNAME
	}

buildWithGccShared()
	{
	OSNAME=$1
	SLIBNAME=$2
	shift
	shift

	make CC=gcc LD=gcc TARGET=$SLIBNAME OBJPATH=$SHARED_OBJ_PATH \
		 CFLAGS="$* $CFLAGS -DOSVERSION=$OSVERSION" $OSNAME
	}

buildWithNativeToolsShared()
	{
	OSNAME=$1
	SLIBNAME=$2
	CC=$3
	shift
	shift
	shift

	make CC=$CC TARGET=$SLIBNAME OBJPATH=$SHARED_OBJ_PATH \
		 CFLAGS="$* $CFLAGS -DOSVERSION=$OSVERSION" $OSNAME
	}

# Build cryptlib, taking into account OS-specific quirks

if [ $SHARED ] ; then
	if [ $BUILDWITHGCC ] ; then
		buildWithGccShared $OSNAME $SLIBNAME $* ;
	else
		buildWithNativeToolsShared $OSNAME $SLIBNAME $CC $* ;
	fi
else
	if [ $BUILDWITHGCC ] ; then
		buildWithGcc $OSNAME $* ;
	else
		buildWithNativeTools $OSNAME $CC $* ;
	fi
fi
