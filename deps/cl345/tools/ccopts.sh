#!/bin/sh
# Obtain appropriate cc options for building cryptlib.
#
# Usage: ccopts.sh [shared] [analyse|special] compiler osname

CCARGS=""
OSNAME=""
SHARED=0
ANALYSE=0
ISCLANG=0
ISCLANG_ANALYSER=0
ISSPECIAL=0
ISDEVELOPMENT=0
HOSTNAME=""

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
if [ $# -lt 2 ] ; then
	echo "Usage: $0 [shared] [analyse|special] osname compiler" >&2 ;
	exit 1 ;
fi

# Juggle the args around to get them the way that we want them.

CC=$1
OSNAME=$2
shift
shift

# Determine the CPU endianness by building and executing the endianness-
# detection program.  Note that we have to use -s rather than the more
# obvious -e since this doesn't exist under the Slowaris sh.

if [ ! -s ./tools/endian ] ; then
	if [ "$OSNAME" = "NONSTOP_KERNEL" ] ; then
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

CCARGS="$(./tools/endian) $(./tools/getseed.sh $OSNAME)"

# Check whether we're building on one of the development systems, which
# allows enabling various unsafe test-only options.  We have to be a bit
# careful with the Gnu compile farm because it doesn't use FQDNs for the
# machines, so we check as much as we can and (for the Gnu compile farm)
# only allow machines on a whitelist to narrow down false positives.

getFQDNName()
	{
	# Check whether the hostname command is available.  It usually is, but
	# if it isn't then we fall back to the machine name via uname.
	if [ ! type hostname 2>/dev/null ] ; then
		HOSTNAME="$(uname -n)" ;
		return ;
	fi

	# Try and get the FQDN of the host.  This doesn't always work, in which
	# case we fall back to the bare host name.
	if [ "$(hostname -f 2>/dev/null)" ] ; then
		HOSTNAME="$(hostname -f)" ;
	else
		HOSTNAME="$(hostname)" ;
	fi
	}

checkForDevSystem()
	{
	TIMEZONE=$1

	# Check for development systems by FQDN
	getFQDNName
	if [ "$(echo $HOSTNAME | grep -c "sitsshprd0[1-3]\.its\.auckland\.ac\.nz")" -gt 0 ] ; then
		ISDEVELOPMENT=1 ;
		return ;
	fi
	if [ "$(echo $HOSTNAME | grep -c "[a-z]*\.cypherpunks\.to")" -gt 0 ] ; then
		ISDEVELOPMENT=1 ;
		return ;
	fi

	# Check for development systems by whitelisted name
	if [ "$(uname -n | grep -c "gcc[0-9][0-9]")" -gt 0 ] ; then
		case "$(uname -n)" in
			'gcc22'|'gcc45'|'gcc40'|'gcc70'|'gcc110'|'gcc113'|'gcc117'|'gcc119'|'gcc202')
				ISDEVELOPMENT=1 ;
				return ;;
		esac ;
	fi

	# Check for local development systems based on their location and
	# network address.  This is vulnerable to FPs, but should be reasonably
	# safe: Devices in the 192.168.1.2x range with gateway 192.168.1.1
	# located in NZ with specific device names.
	if [ $TIMEZONE != "NZDT" ] && [ $TIMEZONE != "NZST" ] ; then
		return ;
	fi
	if [ ! "$(ip addr show eth0)" ] ; then
		return ;
	fi
	if [ "$(ip addr show eth0 | grep -c "inet 192.168.1.2[012]0")" -le 0 ] ; then
		return ;
	fi
	if [ "$(ip route show default | grep -cF "via 192.168.1.1")" -le 0 ] ; then
		return ;
	fi
	if [ $HOSTNAME = "ci20" ] || [ $HOSTNAME = "odroid" ] || [ $HOSTNAME = "odroid64" ] ; then
		ISDEVELOPMENT=1 ;
	fi
	}

if [ "$(uname -s)" = "Linux" ] ; then
	checkForDevSystem "$(date +%Z)" ;
fi

# Check whether we're running clang in a code-analysis mode.  Since some of
# these require a build configuration specific to a development machine, we
# only allow them to be enabled on a development system.
#
# Use of clang is a bit complicated because there's clang the compiler and
# clang the static analyser, to deal with this we detect both the compiler
# (via the "clang" string (general) or the "LLVM" string (Apple) in the
# verbose info) and the analyser (via the "ccc-analyzer" string in the
# verbose info).
#
# In addition the STACK analyser uses an ancient version of clang on a
# static path which is accessed via a wrapper that looks like an equally
# ancient vesion of gcc, so we detect it with a check for the static path
# as part of $CC.

