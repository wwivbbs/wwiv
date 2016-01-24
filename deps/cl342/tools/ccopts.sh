#!/bin/sh
# Obtain appropriate cc options for building cryptlib.  This used to be done
# in the makefile, but with the introduction of gcc 4.x with its idiotic
# default of enabling masses of pointless warnings with no easy way to turn
# them off (the excessive warnings weren't added until 4.x so the option to
# disable them didn't exist before then either, with the result that using it
# with earlier versions causes them to die), it became too complex to handle
# in the makefile, so this shell script is used instead.

CCARGS=""
OSNAME=`uname`

# Make sure that we've been given either single argument consisting of the
# compiler name or the compiler name and an OS name for the shared-lib cc
# options.

if [ "$1" = "" ] ; then
	echo "$0: Missing compiler name." >&2 ;
	exit 1 ;
fi
if [ $# -eq 2 ] ; then
	if [ "$2" != "autodetect" ] ; then
		OSNAME=$2 ;
	fi ;
else
	if [ $# -ne 1 ] ; then
		echo "$0: Can only supply 1 arg." >&2 ;
		exit 1 ;
	fi ;
fi

# Juggle the args around to get them the way that we want them.

CC=$1

# Determine the CPU endianness by building and executing the endianness-
# detection program.  Note that we have to use -s rather than the more
# obvious -e since this doesn't exist under the Slowaris sh.

if [ ! -s ./tools/endian ] ; then
	if gcc -v > /dev/null 2>&1 ; then
		gcc ./tools/endian.c -o ./tools/endian > /dev/null ;
	elif [ `uname` = "NONSTOP_KERNEL" ] ; then
		c89 ./tools/endian.c -o ./tools/endian > /dev/null ;
	else
		$CC ./tools/endian.c -o ./tools/endian > /dev/null ;
	fi ;
	strip tools/endian ;
	if [ ! -s ./tools/endian ] ; then
		echo "Couldn't build endianness-checking program ./tools/endian" >&2 ;
		exit 1 ;
	fi ;
fi

CCARGS="`./tools/endian`"

# Check whether we're building on one of the development clusters, which
# allows enabling various unsafe test-only options.  We have to be a bit
# careful with some of the machines (CS login02, fish, and the Gnu compile 
# farm) because they don't use FQDNs for the machines, so we check as much 
# as we can and (for the Gnu compile farm) only allow machines on a 
# whitelist to narrow down false positives (unfortunately the filtering 
# isn't terribly restrictive because "Linux x86-64" is practically
# universal).

ISDEVELOPMENT=0
if [ `uname -s` = "Linux" ] ; then
	if [ `hostname -f | grep -c login0[0-9].fos.auckland.ac.nz` -gt 0 ] ; then
		ISDEVELOPMENT=1 ;
	elif [ `hostname -f` = "pakastelohi.cypherpunks.to" ] ; then
		ISDEVELOPMENT=1 ;
	elif [ `uname -n | grep -c gcc[0-9][0-9]` -gt 0 ] ; then
		case `uname -n` in
			'gcc10'|'gcc33'|'gcc40'|'gcc51'|'gcc54'|'gcc55'|'gcc61')
				ISDEVELOPMENT=1 ;;
		esac ; 
	fi 
fi

# Check whether we're running using the clang static analyser

ISCLANG=`echo $CC | grep -c "ccc-analyzer"`

# Determine whether various optional system features are installed and
# enable their use if they're present.  Since these additional libs are
# dynamically loaded, we only check for them on systems with dynamic
# loading support.  We could also check for the presence of
# /usr/include/dlfcn.h, but this can lead to false positives on systems
# that have dummy a dlfcn.h for compatibility reasons.
#
# When indicating the presence of a subsystem, we set the HAS_xxx flag to
# indicate its presence rather than unconditionally setting the USE_xxx
# flag.  This allows the facility to be disabled in config.h if required.
# An exception to this is if we're building on a development system in
# which case we always enable it unconditionally.

HASDYNLOAD=0
case $OSNAME in
	'Darwin'|'Linux'|'FreeBSD')
		HASDYNLOAD=1 ;;

	'SunOS')
		if [ `./tools/osversion.sh SunOS` -gt 4 ] ; then
			HASDYNLOAD=1 ;
		fi ;;

	'HP-UX')
		if [ `./tools/osversion.sh HP-UX` -gt 10 ] ; then
			HASDYNLOAD=1 ;
		fi ;;
