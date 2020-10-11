#!/bin/sh
# Build all cryptlib modules
#
# Usage: buildall.sh [shared] [analyse|special] make compiler osname flags

SHARED=0
ANALYSE=0
ISSPECIAL=0

# Make sure that we've been given sufficient arguments.

if [ "$1" = "shared" ] ; then
	SHARED=1 ;
	shift ;
fi
if [ "$1" = "analyse" ] ; then
	ANALYSE=1 ;
	shift ;
elif [ "$1" = "special" ] ; then
	ISSPECIAL=1 ;
	shift ;
fi
if [ $# -lt 4 ] ; then
	echo "Usage: $0 [shared] [analyse|special] make compiler osname flags" >&2 ;
	exit 1 ;
fi

# Juggle the args around to get them the way that we want them.

MAKE=$1
CC=$2
OSNAME=$3
shift 3

# Additional constants from the makefile.

MAJ="3"
MIN="4"
PLV="5"
PROJ="cl"
SHARED_OBJ_PATH="./shared-obj/"
if [ "$OSNAME" = "Darwin" ] ; then
	SLIBNAME="lib$PROJ.$MAJ.$MIN.dylib" ;
else
	SLIBNAME="lib$PROJ.so.$MAJ.$MIN.$PLV" ;
fi

# More SunOS braindamage, deal with with Sun's totally braindamaged handling
# of compiler installs in which cc may be either a shell script telling you
# that there's no compiler installed, an actual compiler, or gcc.  Also, if
# it's an actual compiler than it may be some ancient version, with a newer
# version hidden in a large range of Sun-specific directories whose naming
# convention changes every 1-2 versions (seriously!).  Further also,
# although there's supposed to be a link from /opt/SUNWspro/bin to the
# current version's /bin, it's often some ancient version that was installed
# years ago and the link never got updated.
#
# To deal with this insanity we use a list of possible paths to see whether
# there's a recent version of the Sun compiler installed, walking down
# through older and older installs until we either find something or run out
# of paths to search on.  Once we find a hit, we check whether it matches cc.
# If it does then we're done.  If not, we check whether it matches
# /opt/SUNWspro/bin and tell the user to use that.  Finally, if neither
# match then we tell them to use the path to the newer cc that we've found.
#
# First we need locations of the compiler, which for the list below goes
# back to 2005.  Note how the locations change every few versions, as does
# the software name: SPARCworks, SunSoft Workshop, Forte Developer, Sun ONE
# Studio, Sun Studio, Oracle Solaris Studio, and Oracle Developer Suite.
#
# Note also that the 12.7 entry is speculative for the next release, it
# didn't change from 12.5 to 12.6 so it's due for a change again.

SUNCCPATHS="/opt/developerstudio12.7/bin/cc /opt/developerstudio12.6/bin/cc \
/opt/developerstudio12.5/bin/cc /opt/solarisstudio12.4/bin/cc \
/opt/solarisstudio12.3/bin/cc /opt/solstudio12.2/bin/cc \
/opt/studio/sunstudio12.1/bin/cc /opt/sunstudio12/bin/cc \
/opt/sunstudio11/SUNWspro/bin/cc /opt/sunstudio10/SUNWspro/bin/cc"
SUNCC=0

checkSunCompilerVersion()
	{
	CC=$1
	SUNCCPATH=$2
	SUNWSSTRING=""

	# Get the compiler version info for each of the three possible locations
	# where a compiler could be hidden.
	CCSTRING="$($CC -V 2>&1 | grep "Sun C")"
	SUNCCSTRING="$($SUNCCPATH -V 2>&1 | grep "Sun C")"
	if [ -f /opt/SUNWspro/bin/cc ] ; then
		SUNWSSTRING="$(/opt/SUNWspro/bin/cc -V 2>&1 | grep "Sun C")" ;
	fi

	# If $CC is the latest version, we're done.
	if [ "$SUNCCSTRING" = "$CCSTRING" ] ; then
		SUNCC=1 ;
		return ;
	fi

	# $CC isn't the latest version, tell the user that they need to
	# explicitly enable use of a newer version.
	if [ "$SUNCCSTRING" = "$SUNWSSTRING" ] ; then
		echo "Sun Workshop compiler detected at /opt/SUNWspro/bin/cc but it's more recent" >&2 ;
		echo "than $CC, rerun make with the path set to point to the compiler directories" >&2 ;
		echo "at /opt/SUNWspro/bin." >&2 ;
	else
		echo "Sun Workshop compiler detected but it's not in the path, rerun make with the" >&2 ;
		echo "path set to point to the compiler directories at "$(dirname $SUNCCPATH)"." >&2 ;
	fi
	exit 1;
	}

if [ "$OSNAME" = "SunOS" ] ; then
	for sunccpath in $SUNCCPATHS ; do
		if [ -f $sunccpath ] ; then
			checkSunCompilerVersion $CC $sunccpath ;
			break ;
		fi
	done
	if [ $SUNCC -eq 0 ] && [ "$($CC -v 2>&1 | grep -c "gcc")" -gt 0 ] ; then
		echo "Sun compiler not detected but gcc is present, using that instead." >&2 ;
	fi
fi

# Get the compiler that we'll be using.  This takes the given $CC and
# substitutes are more preferred one if available, unless $CC is a custom
# compiler like a static source code analyser or fuzzer build.

CC="$(./tools/getcompiler.sh $CC $OSNAME)"

# OS X Snow Leopard broke dlopen(), if it's called from a (sub-)thread then it
# dies with a SIGTRAP.  Specifically, if you dlopen() a shared library linked
# with CoreFoundation from a thread and the calling app wasn't linked with
# CoreFoundation then the function CFInitialize() inside dlopen() checks if
# the thread is the main thread and if it isn't it crashes with a SIGTRAP.
#
# This is now handled in cryptlib.c by disabling asynchronous driver binding,
# the following check is left here in case this isn't sufficient.
#
#if [ $OSNAME = "Darwin" -a `sw_vers -productVersion | grep -c "10\.6"` -gt 0 ] ; then
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

# Detect various broken versions of gcc

if [ "$($CC -v 2>&1 | grep -c "gcc")" -gt 0 ] ; then

  # Older OS X boxes shipped with an incredibly buggy Apple-hacked version of
  # gcc 4.0.1, cryptlib contains some workarounds for this but other bugs
  # are too difficult to track down, so we have to declare it to be an
  # unsupported environment.

  if [ "$OSNAME" = "Darwin" ] && [ "$($CC -dumpversion | grep -c "4\.0\.1")" -gt 0 ] ; then
	echo "This version of OS X ships with an extremely buggy, Apple-hacked version of" ;
	echo "gcc 4.0.1 that's more than a decade out of date.  Please upgrade to a newer " ;
	echo "compiler to build cryptlib." ;
	exit 1 ;
  fi

  # Slowaris 11 similarly shipped with a buggy version of gcc 4.8 (described
  # as "pretty much messed up at this point" in
  # https://mail-index.netbsd.org/pkgsrc-users/2014/08/27/msg020283.html) so
  # we similarly have to declare it to be an unsupported environment.

  if [ "$OSNAME" = "SunOS" ] && [ "$($CC -dumpversion | grep -c "4\.8")" -gt 0 ] ; then
	echo "This version of Solaris ships with a buggy version of gcc 4.8 that produces" ;
	echo "broken builds due to stack smashing protection (SSP) options being messed" ;
	echo "up.  Please upgrade to a newer compiler, or even just something other than" ;
	echo "gcc 4.6 or 4.8, to build cryptlib." ;
	exit 1 ;
  fi

fi

# Unicos has a broken uname so we override detection.

if [ "$(uname -m | cut -c 1-4)" = 'CRAY' ] ; then
	OSNAME="CRAY" ;
fi

# Get the compiler flags for the compiler and the OS version and make sure
# that we got a valid result.  This check is necessary because sometimes
# problems (typically sh bugs on some OSes) cause the script to bail out
# without producing any output.  Since the resulting CFLAGS string is empty,
# we add an extra character to the comparison string to avoid syntax errors.

if [ $ANALYSE -gt 0 ] ; then
	CFLAGS="$(./tools/ccopts.sh analyse $CC $OSNAME)" ;
elif [ $ISSPECIAL -gt 0 ] ; then
	CFLAGS="$(./tools/ccopts.sh special $CC $OSNAME)" ;
elif [ $SHARED -gt 0 ] ; then
	CFLAGS="$(./tools/ccopts.sh shared $CC $OSNAME)" ;
else
	CFLAGS="$(./tools/ccopts.sh $CC $OSNAME)" ;
fi
if [ '$(CFLAGS)x' = 'x' ] ; then
	echo "$0: Couldn't get compiler flags via tools/ccopts.sh." >&2 ;
	exit 1 ;
fi

OSVERSION="$(./tools/osversion.sh $OSNAME)"
if [ -z "$OSVERSION" ] || \
   [ "$(echo $OSVERSION | grep -c '[^0-9]')" -gt 0 ] ; then
	echo "$0: Couldn't correctly determine OS version string '$OSVERSION'." >&2 ;
	exit 1 ;
fi

# Check whether we're doing a cross-compile and provide an early-out for
# this special case.  Since $CROSSCOMPILE is usually a null value we add an
# extra character to the comparison string to avoid syntax errors.

if [ '$(CROSSCOMPILE)x' = '1x' ] ; then
	echo "Cross-compiling for OS target $OSNAME" ;
	CFLAGS="$* $(./tools/ccopts-crosscompile.sh $CC $OSNAME) \
			-DOSVERSION=$(./tools/osversion.sh $OSNAME)" ;
	if [ $SHARED -gt 0 ] ; then
		$MAKE TARGET="$SLIBNAME" OBJPATH="$SHARED_OBJ_PATH" "$CFLAGS" "$OSNAME" ;
	else
		$MAKE "$CFLAGS" "$OSNAME" ;
	fi ;
fi

# Build cryptlib.

if [ $SHARED -gt 0 ] ; then
	$MAKE CC="$CC" LD="$CC" TARGET="$SLIBNAME" OBJPATH="$SHARED_OBJ_PATH" \
		  CFLAGS="$* $CFLAGS -DOSVERSION=$OSVERSION" "$OSNAME" ;
else
	$MAKE CC="$CC" LD="$CC" CFLAGS="$* $CFLAGS -DOSVERSION=$OSVERSION" "$OSNAME" ;
fi