if [ "$($CC -v 2>&1 | grep -ci "clang")" -gt 0 ] ; then
	ISCLANG=1 ;
fi
if [ "$(echo $CC | grep -ci "stack-master")" -gt 0 ] ; then
	ISCLANG=1 ;
fi
if [ "$($CC -v 2>&1 | grep -c "ccc-analyzer")" -gt 0 ] ; then
	ISCLANG_ANALYSER=1 ;
fi
if [ $ISCLANG_ANALYSER -gt 0 ] ; then
	ANALYSE=1 ;
	if [ -z "$CCC_CC" ] ; then
		echo "$0: Environment variable CCC_CC must be set to 'clang' for the analysis build." >&2 ;
		exit 1 ;
	fi ;
	if [ $ISDEVELOPMENT -le 0 ] ; then
		echo "$0: clang must be run on a system designated as a development environment by changing the ISDEVELOPMENT in this script." >&2 ;
		exit 1 ;
	fi ;
fi

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
#
# To allow these optional subsystems to be explicitly disabled, we also
# check for the presence of the DISABLE_AUTODETECT flag and skip the
# checking if this is set.

DEVCRYPTOPATHS="/usr/include/crypto/cryptodev.h /usr/local/include/crypto/cryptodev.h"
NCIPHERPATHS="/opt/nfast/toolkits/pkcs11/libcknfast.so /usr/lib/libcknfast.so"
ODBCPATHS="/usr/include/sql.h /usr/local/include/sql.h /usr/include/hpodbc/sql.h"
PKCS11PATHS="/usr/include/pkcs11.h /usr/include/security/pkcs11.h /usr/include/opensc/pkcs11.h /usr/local/include/pkcs11.h"
TPMPATHS="/usr/include/tss/tspi.h /usr/local/include/tss/tspi.h"

HASDYNLOAD=0
case $OSNAME in
	'Darwin'|'Linux'|'FreeBSD'|'OpenBSD'|'NetBSD')
		HASDYNLOAD=1 ;;

	'SunOS')
		if [ "$(./tools/osversion.sh SunOS)" -gt 4 ] ; then
			HASDYNLOAD=1 ;
		fi ;;

	'HP-UX')
		if [ "$(./tools/osversion.sh HP-UX)" -gt 10 ] ; then
			HASDYNLOAD=1 ;
		fi ;;
esac
if [ -z "$DISABLE_AUTODETECT" ] && [ $HASDYNLOAD -gt 0 ] ; then

	# ODBC support
	for includepath in $ODBCPATHS ; do
		if [ -f $includepath ] ; then
			echo "ODBC interface detected, enabling ODBC support." >&2 ;
			CCARGS="$CCARGS -DHAS_ODBC -I"$(dirname $includepath)"" ;
			break ;
		fi
	done

	# LDAP support
	if [ -f /usr/include/ldap.h ] ; then
		echo "LDAP interface detected, enabling LDAP support" >&2 ;
		CCARGS="$CCARGS -DHAS_LDAP" ;
		if [ $ISDEVELOPMENT -gt 0 ] ; then
			CCARGS="$CCARGS -DUSE_LDAP" ;
		fi ;
	fi

	# PKCS #11 support
	for includepath in $PKCS11PATHS ; do
		if [ -f $includepath ] ; then
			echo "PKCS #11 interface detected, enabling PKCS #11 support." >&2 ;
			CCARGS="$CCARGS -DHAS_PKCS11 -I"$(dirname $includepath)"" ;
			break ;
		fi
	done
	for includepath in $NCIPHERPATHS ; do
		if [ -f $includepath ] ; then
			echo "  (Enabling use of additional nCipher PKCS #11 extensions)." >&2 ;
			CCARGS="$CCARGS -DNCIPHER_PKCS11" ;
			break ;
		fi
	done

	# TPM support
	for includepath in $TPMPATHS ; do
		if [ -f $includepath ] ; then
			echo "TPM interface detected, enabling TPM support." >&2 ;
			CCARGS="$CCARGS -DHAS_TPM -I"$(dirname $includepath)"" ;
			break ;
		fi
	done

	# /dev/crypto support
	for includepath in $DEVCRYPTOPATHS ; do
		if [ -f $includepath ] ; then
			echo "/dev/crypto interface detected, enabling crypto hardware support." >&2 ;
			CCARGS="$CCARGS -DHAS_DEVCRYPTO -I"$(dirname $includepath)"" ;
			break ;
		fi
	done

fi
if [ -f /usr/include/zlib.h ] ; then
	echo "  (Enabling use of system zlib)." >&2 ;
	CCARGS="$CCARGS -DHAS_ZLIB" ;
fi

