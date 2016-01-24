#!/bin/sh
# Obtain appropriate gcc options for building cryptlib during a cross-compile.
# This is a stripped-down version of tools/ccopts.sh which only performs
# those checks that are safe in a cross-compile build, which in general means
# checking for gcc bugs, see tools/ccopts.sh for an explanation of what the
# following does.

CCARGS=""

if [ $# -ne 1 ] ; then
	echo "$0: Missing compiler name." >&2 ;
	exit 1 ;
fi
CC=$1

GCC_VER=`$CC -dumpversion | tr -d  '.' | cut -c 1-2`

if [ $GCC_VER -ge 40 ] ; then
	if [ `$CC -Wno-pointer-sign -S -o /dev/null -xc /dev/null > /dev/null 2>&1` ] ; then
		CCARGS="$CCARGS -Wno-pointer-sign" ;
	fi ;
fi

echo $CCARGS
