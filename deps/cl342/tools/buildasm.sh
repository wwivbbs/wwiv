#!/bin/sh
# Build the asm equivalents of various C modules.  These are built before
# any other files and override the .o's that are produced by compiling the C
# equivalents of the asm files, so that (provided the build succeeds) the .o
# files that would be created from the C code will never be created because
# the asm-derived .o's already exist.
#
# The exception to this is the hash asm files, which use an incredible
# amount of preprocessor kludging that requires that both the .c and .s
# files are built.  To handle this the makefile uses EXTRAOBJS to include
# the extra asm-derived objs into the build.

# Make sure we've been given sufficient arguments.

if [ "$1" = "" ] ; then
	echo "$0: Missing as name." >&2 ;
	exit 1 ;
fi
if [ "$2" = "" ] ; then
	echo "$0: Missing object path." >&2 ;
	exit 1 ;
fi

# Juggle the args around to get them the way that we want them.

AS=$1
OBJPATH=$2

# Build the asm files for a particular target type (ELF, a.out, Slowaris).
# Since some of the asm source files may not exist in some configurations
# we check for their presence before we try and build them.

build_asm_file()
	{
	INFILE=$1
	OUTFILE=$2

	if [ -f ${INFILE}.s -a ! -f ${OBJPATH}${OUTFILE}.o ] ; then
		$AS ${INFILE}.s -o ${OBJPATH}${OUTFILE}.o ;
	fi
	}

build_asm_files()
	{
	TARGET=$1

	build_asm_file bn/bn-$TARGET bn_asm
	build_asm_file crypt/b-$TARGET bfenc
	build_asm_file crypt/c-$TARGET castenc
	build_asm_file crypt/d-$TARGET desenc
	build_asm_file crypt/r4-$TARGET rc4enc
	build_asm_file crypt/r5-$TARGET rc5enc
	build_asm_file crypt/m5-$TARGET md5asm
	build_asm_file crypt/rm-$TARGET rmdasm
	build_asm_file crypt/s1-$TARGET sha1asm
	}

# The only difference between the "sol" and the "elf" x86 formats is that
# ELF uses '#' as the comment delimiter while Slowaris uses '/'.  In the
# case where the file has no comments (bn-elf.s), it also functions as
# bn-sol.s.
#
# The assembler included with MinGW appears to be just enough to work with
# gcc, it can't assemble any of the .s files we use so there's no rule for
# it since it can't (currently) be used.
#
# Only the bignum code is done in asm for non-x86 systems.  For gas on
# OSF/1, it may be necessary to use -m<cpu_type> (where <cpu_type> is
# anything, e.g.21064, 21164, etc) if gas dies with an illegal operand error
# (this is a bug in some versions of gas).  For Sparc there are two lots of
# asm code, sparcv8 for SuperSparc (Sparc v8) and sparcv8plus for UltraSparc
# (Sparc v9 with hacks for when the kernel doesn't preserve the upper 32
# bits of some 64-bit registers).

case `uname` in
	'AIX')
		if [ `getconf KERNEL_BITMODE` = "32" ] ; then
			$AS bn/aix_ppc32.s -o ${OBJPATH}bn_asm.o ;
		elif [ `getconf KERNEL_BITMODE` = "64" ] ; then
			$AS bn/aix_ppc64.s -o ${OBJPATH}bn_asm.o ;
		fi ;;

	'BSD386'|'iBSD'|'ISC')
		build_asm_files out ;;

	'BSD/OS'|'FreeBSD'|'NetBSD'|'OpenBSD')
		if [ "`echo __ELF__ | cc -E - | grep __ELF__`" = "" ] ; then
			build_asm_files elf ;
		else
			build_asm_files out ;
		fi ;;

	'CYGWIN_NT-5.1'|'CYGWIN_NT-6.0'|'CYGWIN_NT-6.1'|'Linux'|'QNX'|'SCO'|'UnixWare')
		build_asm_files elf ;;

	'HP-UX')
		$AS bn/pa-risc2.s -o ${OBJPATH}bn_asm.o ;;

	'IRIX'|'IRIX64'|'ULTRIX')
		$AS bn/mips3.s -o ${OBJPATH}bn_asm.o ;;

	'OS/390')
		$AS -c misc/mvsent.s -o ${OBJPATH}mvsent.o ;;

	'SunOS')
		if [ `uname -m` = 'i86pc' ] ; then
			build_asm_files sol ;
		else
			if [ `which $AS | grep -c "no gcc"` = '1' ] ; then
				$AS -V -Qy -s -xarch=v8plusa bn/sparcv8plus.S -o ${OBJPATH}bn_asm.o ;
			elif [ `uname -a | grep -c sun4m` = '1' ] ; then
				gcc -mcpu=supersparc -c bn/sparcv8.S -o ${OBJPATH}bn_asm.o ;
			else
				gcc -mcpu=ultrasparc -c bn/sparcv8plus.S -o ${OBJPATH}bn_asm.o ;
			fi ;
		fi ;;

	*)
		echo "$0: Missing asm build rule for OS type `uname`." >&2 ;
		exit 1 ;
esac