# If we're building a development or analysis build, enable various unsafe
# options that are normally disabled by default

if [ $ANALYSE -gt 0 ] || [ $ISSPECIAL -gt 0 ] ; then
	echo "  (Enabling all source code options for analysis/instrumented build)." >&2 ;
	CCARGS="$CCARGS -DUSE_ANALYSER" ;
elif [ $ISDEVELOPMENT -gt 0 ] ; then
	echo "  (Enabling additional source code options for development version)." >&2 ;
	CCARGS="$CCARGS -DUSE_CERT_DNSTRING -DUSE_DNSSRV -DUSE_ECDSA -DUSE_ECDH" ;
fi

# If we're building with the clang static analyser, set options specific to
# that.  The standard clang uses gcc as a front-end with -Wall enabled so
# we'd have to disable the false-positive-inducing options for that as per
# the long comment at the end of this file for the -Wall debug build,
# however we override the front-end type by setting the CCC_CC environment
# variable to 'clang'.  Since almost all of the clang warnings are
# undocumented outside of analysing the source code, we resort to enabling
# all warnings with -Weverything and then disabling the false positive-
# inducing ones.  These are:
#
#	-Wno-gnu:
#	-Wno-language-extension-token: Use of a gcc'ism, which isn't a problem
#		in this case because clang detects as gcc in conditional-compilation
#		directives.
#
#	-Wno-missing-field-initializers: Missing initialisers in structs.  This
#		also warns about things like the fairly common 'struct foo = { 0 }',
#		which makes it too noisy for detecting problems.
#
#	-Wno-missing-prototypes: Declaration of function without an earlier
#		prototype.
#
#	-Wno-padded: Padding in structs.
#
#	-Wno-pointer-sign: char * to unsigned char * and similar.
#
#	-Wno-shorten-64-to-32: Warn about long -> int conversion, this has the
#		potential to catch issues but at the moment leads to all false
#		positives around things like the sizeofXXX() functions or stell(),
#		as well as anything involving size_t (e.g. strlen()), which return a
#		long in general but are often used in cases where the range is an
#		int (or more generally a short).
#
#	-Wno-sign-compare: Compare int to unsigned int and similar.
#
#	-Wno-sign-conversion: int to unsigned int and similar.
#
#	-Wno-switch:
#	-Wno-switch-enum: Unused enum values in a switch statement.  Since all
#		cryptlib attributes are declared from a single pool of enums but
#		only the values for a particular object class are used in the
#		object-specific code, this leads to huge numbers of warnings about
#		unhandled enum values in case statements.
#
#	-Wno-unused-macros: Macro defined but not used.  This typically occurs
#		when macros are used to define constants (e.g. cipher block sizes),
#		not all of which are used in the code.

if [ $ISCLANG_ANALYSER -gt 0 ] ; then
	echo "  (Enabling specific build options for clang)." >&2 ;
	CCARGS="$CCARGS -Weverything" ;
	CCARGS="$CCARGS -Wno-gnu -Wno-language-extension-token \
			-Wno-missing-field-initializers -Wno-missing-prototypes \
			-Wno-padded -Wno-pointer-sign -Wno-shorten-64-to-32 \
			-Wno-sign-compare -Wno-sign-conversion -Wno-switch \
			-Wno-switch-enum -Wno-unused-macros" ;
fi

# clang (the compiler, not the static analyser) has a subset of the above
# issues, since we're not using -Weverything for the pure compiler we only
# need to disable the two enabled-by default false-positive-inducing ones,
# -Wswitch and -Wpointer-sign.

if [ $ISCLANG -gt 0 ] ; then
	CCARGS="$CCARGS -Wno-pointer-sign -Wno-switch" ;
fi

# If we're using a newer version of clang, turn on stack buffer overflow
# protection unless it's a special-case build.  Given cryptlib's built-in
# protection mechanisms this shouldn't be necessary, but it can't hurt to
# enable it anyway.
#
# Note that this is a weird flag in that it was supposedly added to clang
# in 3.9 but not really supported until about 4.3, and then in clang
# something, maybe 4.7 or so, was also added as a link flag, see
# tools/getlibs.sh, so it's enabled here for clang 4.3 or newer and then in
# tools/getlibs.sh also for clang 4.7 or newer.

if [ $ISCLANG -gt 0 ] && [ $ISSPECIAL -eq 0 ] ; then
	CLANG_VER="$($CC -dumpversion | tr -d  '.' | cut -c 1-2)" ;
	if [ $CLANG_VER -gt 42 ] ; then
		CCARGS="$CCARGS -fsanitize=safe-stack" ;
	fi ;
fi