esac
if [ $HASDYNLOAD -gt 0 ] ; then
	if [ -f /usr/include/sql.h ] ; then
		echo "ODBC interface detected, enabling ODBC support." >&2 ;
		CCARGS="$CCARGS -DHAS_ODBC" ;
		if [ $ISDEVELOPMENT -gt 0 ] ; then
			CCARGS="$CCARGS -DUSE_ODBC" ;
		fi ;
	elif [ -f /usr/local/include/sql.h ] ; then
		echo "ODBC interface detected, enabling ODBC support." >&2 ;
		CCARGS="$CCARGS -DHAS_ODBC -I/usr/local/include" ;
	elif [ "$OSNAME" = "HP-UX" -a -f /usr/include/hpodbc/sql.h ] ; then
		echo "ODBC interface detected, enabling ODBC support." >&2 ;
		CCARGS="$CCARGS -DHAS_ODBC -I/usr/include/hpodbc" ;
	fi
	if [ -f /usr/include/ldap.h ] ; then
		echo "LDAP interface detected, enabling LDAP support" >&2 ;
		CCARGS="$CCARGS -DHAS_LDAP" ;
		if [ $ISDEVELOPMENT -gt 0 ] ; then
			CCARGS="$CCARGS -DUSE_LDAP" ;
		fi ;
	fi
	if [ -f /usr/include/pkcs11.h -o -f /usr/include/security/pkcs11.h -o \
		 -f /usr/include/opensc/pkcs11.h -o -f /usr/local/include/pkcs11.h ] ; then
		echo "PKCS #11 interface detected, enabling PKCS #11 support." >&2 ;
		CCARGS="$CCARGS -DHAS_PKCS11" ;
		if [ $ISDEVELOPMENT -gt 0 ] ; then
			CCARGS="$CCARGS -DUSE_PKCS11" ;
		fi ;
	fi
	if [ -f /opt/nfast/toolkits/pkcs11/libcknfast.so -o \
		 -f /usr/lib/libcknfast.so ] ; then
		echo "  (Enabling use of nCipher PKCS #11 extensions)." >&2 ;
		CCARGS="$CCARGS -DNCIPHER_PKCS11" ;
	fi
fi

# If we're building on the usual development box, enable various unsafe
# options that are normally disabled by default

if [ $ISDEVELOPMENT -gt 0 ] ; then
	echo "  (Enabling unsafe source code options for development version)." >&2 ;
	CCARGS="$CCARGS -DUSE_CERT_DNSTRING -DUSE_DNSSRV -DUSE_ECC" ;
fi

# If we're building with the clang static analyser, set options specific to
# that.  clang uses gcc as a front-end with -Wall enabled so we have to
# disable the false-positive-inducing options for that as per the long
# comment at the end of this file for the -Wall debug build.  We also turn
# off tautological-comparison warnings because the use of range checking
# in REQUIRES() predicates gives nothing but false positives.

if [ $ISCLANG -gt 0 ] ; then
	echo "  (Enabling options for clang)." >&2 ;
	CCARGS="$CCARGS -Wno-switch-enum -Wno-tautological-compare" ;
fi

# If we're building a shared lib, set up the necessary additional cc args.
# The IRIX cc and Cygwin/MinGW gcc (and for Cygwin specifically Cygwin-
# native, not a cross-development toolchain hosted under Cygwin) don't 
# recognise -fPIC, but generate PIC by default anyway.  The PHUX compiler 
# requires +z for PIC, and Solaris cc requires -KPIC for PIC.  OS X 
# generates PIC by default, but doesn't mind having -fPIC specified anyway.  
# In addition it requires -fno-common for DYLIB use.
#
# For the PIC options, the only difference between -fpic and -fPIC is that
# the latter generates large-displacement jumps while the former doesn't,
# bailing out with an error if a large-displacement jump would be required.
# As a side-effect, -fPIC code is slightly less efficient because of the use
# of large-displacement jumps, so if you're tuning the code for size/speed
# you can try -fpic to see if you get any improvement.

