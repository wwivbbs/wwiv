#!/bin/sh
# Build the shared library.
#
# Usage: buildsharedlib.sh [crosscompile] libname osname ld strip version objfiles...

CROSSCOMPILE=0

# Make sure that we've been given sufficient arguments.

if [ "$1" = "crosscompile" ] ; then
	CROSSCOMPILE=1 ;
	shift ;
fi
if [ $# -lt 6 ] ; then
	echo "Usage: $0 [crosscompile] libname osname ld strip version objfiles..." >&2 ;
	exit 1 ;
fi

# Juggle the args around to get them the way that we want them.  What's
# left after this are the object file names.

LIBNAME=$1
OSNAME=$2
LD=$3
STRIP=$4
VERS=$5
shift 5

# The options need to be tuned for some systems since there's no standard
# for shared libraries, and different versions of gcc also changed the way
# that this was handled:
#
# AIX:			AIX requires some weird voodoo which is unlike any other
#				system's way of doing it (probably done by the MVS team,
#				see "AIX Linking and Loading Mechanisms" for a starter).
#				In addition to this, the shared lib (during development)
#				must be given permissions 750 to avoid loading it
#				permanently into the shared memory segment (only root can
#				remove it).  The production shared library must have a
#				555 (or whatever) permission.  Finally, the library has to
#				have a ".a" suffix (even though it's a shared lib), so we
#				tack this on after the $LIBNAME.
#
#				The various AIX options are: '-bnoentry' = don't look for a
#				main(), '-bE' = export the symbols in cryptlib.exp,
#				'-bM:SRE' = make it a shared library.
#				$(LD) -ldl -bE:cryptlib.exp -bM:SRE -bnoentry
# BeOS:			$(LD) -nostart
# Cygwin:		$(LD) -L/usr/local/lib -lcygipc
# HPUX:			Link with gcc (to the GNU libs) if compiled with gcc,
#				otherwise link with the standard compiler into the system
#				libs.  If you're mixing GNU and system libs (e.g. cryptlib
#				built with gcc and the calling app built with the HP cc),
#				you may want to use '-static-libgcc' to avoid having to ship
#				a copy of glibc just for cryptlib.
#				Note that on some versions of PHUX stripping the shared lib.
#				may prevent it from being linked, you may need to remove
#				this command in that case.
#				$(LD) -shared -Wl,-soname,libcl.so.$(MAJ)
# Solaris:		$(LD) -G -ldl -o libcl.so.$(MAJ)
#
# An additional possibility under the *BSDs and Linux is:
#
# *BSDs:		$(LD) -Bshareable -o libcl.so.$(MAJ)
# Linux:		$(LD) -Bshareable -ldl -o libcl.so.$(MAJ)

# Set up any required variables

LINKFILE=link.tmp
LD_IS_GCC=0

# Set up any linker-specific options

if [ "$($LD -v 2>&1 | grep -c gcc)" -gt 0 ] ; then
	LD_IS_GCC=1 ;
	LDFLAGS="$LDFLAGS -shared -Wl,-soname,libcl.so.$VERS" ;
fi

# Create the response file to hold the link command

rm -f $LINKFILE
echo "$@" > $LINKFILE

# Build the shared lib.  The use of x86 asm code causes some issues with
# relocation, to avoid this we use the -Bsymbolic option with the linker,
# which (explicitly) tells the linker to perform symbol lookup in the local
# library first and (implicitly) gets rid of spurious relocations because
# instead of having to perform intra-library references via the global
# offset table using a call to a jump table based at %ebx, the linker
# resolves the reference and uses a direct call as expected.  A nice
# side-effect of this rewiring of indirect to direct calls is that it makes
# it much harder to mess with library internals by getting your own symbols
# referenced before the library-internal ones (for example by using
# LD_PRELOAD), thus redirecting control flow to arbitrary external code.
#
# Note that the various multi-element string arguments $LDFLAGS,
# $(cat $LINKFILE), and getlibs.sh can't be placed in quotes because that
# would turn the output into a single string rather than a list of white-
# space-delimited discrete strings.
#
# The Aches linker supports an alternative to GNU ld's @filename in the form
# of -ffilename, but it requires the contents to be one per line rather than
# a whitespace-delimited string so we use the cat $LINKFILE option instead.
# Similarly, the PHUX linker has -c filename but requires one entry per line.
# Slowaris doesn't support link files at all.
#
# iOS doesn't allow shared libraries because this would allow you to bypass
# their walled garden, so we explicitly disallow them for iOS builds.

case $OSNAME in
	'AIX')
		$LD -o shrlibcl.o -bE:cryptlib.exp -bM:SRE -bnoentry -lpthread \
			$(cat $LINKFILE) $(./tools/getlibs.sh $LD AIX $CROSSCOMPILE) ;
		ar -q "$LIBNAME.a" shrlibcl.o ;
		rm -f shrlibcl.o ;
		chmod 750 "$LIBNAME.a" ;;

	'Android')
		$LD -shared -o "$LIBNAME" @$LINKFILE \
			$(./tools/getlibs.sh $LD Android $CROSSCOMPILE) ;
		$STRIP "$LIBNAME" ;;

	'BeOS' )
		$LD -nostart -o "$LIBNAME" @$LINKFILE \
			$(./tools/getlibs.sh $LD BeOS $CROSSCOMPILE) ;
		$STRIP "$LIBNAME" ;;

	'HP-UX')
		if [ $LD_IS_GCC -gt 0 ] ; then
			$LD $LDFLAGS -o libcl.sl @$LINKFILE \
				$(./tools/getlibs.sh $LD HP-UX $CROSSCOMPILE) ;
		else
			$LD -b -o libcl.sl $(cat $LINKFILE) \
				$(./tools/getlibs.sh $LD HP-UX $CROSSCOMPILE) ;
		fi
		$STRIP libcl.sl ;;

	'iOS')
		echo "iOS doesn't allow shared libraries, you must use a static library." >&2 ;;

	'SunOS')
		if [ $LD_IS_GCC -gt 0 ] ; then
			$LD $LDFLAGS -o "$LIBNAME" @$LINKFILE \
				$(./tools/getlibs.sh $LD SunOS $CROSSCOMPILE) ;
		else
			$LD -G -ldl -o "$LIBNAME" $(cat $LINKFILE) \
				$(./tools/getlibs.sh $LD SunOS $CROSSCOMPILE) ;
		fi
		$STRIP "$LIBNAME" ;;

	*)
		if [ $LD_IS_GCC -gt 0 ] ; then
			if [ "$(gcc -Wl,-Bsymbolic 2>&1 | grep -c unrecognized)" = 0 ] ; then
				$LD $LDFLAGS -Wl,-Bsymbolic -o "$LIBNAME" @$LINKFILE \
					$(./tools/getlibs.sh $LD $OSNAME $CROSSCOMPILE) ;
			else
				$LD $LDFLAGS -o "$LIBNAME" @$LINKFILE \
					$(./tools/getlibs.sh $LD $OSNAME $CROSSCOMPILE) ;
			fi
		else
			$LD -shared -o "$LIBNAME" $(cat $LINKFILE) \
				$(./tools/getlibs.sh $LD $OSNAME $CROSSCOMPILE) ;
		fi
		if [ "$(which objdump)" ] && [ "$(objdump -p $LIBNAME | grep -c TEXTREL)" -gt '0' ] ; then
			echo "Warning: Shared library still contains TEXTREL records." >&2 ;
		fi
		$STRIP "$LIBNAME" ;;
esac
rm -f $LINKFILE