# The Sun compiler has its own set of problems, the biggest of which is
# determining where it is and what it is (see comments elsewhere), but
# another one is that some of the warning options changed across compiler
# versions or possibly target types (there's no obvious pattern), so we have
# to detect use of this compiler and then feed it the options to see which
# one is accepted.

if [ "$OSNAME" = "SunOS" ] && [ "$($CC 2>&1 | grep -c "cc -flags")" -gt 0 ] ; then
	CCARGS="$CCARGS -errtags=yes" ;
	touch suncctest.c ;
	if [ "$($CC -erroff=E_ARG_INCOMPATIBLE_WITH_ARG suncctest.c 2>&1 | grep -c "bad message")" -gt 0 ] ; then
		CCARGS="$CCARGS -erroff=E_ARG_INCOMPATIBLE_WITH_ARG_L" ;
	else
		CCARGS="$CCARGS -erroff=E_ARG_INCOMPATIBLE_WITH_ARG" ;
	fi ;
	rm suncctest.c ;
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

if [ $SHARED -gt 0 ] ; then
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
			if [ "$($CC -v 2>&1 | grep -c "gcc")" = 0 ] ; then
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

if [ $SHARED -eq 0 ] ; then
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
	if [ "$(grep -c PTHREAD_MUTEX_RECURSIVE /usr/include/pthread.h)" -gt 0 ] ; then
		CCARGS="$CCARGS -DHAS_RECURSIVE_MUTEX" ;
	fi ;
	if [ "$(grep -c PTHREAD_MUTEX_ROBUST /usr/include/pthread.h)" -gt 0 ] ; then
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
# feature so the compiler is misidentified as gcc.  In addition because
# of clang's compatibility-with-gcc system clang can be misidentified as
# gcc (or at least clang can appear as both clang and gcc, depending on
# whether the check is for clang or gcc).
#
# To work around this we perform a special-case check for Aches and clang
# and use a somewhat more direct check for gcc, which is likely to be
# explicitily installed as 'gcc' rather than the system 'cc'.

if [ "$OSNAME" = "AIX" ] ; then
	if [ $CC = "xlc" ] ; then
		echo "$CCARGS" ;
		exit 0 ;
	fi ;
	if [ "$(which cc 2>&1 | grep -c "gcc")" = 0 ] ; then
		echo "$CCARGS" ;
		exit 0 ;
	fi ;
fi
if [ $ISCLANG -gt 0 ] ; then
	echo "$CCARGS" ;
	exit 0 ;
fi

if [ "$($CC -v 2>&1 | grep -c "gcc")" = 0 ] ; then
	echo "$CCARGS" ;
	exit 0 ;
fi

# Find out which version of gcc we're using.  The check for the gcc version
# is equally complicated by the fact that a (rare) few localised gcc's don't
# use a consistent version number string.  Almost all versions print "gcc
# version", but the French localisation has "version gcc" (we can't use just
# "gcc" by itself since this appears elsewhere in the gcc -v output).
#
# To make things even more confusing, Apple's hacked-up gcc branch (before
# they switched to clang) printed something like
# "PowerPC-Apple-Is-Great-I-Love-Darwin-4567-Hup234-gcc-x.y.z", so any
# simple attempt at extracting what looks like a version number will fail.
# The only way to get around this is to look for the first set of numeric
# values that follow the string "gcc" and use that as the version number.
#
# In order to avoid this mess we use the "-dumpversion" option, which has
# worked since at least 2.7.2 although it wasn't actually documented until
# the first 3.x releases).
#
# However, dumpversion has its own problems in that it lists major-version
# releases as a single-digit number, '6' rather than '6.0', so if we find an
# apparent version less than 10 we add a trailing zero to the string to make
# the checks that follow work.

GCC_VER="$($CC -dumpversion | tr -d  '.' | cut -c 1-2)"
if [ "$GCC_VER" -lt 10 ] ; then
	GCC_VER="${GCC_VER}0" ;
fi

# Try and determine the CPU type.  This is made more complex by a pile of
# *BSE's which, along with antideluvian tools like an as that doesn't
# recognise 486 opcodes, report the CPU type as i386.  Even sysctl reports
# the CPU as being i386, so if we find this we assume it's some *BSE which
# is actually running on a P4 or Athlon or something similar (unfortunately
# there's no real way to detect this, but it's 99.9% likely that it's not
# actually still running on an 80386).

ARCH=$(uname -m)