if [ $# -eq 2 ] ; then
	case $OSNAME in
		'Darwin')
			CCARGS="$CCARGS -fPIC -fno-common" ;;

		'CYGWIN_NT-5.0'|'CYGWIN_NT-5.1'|'CYGWIN_NT-6.1')
			;;
		
		'HP-UX')
			CCARGS="$CCARGS +z" ;;

		'IRIX'|'IRIX64')
			;;

		'MINGW_NT-5.0'|'MINGW_NT-5.1'|'MINGW_NT-6.1')
			;;

		'SunOS')
			if [ `$CC -v 2>&1 | grep -c "gcc"` = '0' ] ; then
				CCARGS="$CCARGS -KPIC" ;
			else
				CCARGS="$CCARGS -fPIC" ;
			fi ;;

		*)
			CCARGS="$CCARGS -fPIC" ;;
	esac ;
fi

# Conversely, if we're building a static lib and the system requires it, set
# up static lib-specific options.

if [ $# -ne 2 ] ; then
	case $OSNAME in

		'BeOS')
			CCARGS="$CCARGS -D_STATIC_LINKING" ;;
	esac ;
fi

# If the system supports recursive and/or robust mutexes, indicate that
# they're available.  We don't use recursive mutexes by default because they
# tend to be somewhat hit-and-miss but we at least indicate their presence
# via a define.

if [ -f /usr/include/pthread.h ] ; then
	if [ `grep -c PTHREAD_MUTEX_RECURSIVE /usr/include/pthread.h` -ge 0 ] ; then
		CCARGS="$CCARGS -DHAS_RECURSIVE_MUTEX" ;
	fi ;
	if [ `grep -c PTHREAD_MUTEX_ROBUST /usr/include/pthread.h` -ge 0 ] ; then
		CCARGS="$CCARGS -DHAS_ROBUST_MUTEX" ;
	fi ;
fi

# If we're not using gcc, we're done.  This isn't as simple as a straight
# name comparison of cc vs. gcc, sometimes gcc is installed as cc so we
# have to check whether the compiler is really gcc even if it's referred to
# as cc.  In addition we have to be careful about which strings we check for
# because i18n of the gcc -v output makes many strings highly mutable.  The
# safest value to check for is "gcc", hopefully this won't yield any false
# positives (apart from Aches, see below).
#
# To make things more entertaining, the Aches cc displays a manpage in
# response to 'cc -v' (!!) and the manpage mentions a gcc-compatibility
# feature so the compiler is misidentified as gcc.  To work around this we
# perform a special-case check for Aches and use a somewhat more direct
# check for gcc, which is likely to be explicitily installed as 'gcc' rather
# than the system 'cc'.

if [ $OSNAME = "AIX" ] ; then
	if [ `which cc | grep -c "gcc"` = '0' ] ; then
		echo $CCARGS ;
		exit 0 ;
	fi ;
fi

if [ `$CC -v 2>&1 | grep -c "gcc"` = '0' ] ; then
	echo $CCARGS ;
	exit 0 ;
fi

# Find out which version of gcc we're using.  The check for the gcc version
# is equally complicated by the fact that a (rare) few localised gcc's don't
# use a consistent version number string.  Almost all versions print "gcc
# version", but the French localisation has "version gcc" (we can't use just
# "gcc" by itself since this appears elsewhere in the gcc -v output).
#
# To make things even more confusing, Apple's hacked-up gcc branch prints
# something like "PowerPC-Apple-Is-Great-I-Love-Darwin-4567-Hup234-gcc-x.y.z",
# so any simple attempt at extracting what looks like a version number will
# fail.  The only way to get around this is to look for the first set of
# numeric values that follow the string "gcc" and use that as the version
# number.
#
# In order to avoid this mess we use the "-dumpversion" option, which has
# worked since at least 2.7.2 although it wasn't actually documented until
# the first 3.x releases).

GCC_VER=`gcc -dumpversion | tr -d  '.' | cut -c 1-2`

# Try and determine the CPU type.  This is made more complex by a pile of
# *BSE's which, along with antideluvian tools like an as that doesn't
# recognise 486 opcodes, report the CPU type as i386.  Even sysctl reports
# the CPU as being i386, so if we find this we assume it's some *BSE which
# is actually running on a P4 or Athlon or something similar (unfortunately
# there's no real way to detect this, but it's 99.9% likely that it's not
# actually still running on an 80386).

ARCH=`uname -m`

