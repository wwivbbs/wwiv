#!/bin/sh
# Build the shared library.  The options need to be tuned for some systems
# since there's no standard for shared libraries, and different versions of
# gcc also changed the way this was handled:
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
#				$(LD) -shared -Wl,-soname,lib$(PROJ).so.$(MAJ)
# Solaris:		$(LD) -G -ldl -o lib$(PROJ).so.$(MAJ)
#
# An additional possibility under the *BSDs and Linux is:
#
# *BSDs:		$(LD) -Bshareable -o lib$(PROJ).so.$(MAJ)
# Linux:		$(LD) -Bshareable -ldl -o lib$(PROJ).so.$(MAJ)

LINKFILE=link.tmp

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
	echo "$0: Missing linker name." >&2 ;
	exit 1 ;
fi
if [ -z "$4" ] ; then
	echo "$0: Missing 'strip' command name." >&2 ;
	exit 1 ;
fi
if [ -z "$5" ] ; then
	echo "$0: Missing object filenames." >&2 ;
	exit 1 ;
fi

# Juggle the args around to get them the way that we want them.

OSNAME=$1
LIBNAME=$2
LD=$3
STRIP=$4
shift
shift
shift
shift

# Create the response file to hold the link command

rm -f $LINKFILE
echo $* > $LINKFILE

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
# iOS doesn't allow shared libraries because this would allow you to bypass
# their walled garden, so we explicitly disallow them for iOS builds.

case $OSNAME in
	'AIX')
		$LD -o shrlibcl.o -bE:cryptlib.exp -bM:SRE -bnoentry -lpthread \
			`cat $LINKFILE` `./tools/getlibs.sh AIX` ;
		ar -q $LIBNAME.a shrlibcl.o ;
		rm -f shrlibcl.o ;
		chmod 750 $LIBNAME.a ;;

	'Android')
		$LD -shared -o $LIBNAME `cat $LINKFILE` `./tools/getlibs.sh Android` ;
		$STRIP $LIBNAME ;;

	'BeOS' )
		$LD -nostart -o $LIBNAME `cat $LINKFILE` `./tools/getlibs.sh BeOS` ;
		$STRIP $LIBNAME ;;

	'HP-UX')
		if [ $LD = "gcc" ] ; then
			$LD -shared -o libcl.sl `cat $LINKFILE` `./tools/getlibs.sh HP-UX` ;
		else
			$LD -b -o libcl.sl `cat $LINKFILE` `./tools/getlibs.sh HP-UX` ;
		fi
		$STRIP libcl.sl ;;

	'iOS')
		echo "iOS doesn't allow shared libraries, you must use a static library." ;;

	'SunOS')
		if [ $LD = "gcc" ] ; then
			$LD -shared -o $LIBNAME `cat $LINKFILE` `./tools/getlibs.sh autodetect` ;
		else
			$LD -G -ldl -o $LIBNAME `cat $LINKFILE` `./tools/getlibs.sh autodetect` ;
		fi
		$STRIP $LIBNAME ;;

	*)
		if [ `$LD -v 2>&1 | grep -c gcc` -gt 0 -a \
			`gcc -Wl,-Bsymbolic 2>&1 | grep -c unrecognized` = 0 ] ; then
			$LD -shared -Wl,-Bsymbolic -o $LIBNAME `cat $LINKFILE` `./tools/getlibs.sh autodetect` ;
		else
			$LD -shared -o $LIBNAME `cat $LINKFILE` `./tools/getlibs.sh autodetect` ;
		fi
		if [ `which objdump` -a `objdump -p $LIBNAME | grep -c TEXTREL` -gt '0' ] ; then
			echo "Warning: Shared library still contains TEXTREL records." ;
		fi
		$STRIP $LIBNAME ;;
esac
rm -f $LINKFILE