if [ "$ARCH" = "i386" ] && [ "$(uname | grep -c BSD)" -gt 0 ] ; then
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
# set but scheduled for the P4 CPU.  Usually -march=X also imples -mtune=X,
# but newer versions of gcc allow an mtune=generic to tune for the most
# widely-used current CPUs.  In particular, specifying -march=<oldest CPU
# type to support> requires -mtune=generic otherwise it'll also -mtune for
# the oldest CPU type rather than any current one.  To see what the current
# setting is, use:
#
#	gcc -Q --help=target
#
# For x86-64 it's -march=x86-64 -mtune=generic, the default config for gcc
# on x86, which means that pretty much every instruction-set extension
# beyond basic x64 is disabled.
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
# use of this option for anything before about gcc 4.5.
#
# For x86-64 we have to enable the use of PIC because of obscure linker
# errors ("relocation R_X86_64_32S can not be used when making a shared
# object") that crop up when building the shared-lib version, in the case
# of x86-64 the use of PIC should have minimum overhead so it shouldn't be
# a big deal.  As a convenient side-effect, this also enables the use of
# ASLR where it's supported.

if [ "$ARCH" = "i586" ] || [ "$ARCH" = "i686" ] || [ "$ARCH" = "x86_64" ] ; then
	if [ "$GCC_VER" -ge 45 ] ; then
		CCARGS="$CCARGS -march=native -mtune=generic" ;
		if [ "$ARCH" = "x86_64" ] ; then
			CCARGS="$CCARGS -fPIC" ;
		fi ;
	elif [ "$GCC_VER" -ge 30 ] ; then
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
#
# (Update: Having optimisation set via this script rather than in the
#  makefile purely for gcc is awkward because if we're building under the
#  same OS but using clang then the optimisation level is never set.  Since
#  newer versions of gcc might be less buggy than the awful 4.x series we
#  go back to specifying the optimisation level in the makefile).
#
#if [ "$OSNAME" = "Linux" ] || [ "$OSNAME" = "FreeBSD" ] || \
#   [ "$OSNAME" = "NetBSD" ] || [ "$OSNAME" = "OpenBSD" ] || \
#   [ "$OSNAME" = "Darwin" ] ; then
#	if [ "$GCC_VER" -ge 40 ] ; then
#		CCARGS="$CCARGS -O2" ;
#	else
#		CCARGS="$CCARGS -O3" ;
#	fi ;
#fi

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

if [ "$GCC_VER" -ge 40 ] ; then
	if [ "$($CC -Wno-pointer-sign -S -o /dev/null -xc /dev/null 2>&1 | grep -c "unrecog")" -eq 0 ] ; then
		CCARGS="$CCARGS -Wno-pointer-sign" ;
	fi ;
	CCARGS="$CCARGS -Wno-strict-aliasing" ;
fi

# The gcc developers are of the opinion that once the compiler encounters
# anything that can be classed as undefined behaviour (UB, e.g. an integer
# addition) then the compiler is allowed to do anything it wants.  While
# they very generously refrain from reformatting your hard drive, what they
# do do is remove code that does things like check for integer overflow or
# null pointers that may exist beyond the point where the UB can occur.
# Compilers like MSVC assume that they're running on a two's-complement
# machine and act accordingly, while gcc knows that it's also running on a
# two's-complement machine but nevertheless can't exlude the theoretical
# possibility that it's running on a one's-complement CDC 6600 from 1965 and
# therefore can't assume two's-complement behaviour.
#
# gcc then allows you to extend the braindamage by specifying -ftrapv, which
# generates a trap if overflow is encountered.  This means that gcc's
# default behaviour is to be braindamaged, and if -ftrapv is specified, to
# be braindamaged and then cause your app to crash.
#
# To get around this, we specify -fwrapv (yes, you really can assume that
# you're on a two's-complement machine, which has been the case for
# everything from the last half-century or so) and -fno-delete-null-pointer-
# checks (behaviour that's so totally stupid that it's incredible that you
# actually need to specify an option to fix it).
#
# There are actually two variants of the overflow-braindamage-limiting
# mechanism, -fwrapv and -fno-strict-overflow, which do more or less the
# same thing but in subtly different ways that no-one is quite clear on.
# One explanation is that -fwrapv tells the compiler that integer overflow
# wraps while -fno-strict-overflow tells the compiler that integer overflow
# can happen, but not what happens (in other words it tells it not to remove
# code based on this).  Since we know that we're running on a two's-
# complement machine, we use -fwrapv to reflect this.

if [ "$GCC_VER" -ge 40 ] ; then
	CCARGS="$CCARGS -fwrapv -fno-delete-null-pointer-checks" ;
fi

# The AES code uses 64-bit data types, which older vesions of gcc don't
# support (at least via limits.h) unless they're operating in C99 mode.  So
# in order to have the AES auto-config work we have to explicitly run gcc
# in C99 (or newer) mode, which isn't the default for the gcc 3.x and some
# 4.6 versions.  Since the code also uses gcc extensions we have to specify
# the mode as gcc + C99, not just C99.