if [ "$ARCH" = "i386" -a `uname | grep -c BSD` = '1' ] ; then
	echo "Warning: uname/sysctl reports that this machine is using an 80386 CPU (!!)," >&2 ;
	echo "         continuing under the assumption that it's at least a Pentium." >&2 ;
	echo >&2 ;
	ARCH="i586" ;
fi

# gcc changed its CPU architecture-specific tuning option from -mcpu to
# -march in about 2003, so when using gcc to build for x86 systems (where
# we specify the architecture as P5 rather than the default 386) we have
# to use an intermediate build rule that changes the compiler arguments
# based on compiler version info.  The reason for the change was to
# distinguish -march (choice of instruction set used) from -mtune
# (scheduling of instructions), so for example -march=pentium
# -mtune=pentium4 would generate instructions from the pentium instruction
# set but scheduled for the P4 CPU.
#
# (The changeover is in fact somewhat messier than that, newer 2.9.x versions
# (as well as 3.x onwards) recognised -march (depending on the CPU they
# targeted and patch level) and all versions still recognise -mcpu, however
# as of about 3.4.x the compiler complains about deprecated options whenever
# it sees -mcpu used, which is why we use -march for 3.x and newer).
#
# As of version 4.2.0, gcc finally supports an option "optimise for the
# machine I'm building on", eliminating the need to perform complex
# guesswork for the CPU type, so if we're using any recent version we use
# this by default.  If not, we fall back to guessing, but since it's not
# really possible to determine the exact CPU type the only options that we
# have (aside from the broken *BSE's reporting of "80386" mentioned above)
# are "586" (generic pre-MMX(!!) Pentium), "686" (generic Pentium Pro), and
# "x86-64" (generic x86-64).  The lowest common denominator is the generic
# "pentium", which just means "something better than the default 80386",
# unfortunately for x86-64 there's both no way to tell whose x86-64 we're
# running on and no way to tell gcc that we want either generic Intel x86-64
# or AMD x86-64.  The best that we can do is use "opteron", which is for
# generic Opteron/Athlon64/K8/Athlon FX processors, but also works for
# Intel's x86-64.
#
# Unfortunately handling of "-march=native" is pretty comprehensively broken
# for the gcc 4.2 versions because of the hit-and-miss way that the
# information is passed by the compiler driver to the compiler back-end.
# Sometimes it works, sometimes it produces a "bad value (native) for
# -march= switch; bad value (native) for -mtune= switch" error, and
# sometimes it just bails out and falls back to "-march=generic" which
# often produces very poor code.  As a result it's not safe to enable the
# use of this option.
#
# If this ever gets fixed it can be enabled with:
#
#	if [ "$GCC_VER" -ge 43 ] ; then
#		CCARGS="$CCARGS -march=native" ;
#	elif [ "$GCC_VER" -ge 30 ] ; then
#
# (or whatever version gcc starts handling it properly at).
#
# For x86-64 we have to enable the use of PIC because of obscure linker
# errors ("relocation R_X86_64_32S can not be used when making a shared
# object") that crop up when the static-lib version of cryptlib is used
# in situations that also use shared libs, in the case of x86-64 the use
# of PIC should have minimum overhead so it shouldn't be a big deal.

if [ "$ARCH" = "i586" -o "$ARCH" = "i686" -o "$ARCH" = "x86_64" ] ; then
	if [ "$GCC_VER" -ge 30 ] ; then
		case $ARCH in
			'x86_64')
				CCARGS="$CCARGS -march=opteron -fPIC" ;;

			'i686')
				CCARGS="$CCARGS -march=pentiumpro" ;;

			*)
				CCARGS="$CCARGS -march=pentium" ;;
		esac ;
	else
		CCARGS="$CCARGS -mcpu=pentium" ;
	fi ;
fi

