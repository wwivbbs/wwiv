#!/bin/sh
# Get the compiler to use to build cryptlib
#
# Usage: getcompiler.sh compiler osname

# Make sure that we've been given sufficient arguments.

if [ $# -lt 2 ] ; then
	echo "Usage: $0 compiler osname" >&2 ;
	exit 1 ;
fi

# Juggle the args around to get them the way that we want them.

CC=$1
OSNAME=$2
shift 2

# If we're explicitly building with a compiler wrapper that provides
# extended functionality, don't try and select another compiler.

case "$CC" in
	*"stack-master"*|*"afl-clang"*|*"afl-gcc"*|*"hfuzz-clang"*|*"ccc-analyzer"*)
		echo "$CC" ;
		exit 0 ;
esac

# Some systems (Aches, PHUX, Slowaris) have the development tools as
# optional components, or the tools suck so much that everyone used
# something else instead, typically gcc.
#
# We used to override the native tools with gcc if possible but don't any
# more for two reasons, firstly some of the native tools now suck a bit
# less, and secondly with the general move by everything that could move to
# x86 most users still on Aches/PHUX/Slowaris are there because they've
# invested heavily in the whole ecosystem, so will be using the native tools
# rather than adding gcc.  In some cases this is actually essential, for
# example under AIX mixing gcc-created and IBM-sourced components isn't a
# good idea.  For these reasons we use the system compiler rather than gcc
# if it's available.

if [ "$OSNAME" = "HP-UX" ] ; then
	if command -v clang > /dev/null ; then
		echo "clang" ;
		exit 0 ;
	fi
	if command -v gcc > /dev/null ; then
		echo "gcc" ;
		exit 0 ;
	fi
fi

# If it's Aches or Slowaris then the compiler is hidden somewhere weird
# because it's not installed by default (for Slowaris it's particularly bad,
# see the long comment in buildall.sh), so we try and use that if possible
# for the reason given above.

if [ "$OSNAME" = "SunOS" ] ; then
	if [ $($CC -V 2>&1 | grep -c "Sun C") -gt 0 ] ; then
		echo $CC ;
		exit 0 ;
	fi
fi
if [ "$OSNAME" = "AIX" ] ; then
	if command -v xlc > /dev/null ; then
		echo "xlc" ;
		exit 0 ;
	fi
fi

# If we've got clang installed, default to that.

if command -v clang > /dev/null ; then
	echo "clang" ;
	exit 0 ;
fi

# If cc is gcc, use that.

if [ "$($CC -v 2>&1 | grep -c "gcc")" -gt 0 ] ; then
	echo "gcc" ;
	exit 0 ;
fi

# Use whatever the native cc is

echo "$CC"