if [ "$GCC_VER" -ge 30 ] && [ "$GCC_VER" -le 46 ] ; then
	CCARGS="$CCARGS -std=gnu99" ;
fi

# Use of static_assert() requires that gnu11 mode be enabled, this isn't
# recognised by gcc 4.6, is recognised but not the default for gcc 4.7-4.9
# (the alleged default is gnu90 but it actually seems to be gnu99), and is
# the default for gcc 5.0 and above */

if [ "$GCC_VER" -ge 47 ] && [ "$GCC_VER" -le 49 ] ; then
	CCARGS="$CCARGS -std=gnu11" ;
fi

# Enable stack protection and extra checking for buffer overflows if it's
# available.  This was introduced (in a slightly hit-and-miss fashion) in
# later versions of gcc 4.1.x, to be on the safe side we only enable it
# for gcc 4.2 and newer.  gcc 4.9 introduced a slightly more comprehensive
# version so we use that if it's available.  Some people like to add
# '--param=ssp-buffer-size=4' (the default size is 8), but this isn't
# necessary for cryptlib since it doesn't allocate any 4-byte buffers.

if [ "$GCC_VER" -ge 49 ] ; then
	CCARGS="$CCARGS -fstack-protector-strong -D_FORTIFY_SOURCE=2" ;