# gcc 4.x for 64-bit architectures has an optimiser bug that removes an
# empty-list check in cryptlib's list-management code (this has been
# verified for at least 4.0.x and 4.1.x for x86-64 and ppc64).  When running
# the self-test, this is first detectable in cert/dn.c in the function
# deleteComponent(), where the missing check for an empty list causes a
# segfault when the code tries to access a nonexistent list element.
# There's not much that we can do about this except warn the user.
#
# (Update: Rearranging some lines in the source causes the compiler to emit
#  correct code, so hopefully this shouldn't be necessary any longer).
#
# In theory we should also use '-s' for read to turn off echoing of
# keystrokes, however not all shells (e.g. Debian's stupid dash, which is
# symlinked to /bin/sh in some distros) support this.
#
#if [ "$GCC_VER" -ge 40 -a '(' "$ARCH" = "x86_64" -o "$ARCH" = "ppc64" ')' ] ; then
#	echo >&2 ;
#	echo "Warning: The version of gcc that this system uses has an optimiser bug in" >&2 ;
#	echo "         its 64-bit code generation.  If the cryptlib self-test segfaults" >&2 ;
#	echo "         during the certificate self-test, rebuild the code with -O2" >&2 ;
#	echo "         instead of the current -O3." >&2 ;
#	read -n1 -p "Hit a key..." ;
#	echo >&2 ;
#fi

# gcc 4.x changed the way that it performs optimisation so that -O3 often
# results in the creation of far larger binaries than -O2, with ensuing poor
# cache localisation properties.  In addition it enhances the triggering of
# gcc optimiser bugs, something that seems to be particularly bad in 4.x.
# While cryptlib contains numerous code-generation bug workarounds for gcc 4.x
# (and 3.x, and 2.x), the potential performance problems with -O3 means that
# it's better to just turn it off.

if [ "$OSNAME" = "Linux" -o "$OSNAME" = "FreeBSD" -o \
	 "$OSNAME" = "NetBSD" -o "$OSNAME" = "OpenBSD" -o \
	 "$OSNAME" = "Darwin" ] ; then
	if [ $GCC_VER -ge 40 ] ; then
		CCARGS="$CCARGS -O2" ;
	else
		CCARGS="$CCARGS -O3" ;
	fi ;
fi

# Check for gcc 4.x with its stupid default setting of -Wpointer-sign,
# which leads to endless warnings about signed vs.unsigned char problems -
# you can't even call strlen() without getting warnings.
#
# Older versions of Solaris' sh can't handle the test below as a single line
# so we have to break it apart into two lines.  In addition without the
# backquotes the script will silently exit at this point (!!) so we quote the
# argument to 'test'.
#
# Unfortunately enabling C99 with gcc (see below) also enables the C99
# aliasing rules, or at least endless whiny warnings about potential
# problems with C99 aliasing rules, reported as type punning by gcc 4.x.
# Because of the way the cryptlib kernel works there's no way to work around
# this (well, except for horrible kludges with unions) because it uses a
# generic message-payload type that's always passed as a void *.  There's no
# easy way to fix this, we could in theory perform a massive janitorial run-
# through applying intermediate void casts between source and target (e.g.
# '( struct foo * ) ( void * ) whatever') but this just masks the problem,
# makes the code look ugly, and could quite well hide other problems because
# of the make-it-go-away void cast.  Telling the compiler to shut is far
# cleaner, since it doesn't seem to have any effect on the code generated
# anyway.

if [ $GCC_VER -ge 40 ] ; then
	if [ `$CC -Wno-pointer-sign -S -o /dev/null -xc /dev/null 2>&1 | grep -c "unrecog"` -eq 0 ] ; then
		CCARGS="$CCARGS -Wno-pointer-sign" ;
	fi ;
	CCARGS="$CCARGS -Wno-strict-aliasing" ;
fi

# The AES code uses 64-bit data types, which gcc doesn't support (at least
# via limits.h) unless it's operating in C99 mode.  So in order to have the
# AES auto-config work we have to explicitly run gcc in C99 mode, which
# isn't the default for the gcc 3.x versions.  Since the code also uses gcc
# extensions we have to specify the mode as gcc + C99, not just C99.
# Finally, older versions of gcc don't understand C99 so we have to surround
# this with a guard based on the version number.

if [ $GCC_VER -ge 30 ] ; then
	CCARGS="$CCARGS -std=gnu99" ;
fi

# Enable stack protection and extra checking for buffer overflows if it's
# available.  This was introduced (in a slightly hit-and-miss fashion) in
# later versions of gcc 4.1.x, to be on the safe side we only enable it
# for gcc 4.2 and newer.
#
# Note that this may require adding '-lssp -fno-stack-protector' to the
# linker command line when building the self-test code.