elif [ "$GCC_VER" -ge 42 ] ; then
	if [ "$($CC -fstack-protector -S -o /dev/null -xc /dev/null 2>&1 | grep -c "unrecog")" -eq 0 ] ; then
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
# -Waggregate-return: Warn about functions that return structs.  This isn't
#		used any more as of 3.4.4 due to the use of safe pointers, which are
#		scalar values.
#
# -Walloc-zero: Warn about call to malloc( 0 ).
#
# -Walloc-size-larger-than=value: Warn when a call to malloc() exceeds
#		'value', potentially caused by arithmetic overflow
#		(-Wall = -Walloc-size-larger-than=PTRDIFF_MAX).
#
# -Walloca: Warn about use of alloc().
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
# -Wdangling-else: Warn about dangling else.
#
# -Wdeclaration-after-statement: Warn about a variable declaration found
#		after a statement in a function (VC++ 6.0 complains about this too).
#
# -Wduplicate-decl-specifier: Warn about duplicate 'const', 'volatile' etc
#		in a declaration (-Wall).
#
# -Wduplicated-branches: Warn if an if/else as identical branches (no idea
#		how this differs from -Wduplicated-cond).
#
# -Wduplicated-cond: Warn about duplicate conditions in an if/else chain.
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
# -Wformat-overflow: Check for problems with overflows in arguments to
#		sprintf().
#
# -Wformat-security: Check for potential security problems in format strings.
#
# -Wformat-truncation: Chek for problems with truncation in arguments to
#		sprintf() (-Wall).
#
# -Wimplicit-int: Warn about typeless variable declarations (-Wall).
#
# -Wimplicit-fallthrough=5: Warn about falling through a case statement in a
#		switch without it being annotated as a fallthrough via an attribute
#		(-Wextra, but default is level 3, not level 5).
#
# -Winit-self: Warn if a value is initialised to itself, e.g. 'int i=i'.
#
# -Wjump-misses-init: Warn if a goto or switch misses initialisation of a
#		variable.
#
# -Wlogical-op: Warn about suspicious use of logical operators in
#		expressions, e.g. '|' vs '||'.
#
# -Wlogical-not-parentheses: Warn about "logical not" used on the left hand
#		side operand of a comparison.
#
# -Wmemset-elt-size: Warn when size argument for memset() of array doesn't
#		include sizeof() the elemtns (-Wall).
#
# -Wmemset-transposed-args: Warn about memset( ..., n, 0 ) where
#		memset( ..., 0, n ) was probably meant (-Wall).
#
# -Wmisleading-indentation: goto fail (-Wall).
#
# -Wmissing-braces: Warn if an array initialiser isn't fully bracketed, e.g.
#		int a[2][2] = { 0, 1, 2, 3 } (-Wall).
#
# -Wmissing-parameter-type: Warn if a function has a K&R-style declaration,
#		int foo() { ... } (-Wextra).
#
# -Wmultistatement-macros: Warn about macros that expand to multiple
#		statements, causing problems if not enclosed in braces (-Wall).
#
# -Wnonnull: Warn about passing a null for function args tagged as being
#		__nonnull (-Wall).
#
# -Wnonnull-compare: Warn about checking an argument marked __nonnull for
#		NULL (-Wall).
#
# -Wnull-dereference: Guess at potential derefencing of null pointers, only
#		enabled if -fdelete-null-pointer-checks is active, which it is if
#		optimisation is enabled (note however that we disable this in order
#		to limit the braindamage that it causes, see the comment earlier).
#		This option appears to be a grudging admission of the braindamage of
#		existing nonnull behaviour.
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
# -Wshift-count-negative: Warn about problems with shift counts (default).
# -Wshift-count-overflow:
#
# -Wshift-negative-value: Warn about left-shifting a negative value.
#
# -Wshift-overflow: Warn about shift overflows.
#
# -Wsizeof-array-argument: Warn when the sizeof operator is applied to a
#		parameter that has been declared as an array in a function
#		definition.
#
# -Wsizeof-pointer-div: Warn about e.g. sizeof( ptr ) / sizeof(ptr[ 0 ])
#		when ptr isn't an array (-Wall).
#
# -Wsizeof-pointer-memaccess: Warn about suspicious use of sizeof with some
#		string and memory functions, e.g.
#		memcpy( &foo, ptr, sizeof( &foo ) ) (-Wall)
#
# -Wstrict-overflow: Warn about potential integer overflow.  This is probably
#		negated by the options to limit gcc's braindamaged handling of
#		(potential) overflow, see the comments earlier on for more on this.
#
# -Wstrict-prototypes: Warn if a function is declared or defined K&R-style.
#
# -Wstringop-overflow: Warn of string/mem functions that overflow the
#		destination buffer (-W..=2, default).
#
# -Wstringop-truncation: Warn about string ops that truncate the operation,
#		e.g. 'strncat( buf, ".txt", 3 );' will only copy 3 of the 4 string
#		chars (-Wall).
#
# -Wswitch-bool: Warn whenever a switch statement has an index of boolean
#		type.
#
# -Wswitch-unreachable: Warn whenever a switch statement has unreachable
#		code, e.g. code before the first "case:" (default).
#
# -Wtautological-compare: Warn about self-comparison, e.g. if( i == i )
#		(-Wall).
#
# -Wtrampolines: Warn if trampolines are being generated, which requires an
#		executable stack.  This is only done for nested functions:
#
#		foo( int x )
#			{
#			bar( int y )
#				{ };
#			}
#
#		which are a gcc-ism and not used anywhere, but we enable it anyway
#		just in case.
#
# -Wtype-limits: Warn if a comparison is always true due to the limited
#		range of a data type, e.g. unsigned >= 0.
#
# -Wundef: Warn if undefined identifier is used in #if.
#
# -Wunused-const-variable: Warn of unused const variables.
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
# -Wunsafe-loop-optimizations: Warn if the compiler can't reason about loop
#		bounds.
#
# -Wvla: Warn if variable-length array is used.
#
# -Wwrite-strings: Warn on attempts to assign/use a constant string value
#		with a non-const pointer.
#
# -Wzero-as-null-pointer-constant: Warn if an explicit 0 or '\0' is used
#		instead of NULL.  Unfortunately this is currently valid (gcc 4.7)
#		only for C++ so we can't use it.
#
# Note that some of these require the use of at least -O2 in order to be
# detected because they require the use of various levels of data flow
# analysis by the compiler.
#
# Finally, there are warnings that, as implemented by gcc, are more or less
# useless due to excessive false positives, for example warning on nearly
# every single stdlib function call.  See for example "Twice the Bits, Twice
# the Trouble", Wressnegger et al, CCS'16, for more on this, which found
# 45,000 false positives covering up the few actual issues.  Because they're
# essentially useless due to the level of noise, we disable them (if they're
# enabled through -Wall) or don't use them if they're optional.  VC++
# provides a usable level of warnings about these issues so we rely on that:
#
# -Wconversion: Warn for potentially problematic conversions, e.g.
#		'unsigned foo = -1'.  This also warns for things like conversion
#		from int to long unsigned int/size_t, leading to avalanches of
#		pointless warnings.  For example pretty much every stdlib function
#		that takes a length parameter anywhere produces a warning when a
#		generic int variable is converted to a size_t.
#
# -Wno-ignored-qualifiers: Use of const with by-value return, for example
#		in 'const DATAPTR dataptrAttributeMoveCursor( ... )'.  This was
#		required starting with 3.4.4's use of safe pointers, see also the
#		comment for -Waggregate-return.
#
# -Wno-missing-field-initializers: Warn about missing initialisers in structs.
#		This also warns about things like the fairly common 'struct foo = { 0 }',
#		which makes it too noisy for detecting problems (-Wextra).
#
# -Wno-nonnull-compare: An extension of the nonnull braindamage in which,
#		alongside removing NULL checks, they're all warned about as well.
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
#		Because of this the capability was removed in January 2010
#		(http://gcc.gnu.org/ml/gcc-patches/2009-11/msg00179.html), so while
#		the compiler still accepts the option it silently ignores it (par
#		for the course for gcc).
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
#
# Alongside the warnings, we also enable various sanitisers if they're
# available.  We can't use -fsanitize=thread because it's incompatible
# with -fsanitize=address, not because of the compiler but because they
# require different memory layouts in the runtime libraries.  In addition
# the sanitizers require specific library support and since the clang/
# fuzzing build incorporates them anyway it's easier not to bother:
#
#	if [ "$GCC_VER" -ge 48 ] ; then
#		echo "  (Enabling additional compiler options for gcc 4.8.x)." >&2 ;
#		CCARGS="$CCARGS -fsanitize=address -lasan" ;
#	fi ;
#	if [ "$GCC_VER" -ge 49 ] ; then
#		echo "  (Enabling additional compiler options for gcc 4.9.x)." >&2 ;
#		CCARGS="$CCARGS -fsanitize=undefined -lubsan" ;
#	fi ;
#
# We also enable -finstrument-functions and -fexceptions if possible in
# order to be able to print stack trace information when debugging.  We
# can't enable it on x86-64 where SSE intrinsics are used for AES support
# because then gcc in its infinite buggyness decides to instrument SSE
# intrinsics, which aren't functions, and compilation fails with a string of
# undefined-reference errors.

if [ $ANALYSE -gt 0 ] ; then
	CCARGS="$CCARGS -DUSE_GCC_ATTRIBUTES" ;
elif [ $ISDEVELOPMENT -gt 0 ] ; then
	echo "  (Enabling unsafe compiler options for development version)." >&2 ;
	CCARGS="$CCARGS -Wall -Wcast-align -Wdeclaration-after-statement \
					-Wempty-body -Wextra -Wformat-nonliteral \
					-Wformat-security -Winit-self -Wlogical-op \
					-Wparentheses -Wpointer-arith -Wredundant-decls \
					-Wshadow -Wstrict-overflow -Wstrict-prototypes \
					-Wtype-limits -Wundef -Wvla" ;
	CCARGS="$CCARGS -Wno-ignored-qualifiers -Wno-missing-field-initializers \
					-Wno-switch -Wno-sign-compare" ;
	CCARGS="$CCARGS -DUSE_GCC_ATTRIBUTES -UNDEBUG" ;
	if [ "$GCC_VER" -ge 45 ] ; then
		echo "  (Enabling additional compiler options for gcc 4.5.x)." >&2 ;
		CCARGS="$CCARGS -Wlogical-op -Wjump-misses-init" ;
	fi ;
	if [ "$GCC_VER" -ge 47 ] ; then
		echo "  (Enabling additional compiler options for gcc 4.7.x)." >&2 ;
		CCARGS="$CCARGS -Wtrampolines -Wunused-local-typedefs" ;
	fi ;
	if [ "$GCC_VER" -ge 50 ] ; then
		echo "  (Enabling additional compiler options for gcc 5.x)." >&2 ;
		CCARGS="$CCARGS -Wlogical-not-parentheses -Wsizeof-array-argument \
						-Wswitch-bool -Wmissing-parameter-type" ;
	fi ;
	if [ "$GCC_VER" -ge 60 ] ; then
		echo "  (Enabling additional compiler options for gcc 6.x)." >&2 ;
		CCARGS="$CCARGS -Wshift-negative-value -Wshift-overflow \
						-Wnull-dereference -Wduplicated-cond \
						-Wno-nonnull-compare -Wundef" ;
	fi ;
	if [ "$GCC_VER" -ge 70 ] ; then
		echo "  (Enabling additional compiler options for gcc 7.x)." >&2 ;
		CCARGS="$CCARGS -Walloca -Wduplicated-branches -Wmemset-elt-size \
						-Wduplicate-decl-specifier -Wdangling-else \
						-Walloc-size-larger-than=1000000 -Walloc-zero \
						-Wformat-overflow -Wformat-truncation \
						-Wimplicit-fallthrough=5 -Wunused-const-variable=1 \
						-Wunsafe-loop-optimizations" ;
	fi ;
	if [ "$GCC_VER" -ge 80 ] ; then
		echo "  (Enabling additional compiler options for gcc 8.x)." >&2 ;
		CCARGS="$CCARGS -Wmultistatement-macros" ;
	fi ;
	if [ "$ARCH" != "x86_64" ] ; then
		CCARGS="$CCARGS -finstrument-functions -fexceptions" ;
	fi ;
fi

# Finally, report what we've found

echo "$CCARGS"