if [ $GCC_VER -ge 42 ] ; then
  if [ `$CC -fstack-protector -S -o /dev/null -xc /dev/null 2>&1 | grep -c "unrecog"` -eq 0 ] ; then
	CCARGS="$CCARGS -fstack-protector" ;
  fi ;
  CCARGS="$CCARGS -D_FORTIFY_SOURCE=2" ;
fi

# Newer versions of gcc support marking the stack as nonexecutable (e.g.
# using the x86-64 NX bit), so if it's available we enable it.  This is
# easier than the alternative of adding a:
#
# #if defined( __linux__ ) && defined( __ELF__ )
#   .section .note.GNU-stack, "", %progbits
# #endif
#
# to .S files since (a) we don't control most of the .S files and (b)
# some of the code is inline asm in C functions.
#
# Unfortunately this isn't possible to check for easily, at best we can
# do something like:
#
# if (echo|as --noexecstack -o /dev/null > /dev/null 2>&1); then
#	CCARGS="$CCARGS -Wa,--noexecstack" ;
# fi
#
# (which is necessary because no two assemblers have a consistent command-
# line interface so that we can't even reliably get version information as
# we can for gcc) but even this is problematic because even if the assembler
# claims to support it actual handling is still rather hit-and-miss.

# Enable additional compiler diagnostics if we're building on the usual
# development box.  We only enable it on this one system to avoid having
# users complain about getting warnings when they build it.
#
# The warnings are:
#
# -Waddress: Warn about suspicious use of memory addresses, e.g. 
#		'x == "abc"'  (-Wall).
#
# -Waggregate-return: Warn about functions that return structs.
#
# -Warray-bounds: Warn about out-of-bounds array accesses, requires the use 
#		of -ftree-vrp (which is enabled for -O2 and above) (-Wall).
#
# -Wcast-align: Warn whenever a pointer is cast such that the required
#		alignment of the target is increased, for example if a "char *" is
#		cast to an "int *".
#
# -Wchar-subscripts: Warn when an array has a char subscript (-Wall).
#
# -Wdeclaration-after-statement: Warn about a variable declaration found 
#	after a statement in a function (VC++ 6.0 complains about this too).
#
# -Wendif-labels: Warn if an endif is followed by text.
#
# -Wempty-body: Warn if an empty body occurs in and if/else or do/while.
#
# -Wextra: Extra warnings on top of -Wall
#
# -Wformat: Check calls to "printf" etc to make sure that the args supplied
#		have types appropriate to the format string (-Wall).
#
# -Wformat-nonliteral: Check whether a format string is not a string literal,
#		i.e. argPtr vs "%s".
#
# -Wformat-security: Check for potential security problems in format strings.
#
# -Wimplicit-int: Warn about typeless variable declarations (-Wall)
#
# -Winit-self: Warn if a value is initialised to itself, e.g. 'int i=i'.
#
# -Wjump-misses-init: Warn if a goto or switch misses initialisation of a
#		variable.
#
# -Wlogical-op: Warn about suspicious use of logical operators in 
#		expressions, e.g. '|' vs '||'.
#
# -Wmissing-braces: Warn if an array initialiser isn't fully bracketed, e.g.
#		int a[2][2] = { 0, 1, 2, 3 } (-Wall).
#
# -Wnonnull: Warn about passing a null for function args tagged as being
#		__nonnull (-Wall).
#
# -Wparentheses: Warn about missing parantheses where the resulting
#		expression is ambiguous (or at least nonobvious) (-Wall).
#
# -Wpointer-arith: Warn about anything that depends on the sizeof a
#		function type or of void.
#
# -Wredundant-decls: Warn if anything is declared more than once in the same
#		scope.
#
# -Wreturn-type: Warn about incorrect return type for function, e.g. 
#		return( 1 ) for void function (-Wall).
#
# -Wsequence-point: Warn about sequence point violations, e.g. a = a++ (-Wall).
#
# -Wshadow: Warn whenever a local variable shadows another local variable,
#		parameter or global variable (that is, a local of the same name as
#		an existing variable is declared in a nested scope).  Note that this
#		leads to some false positives as gcc treats forward declarations of
#		functions within earlier functions that have the same parameters as
#		the function they're declared within as shadowing.  This can be
#		usually detected in the output by noting that a pile of supposedly
#		shadow declarations occur within a few lines of one another.
#
# -Wstrict-prototypes: Warn if a function is declared or defined K&R-style.
#
# -Wtype-limits: Warn if a comparison is always true due to the limited 
#		range of a data type, e.g. unsigned >= 0.
#
# -Wunused-function: Warn if a static function isn't used (-Wall).
#
# -Wunused-label: Warn if a label isn't used (-Wall).
#
# -Wunused-variable: Warn if a local variable isn't used (-Wall).
#
# -Wunused-but-set-variable: Warn when a variable is assigned to but not 
#		used (-Wall).
#
# -Wunused-local-typedefs: Warn about unused typedef (-Wall).
#
# -Wunused-value: Warn if a statement produces a result that isn't used, 
#		e.g. 'x[i]' as a standalone statement (-Wall).
#
# -Wundef: Warn if an undefined identifier is used in a #if.
#
# -Wvla: Warn if variable-length array is used.
#
# -Wwrite-strings: Warn on attempts to assign/use a constant string value
#		with a non-const pointer.
#
# Note that some of these require the use of at least -O2 in order to be
# detected because they require the use of various levels of data flow
# analysis by the compiler.
#
# Warnings that we don't use, or -Wall warnings that we disable, due to 
# excessive false positives are:
#
# -Wconversion: Warn for potentially problematic conversions, e.g. 
#		'unsigned foo = -1'.  This also warns for things like conversion
#		from int to long unsigned int, leading to avalanches of pointless
#		warnings.
#
# -Wno-missing-field-initializers: Warn about missing initialisers in structs.
#		This also warns about things like the fairly common 'struct foo = { 0 }', 
#		which makes it too noisy for detecting problems (-Wextra).
#
# -Wno-sign-compare: Warn about compares between signed and unsigned values.
#		This leads to endless warnings about comparing a signed to an 
#		unsigned value, particularly problematic when comparing an integer 
#		to a CRYPT_xxx_yyy enum because enums are treated as unsigned so 
#		every comparison leads to a warning (-Wall, -Wextra).
#
# -Wno-switch: Warn about unused enum values in a switch statement.  Since 
#		all cryptlib attributes are declared from a single pool of enums but 
#		only the values for a particular object class are used in the 
#		object-specific code, this leads to huge numbers of warnings about 
#		unhandled enum values in case statements (-Wall).
#
# -Wuninitialized: Warn about used-before-initialised.  The detection of this
#		isn't very good, variables initialised conditionally always produce
#		warnings.
#
# -Wunreachable-code: The use of -O2/-O3 interacts badly with this due to
#		statements rearranged by the optimiser being declared unreachable.
#
# Finally there's also -Wextra, which warns about even more potential
# problems, at the cost of more false positives.
#
# In addition to the standard warnings we also enable the use of gcc
# attributes warn_unused_result and nonnull, which are too broken (and in
# the case of nonnull far too dangerous) to use in production code (see the
# long comment in misc/analyse.h for details), and to catch the compiler
# brokenness we undefine NDEBUG to enable the use of assertion checks that
# will catch the problem.

if [ $ISDEVELOPMENT -gt 0 ] ; then
	echo "  (Enabling unsafe compiler options for development version)." >&2 ;
	CCARGS="$CCARGS -Wall -Waggregate-return -Wcast-align \
					-Wdeclaration-after-statement -Wempty-body -Wextra \
					-Wformat-nonliteral -Wformat-security -Winit-self \
					-Wlogical-op -Wpointer-arith -Wredundant-decls \
					-Wshadow -Wstrict-prototypes -Wtype-limits -Wundef \
					-Wvla" ;
	CCARGS="$CCARGS -Wno-missing-field-initializers -Wno-switch \
					-Wno-sign-compare" ;
	CCARGS="$CCARGS -DUSE_GCC_ATTRIBUTES -UNDEBUG" ;
	if [ "$GCC_VER" -ge 43 ] ; then
		echo "  (Enabling additional compiler options for gcc 4.3.x)." >&2 ;
		CCARGS="$CCARGS -Wparentheses" ;
	fi ;
	if [ "$GCC_VER" -ge 45 ] ; then
		echo "  (Enabling additional compiler options for gcc 4.5.x)." >&2 ;
		CCARGS="$CCARGS -Wlogical-op -Wjump-misses-init" ;
	fi ;
fi

# Finally, report what we've found

echo $CCARGS
