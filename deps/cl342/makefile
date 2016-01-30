#****************************************************************************
#*																			*
#*							Makefile for cryptlib 3.4.x						*
#*						Copyright Peter Gutmann 1995-2012					*
#*																			*
#****************************************************************************

# This makefile contains extensive amounts of, uhh, business logic which,
# alongside further logic in the cryptlib OS-specific header files, ensures
# that cryptlib auto-configures itself and builds out of the box on most
# systems.  Before you ask about redoing the makefile using autoconf, have a
# look at what it would take to move all of this logic across to another
# build mechanism.
#
# "The makefile is looking really perverse.  You're getting the hang of it"
#														- Chris Wedgwood.
# At least it doesn't pipe itself through sed yet.
#
# (Note that as of 3.1 beta 3 it does pipe itself through sed on non-Unix
#  systems to retarget Unix-specific files to OS-specific ones and perform
#  various other operations that aren't easily possible by adding another
#  level of recursion).
#
# The self-test program pulls in parts of cryptlib to ensure that the self-
# configuration works.  Because this is handled by the makefile, you can't
# just 'make testlib' after making changes, you need to use 'make; make
# testlib'.

# Naming information: Major and minor version numbers and project and library
# names (static lib, shared lib, and OS X dylib).  The patch level is always
# zero because patches imply bugs and my code is perfect (although sometimes
# reality isn't).
#
# Note that when updating these values it'll also be necessary to update the
# equivalents in tools/buildall.sh.

MAJ		= 3
MIN		= 4
PLV		= 2
PROJ	= cl
LIBNAME	= lib$(PROJ).a
SLIBNAME = lib$(PROJ).so.$(MAJ).$(MIN).$(PLV)
DYLIBNAME = lib$(PROJ).$(MAJ).$(MIN).dylib

# Compiler options.  By default this builds the release version of the code,
# to build the debug version (which is useful for finding compiler bugs and
# system-specific peculiarities) remove the NDEBUG define.  Many problems
# will now trigger an assertion at the point of failure rather than returning
# an error status from 100 levels down in the code, although as of 3.3.2
# many of the earlier assertions have been turned into REQUIRES/ENSURES
# predicates that are applied even in the release version.
#
# Note that the gcc build uses -fomit-frame-pointer to free up an extra
# register on x86 (which desperately needs it), this may screw up some
# versions of gdb if you try and debug a version (compile with '-g')
# compiled with this option.  As a general comment, to build the debug
# version remove the -DNDEBUG below and build with
# 'make CFLAGS="-g3 -ggdb -O0"' (the -O0 is required because the default
# is -O1, which is enough to mess up debugging at times), or use the debug
# target "make debug".  This assumes a certain amount of gnu-ishness in the
# debug environment (which seems to be the universal default), if you're
# using something else then you'll have to modify CFLAGS_DEBUG below.  In
# addition it's probably a good idea to remove -fomit-frame-pointer if it's
# set for the target environment.
#
# If the OS supports it, the multithreaded version of cryptlib will be built.
# To specifically disable this add -DNO_THREADS.
#
# If you're building the 64-bit version on a system that defaults to 32-bit
# binaries then you can get the 64-bit version by adding "-m64" to CFLAGS
# and LDFLAGS, at least for gcc.
#
# Further cc flags are gathered dynamically at runtime via the ccopts.sh
# script.

CFLAGS		= -c -D__UNIX__ -DNDEBUG -I.
CFLAGS_DEBUG = -c -D__UNIX__ -I. -g3 -ggdb -O0

# Paths and command names.  We have to be careful with comments attached to
# path defines because some makes don't strip trailing spaces.
#
# The reason for the almost-identical defines for path and dir is because of
# the braindamaged BSDI mkdir (and rmdir) that break if the path ends in a
# '/', it's easier to have separate defines than to drop a '/' into every
# path.

STATIC_OBJ_PATH = ./static-obj/
STATIC_OBJ_DIR = ./static-obj
SHARED_OBJ_PATH = ./shared-obj/
SHARED_OBJ_DIR = ./shared-obj
CPP			= $(CC) -E
LD			= $(CC)
LDFLAGS		=
AR			= ar
STRIP		= strip
SHELL		= /bin/sh
OSNAME		= `uname`
LINKFILE	= link.tmp

# Default target and obj file path.  This is changed depending on whether
# we're building the static or shared library, the default is to build the
# static library.

TARGET		= $(LIBNAME)
OBJPATH		= $(STATIC_OBJ_PATH)

# Some makes don't pass defines down when they recursively invoke make, so we
# need to manually pass them along.  The following macro contains all defines
# that we want to pass to recursive calls to make.

DEFINES		= $(TARGET) OBJPATH=$(OBJPATH) OSNAME=$(OSNAME)

# Cross-compilation/non-Unix options, which are just the standard ones with
# Unix-specific entries (-D__UNIX__, use of uname to identify the system)
# removed.  The actual values are explicitly given in the rules for each non-
# Unix target.

XCFLAGS		= -c -DNDEBUG -I.
XDEFINES	= $(TARGET) OBJPATH=$(OBJPATH) CROSSCOMPILE=1

# Cross-compilation paths.  The Palm SDK under Cygwin only understands
# heavily-escaped absolute MSDOS pathnames, so it's necessary to specify
# (for example)
# -I"c:/Program\\\ Files/PalmSource/Palm\\\ OS\\\ Developer\\\ Suite/sdk-6/"
# as the SDK path.  In practice it's easier to dump all the files in their
# own partition, which is what the Palm SDK target below assumes.  Note that
# if you change this you'll also have to change the path value in
# tools/buildlib.sh.

PALMSDK_PATH	= "d:/Palm\\\ SDK/sdk-6"

#****************************************************************************
#*																			*
#*								Common Dependencies							*
#*																			*
#****************************************************************************

# The object files that make up cryptlib.

ASMOBJS		= $(OBJPATH)md5asm.o $(OBJPATH)rmdasm.o $(OBJPATH)sha1asm.o

BNOBJS		= $(OBJPATH)bn_add.o $(OBJPATH)bn_asm.o $(OBJPATH)bn_ctx.o \
			  $(OBJPATH)bn_div.o $(OBJPATH)bn_exp.o $(OBJPATH)bn_exp2.o \
			  $(OBJPATH)bn_gcd.o $(OBJPATH)bn_lib.o $(OBJPATH)bn_mod.o \
			  $(OBJPATH)bn_mont.o $(OBJPATH)bn_mul.o $(OBJPATH)bn_recp.o \
			  $(OBJPATH)bn_shift.o $(OBJPATH)bn_sqr.o $(OBJPATH)bn_word.o \
			  $(OBJPATH)ec_lib.o $(OBJPATH)ecp_mont.o $(OBJPATH)ecp_smpl.o \
			  $(OBJPATH)ec_mult.o $(OBJPATH)ec_rand.o $(OBJPATH)ec_kron.o \
			  $(OBJPATH)ec_sqrt.o

CERTOBJS	= $(OBJPATH)certrev.o $(OBJPATH)certschk.o $(OBJPATH)certsign.o \
			  $(OBJPATH)certval.o $(OBJPATH)chain.o $(OBJPATH)chk_cert.o \
			  $(OBJPATH)chk_chn.o $(OBJPATH)chk_use.o $(OBJPATH)comp_cert.o \
			  $(OBJPATH)comp_curs.o $(OBJPATH)comp_del.o $(OBJPATH)comp_get.o \
			  $(OBJPATH)comp_gets.o $(OBJPATH)comp_set.o $(OBJPATH)dn.o \
			  $(OBJPATH)dn_rw.o $(OBJPATH)dnstring.o $(OBJPATH)ext.o \
			  $(OBJPATH)ext_add.o $(OBJPATH)ext_chk.o $(OBJPATH)ext_copy.o \
			  $(OBJPATH)ext_def.o $(OBJPATH)ext_rd.o $(OBJPATH)ext_wr.o \
			  $(OBJPATH)imp_chk.o $(OBJPATH)imp_exp.o $(OBJPATH)read.o \
			  $(OBJPATH)trustmgr.o $(OBJPATH)write.o $(OBJPATH)write_pre.o

CRYPTOBJS	= $(OBJPATH)aes_modes.o $(OBJPATH)aescrypt.o $(OBJPATH)aeskey.o \
			  $(OBJPATH)aestab.o $(OBJPATH)bfecb.o $(OBJPATH)bfenc.o \
			  $(OBJPATH)bfskey.o $(OBJPATH)castecb.o $(OBJPATH)castenc.o \
			  $(OBJPATH)castskey.o $(OBJPATH)descbc.o $(OBJPATH)desecb.o \
			  $(OBJPATH)desecb3.o $(OBJPATH)desenc.o $(OBJPATH)desskey.o \
			  $(OBJPATH)gcm.o $(OBJPATH)gf128mul.o $(OBJPATH)icbc.o \
			  $(OBJPATH)iecb.o $(OBJPATH)iskey.o $(OBJPATH)rc2cbc.o \
			  $(OBJPATH)rc2ecb.o $(OBJPATH)rc2skey.o $(OBJPATH)rc4enc.o \
			  $(OBJPATH)rc4skey.o $(OBJPATH)rc5ecb.o $(OBJPATH)rc5enc.o \
			  $(OBJPATH)rc5skey.o

CTXOBJS		= $(OBJPATH)ctx_3des.o $(OBJPATH)ctx_aes.o $(OBJPATH)ctx_attr.o \
			  $(OBJPATH)ctx_bf.o $(OBJPATH)ctx_cast.o $(OBJPATH)ctx_des.o \
			  $(OBJPATH)ctx_dh.o $(OBJPATH)ctx_dsa.o $(OBJPATH)ctx_ecdh.o \
			  $(OBJPATH)ctx_ecdsa.o $(OBJPATH)ctx_elg.o \
			  $(OBJPATH)ctx_generic.o $(OBJPATH)ctx_hmd5.o $(OBJPATH)ctx_hrmd.o \
			  $(OBJPATH)ctx_hsha.o $(OBJPATH)ctx_hsha2.o $(OBJPATH)ctx_idea.o \
			  $(OBJPATH)ctx_md5.o $(OBJPATH)ctx_misc.o $(OBJPATH)ctx_rc2.o \
			  $(OBJPATH)ctx_rc4.o $(OBJPATH)ctx_rc5.o $(OBJPATH)ctx_ripe.o \
			  $(OBJPATH)ctx_rsa.o $(OBJPATH)ctx_sha.o $(OBJPATH)ctx_sha2.o \
			  $(OBJPATH)kg_dlp.o $(OBJPATH)kg_ecc.o $(OBJPATH)kg_prime.o \
			  $(OBJPATH)kg_rsa.o $(OBJPATH)keyload.o $(OBJPATH)key_id.o \
			  $(OBJPATH)key_rd.o $(OBJPATH)key_wr.o

DEVOBJS		= $(OBJPATH)dev_attr.o $(OBJPATH)hardware.o $(OBJPATH)hw_dummy.o \
			  $(OBJPATH)pkcs11.o $(OBJPATH)pkcs11_init.o $(OBJPATH)pkcs11_pkc.o \
			  $(OBJPATH)pkcs11_rd.o $(OBJPATH)pkcs11_wr.o $(OBJPATH)system.o

ENCDECOBJS	= $(OBJPATH)asn1_algid.o $(OBJPATH)asn1_chk.o $(OBJPATH)asn1_rd.o \
			  $(OBJPATH)asn1_wr.o $(OBJPATH)asn1_ext.o $(OBJPATH)base64.o \
			  $(OBJPATH)base64_id.o $(OBJPATH)misc_rw.o $(OBJPATH)pgp_rw.o

ENVOBJS		= $(OBJPATH)cms_denv.o $(OBJPATH)cms_env.o $(OBJPATH)cms_envpre.o \
			  $(OBJPATH)decode.o $(OBJPATH)encode.o $(OBJPATH)env_attr.o \
			  $(OBJPATH)pgp_denv.o $(OBJPATH)pgp_env.o $(OBJPATH)res_actn.o \
			  $(OBJPATH)res_denv.o $(OBJPATH)res_env.o

HASHOBJS	= $(OBJPATH)md5dgst.o $(OBJPATH)rmddgst.o $(OBJPATH)sha1dgst.o \
			  $(OBJPATH)sha2.o

IOOBJS		= $(OBJPATH)dns.o $(OBJPATH)dns_srv.o $(OBJPATH)file.o \
			  $(OBJPATH)http_rd.o $(OBJPATH)http_parse.o $(OBJPATH)http_wr.o \
			  $(OBJPATH)memory.o $(OBJPATH)net.o $(OBJPATH)net_proxy.o \
			  $(OBJPATH)net_trans.o $(OBJPATH)net_url.o $(OBJPATH)stream.o \
			  $(OBJPATH)tcp.o

KEYSETOBJS	= $(OBJPATH)dbms.o $(OBJPATH)ca_add.o $(OBJPATH)ca_clean.o \
			  $(OBJPATH)ca_issue.o $(OBJPATH)ca_misc.o $(OBJPATH)ca_rev.o \
			  $(OBJPATH)dbx_misc.o $(OBJPATH)dbx_rd.o $(OBJPATH)dbx_wr.o \
			  $(OBJPATH)http.o $(OBJPATH)key_attr.o $(OBJPATH)ldap.o \
			  $(OBJPATH)odbc.o $(OBJPATH)pgp.o $(OBJPATH)pgp_rd.o \
			  $(OBJPATH)pkcs12.o $(OBJPATH)pkcs12_rd.o $(OBJPATH)pkcs12_rdo.o \
			  $(OBJPATH)pkcs12_wr.o $(OBJPATH)pkcs15.o $(OBJPATH)pkcs15_add.o \
			  $(OBJPATH)pkcs15_adpb.o $(OBJPATH)pkcs15_adpr.o \
			  $(OBJPATH)pkcs15_atrd.o $(OBJPATH)pkcs15_atwr.o \
			  $(OBJPATH)pkcs15_get.o $(OBJPATH)pkcs15_getp.o \
			  $(OBJPATH)pkcs15_rd.o $(OBJPATH)pkcs15_set.o \
			  $(OBJPATH)pkcs15_wr.o

KRNLOBJS	= $(OBJPATH)attr_acl.o $(OBJPATH)certm_acl.o $(OBJPATH)init.o \
			  $(OBJPATH)int_msg.o $(OBJPATH)key_acl.o $(OBJPATH)mech_acl.o \
			  $(OBJPATH)msg_acl.o $(OBJPATH)obj_acc.o $(OBJPATH)objects.o \
			  $(OBJPATH)sec_mem.o $(OBJPATH)selftest.o $(OBJPATH)semaphore.o \
			  $(OBJPATH)sendmsg.o

LIBOBJS		= $(OBJPATH)cryptapi.o $(OBJPATH)cryptcrt.o $(OBJPATH)cryptctx.o \
			  $(OBJPATH)cryptdev.o $(OBJPATH)cryptenv.o $(OBJPATH)cryptkey.o \
			  $(OBJPATH)cryptlib.o $(OBJPATH)cryptses.o $(OBJPATH)cryptusr.o

MECHOBJS	= $(OBJPATH)keyex.o $(OBJPATH)keyex_int.o $(OBJPATH)keyex_rw.o \
			  $(OBJPATH)mech_cwrap.o $(OBJPATH)mech_drv.o $(OBJPATH)mech_int.o \
			  $(OBJPATH)mech_pkwrap.o $(OBJPATH)mech_privk.o \
			  $(OBJPATH)mech_sig.o $(OBJPATH)obj_qry.o $(OBJPATH)sign.o \
			  $(OBJPATH)sign_cms.o $(OBJPATH)sign_int.o $(OBJPATH)sign_pgp.o \
			  $(OBJPATH)sign_rw.o $(OBJPATH)sign_x509.o

MISCOBJS	= $(OBJPATH)int_api.o $(OBJPATH)int_attr.o $(OBJPATH)int_debug.o \
			  $(OBJPATH)int_env.o $(OBJPATH)int_err.o $(OBJPATH)int_mem.o \
			  $(OBJPATH)int_string.o $(OBJPATH)int_time.o $(OBJPATH)java_jni.o \
			  $(OBJPATH)os_spec.o $(OBJPATH)pgp_misc.o $(OBJPATH)random.o \
			  $(OBJPATH)rand_x917.o $(OBJPATH)unix.o $(OBJPATH)user.o \
			  $(OBJPATH)user_attr.o $(OBJPATH)user_cfg.o $(OBJPATH)user_rw.o

SESSOBJS	= $(OBJPATH)certstore.o $(OBJPATH)cmp.o $(OBJPATH)cmp_cli.o \
			  $(OBJPATH)cmp_cry.o $(OBJPATH)cmp_err.o $(OBJPATH)cmp_rd.o \
			  $(OBJPATH)cmp_rdmsg.o $(OBJPATH)cmp_svr.o $(OBJPATH)cmp_wr.o \
			  $(OBJPATH)cmp_wrmsg.o $(OBJPATH)ocsp.o $(OBJPATH)pnppki.o \
			  $(OBJPATH)rtcs.o $(OBJPATH)scep.o $(OBJPATH)scep_cli.o \
			  $(OBJPATH)scep_svr.o $(OBJPATH)scorebrd.o $(OBJPATH)sess_attr.o \
			  $(OBJPATH)sess_iattr.o $(OBJPATH)sess_rw.o $(OBJPATH)session.o \
			  $(OBJPATH)ssh.o $(OBJPATH)ssh1.o $(OBJPATH)ssh2.o \
			  $(OBJPATH)ssh2_authc.o $(OBJPATH)ssh2_auths.o \
			  $(OBJPATH)ssh2_chn.o $(OBJPATH)ssh2_cli.o $(OBJPATH)ssh2_cry.o \
			  $(OBJPATH)ssh2_msg.o $(OBJPATH)ssh2_msgc.o \
			  $(OBJPATH)ssh2_msgs.o $(OBJPATH)ssh2_rd.o $(OBJPATH)ssh2_svr.o \
			  $(OBJPATH)ssh2_wr.o $(OBJPATH)ssl.o $(OBJPATH)ssl_cli.o \
			  $(OBJPATH)ssl_cry.o $(OBJPATH)ssl_ext.o $(OBJPATH)ssl_hs.o \
			  $(OBJPATH)ssl_hsc.o $(OBJPATH)ssl_kmgmt.o $(OBJPATH)ssl_rd.o \
			  $(OBJPATH)ssl_suites.o $(OBJPATH)ssl_svr.o $(OBJPATH)ssl_wr.o \
			  $(OBJPATH)tsp.o

ZLIBOBJS	= $(OBJPATH)adler32.o $(OBJPATH)deflate.o $(OBJPATH)inffast.o \
			  $(OBJPATH)inflate.o $(OBJPATH)inftrees.o $(OBJPATH)trees.o \
			  $(OBJPATH)zutil.o

OBJS		= $(BNOBJS) $(CERTOBJS) $(CRYPTOBJS) $(CTXOBJS) $(DEVOBJS) \
			  $(ENCDECOBJS) $(ENVOBJS) $(HASHOBJS) $(IOOBJS) $(KEYSETOBJS) \
			  $(KRNLOBJS) $(LIBOBJS) $(MECHOBJS) $(MISCOBJS) $(SESSOBJS) \
			  $(ZLIBOBJS) $(OSOBJS)

# Win32 object files that replace ASMOBJS when building for Win32
# using Unix tools

WIN32ASMOBJS= bn/bn-win32.obj crypt/aescrypt2.obj crypt/b-win32.obj \
			  crypt/c-win32.obj crypt/d-win32.obj crypt/m5-win32.obj \
			  crypt/r4-win32.obj crypt/r5-win32.obj crypt/rm-win32.obj \
			  crypt/s1-win32.obj zlib/inffas32.obj zlib/match686.obj

# Object files for the self-test code

TESTOBJS	= certimp.o certproc.o certs.o devices.o envelope.o highlvl.o \
			  keydbx.o keyfile.o loadkey.o lowlvl.o s_cmp.o s_scep.o \
			  sreqresp.o ssh.o ssl.o stress.o suiteb.o testfunc.o testlib.o \
			  utils.o

# Various functions all make use of certain headers so we define the
# dependencies once here

IO_DEP = io/stream.h enc_dec/misc_rw.h enc_dec/pgp_rw.h

ASN1_DEP = $(IO_DEP) enc_dec/asn1.h enc_dec/asn1_ext.h

CERT_DEP = cert/cert.h cert/certfn.h

CRYPT_DEP	= cryptlib.h crypt.h cryptkrn.h misc/config.h misc/consts.h \
			  misc/debug.h misc/int_api.h misc/os_spec.h

KERNEL_DEP	= kernel/acl.h kernel/acl_perm.h kernel/kernel.h kernel/thread.h

ZLIB_DEP = zlib/zconf.h zlib/zlib.h zlib/zutil.h

#****************************************************************************
#*																			*
#*							Default and High-level Targets					*
#*																			*
#****************************************************************************

# Find the system type and use a conditional make depending on that.
#
# Slowaris doesn't ship with a compiler by default, so Sun had to provide
# something that pretends to be one for things that look for a cc.  This
# makes it really hard to figure out what's really going on.  The default cc,
# /usr/ucb/cc, is a script that looks for a real compiler elsewhere.  If the
# Sun compiler is installed, this will be via a link /usr/ccs/bin/ucbcc,
# which in turn points to /opt/SUNWspro.  If it's not installed, or installed
# incorrectly, it will bail out with a "package not installed" error.  We
# check for this bogus compiler and if we get the error message fall back to
# gcc, which is how most people just fix this mess.
#
# The MVS USS c89 compiler has a strict ordering of options.  That ordering
# can be relaxed with the _C89_CCMODE environment variable to accept options
# and file names in any order, so we check to make sure that this is set.
#
# The Cray uname reports the machine serial number instead of the machine
# type by default, so we have to explicitly check for Cray systems and
# modify the machine-detection mechanism to handle this.
#
# The '-' to disable error-checking in several cases below is necessary for
# the braindamaged QNX make, which bails out as soon as one of the tests
# fails, whether this would affect the make or not.
#
# We have to special-case the situation where the OS name is an alias for
# uname rather than being predefined (this occurs when cross-compiling),
# because the resulting expansion would contain two levels of `` escapes.  To
# handle this, we leave a predefined OS name in place, but replace a call to
# uname with instructions to the osversion.sh script to figure it out for
# itself.  In addition since $(CROSSCOMPILE) is usually a null value, we add
# an extra character to the comparison string to avoid syntax errors.

default:
	@make directories
	@make toolscripts
	@- if [ $(OSNAME) = 'OS/390' -a "$(_C89_CCMODE)" != "1" ] ; then \
		echo "The c89 environment variable _C89_CCMODE must be set to 1." >&2 ; \
		exit 1 ; \
	fi
	@./tools/buildall.sh $(OSNAME) $(CC) $(CFLAGS)

shared:
	@make directories
	@make toolscripts
	@- if [ $(OSNAME) = 'OS/390' -a "$(_C89_CCMODE)" != "1" ] ; then \
		echo "The c89 environment variable _C89_CCMODE must be set to 1." >&2 ; \
		exit 1; \
	fi
	@./tools/buildall.sh shared $(OSNAME) $(CC) $(CFLAGS)

debug:
	@make directories
	@make toolscripts
	@- if [ $(OSNAME) = 'OS/390' -a "$(_C89_CCMODE)" != "1" ] ; then \
		echo "The c89 environment variable _C89_CCMODE must be set to 1." >&2 ; \
		exit 1; \
	fi
	@./tools/buildall.sh $(OSNAME) $(CC) $(CFLAGS_DEBUG)

directories:
	@- if [ ! -d $(STATIC_OBJ_PATH) ] ; then mkdir $(STATIC_OBJ_DIR) ; fi
	@- if [ ! -d $(SHARED_OBJ_PATH) ] ; then mkdir $(SHARED_OBJ_DIR) ; fi

toolscripts:
	@for file in ./tools/*.sh ; do \
		if [ ! -x $$file ] ; then chmod +x $$file ; fi \
	done

# Frohe Ostern.

babies:
	@echo "Good grief, what do you think I am?  Unix is capable, but not that capable."

cookies:
	@echo "Mix 250g flour, 150g sugar, 125g butter, an egg, a few drops of vanilla"
	@echo "essence, and 1 tsp baking powder into a dough, cut cookies from rolls of"
	@echo "dough, bake for about 15 minutes at 180C until they turn very light brown"
	@echo "at the edges."

love:
	@echo "Nicht wahr?"

#****************************************************************************
#*																			*
#*								C Module Targets							*
#*																			*
#****************************************************************************

# Main directory

$(OBJPATH)cryptapi.o:	$(CRYPT_DEP) cryptapi.c
						$(CC) $(CFLAGS) -o $(OBJPATH)cryptapi.o cryptapi.c

$(OBJPATH)cryptcrt.o:	$(CRYPT_DEP) $(CERT_DEP) cryptcrt.c
						$(CC) $(CFLAGS) -o $(OBJPATH)cryptcrt.o cryptcrt.c

$(OBJPATH)cryptctx.o:	$(CRYPT_DEP) context/context.h cryptctx.c
						$(CC) $(CFLAGS) -o $(OBJPATH)cryptctx.o cryptctx.c

$(OBJPATH)cryptdev.o:	$(CRYPT_DEP) device/device.h cryptdev.c
						$(CC) $(CFLAGS) -o $(OBJPATH)cryptdev.o cryptdev.c

$(OBJPATH)cryptenv.o:	$(CRYPT_DEP) envelope/envelope.h $(ASN1_DEP) \
						cryptenv.c
						$(CC) $(CFLAGS) -o $(OBJPATH)cryptenv.o cryptenv.c

$(OBJPATH)cryptkey.o:	$(CRYPT_DEP) keyset/keyset.h cryptkey.c
						$(CC) $(CFLAGS) -o $(OBJPATH)cryptkey.o cryptkey.c

$(OBJPATH)cryptlib.o:	$(CRYPT_DEP) cryptlib.c
						$(CC) $(CFLAGS) -o $(OBJPATH)cryptlib.o cryptlib.c

$(OBJPATH)cryptses.o:	$(CRYPT_DEP) session/session.h cryptses.c
						$(CC) $(CFLAGS) -o $(OBJPATH)cryptses.o cryptses.c

$(OBJPATH)cryptusr.o:	$(CRYPT_DEP) misc/user.h cryptusr.c
						$(CC) $(CFLAGS) -o $(OBJPATH)cryptusr.o cryptusr.c

# Additional modules whose use needs to be explicitly enabled by the user.

$(OBJPATH)java_jni.o:	$(CRYPT_DEP) bindings/java_jni.c
						$(CC) $(CFLAGS) -o $(OBJPATH)java_jni.o bindings/java_jni.c

# bn subdirectory

$(OBJPATH)bn_add.o:		crypt/osconfig.h bn/bn.h bn/bn_lcl.h bn/bn_add.c
						$(CC) $(CFLAGS) -o $(OBJPATH)bn_add.o bn/bn_add.c

$(OBJPATH)bn_asm.o:		crypt/osconfig.h bn/bn.h bn/bn_lcl.h bn/bn_asm.c
						$(CC) $(CFLAGS) -o $(OBJPATH)bn_asm.o bn/bn_asm.c

$(OBJPATH)bn_ctx.o:		crypt/osconfig.h bn/bn.h bn/bn_lcl.h bn/bn_ctx.c
						$(CC) $(CFLAGS) -o $(OBJPATH)bn_ctx.o bn/bn_ctx.c

$(OBJPATH)bn_div.o:		crypt/osconfig.h bn/bn.h bn/bn_lcl.h bn/bn_div.c
						$(CC) $(CFLAGS) -o $(OBJPATH)bn_div.o bn/bn_div.c

$(OBJPATH)bn_exp.o:		crypt/osconfig.h bn/bn.h bn/bn_lcl.h bn/bn_exp.c
						$(CC) $(CFLAGS) -o $(OBJPATH)bn_exp.o bn/bn_exp.c

$(OBJPATH)bn_exp2.o:	crypt/osconfig.h bn/bn.h bn/bn_lcl.h bn/bn_exp2.c
						$(CC) $(CFLAGS) -o $(OBJPATH)bn_exp2.o bn/bn_exp2.c

$(OBJPATH)bn_gcd.o:		crypt/osconfig.h bn/bn.h bn/bn_lcl.h bn/bn_gcd.c
						$(CC) $(CFLAGS) -o $(OBJPATH)bn_gcd.o bn/bn_gcd.c

$(OBJPATH)bn_lib.o:		crypt/osconfig.h bn/bn.h bn/bn_lcl.h bn/bn_lib.c
						$(CC) $(CFLAGS) -o $(OBJPATH)bn_lib.o bn/bn_lib.c

$(OBJPATH)bn_mod.o:		crypt/osconfig.h bn/bn.h bn/bn_lcl.h bn/bn_mod.c
						$(CC) $(CFLAGS) -o $(OBJPATH)bn_mod.o bn/bn_mod.c

$(OBJPATH)bn_mont.o:	crypt/osconfig.h bn/bn.h bn/bn_lcl.h bn/bn_mont.c
						$(CC) $(CFLAGS) -o $(OBJPATH)bn_mont.o bn/bn_mont.c

$(OBJPATH)bn_mul.o:		crypt/osconfig.h bn/bn.h bn/bn_lcl.h bn/bn_mul.c
						$(CC) $(CFLAGS) -o $(OBJPATH)bn_mul.o bn/bn_mul.c

$(OBJPATH)bn_recp.o:	crypt/osconfig.h bn/bn.h bn/bn_lcl.h bn/bn_recp.c
						$(CC) $(CFLAGS) -o $(OBJPATH)bn_recp.o bn/bn_recp.c

$(OBJPATH)bn_shift.o:	crypt/osconfig.h bn/bn.h bn/bn_lcl.h bn/bn_shift.c
						$(CC) $(CFLAGS) -o $(OBJPATH)bn_shift.o bn/bn_shift.c

$(OBJPATH)bn_sqr.o:		crypt/osconfig.h bn/bn.h bn/bn_lcl.h bn/bn_sqr.c
						$(CC) $(CFLAGS) -o $(OBJPATH)bn_sqr.o bn/bn_sqr.c

$(OBJPATH)bn_word.o:	crypt/osconfig.h bn/bn.h bn/bn_lcl.h bn/bn_word.c
						$(CC) $(CFLAGS) -o $(OBJPATH)bn_word.o bn/bn_word.c

$(OBJPATH)ec_lib.o:		crypt/osconfig.h bn/bn.h bn/bn_lcl.h bn/ec.h bn/ec_lib.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ec_lib.o bn/ec_lib.c

$(OBJPATH)ecp_mont.o:	crypt/osconfig.h bn/bn.h bn/bn_lcl.h bn/ec.h bn/ecp_mont.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ecp_mont.o bn/ecp_mont.c

$(OBJPATH)ecp_smpl.o:	crypt/osconfig.h bn/bn.h bn/bn_lcl.h bn/ec.h bn/ecp_smpl.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ecp_smpl.o bn/ecp_smpl.c

$(OBJPATH)ec_mult.o:	crypt/osconfig.h bn/bn.h bn/bn_lcl.h bn/ec.h bn/ec_mult.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ec_mult.o bn/ec_mult.c

$(OBJPATH)ec_rand.o:	crypt/osconfig.h bn/bn.h bn/bn_lcl.h bn/ec.h bn/ec_rand.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ec_rand.o bn/ec_rand.c

$(OBJPATH)ec_kron.o:	crypt/osconfig.h bn/bn.h bn/bn_lcl.h bn/ec.h bn/ec_kron.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ec_kron.o bn/ec_kron.c

$(OBJPATH)ec_sqrt.o:	crypt/osconfig.h bn/bn.h bn/bn_lcl.h bn/ec.h bn/ec_sqrt.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ec_sqrt.o bn/ec_sqrt.c

# cert subdirectory

$(OBJPATH)certrev.o:	$(CRYPT_DEP) $(ASN1_DEP) $(CERT_DEP) cert/certrev.c
						$(CC) $(CFLAGS) -o $(OBJPATH)certrev.o cert/certrev.c

$(OBJPATH)certschk.o:	$(CRYPT_DEP) $(ASN1_DEP) $(CERT_DEP) cert/certschk.c
						$(CC) $(CFLAGS) -o $(OBJPATH)certschk.o cert/certschk.c

$(OBJPATH)certsign.o:	$(CRYPT_DEP) $(ASN1_DEP) $(CERT_DEP) cert/certsign.c
						$(CC) $(CFLAGS) -o $(OBJPATH)certsign.o cert/certsign.c

$(OBJPATH)certval.o:	$(CRYPT_DEP) $(ASN1_DEP) $(CERT_DEP) cert/certval.c
						$(CC) $(CFLAGS) -o $(OBJPATH)certval.o cert/certval.c

$(OBJPATH)chain.o:		$(CRYPT_DEP) $(ASN1_DEP) $(CERT_DEP) cert/chain.c
						$(CC) $(CFLAGS) -o $(OBJPATH)chain.o cert/chain.c

$(OBJPATH)chk_cert.o:	$(CRYPT_DEP) $(ASN1_DEP) $(CERT_DEP) cert/chk_cert.c
						$(CC) $(CFLAGS) -o $(OBJPATH)chk_cert.o cert/chk_cert.c

$(OBJPATH)chk_chn.o:	$(CRYPT_DEP) $(ASN1_DEP) $(CERT_DEP) cert/chk_chn.c
						$(CC) $(CFLAGS) -o $(OBJPATH)chk_chn.o cert/chk_chn.c

$(OBJPATH)chk_use.o:	$(CRYPT_DEP) $(ASN1_DEP) $(CERT_DEP) cert/chk_use.c
						$(CC) $(CFLAGS) -o $(OBJPATH)chk_use.o cert/chk_use.c

$(OBJPATH)comp_cert.o:	$(CRYPT_DEP) $(ASN1_DEP) $(CERT_DEP) cert/comp_cert.c
						$(CC) $(CFLAGS) -o $(OBJPATH)comp_cert.o cert/comp_cert.c

$(OBJPATH)comp_curs.o:	$(CRYPT_DEP) $(ASN1_DEP) $(CERT_DEP) cert/comp_curs.c
						$(CC) $(CFLAGS) -o $(OBJPATH)comp_curs.o cert/comp_curs.c

$(OBJPATH)comp_del.o:	$(CRYPT_DEP) $(ASN1_DEP) $(CERT_DEP) cert/comp_del.c
						$(CC) $(CFLAGS) -o $(OBJPATH)comp_del.o cert/comp_del.c

$(OBJPATH)comp_get.o:	$(CRYPT_DEP) $(ASN1_DEP) $(CERT_DEP) cert/comp_get.c
						$(CC) $(CFLAGS) -o $(OBJPATH)comp_get.o cert/comp_get.c

$(OBJPATH)comp_gets.o:	$(CRYPT_DEP) $(ASN1_DEP) $(CERT_DEP) cert/comp_gets.c
						$(CC) $(CFLAGS) -o $(OBJPATH)comp_gets.o cert/comp_gets.c

$(OBJPATH)comp_set.o:	$(CRYPT_DEP) $(ASN1_DEP) $(CERT_DEP) cert/comp_set.c
						$(CC) $(CFLAGS) -o $(OBJPATH)comp_set.o cert/comp_set.c

$(OBJPATH)dn.o:			$(CRYPT_DEP) $(ASN1_DEP) $(CERT_DEP) cert/dn.h cert/dn.h cert/dn.c
						$(CC) $(CFLAGS) -o $(OBJPATH)dn.o cert/dn.c

$(OBJPATH)dn_rw.o:		$(CRYPT_DEP) $(ASN1_DEP) $(CERT_DEP) cert/dn.h cert/dn_rw.c
						$(CC) $(CFLAGS) -o $(OBJPATH)dn_rw.o cert/dn_rw.c

$(OBJPATH)dnstring.o:	$(CRYPT_DEP) $(ASN1_DEP) $(CERT_DEP) cert/dn.h cert/dnstring.c
						$(CC) $(CFLAGS) -o $(OBJPATH)dnstring.o cert/dnstring.c

$(OBJPATH)ext.o:		$(CRYPT_DEP) $(ASN1_DEP) $(CERT_DEP) cert/certattr.h cert/ext.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ext.o cert/ext.c

$(OBJPATH)ext_add.o:	$(CRYPT_DEP) $(ASN1_DEP) $(CERT_DEP) cert/certattr.h cert/ext_add.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ext_add.o cert/ext_add.c

$(OBJPATH)ext_chk.o:	$(CRYPT_DEP) $(ASN1_DEP) $(CERT_DEP) cert/certattr.h cert/ext_chk.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ext_chk.o cert/ext_chk.c

$(OBJPATH)ext_copy.o:	$(CRYPT_DEP) $(ASN1_DEP) $(CERT_DEP) cert/certattr.h cert/ext_copy.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ext_copy.o cert/ext_copy.c

$(OBJPATH)ext_def.o:	$(CRYPT_DEP) $(ASN1_DEP) $(CERT_DEP) cert/certattr.h cert/ext_def.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ext_def.o cert/ext_def.c

$(OBJPATH)ext_rd.o:		$(CRYPT_DEP) $(ASN1_DEP) $(CERT_DEP) cert/certattr.h cert/ext_rd.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ext_rd.o cert/ext_rd.c

$(OBJPATH)ext_wr.o:		$(CRYPT_DEP) $(ASN1_DEP) $(CERT_DEP) cert/certattr.h cert/ext_wr.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ext_wr.o cert/ext_wr.c

$(OBJPATH)imp_chk.o:	$(CRYPT_DEP) $(ASN1_DEP) $(CERT_DEP) cert/imp_chk.c
						$(CC) $(CFLAGS) -o $(OBJPATH)imp_chk.o cert/imp_chk.c

$(OBJPATH)imp_exp.o:	$(CRYPT_DEP) $(ASN1_DEP) $(CERT_DEP) cert/imp_exp.c
						$(CC) $(CFLAGS) -o $(OBJPATH)imp_exp.o cert/imp_exp.c

$(OBJPATH)read.o:		$(CRYPT_DEP) $(ASN1_DEP) $(CERT_DEP) cert/read.c
						$(CC) $(CFLAGS) -o $(OBJPATH)read.o cert/read.c

$(OBJPATH)trustmgr.o:	$(CRYPT_DEP) $(ASN1_DEP) $(CERT_DEP) cert/trustmgr.h cert/trustmgr.c
						$(CC) $(CFLAGS) -o $(OBJPATH)trustmgr.o cert/trustmgr.c

$(OBJPATH)write.o:		$(CRYPT_DEP) $(ASN1_DEP) $(CERT_DEP) cert/write.c
						$(CC) $(CFLAGS) -o $(OBJPATH)write.o cert/write.c

$(OBJPATH)write_pre.o:	$(CRYPT_DEP) $(ASN1_DEP) $(CERT_DEP) cert/write_pre.c
						$(CC) $(CFLAGS) -o $(OBJPATH)write_pre.o cert/write_pre.c

# context subdirectory

$(OBJPATH)ctx_3des.o:	$(CRYPT_DEP) context/context.h crypt/des.h context/ctx_3des.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ctx_3des.o context/ctx_3des.c

$(OBJPATH)ctx_aes.o:	$(CRYPT_DEP) context/context.h crypt/aes.h crypt/aesopt.h \
						context/ctx_aes.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ctx_aes.o context/ctx_aes.c

$(OBJPATH)ctx_attr.o:	$(CRYPT_DEP) context/context.h context/ctx_attr.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ctx_attr.o context/ctx_attr.c

$(OBJPATH)ctx_bf.o:		$(CRYPT_DEP) context/context.h crypt/blowfish.h context/ctx_bf.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ctx_bf.o context/ctx_bf.c

$(OBJPATH)ctx_cast.o:	$(CRYPT_DEP) context/context.h crypt/cast.h context/ctx_cast.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ctx_cast.o context/ctx_cast.c

$(OBJPATH)ctx_des.o:	$(CRYPT_DEP) context/context.h crypt/testdes.h crypt/des.h \
						context/ctx_des.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ctx_des.o context/ctx_des.c

$(OBJPATH)ctx_dh.o:		$(CRYPT_DEP) context/context.h bn/bn.h context/ctx_dh.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ctx_dh.o context/ctx_dh.c

$(OBJPATH)ctx_dsa.o:	$(CRYPT_DEP) context/context.h bn/bn.h context/ctx_dsa.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ctx_dsa.o context/ctx_dsa.c

$(OBJPATH)ctx_ecdh.o:	$(CRYPT_DEP) context/context.h bn/bn.h context/ctx_ecdh.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ctx_ecdh.o context/ctx_ecdh.c

$(OBJPATH)ctx_ecdsa.o:	$(CRYPT_DEP) context/context.h bn/bn.h context/ctx_ecdsa.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ctx_ecdsa.o context/ctx_ecdsa.c

$(OBJPATH)ctx_elg.o:	$(CRYPT_DEP) context/context.h bn/bn.h context/ctx_elg.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ctx_elg.o context/ctx_elg.c

$(OBJPATH)ctx_generic.o: $(CRYPT_DEP) context/context.h context/ctx_generic.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ctx_generic.o context/ctx_generic.c

$(OBJPATH)ctx_hmd5.o:	$(CRYPT_DEP) context/context.h crypt/md5.h context/ctx_hmd5.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ctx_hmd5.o context/ctx_hmd5.c

$(OBJPATH)ctx_hrmd.o:	$(CRYPT_DEP) context/context.h crypt/ripemd.h context/ctx_hrmd.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ctx_hrmd.o context/ctx_hrmd.c

$(OBJPATH)ctx_hsha.o:	$(CRYPT_DEP) context/context.h crypt/sha.h context/ctx_hsha.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ctx_hsha.o context/ctx_hsha.c

$(OBJPATH)ctx_hsha2.o:	$(CRYPT_DEP) context/context.h crypt/sha2.h context/ctx_hsha2.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ctx_hsha2.o context/ctx_hsha2.c

$(OBJPATH)ctx_idea.o:	$(CRYPT_DEP) context/context.h crypt/idea.h context/ctx_idea.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ctx_idea.o context/ctx_idea.c

$(OBJPATH)ctx_md5.o:	$(CRYPT_DEP) context/context.h crypt/md5.h context/ctx_md5.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ctx_md5.o context/ctx_md5.c

$(OBJPATH)ctx_misc.o:	$(CRYPT_DEP) context/context.h context/ctx_misc.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ctx_misc.o context/ctx_misc.c

$(OBJPATH)ctx_rc2.o:	$(CRYPT_DEP) context/context.h crypt/rc2.h context/ctx_rc2.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ctx_rc2.o context/ctx_rc2.c

$(OBJPATH)ctx_rc4.o:	$(CRYPT_DEP) context/context.h crypt/rc4.h context/ctx_rc4.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ctx_rc4.o context/ctx_rc4.c

$(OBJPATH)ctx_rc5.o:	$(CRYPT_DEP) context/context.h crypt/rc5.h context/ctx_rc5.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ctx_rc5.o context/ctx_rc5.c

$(OBJPATH)ctx_ripe.o:	$(CRYPT_DEP) context/context.h crypt/ripemd.h context/ctx_ripe.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ctx_ripe.o context/ctx_ripe.c

$(OBJPATH)ctx_rsa.o:	$(CRYPT_DEP) context/context.h bn/bn.h context/ctx_rsa.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ctx_rsa.o context/ctx_rsa.c

$(OBJPATH)ctx_sha.o:	$(CRYPT_DEP) context/context.h crypt/sha.h context/ctx_sha.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ctx_sha.o context/ctx_sha.c

$(OBJPATH)ctx_sha2.o:	$(CRYPT_DEP) context/context.h crypt/sha2.h context/ctx_sha2.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ctx_sha2.o context/ctx_sha2.c

$(OBJPATH)kg_dlp.o:		$(CRYPT_DEP) context/context.h bn/bn_prime.h context/kg_dlp.c
						$(CC) $(CFLAGS) -o $(OBJPATH)kg_dlp.o context/kg_dlp.c

$(OBJPATH)kg_ecc.o:		$(CRYPT_DEP) context/context.h bn/bn_prime.h context/kg_ecc.c
						$(CC) $(CFLAGS) -o $(OBJPATH)kg_ecc.o context/kg_ecc.c

$(OBJPATH)kg_prime.o:	$(CRYPT_DEP) context/context.h bn/bn_prime.h context/kg_prime.c
						$(CC) $(CFLAGS) -o $(OBJPATH)kg_prime.o context/kg_prime.c

$(OBJPATH)kg_rsa.o:		$(CRYPT_DEP) context/context.h bn/bn_prime.h context/kg_rsa.c
						$(CC) $(CFLAGS) -o $(OBJPATH)kg_rsa.o context/kg_rsa.c

$(OBJPATH)keyload.o:	$(CRYPT_DEP) context/context.h context/keyload.c
						$(CC) $(CFLAGS) -o $(OBJPATH)keyload.o context/keyload.c

$(OBJPATH)key_id.o:		$(CRYPT_DEP) $(ASN1_DEP) context/key_id.c
						$(CC) $(CFLAGS) -o $(OBJPATH)key_id.o context/key_id.c

$(OBJPATH)key_rd.o:		$(CRYPT_DEP) $(ASN1_DEP) context/key_rd.c
						$(CC) $(CFLAGS) -o $(OBJPATH)key_rd.o context/key_rd.c

$(OBJPATH)key_wr.o:		$(CRYPT_DEP) $(ASN1_DEP) context/key_wr.c
						$(CC) $(CFLAGS) -o $(OBJPATH)key_wr.o context/key_wr.c

# crypt subdirectory - crypt algos

$(OBJPATH)aes_modes.o:	$(CRYPT_DEP) crypt/aes.h crypt/aesopt.h crypt/aes_modes.c
						$(CC) $(CFLAGS) -o $(OBJPATH)aes_modes.o crypt/aes_modes.c

$(OBJPATH)aescrypt.o:	$(CRYPT_DEP) crypt/aes.h crypt/aesopt.h crypt/aescrypt.c
						$(CC) $(CFLAGS) -o $(OBJPATH)aescrypt.o crypt/aescrypt.c

$(OBJPATH)aeskey.o:		$(CRYPT_DEP) crypt/aes.h crypt/aesopt.h crypt/aeskey.c
						$(CC) $(CFLAGS) -o $(OBJPATH)aeskey.o crypt/aeskey.c

$(OBJPATH)aestab.o:		$(CRYPT_DEP) crypt/aes.h crypt/aesopt.h crypt/aestab.c
						$(CC) $(CFLAGS) -o $(OBJPATH)aestab.o crypt/aestab.c

$(OBJPATH)bfecb.o:		crypt/osconfig.h crypt/blowfish.h crypt/bflocl.h crypt/bfecb.c
						$(CC) $(CFLAGS) -o $(OBJPATH)bfecb.o crypt/bfecb.c

$(OBJPATH)bfenc.o:		crypt/osconfig.h crypt/blowfish.h crypt/bflocl.h crypt/bfenc.c
						$(CC) $(CFLAGS) -o $(OBJPATH)bfenc.o crypt/bfenc.c

$(OBJPATH)bfskey.o:		crypt/osconfig.h crypt/blowfish.h crypt/bflocl.h crypt/bfpi.h \
						crypt/bfskey.c
						$(CC) $(CFLAGS) -o $(OBJPATH)bfskey.o crypt/bfskey.c

$(OBJPATH)castecb.o:	crypt/osconfig.h crypt/cast.h crypt/castlcl.h crypt/castecb.c
						$(CC) $(CFLAGS) -o $(OBJPATH)castecb.o crypt/castecb.c

$(OBJPATH)castenc.o:	crypt/osconfig.h crypt/cast.h crypt/castlcl.h crypt/castenc.c
						$(CC) $(CFLAGS) -o $(OBJPATH)castenc.o crypt/castenc.c

$(OBJPATH)castskey.o:	crypt/osconfig.h crypt/cast.h crypt/castlcl.h crypt/castsbox.h \
						crypt/castskey.c
						$(CC) $(CFLAGS) -o $(OBJPATH)castskey.o crypt/castskey.c

$(OBJPATH)descbc.o:		crypt/osconfig.h crypt/des.h crypt/deslocl.h crypt/descbc.c
						$(CC) $(CFLAGS) -o $(OBJPATH)descbc.o crypt/descbc.c

$(OBJPATH)desecb.o:		crypt/osconfig.h crypt/des.h crypt/deslocl.h crypt/desecb.c
						$(CC) $(CFLAGS) -o $(OBJPATH)desecb.o crypt/desecb.c

$(OBJPATH)desecb3.o:	crypt/osconfig.h crypt/des.h crypt/deslocl.h crypt/desecb3.c
						$(CC) $(CFLAGS) -o $(OBJPATH)desecb3.o crypt/desecb3.c

$(OBJPATH)desenc.o:		crypt/osconfig.h crypt/des.h crypt/deslocl.h crypt/desenc.c
						$(CC) $(CFLAGS) -o $(OBJPATH)desenc.o crypt/desenc.c

$(OBJPATH)desskey.o:	crypt/osconfig.h crypt/des.h crypt/deslocl.h crypt/desskey.c
						$(CC) $(CFLAGS) -o $(OBJPATH)desskey.o crypt/desskey.c

$(OBJPATH)gcm.o:		$(CRYPT_DEP) crypt/gf128mul.h crypt/gcm.h crypt/mode_hdr.h \
						crypt/gcm.c
						$(CC) $(CFLAGS) -o $(OBJPATH)gcm.o crypt/gcm.c

$(OBJPATH)gf128mul.o:	$(CRYPT_DEP) crypt/gf128mul.h crypt/mode_hdr.h \
						crypt/gf_mul_lo.h crypt/gf128mul.c
						$(CC) $(CFLAGS) -o $(OBJPATH)gf128mul.o crypt/gf128mul.c

$(OBJPATH)icbc.o:		$(CRYPT_DEP) crypt/idea.h crypt/idealocl.h crypt/icbc.c
						$(CC) $(CFLAGS) -o $(OBJPATH)icbc.o crypt/icbc.c

$(OBJPATH)iecb.o:		$(CRYPT_DEP) crypt/idea.h crypt/idealocl.h crypt/iecb.c
						$(CC) $(CFLAGS) -o $(OBJPATH)iecb.o crypt/iecb.c

$(OBJPATH)iskey.o:		$(CRYPT_DEP) crypt/idea.h crypt/idealocl.h crypt/iskey.c
						$(CC) $(CFLAGS) -o $(OBJPATH)iskey.o crypt/iskey.c

$(OBJPATH)rc2cbc.o:		crypt/osconfig.h crypt/rc2.h crypt/rc2locl.h crypt/rc2cbc.c
						$(CC) $(CFLAGS) -o $(OBJPATH)rc2cbc.o crypt/rc2cbc.c

$(OBJPATH)rc2ecb.o:		crypt/osconfig.h crypt/rc2.h crypt/rc2locl.h crypt/rc2ecb.c
						$(CC) $(CFLAGS) -o $(OBJPATH)rc2ecb.o crypt/rc2ecb.c

$(OBJPATH)rc2skey.o:	crypt/osconfig.h crypt/rc2.h crypt/rc2locl.h crypt/rc2skey.c
						$(CC) $(CFLAGS) -o $(OBJPATH)rc2skey.o crypt/rc2skey.c

$(OBJPATH)rc4enc.o:		crypt/osconfig.h crypt/rc4.h crypt/rc4locl.h crypt/rc4enc.c
						$(CC) $(CFLAGS) -o $(OBJPATH)rc4enc.o crypt/rc4enc.c

$(OBJPATH)rc4skey.o:	crypt/osconfig.h crypt/rc4.h crypt/rc4locl.h crypt/rc4skey.c
						$(CC) $(CFLAGS) -o $(OBJPATH)rc4skey.o crypt/rc4skey.c

$(OBJPATH)rc5ecb.o:		crypt/osconfig.h crypt/rc5.h crypt/rc5locl.h crypt/rc5ecb.c
						$(CC) $(CFLAGS) -o $(OBJPATH)rc5ecb.o crypt/rc5ecb.c

$(OBJPATH)rc5enc.o:		crypt/osconfig.h crypt/rc5.h crypt/rc5locl.h crypt/rc5enc.c
						$(CC) $(CFLAGS) -o $(OBJPATH)rc5enc.o crypt/rc5enc.c

$(OBJPATH)rc5skey.o:	crypt/osconfig.h crypt/rc5.h crypt/rc5locl.h crypt/rc5skey.c
						$(CC) $(CFLAGS) -o $(OBJPATH)rc5skey.o crypt/rc5skey.c

# crypt subdirectory - hash algos

$(OBJPATH)md5dgst.o:	crypt/osconfig.h crypt/md5.h crypt/md5locl.h \
						crypt/md5dgst.c
						$(CC) $(CFLAGS) -o $(OBJPATH)md5dgst.o crypt/md5dgst.c

$(OBJPATH)rmddgst.o:	crypt/osconfig.h crypt/ripemd.h crypt/rmdlocl.h \
						crypt/rmddgst.c
						$(CC) $(CFLAGS) -o $(OBJPATH)rmddgst.o crypt/rmddgst.c

$(OBJPATH)sha1dgst.o:	crypt/osconfig.h crypt/sha.h crypt/sha1locl.h \
						crypt/sha1dgst.c
						$(CC) $(CFLAGS) -o $(OBJPATH)sha1dgst.o crypt/sha1dgst.c

$(OBJPATH)sha2.o:		crypt/osconfig.h crypt/sha.h crypt/sha1locl.h crypt/sha2.c
						$(CC) $(CFLAGS) -o $(OBJPATH)sha2.o crypt/sha2.c

# device subdirectory

$(OBJPATH)dev_attr.o:	$(CRYPT_DEP) device/device.h device/dev_attr.c
						$(CC) $(CFLAGS) -o $(OBJPATH)dev_attr.o device/dev_attr.c

$(OBJPATH)hardware.o:	$(CRYPT_DEP) device/device.h device/hardware.c
						$(CC) $(CFLAGS) -o $(OBJPATH)hardware.o device/hardware.c

$(OBJPATH)hw_dummy.o:	$(CRYPT_DEP) device/device.h device/hw_dummy.c
						$(CC) $(CFLAGS) -o $(OBJPATH)hw_dummy.o device/hw_dummy.c

$(OBJPATH)pkcs11.o:		$(CRYPT_DEP) device/device.h device/pkcs11_api.h device/pkcs11.c
						$(CC) $(CFLAGS) -o $(OBJPATH)pkcs11.o device/pkcs11.c

$(OBJPATH)pkcs11_init.o: $(CRYPT_DEP) device/device.h device/pkcs11_api.h \
						device/pkcs11_init.c
						$(CC) $(CFLAGS) -o $(OBJPATH)pkcs11_init.o device/pkcs11_init.c

$(OBJPATH)pkcs11_pkc.o:	$(CRYPT_DEP) device/device.h device/pkcs11_api.h \
						device/pkcs11_pkc.c
						$(CC) $(CFLAGS) -o $(OBJPATH)pkcs11_pkc.o device/pkcs11_pkc.c

$(OBJPATH)pkcs11_rd.o:	$(CRYPT_DEP) device/device.h device/pkcs11_api.h \
						device/pkcs11_rd.c
						$(CC) $(CFLAGS) -o $(OBJPATH)pkcs11_rd.o device/pkcs11_rd.c

$(OBJPATH)pkcs11_wr.o:	$(CRYPT_DEP) device/device.h device/pkcs11_api.h \
						device/pkcs11_wr.c
						$(CC) $(CFLAGS) -o $(OBJPATH)pkcs11_wr.o device/pkcs11_wr.c

$(OBJPATH)system.o:		$(CRYPT_DEP) device/device.h device/capabil.h device/system.c
						$(CC) $(CFLAGS) -o $(OBJPATH)system.o device/system.c

# enc_dec subdirectory

$(OBJPATH)asn1_algid.o:	$(CRYPT_DEP) $(ASN1_DEP) enc_dec/asn1_algid.c
						$(CC) $(CFLAGS) -o $(OBJPATH)asn1_algid.o enc_dec/asn1_algid.c

$(OBJPATH)asn1_chk.o:	$(CRYPT_DEP) $(ASN1_DEP) enc_dec/asn1_chk.c
						$(CC) $(CFLAGS) -o $(OBJPATH)asn1_chk.o enc_dec/asn1_chk.c

$(OBJPATH)asn1_ext.o:	$(CRYPT_DEP) $(ASN1_DEP) enc_dec/asn1_oids.h enc_dec/asn1_ext.c
						$(CC) $(CFLAGS) -o $(OBJPATH)asn1_ext.o enc_dec/asn1_ext.c

$(OBJPATH)asn1_rd.o:	$(CRYPT_DEP) $(ASN1_DEP) enc_dec/asn1_rd.c
						$(CC) $(CFLAGS) -o $(OBJPATH)asn1_rd.o enc_dec/asn1_rd.c

$(OBJPATH)asn1_wr.o:	$(CRYPT_DEP) $(ASN1_DEP) enc_dec/asn1_wr.c
						$(CC) $(CFLAGS) -o $(OBJPATH)asn1_wr.o enc_dec/asn1_wr.c

$(OBJPATH)base64.o:		$(CRYPT_DEP) enc_dec/base64.c
						$(CC) $(CFLAGS) -o $(OBJPATH)base64.o enc_dec/base64.c

$(OBJPATH)base64_id.o:	$(CRYPT_DEP) enc_dec/base64_id.c
						$(CC) $(CFLAGS) -o $(OBJPATH)base64_id.o enc_dec/base64_id.c

$(OBJPATH)misc_rw.o:	$(CRYPT_DEP) $(IO_DEP) enc_dec/misc_rw.c
						$(CC) $(CFLAGS) -o $(OBJPATH)misc_rw.o enc_dec/misc_rw.c

$(OBJPATH)pgp_rw.o:		$(CRYPT_DEP) $(IO_DEP) enc_dec/pgp_rw.c
						$(CC) $(CFLAGS) -o $(OBJPATH)pgp_rw.o enc_dec/pgp_rw.c

# envelope subdirectory

$(OBJPATH)cms_denv.o:	$(CRYPT_DEP) envelope/envelope.h $(ASN1_DEP) \
						envelope/cms_denv.c
						$(CC) $(CFLAGS) -o $(OBJPATH)cms_denv.o envelope/cms_denv.c

$(OBJPATH)cms_env.o:	$(CRYPT_DEP) envelope/envelope.h $(ASN1_DEP) \
						envelope/cms_env.c
						$(CC) $(CFLAGS) -o $(OBJPATH)cms_env.o envelope/cms_env.c

$(OBJPATH)cms_envpre.o:	$(CRYPT_DEP) envelope/envelope.h $(ASN1_DEP) \
						envelope/cms_envpre.c
						$(CC) $(CFLAGS) -o $(OBJPATH)cms_envpre.o envelope/cms_envpre.c

$(OBJPATH)decode.o:		$(CRYPT_DEP) envelope/envelope.h $(ASN1_DEP) \
						envelope/decode.c
						$(CC) $(CFLAGS) -o $(OBJPATH)decode.o envelope/decode.c

$(OBJPATH)encode.o:		$(CRYPT_DEP) envelope/envelope.h $(ASN1_DEP) \
						envelope/encode.c
						$(CC) $(CFLAGS) -o $(OBJPATH)encode.o envelope/encode.c

$(OBJPATH)env_attr.o:	$(CRYPT_DEP) envelope/envelope.h envelope/env_attr.c
						$(CC) $(CFLAGS) -o $(OBJPATH)env_attr.o envelope/env_attr.c

$(OBJPATH)pgp_denv.o:	$(CRYPT_DEP) $(IO_DEP) misc/pgp.h envelope/pgp_denv.c
						$(CC) $(CFLAGS) -o $(OBJPATH)pgp_denv.o envelope/pgp_denv.c

$(OBJPATH)pgp_env.o:	$(CRYPT_DEP) $(IO_DEP) misc/pgp.h envelope/pgp_env.c
						$(CC) $(CFLAGS) -o $(OBJPATH)pgp_env.o envelope/pgp_env.c

$(OBJPATH)res_actn.o:	$(CRYPT_DEP) envelope/envelope.h envelope/res_actn.c
						$(CC) $(CFLAGS) -o $(OBJPATH)res_actn.o envelope/res_actn.c

$(OBJPATH)res_denv.o:	$(CRYPT_DEP) envelope/envelope.h envelope/res_denv.c
						$(CC) $(CFLAGS) -o $(OBJPATH)res_denv.o envelope/res_denv.c

$(OBJPATH)res_env.o:	$(CRYPT_DEP) envelope/envelope.h envelope/res_env.c
						$(CC) $(CFLAGS) -o $(OBJPATH)res_env.o envelope/res_env.c

# io subdirectory

$(OBJPATH)dns.o:		$(CRYPT_DEP) io/tcp.h io/dns.c
						$(CC) $(CFLAGS) -o $(OBJPATH)dns.o io/dns.c

$(OBJPATH)dns_srv.o:	$(CRYPT_DEP) io/tcp.h io/dns_srv.c
						$(CC) $(CFLAGS) -o $(OBJPATH)dns_srv.o io/dns_srv.c

$(OBJPATH)file.o:		$(CRYPT_DEP) $(ASN1_DEP) io/file.h io/file.c
						$(CC) $(CFLAGS) -o $(OBJPATH)file.o io/file.c

$(OBJPATH)http_rd.o:	$(CRYPT_DEP) io/http.h io/http_rd.c
						$(CC) $(CFLAGS) -o $(OBJPATH)http_rd.o io/http_rd.c

$(OBJPATH)http_parse.o:	$(CRYPT_DEP) io/http.h io/http_parse.c
						$(CC) $(CFLAGS) -o $(OBJPATH)http_parse.o io/http_parse.c

$(OBJPATH)http_wr.o:	$(CRYPT_DEP) io/http.h io/http_wr.c
						$(CC) $(CFLAGS) -o $(OBJPATH)http_wr.o io/http_wr.c

$(OBJPATH)memory.o:		$(CRYPT_DEP) $(ASN1_DEP) io/memory.c
						$(CC) $(CFLAGS) -o $(OBJPATH)memory.o io/memory.c

$(OBJPATH)net.o:		$(CRYPT_DEP) $(ASN1_DEP) io/tcp.h io/net.c
						$(CC) $(CFLAGS) -o $(OBJPATH)net.o io/net.c

$(OBJPATH)net_proxy.o:	$(CRYPT_DEP) $(ASN1_DEP) io/tcp.h io/net_proxy.c
						$(CC) $(CFLAGS) -o $(OBJPATH)net_proxy.o io/net_proxy.c

$(OBJPATH)net_trans.o:	$(CRYPT_DEP) $(ASN1_DEP) io/tcp.h io/net_trans.c
						$(CC) $(CFLAGS) -o $(OBJPATH)net_trans.o io/net_trans.c

$(OBJPATH)net_url.o:	$(CRYPT_DEP) $(ASN1_DEP) io/tcp.h io/net_url.c
						$(CC) $(CFLAGS) -o $(OBJPATH)net_url.o io/net_url.c

$(OBJPATH)stream.o:		$(CRYPT_DEP) $(ASN1_DEP) io/stream.c
						$(CC) $(CFLAGS) -o $(OBJPATH)stream.o io/stream.c

$(OBJPATH)tcp.o:		$(CRYPT_DEP) io/tcp.h io/tcp.c
						$(CC) $(CFLAGS) -o $(OBJPATH)tcp.o io/tcp.c

# kernel subdirectory

$(OBJPATH)attr_acl.o:	$(CRYPT_DEP) $(KERNEL_DEP) kernel/attr_acl.c
						$(CC) $(CFLAGS) -o $(OBJPATH)attr_acl.o kernel/attr_acl.c

$(OBJPATH)certm_acl.o:	$(CRYPT_DEP) $(KERNEL_DEP) kernel/certm_acl.c
						$(CC) $(CFLAGS) -o $(OBJPATH)certm_acl.o kernel/certm_acl.c

$(OBJPATH)init.o:		$(CRYPT_DEP) $(KERNEL_DEP) kernel/init.c
						$(CC) $(CFLAGS) -o $(OBJPATH)init.o kernel/init.c

$(OBJPATH)int_msg.o:	$(CRYPT_DEP) $(KERNEL_DEP) kernel/int_msg.c
						$(CC) $(CFLAGS) -o $(OBJPATH)int_msg.o kernel/int_msg.c

$(OBJPATH)key_acl.o:	$(CRYPT_DEP) $(KERNEL_DEP) kernel/key_acl.c
						$(CC) $(CFLAGS) -o $(OBJPATH)key_acl.o kernel/key_acl.c

$(OBJPATH)mech_acl.o:	$(CRYPT_DEP) $(KERNEL_DEP) kernel/mech_acl.c
						$(CC) $(CFLAGS) -o $(OBJPATH)mech_acl.o kernel/mech_acl.c

$(OBJPATH)msg_acl.o:	$(CRYPT_DEP) $(KERNEL_DEP) kernel/msg_acl.c
						$(CC) $(CFLAGS) -o $(OBJPATH)msg_acl.o kernel/msg_acl.c

$(OBJPATH)obj_acc.o:	$(CRYPT_DEP) $(KERNEL_DEP) kernel/obj_acc.c
						$(CC) $(CFLAGS) -o $(OBJPATH)obj_acc.o kernel/obj_acc.c

$(OBJPATH)objects.o:	$(CRYPT_DEP) $(KERNEL_DEP) kernel/objects.c
						$(CC) $(CFLAGS) -o $(OBJPATH)objects.o kernel/objects.c

$(OBJPATH)sec_mem.o:	$(CRYPT_DEP) $(KERNEL_DEP) kernel/sec_mem.c
						$(CC) $(CFLAGS) -o $(OBJPATH)sec_mem.o kernel/sec_mem.c

$(OBJPATH)selftest.o:	$(CRYPT_DEP) $(KERNEL_DEP) kernel/selftest.c
						$(CC) $(CFLAGS) -o $(OBJPATH)selftest.o kernel/selftest.c

$(OBJPATH)semaphore.o:	$(CRYPT_DEP) $(KERNEL_DEP) kernel/semaphore.c
						$(CC) $(CFLAGS) -o $(OBJPATH)semaphore.o kernel/semaphore.c

$(OBJPATH)sendmsg.o:	$(CRYPT_DEP) $(KERNEL_DEP) kernel/sendmsg.c
						$(CC) $(CFLAGS) -o $(OBJPATH)sendmsg.o kernel/sendmsg.c

# keyset subdirectory

$(OBJPATH)dbms.o:		$(CRYPT_DEP) keyset/keyset.h keyset/dbms.c
						$(CC) $(CFLAGS) -o $(OBJPATH)dbms.o keyset/dbms.c

$(OBJPATH)ca_add.o:		$(CRYPT_DEP) keyset/keyset.h keyset/ca_add.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ca_add.o keyset/ca_add.c

$(OBJPATH)ca_clean.o:	$(CRYPT_DEP) keyset/keyset.h keyset/ca_clean.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ca_clean.o keyset/ca_clean.c

$(OBJPATH)ca_issue.o:	$(CRYPT_DEP) keyset/keyset.h keyset/ca_issue.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ca_issue.o keyset/ca_issue.c

$(OBJPATH)ca_misc.o:	$(CRYPT_DEP) keyset/keyset.h keyset/ca_misc.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ca_misc.o keyset/ca_misc.c

$(OBJPATH)ca_rev.o:		$(CRYPT_DEP) keyset/keyset.h keyset/ca_rev.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ca_rev.o keyset/ca_rev.c

$(OBJPATH)dbx_misc.o:	$(CRYPT_DEP) keyset/keyset.h keyset/dbx_misc.c
						$(CC) $(CFLAGS) -o $(OBJPATH)dbx_misc.o keyset/dbx_misc.c

$(OBJPATH)dbx_rd.o:		$(CRYPT_DEP) keyset/keyset.h keyset/dbx_rd.c
						$(CC) $(CFLAGS) -o $(OBJPATH)dbx_rd.o keyset/dbx_rd.c

$(OBJPATH)dbx_wr.o:		$(CRYPT_DEP) keyset/keyset.h keyset/dbx_wr.c
						$(CC) $(CFLAGS) -o $(OBJPATH)dbx_wr.o keyset/dbx_wr.c

$(OBJPATH)http.o:		$(CRYPT_DEP) keyset/keyset.h keyset/http.c
						$(CC) $(CFLAGS) -o $(OBJPATH)http.o keyset/http.c

$(OBJPATH)key_attr.o:	$(CRYPT_DEP) keyset/keyset.h keyset/key_attr.c
						$(CC) $(CFLAGS) -o $(OBJPATH)key_attr.o keyset/key_attr.c

$(OBJPATH)ldap.o:		$(CRYPT_DEP) keyset/keyset.h keyset/ldap.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ldap.o keyset/ldap.c

$(OBJPATH)odbc.o:		$(CRYPT_DEP) keyset/keyset.h keyset/odbc.c
						$(CC) $(CFLAGS) -o $(OBJPATH)odbc.o keyset/odbc.c

$(OBJPATH)pgp.o:		$(CRYPT_DEP) misc/pgp.h keyset/pgp.c
						$(CC) $(CFLAGS) -o $(OBJPATH)pgp.o keyset/pgp.c

$(OBJPATH)pgp_rd.o:		$(CRYPT_DEP) misc/pgp.h keyset/pgp_rd.c
						$(CC) $(CFLAGS) -o $(OBJPATH)pgp_rd.o keyset/pgp_rd.c

$(OBJPATH)pkcs12.o:		$(CRYPT_DEP) keyset/keyset.h keyset/pkcs12.h keyset/pkcs12.c
						$(CC) $(CFLAGS) -o $(OBJPATH)pkcs12.o keyset/pkcs12.c

$(OBJPATH)pkcs12_rd.o:	$(CRYPT_DEP) keyset/keyset.h keyset/pkcs12.h keyset/pkcs12_rd.c
						$(CC) $(CFLAGS) -o $(OBJPATH)pkcs12_rd.o keyset/pkcs12_rd.c

$(OBJPATH)pkcs12_rdo.o:	$(CRYPT_DEP) keyset/keyset.h keyset/pkcs12.h keyset/pkcs12_rdo.c
						$(CC) $(CFLAGS) -o $(OBJPATH)pkcs12_rdo.o keyset/pkcs12_rdo.c

$(OBJPATH)pkcs12_wr.o:	$(CRYPT_DEP) keyset/keyset.h keyset/pkcs12.h keyset/pkcs12_wr.c
						$(CC) $(CFLAGS) -o $(OBJPATH)pkcs12_wr.o keyset/pkcs12_wr.c

$(OBJPATH)pkcs15.o:		$(CRYPT_DEP) keyset/keyset.h keyset/pkcs15.h keyset/pkcs15.c
						$(CC) $(CFLAGS) -o $(OBJPATH)pkcs15.o keyset/pkcs15.c

$(OBJPATH)pkcs15_add.o:	$(CRYPT_DEP) keyset/keyset.h keyset/pkcs15.h keyset/pkcs15_add.c
						$(CC) $(CFLAGS) -o $(OBJPATH)pkcs15_add.o keyset/pkcs15_add.c

$(OBJPATH)pkcs15_adpb.o: $(CRYPT_DEP) keyset/keyset.h keyset/pkcs15.h keyset/pkcs15_adpb.c
						$(CC) $(CFLAGS) -o $(OBJPATH)pkcs15_adpb.o keyset/pkcs15_adpb.c

$(OBJPATH)pkcs15_adpr.o: $(CRYPT_DEP) keyset/keyset.h keyset/pkcs15.h keyset/pkcs15_adpr.c
						$(CC) $(CFLAGS) -o $(OBJPATH)pkcs15_adpr.o keyset/pkcs15_adpr.c

$(OBJPATH)pkcs15_atrd.o: $(CRYPT_DEP) keyset/keyset.h keyset/pkcs15.h keyset/pkcs15_atrd.c
						$(CC) $(CFLAGS) -o $(OBJPATH)pkcs15_atrd.o keyset/pkcs15_atrd.c

$(OBJPATH)pkcs15_atwr.o: $(CRYPT_DEP) keyset/keyset.h keyset/pkcs15.h keyset/pkcs15_atwr.c
						$(CC) $(CFLAGS) -o $(OBJPATH)pkcs15_atwr.o keyset/pkcs15_atwr.c

$(OBJPATH)pkcs15_get.o:	$(CRYPT_DEP) keyset/keyset.h keyset/pkcs15.h keyset/pkcs15_get.c
						$(CC) $(CFLAGS) -o $(OBJPATH)pkcs15_get.o keyset/pkcs15_get.c

$(OBJPATH)pkcs15_getp.o: $(CRYPT_DEP) keyset/keyset.h keyset/pkcs15.h keyset/pkcs15_getp.c
						$(CC) $(CFLAGS) -o $(OBJPATH)pkcs15_getp.o keyset/pkcs15_getp.c

$(OBJPATH)pkcs15_rd.o:	$(CRYPT_DEP) keyset/keyset.h keyset/pkcs15.h keyset/pkcs15_rd.c
						$(CC) $(CFLAGS) -o $(OBJPATH)pkcs15_rd.o keyset/pkcs15_rd.c

$(OBJPATH)pkcs15_set.o:	$(CRYPT_DEP) keyset/keyset.h keyset/pkcs15.h keyset/pkcs15_set.c
						$(CC) $(CFLAGS) -o $(OBJPATH)pkcs15_set.o keyset/pkcs15_set.c

$(OBJPATH)pkcs15_wr.o:	$(CRYPT_DEP) keyset/keyset.h keyset/pkcs15.h keyset/pkcs15_wr.c
						$(CC) $(CFLAGS) -o $(OBJPATH)pkcs15_wr.o keyset/pkcs15_wr.c

# mechanism subdirectory

$(OBJPATH)keyex.o:		$(CRYPT_DEP) $(ASN1_DEP) mechs/mech.h mechs/keyex.c
						$(CC) $(CFLAGS) -o $(OBJPATH)keyex.o mechs/keyex.c

$(OBJPATH)keyex_int.o:	$(CRYPT_DEP) $(ASN1_DEP) mechs/mech.h mechs/keyex_int.c
						$(CC) $(CFLAGS) -o $(OBJPATH)keyex_int.o mechs/keyex_int.c

$(OBJPATH)keyex_rw.o:	$(CRYPT_DEP) $(ASN1_DEP) mechs/mech.h mechs/keyex_rw.c
						$(CC) $(CFLAGS) -o $(OBJPATH)keyex_rw.o mechs/keyex_rw.c

$(OBJPATH)mech_cwrap.o:	$(CRYPT_DEP) $(ASN1_DEP) mechs/mech.h mechs/mech_cwrap.c
						$(CC) $(CFLAGS) -o $(OBJPATH)mech_cwrap.o mechs/mech_cwrap.c

$(OBJPATH)mech_drv.o:	$(CRYPT_DEP) $(ASN1_DEP) mechs/mech.h mechs/mech_drv.c
						$(CC) $(CFLAGS) -o $(OBJPATH)mech_drv.o mechs/mech_drv.c

$(OBJPATH)mech_int.o:	$(CRYPT_DEP) $(ASN1_DEP) mechs/mech.h mechs/mech_int.c
						$(CC) $(CFLAGS) -o $(OBJPATH)mech_int.o mechs/mech_int.c

$(OBJPATH)mech_pkwrap.o: $(CRYPT_DEP) $(ASN1_DEP) mechs/mech.h mechs/mech_pkwrap.c
						$(CC) $(CFLAGS) -o $(OBJPATH)mech_pkwrap.o mechs/mech_pkwrap.c

$(OBJPATH)mech_privk.o:	$(CRYPT_DEP) $(ASN1_DEP) mechs/mech.h mechs/mech_privk.c
						$(CC) $(CFLAGS) -o $(OBJPATH)mech_privk.o mechs/mech_privk.c

$(OBJPATH)mech_sig.o:	$(CRYPT_DEP) $(ASN1_DEP) mechs/mech.h mechs/mech_sig.c
						$(CC) $(CFLAGS) -o $(OBJPATH)mech_sig.o mechs/mech_sig.c

$(OBJPATH)obj_qry.o:	$(CRYPT_DEP) $(ASN1_DEP) mechs/mech.h mechs/obj_qry.c
						$(CC) $(CFLAGS) -o $(OBJPATH)obj_qry.o mechs/obj_qry.c

$(OBJPATH)sign.o:		$(CRYPT_DEP) $(ASN1_DEP) mechs/mech.h mechs/sign.c
						$(CC) $(CFLAGS) -o $(OBJPATH)sign.o mechs/sign.c

$(OBJPATH)sign_cms.o:	$(CRYPT_DEP) $(ASN1_DEP) mechs/mech.h mechs/sign_cms.c
						$(CC) $(CFLAGS) -o $(OBJPATH)sign_cms.o mechs/sign_cms.c

$(OBJPATH)sign_int.o:	$(CRYPT_DEP) $(ASN1_DEP) mechs/mech.h mechs/sign_int.c
						$(CC) $(CFLAGS) -o $(OBJPATH)sign_int.o mechs/sign_int.c

$(OBJPATH)sign_pgp.o:	$(CRYPT_DEP) $(ASN1_DEP) mechs/mech.h mechs/sign_pgp.c
						$(CC) $(CFLAGS) -o $(OBJPATH)sign_pgp.o mechs/sign_pgp.c

$(OBJPATH)sign_rw.o:	$(CRYPT_DEP) $(ASN1_DEP) mechs/mech.h mechs/sign_rw.c
						$(CC) $(CFLAGS) -o $(OBJPATH)sign_rw.o mechs/sign_rw.c

$(OBJPATH)sign_x509.o:	$(CRYPT_DEP) $(ASN1_DEP) mechs/mech.h mechs/sign_x509.c
						$(CC) $(CFLAGS) -o $(OBJPATH)sign_x509.o mechs/sign_x509.c

# misc subdirectory

$(OBJPATH)int_api.o:	$(CRYPT_DEP) misc/int_api.c
						$(CC) $(CFLAGS) -o $(OBJPATH)int_api.o misc/int_api.c

$(OBJPATH)int_attr.o:	$(CRYPT_DEP) misc/int_attr.c
						$(CC) $(CFLAGS) -o $(OBJPATH)int_attr.o misc/int_attr.c

$(OBJPATH)int_debug.o:	$(CRYPT_DEP) misc/int_debug.c
						$(CC) $(CFLAGS) -o $(OBJPATH)int_debug.o misc/int_debug.c

$(OBJPATH)int_env.o:	$(CRYPT_DEP) misc/int_env.c
						$(CC) $(CFLAGS) -o $(OBJPATH)int_env.o misc/int_env.c

$(OBJPATH)int_err.o:	$(CRYPT_DEP) misc/int_err.c
						$(CC) $(CFLAGS) -o $(OBJPATH)int_err.o misc/int_err.c

$(OBJPATH)int_mem.o:	$(CRYPT_DEP) misc/int_mem.c
						$(CC) $(CFLAGS) -o $(OBJPATH)int_mem.o misc/int_mem.c

$(OBJPATH)int_string.o:	$(CRYPT_DEP) misc/int_string.c
						$(CC) $(CFLAGS) -o $(OBJPATH)int_string.o misc/int_string.c

$(OBJPATH)int_time.o:	$(CRYPT_DEP) misc/int_time.c
						$(CC) $(CFLAGS) -o $(OBJPATH)int_time.o misc/int_time.c

$(OBJPATH)os_spec.o: 	$(CRYPT_DEP) misc/os_spec.c
						$(CC) $(CFLAGS) -o $(OBJPATH)os_spec.o misc/os_spec.c

$(OBJPATH)pgp_misc.o:	$(CRYPT_DEP) $(IO_DEP) misc/pgp.h misc/pgp_misc.c
						$(CC) $(CFLAGS) -o $(OBJPATH)pgp_misc.o misc/pgp_misc.c

$(OBJPATH)random.o:		$(CRYPT_DEP) random/random.c
						$(CC) $(CFLAGS) -o $(OBJPATH)random.o random/random.c

$(OBJPATH)rand_x917.o:	$(CRYPT_DEP) random/rand_x917.c
						$(CC) $(CFLAGS) -o $(OBJPATH)rand_x917.o random/rand_x917.c

$(OBJPATH)unix.o:		$(CRYPT_DEP) random/unix.c
						$(CC) $(CFLAGS) -o $(OBJPATH)unix.o random/unix.c

$(OBJPATH)user.o:		$(CRYPT_DEP) misc/user.h misc/user.c
						$(CC) $(CFLAGS) -o $(OBJPATH)user.o misc/user.c

$(OBJPATH)user_attr.o:	$(CRYPT_DEP) misc/user.h misc/user_int.h misc/user_attr.c
						$(CC) $(CFLAGS) -o $(OBJPATH)user_attr.o misc/user_attr.c

$(OBJPATH)user_cfg.o:	$(CRYPT_DEP) misc/user.h misc/user_int.h misc/user_cfg.c
						$(CC) $(CFLAGS) -o $(OBJPATH)user_cfg.o misc/user_cfg.c

$(OBJPATH)user_rw.o:	$(CRYPT_DEP) misc/user.h misc/user_int.h misc/user_rw.c
						$(CC) $(CFLAGS) -o $(OBJPATH)user_rw.o misc/user_rw.c

# session subdirectory

$(OBJPATH)certstore.o:	$(CRYPT_DEP) $(IO_DEP) session/session.h session/certstore.h \
						session/certstore.c
						$(CC) $(CFLAGS) -o $(OBJPATH)certstore.o session/certstore.c

$(OBJPATH)cmp.o:		$(CRYPT_DEP) $(ASN1_DEP) session/session.h session/cmp.h \
						session/cmp.c
						$(CC) $(CFLAGS) -o $(OBJPATH)cmp.o session/cmp.c

$(OBJPATH)cmp_cli.o:	$(CRYPT_DEP) $(ASN1_DEP) session/session.h session/cmp.h \
						session/cmp_cli.c
						$(CC) $(CFLAGS) -o $(OBJPATH)cmp_cli.o session/cmp_cli.c

$(OBJPATH)cmp_cry.o:	$(CRYPT_DEP) $(ASN1_DEP) session/session.h session/cmp.h \
						session/cmp_cry.c
						$(CC) $(CFLAGS) -o $(OBJPATH)cmp_cry.o session/cmp_cry.c

$(OBJPATH)cmp_err.o:	$(CRYPT_DEP) $(ASN1_DEP) session/session.h session/cmp.h \
						session/cmp_err.c
						$(CC) $(CFLAGS) -o $(OBJPATH)cmp_err.o session/cmp_err.c

$(OBJPATH)cmp_rd.o:		$(CRYPT_DEP) $(ASN1_DEP) session/session.h session/cmp.h \
						session/cmp_rd.c
						$(CC) $(CFLAGS) -o $(OBJPATH)cmp_rd.o session/cmp_rd.c

$(OBJPATH)cmp_rdmsg.o:	$(CRYPT_DEP) $(ASN1_DEP) session/session.h session/cmp.h \
						session/cmp_rdmsg.c
						$(CC) $(CFLAGS) -o $(OBJPATH)cmp_rdmsg.o session/cmp_rdmsg.c

$(OBJPATH)cmp_svr.o:	$(CRYPT_DEP) $(ASN1_DEP) session/session.h session/cmp.h \
						session/cmp_svr.c
						$(CC) $(CFLAGS) -o $(OBJPATH)cmp_svr.o session/cmp_svr.c

$(OBJPATH)cmp_wr.o:		$(CRYPT_DEP) $(ASN1_DEP) session/session.h session/cmp.h \
						session/cmp_wr.c
						$(CC) $(CFLAGS) -o $(OBJPATH)cmp_wr.o session/cmp_wr.c

$(OBJPATH)cmp_wrmsg.o:	$(CRYPT_DEP) $(ASN1_DEP) session/session.h session/cmp.h \
						session/cmp_wrmsg.c
						$(CC) $(CFLAGS) -o $(OBJPATH)cmp_wrmsg.o session/cmp_wrmsg.c

$(OBJPATH)ocsp.o:		$(CRYPT_DEP) $(ASN1_DEP) session/session.h session/ocsp.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ocsp.o session/ocsp.c

$(OBJPATH)pnppki.o:		$(CRYPT_DEP) $(ASN1_DEP) session/session.h session/cmp.h \
						session/pnppki.c
						$(CC) $(CFLAGS) -o $(OBJPATH)pnppki.o session/pnppki.c

$(OBJPATH)rtcs.o:		$(CRYPT_DEP) $(ASN1_DEP) session/session.h session/rtcs.c
						$(CC) $(CFLAGS) -o $(OBJPATH)rtcs.o session/rtcs.c

$(OBJPATH)scep.o:		$(CRYPT_DEP) $(ASN1_DEP) session/session.h session/scep.h \
						session/scep.c
						$(CC) $(CFLAGS) -o $(OBJPATH)scep.o session/scep.c

$(OBJPATH)scep_cli.o:	$(CRYPT_DEP) $(ASN1_DEP) session/session.h session/scep.h \
						session/scep_cli.c
						$(CC) $(CFLAGS) -o $(OBJPATH)scep_cli.o session/scep_cli.c

$(OBJPATH)scep_svr.o:	$(CRYPT_DEP) $(ASN1_DEP) session/session.h session/certstore.h \
						session/scep.h session/scep_svr.c
						$(CC) $(CFLAGS) -o $(OBJPATH)scep_svr.o session/scep_svr.c

$(OBJPATH)scorebrd.o:	$(CRYPT_DEP) $(ASN1_DEP) session/session.h session/scorebrd.c
						$(CC) $(CFLAGS) -o $(OBJPATH)scorebrd.o session/scorebrd.c

$(OBJPATH)sess_attr.o:	$(CRYPT_DEP) session/session.h session/sess_attr.c
						$(CC) $(CFLAGS) -o $(OBJPATH)sess_attr.o session/sess_attr.c

$(OBJPATH)sess_iattr.o:	$(CRYPT_DEP) session/session.h session/sess_iattr.c
						$(CC) $(CFLAGS) -o $(OBJPATH)sess_iattr.o session/sess_iattr.c

$(OBJPATH)sess_rw.o:	$(CRYPT_DEP) $(ASN1_DEP) session/session.h session/sess_rw.c
						$(CC) $(CFLAGS) -o $(OBJPATH)sess_rw.o session/sess_rw.c

$(OBJPATH)session.o:	$(CRYPT_DEP) $(IO_DEP) session/session.h session/session.c
						$(CC) $(CFLAGS) -o $(OBJPATH)session.o session/session.c

$(OBJPATH)ssh.o:		$(CRYPT_DEP) $(IO_DEP) session/session.h session/ssh.h \
						session/ssh.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ssh.o session/ssh.c

$(OBJPATH)ssh1.o:		$(CRYPT_DEP) $(IO_DEP) session/session.h session/ssh.h \
						session/ssh1.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ssh1.o session/ssh1.c

$(OBJPATH)ssh2.o:		$(CRYPT_DEP) $(IO_DEP) session/session.h session/ssh.h \
						session/ssh2.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ssh2.o session/ssh2.c

$(OBJPATH)ssh2_authc.o:	$(CRYPT_DEP) $(IO_DEP) session/session.h session/ssh.h \
						session/ssh2_authc.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ssh2_authc.o session/ssh2_authc.c

$(OBJPATH)ssh2_auths.o:	$(CRYPT_DEP) $(IO_DEP) session/session.h session/ssh.h \
						session/ssh2_auths.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ssh2_auths.o session/ssh2_auths.c

$(OBJPATH)ssh2_chn.o:	$(CRYPT_DEP) $(IO_DEP) session/session.h session/ssh.h \
						session/ssh2_chn.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ssh2_chn.o session/ssh2_chn.c

$(OBJPATH)ssh2_cli.o:	$(CRYPT_DEP) $(IO_DEP) session/session.h session/ssh.h \
						session/ssh2_cli.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ssh2_cli.o session/ssh2_cli.c

$(OBJPATH)ssh2_cry.o:	$(CRYPT_DEP) $(IO_DEP) session/session.h session/ssh.h \
						session/ssh2_cry.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ssh2_cry.o session/ssh2_cry.c

$(OBJPATH)ssh2_msg.o:	$(CRYPT_DEP) $(IO_DEP) session/session.h session/ssh.h \
						session/ssh2_msg.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ssh2_msg.o session/ssh2_msg.c

$(OBJPATH)ssh2_msgc.o:	$(CRYPT_DEP) $(IO_DEP) session/session.h session/ssh.h \
						session/ssh2_msgc.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ssh2_msgc.o session/ssh2_msgc.c

$(OBJPATH)ssh2_msgs.o:	$(CRYPT_DEP) $(IO_DEP) session/session.h session/ssh.h \
						session/ssh2_msgs.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ssh2_msgs.o session/ssh2_msgs.c

$(OBJPATH)ssh2_rd.o:	$(CRYPT_DEP) $(IO_DEP) session/session.h session/ssh.h \
						session/ssh2_rd.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ssh2_rd.o session/ssh2_rd.c

$(OBJPATH)ssh2_svr.o:	$(CRYPT_DEP) $(IO_DEP) session/session.h session/ssh.h \
						session/ssh2_svr.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ssh2_svr.o session/ssh2_svr.c

$(OBJPATH)ssh2_wr.o:	$(CRYPT_DEP) $(IO_DEP) session/session.h session/ssh.h \
						session/ssh2_wr.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ssh2_wr.o session/ssh2_wr.c

$(OBJPATH)ssl.o:		$(CRYPT_DEP) $(IO_DEP) session/session.h session/ssl.h \
						session/ssl.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ssl.o session/ssl.c

$(OBJPATH)ssl_cli.o:	$(CRYPT_DEP) $(IO_DEP) session/session.h session/ssl.h \
						session/ssl_cli.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ssl_cli.o session/ssl_cli.c

$(OBJPATH)ssl_cry.o:	$(CRYPT_DEP) $(IO_DEP) session/session.h session/ssl.h \
						session/ssl_cry.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ssl_cry.o session/ssl_cry.c

$(OBJPATH)ssl_ext.o:	$(CRYPT_DEP) $(IO_DEP) session/session.h session/ssl.h \
						session/ssl_ext.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ssl_ext.o session/ssl_ext.c

$(OBJPATH)ssl_hs.o:		$(CRYPT_DEP) $(IO_DEP) session/session.h session/ssl.h \
						session/ssl_hs.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ssl_hs.o session/ssl_hs.c

$(OBJPATH)ssl_hsc.o:	$(CRYPT_DEP) $(IO_DEP) session/session.h session/ssl.h \
						session/ssl_hsc.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ssl_hsc.o session/ssl_hsc.c

$(OBJPATH)ssl_kmgmt.o:	$(CRYPT_DEP) $(IO_DEP) session/session.h session/ssl.h \
						session/ssl_kmgmt.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ssl_kmgmt.o session/ssl_kmgmt.c

$(OBJPATH)ssl_rd.o:		$(CRYPT_DEP) $(IO_DEP) session/session.h session/ssl.h \
						session/ssl_rd.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ssl_rd.o session/ssl_rd.c

$(OBJPATH)ssl_suites.o:	$(CRYPT_DEP) $(IO_DEP) session/session.h session/ssl.h \
						session/ssl_suites.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ssl_suites.o session/ssl_suites.c

$(OBJPATH)ssl_svr.o:	$(CRYPT_DEP) $(IO_DEP) session/session.h session/ssl.h \
						session/ssl_svr.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ssl_svr.o session/ssl_svr.c

$(OBJPATH)ssl_wr.o:		$(CRYPT_DEP) $(IO_DEP) session/session.h session/ssl.h \
						session/ssl_wr.c
						$(CC) $(CFLAGS) -o $(OBJPATH)ssl_wr.o session/ssl_wr.c

$(OBJPATH)tsp.o:		$(CRYPT_DEP) $(ASN1_DEP) session/session.h session/tsp.c
						$(CC) $(CFLAGS) -o $(OBJPATH)tsp.o session/tsp.c

# zlib subdirectory

$(OBJPATH)adler32.o:	$(ZLIB_DEP) zlib/adler32.c
						$(CC) $(CFLAGS) -o $(OBJPATH)adler32.o zlib/adler32.c

$(OBJPATH)deflate.o:	$(ZLIB_DEP) zlib/deflate.c
						$(CC) $(CFLAGS) -o $(OBJPATH)deflate.o zlib/deflate.c

$(OBJPATH)inffast.o:	$(ZLIB_DEP) zlib/inffast.h zlib/inffixed.h \
						zlib/inftrees.h zlib/inffast.c
						$(CC) $(CFLAGS) -o $(OBJPATH)inffast.o zlib/inffast.c

$(OBJPATH)inflate.o:	$(ZLIB_DEP) zlib/inflate.c
						$(CC) $(CFLAGS) -o $(OBJPATH)inflate.o zlib/inflate.c

$(OBJPATH)inftrees.o:	$(ZLIB_DEP) zlib/inftrees.h zlib/inftrees.c
						$(CC) $(CFLAGS) -o $(OBJPATH)inftrees.o zlib/inftrees.c

$(OBJPATH)trees.o:		$(ZLIB_DEP) zlib/trees.h zlib/trees.c
						$(CC) $(CFLAGS) -o $(OBJPATH)trees.o zlib/trees.c

$(OBJPATH)zutil.o:		$(ZLIB_DEP) zlib/zutil.c
						$(CC) $(CFLAGS) -o $(OBJPATH)zutil.o zlib/zutil.c

#****************************************************************************
#*																			*
#*								Test Code Targets							*
#*																			*
#****************************************************************************

# The test code

utils.o:				cryptlib.h crypt.h test/test.h test/utils.c
						$(CC) $(CFLAGS) test/utils.c

certimp.o:				cryptlib.h crypt.h test/test.h test/certimp.c
						$(CC) $(CFLAGS) test/certimp.c

certproc.o:				cryptlib.h crypt.h test/test.h test/certproc.c
						$(CC) $(CFLAGS) test/certproc.c

certs.o:				cryptlib.h crypt.h test/test.h test/certs.c
						$(CC) $(CFLAGS) test/certs.c

devices.o:				cryptlib.h crypt.h test/test.h test/devices.c
						$(CC) $(CFLAGS) test/devices.c

envelope.o:				cryptlib.h crypt.h test/test.h test/envelope.c
						$(CC) $(CFLAGS) test/envelope.c

highlvl.o:				cryptlib.h crypt.h test/test.h test/highlvl.c
						$(CC) $(CFLAGS) test/highlvl.c

keydbx.o:				cryptlib.h crypt.h test/test.h test/keydbx.c
						$(CC) $(CFLAGS) test/keydbx.c

keyfile.o:				cryptlib.h crypt.h test/test.h test/keyfile.c
						$(CC) $(CFLAGS) test/keyfile.c

loadkey.o:				cryptlib.h crypt.h test/test.h test/loadkey.c
						$(CC) $(CFLAGS) test/loadkey.c

lowlvl.o:				cryptlib.h crypt.h test/test.h test/lowlvl.c
						$(CC) $(CFLAGS) test/lowlvl.c

s_cmp.o:				cryptlib.h crypt.h test/test.h test/s_cmp.c
						$(CC) $(CFLAGS) test/s_cmp.c

s_scep.o:				cryptlib.h crypt.h test/test.h test/s_scep.c
						$(CC) $(CFLAGS) test/s_scep.c

sreqresp.o:				cryptlib.h crypt.h test/test.h test/sreqresp.c
						$(CC) $(CFLAGS) test/sreqresp.c

ssh.o:					cryptlib.h crypt.h test/test.h test/ssh.c
						$(CC) $(CFLAGS) test/ssh.c

ssl.o:					cryptlib.h crypt.h test/test.h test/ssl.c
						$(CC) $(CFLAGS) test/ssl.c

stress.o:				cryptlib.h crypt.h test/test.h test/stress.c
						$(CC) $(CFLAGS) test/stress.c

suiteb.o:				cryptlib.h crypt.h test/test.h test/suiteb.c
						$(CC) $(CFLAGS) test/suiteb.c

testfunc.o:				cryptlib.h crypt.h test/test.h test/testfunc.c
						$(CC) $(CFLAGS) test/testfunc.c

testlib.o:				cryptlib.h crypt.h test/test.h test/testlib.c
						$(CC) $(CFLAGS) test/testlib.c

#****************************************************************************
#*																			*
#*									Link Targets							*
#*																			*
#****************************************************************************

# Create the static and shared libraries.  The main test program is also
# listed as a dependency since we need to use OS-specific compiler options
# for it that a simple 'make testlib' won't give us (the test program checks
# whether the compiler options were set correctly when building the library,
# so it needs to include a few library-specific files that wouldn't be used
# in a normal program).
#
# When cross-compiling, we have to use the hosted tools and libraries rather
# than the system tools and libraries for the build, so we special-case this
# step based on the $(OSNAME) setting supplied to the build script.

$(LIBNAME):		$(OBJS) $(EXTRAOBJS) $(TESTOBJS)
				@./tools/buildlib.sh $(OSNAME) $(LIBNAME) $(AR) \
					$(OBJS) $(EXTRAOBJS)

$(SLIBNAME):	$(OBJS) $(EXTRAOBJS) $(TESTOBJS)
				@./tools/buildsharedlib.sh $(OSNAME) $(SLIBNAME) $(LD) \
					$(STRIP) $(OBJS) $(EXTRAOBJS)

$(DYLIBNAME):	$(OBJS) $(EXTRAOBJS) $(TESTOBJS)
				@$(LD) -dynamiclib -compatibility_version $(MAJ).$(MIN) \
					-current_version $(MAJ).$(MIN).$(PLV) \
					-o $(DYLIBNAME) $(OBJS) $(EXTRAOBJS)

# If installing cryptlib as a systemwide lib, run ldconfig (which normally
# reads /etc/ld.so.conf, sets up the appropriate symbolic links in the
# shared lib directory, and writes a cache file /etc/ld.so.cache for use by
# other programs). The loader the consults /etc/ld.so.cache to find the
# libraries it needs.  This is why ldconfig has to be run when a new lib is
# added or removed.
#
#	ldconfig -n <cryptlib .so directory path>
#
# A temporary workaround for testing is to set LD_LIBRARY_PATH to the
# directory containing the cryptlib shared lib.  This (colon-separated) list
# of directories is searched before the standard library directories.  This
# may have system-specific variations, e.g. under PHUX it's called
# SHLIB_PATH and under Aches it's LIBPATH.  BeOS uses LIBRARY_PATH, and
# needs to have it pointed to . to find the shared lib, otherwise it fails
# with a "Missing library" error without indicating which library is missing.
#
# To run stestlib with a one-off lib path change, use either the universal:
#
#	env LD_LIBRARY_PATH=. ./stestlib
#
# or the shell-specific (csh/tcsh):
#
#	setenv LD_LIBRARY_PATH .
#	./stestlib
#
# or (sh/bash):
#
#	LD_LIBRARY_PATH=. ; export LD_LIBRARY_PATH
#	./stestlib
#
# Finally, ldd <filename> will print out shared lib dependencies.
#
# We don't give the library as a dependency since the user has to make this
# explicitly rather than implicitly via testlib in order to go via the
# auto-config mechanism.  Since OS X uses special dylibs instead of normal
# shared libs, we detect this and build the appropriate lib type.
#
# To test the code with extended malloc diagnostics on systems with
# phkmalloc, use:
#
#	env MALLOC_OPTIONS="AJVX" ./testlib

testlib:		$(TESTOBJS)
				@rm -f $(LINKFILE)
				@echo $(TESTOBJS) > $(LINKFILE)
				@if [ $(OSNAME) = 'SunOS' ] ; then \
					if [ `/usr/ucb/cc 2>&1 | grep -c installed` = '1' ] ; then \
						gcc -o testlib $(LDFLAGS) `cat $(LINKFILE)` -L. -l$(PROJ) \
							`./tools/getlibs.sh autodetect` ; \
					else \
						$(LD) -o testlib $(LDFLAGS) `cat $(LINKFILE)` -L. -l$(PROJ) \
							`./tools/getlibs.sh autodetect` ; \
					fi ; \
				elif [ $(OSNAME) = 'HP-UX' ] ; then \
					if gcc -v > /dev/null 2>&1 ; then \
						gcc -o testlib $(LDFLAGS) `cat $(LINKFILE)` -L. -l$(PROJ) \
							`./tools/getlibs.sh autodetect` ; \
					else \
						$(LD) -o testlib $(LDFLAGS) `cat $(LINKFILE)` -L. -l$(PROJ) \
							`./tools/getlibs.sh autodetect` ; \
					fi ; \
				else \
					$(LD) -o testlib $(LDFLAGS) `cat $(LINKFILE)` -L. -l$(PROJ) \
						`./tools/getlibs.sh autodetect` ; \
				fi
				@rm -f $(LINKFILE)

stestlib:		$(TESTOBJS)
				@rm -f $(LINKFILE)
				@echo $(TESTOBJS) > $(LINKFILE)
				@if [ $(OSNAME) = 'AIX' ] ; then \
					$(LD) -o stestlib $(LDFLAGS) `cat $(LINKFILE)` -L. $(SLIBNAME).a \
						`./tools/getlibs.sh AIX` ; \
				elif [ $(OSNAME) = 'Darwin' ] ; then \
					$(LD) -o stestlib $(LDFLAGS) `cat $(LINKFILE)` -L. $(DYLIBNAME) \
						`./tools/getlibs.sh Darwin` ; \
				elif [ $(OSNAME) = 'HP-UX' ] ; then \
					$(LD) -o stestlib $(LDFLAGS) `cat $(LINKFILE)` -L. lib$(PROJ).sl \
						`./tools/getlibs.sh HP-UX` ; \
				else \
					$(LD) -o stestlib $(LDFLAGS) `cat $(LINKFILE)` -L. $(SLIBNAME) \
						`./tools/getlibs.sh autodetect` ; \
				fi
				@rm -f $(LINKFILE)

#****************************************************************************
#*																			*
#*								Unix OS Targets								*
#*																			*
#****************************************************************************

# Aches: A vaguely Unix-compatible OS designed by IBM.  The maxmem option
#		 is to give the optimizer more headroom, it's not really needed
#		 but avoids millions of informational messages telling you to
#		 increase it from the default 2048.  The roconst puts const data
#		 into read-only memory (this may happen anyway on some versions of
#		 the compiler).
#
#		 In theory we could also add:
#
#			@./tools/buildasm.sh $(AS) $(OBJPATH)
#
#		 as the first line of the build rules but this causes problems in
#		 some cases, the most obvious symptom being that all of the self-
#		 tests involving bignum algorithms fail.  This may be due to changes
#		 in which registers get preserved across asm calls or who knows what,
#		 the asm code dates from 2002 so it probably hasn't kept pace with
#		 the compiler.

AIX:
	@if [ $(CC) = "gcc" ] ; then \
		make $(DEFINES) CFLAGS="$(CFLAGS) -O3 -D_REENTRANT" ; \
	else \
		make $(DEFINES) CFLAGS="$(CFLAGS) -O2 -qmaxmem=-1 -qroconst -D_REENTRANT" ; \
	fi

# Apollo: Yeah, this makefile has been around for awhile.  Why do you ask?

Apollo:
	@make $(DEFINES) CFLAGS="$(CFLAGS) -O4"

# AUX: su root; rm -rf /; echo "Now install MkLinux"

A/UX:
	@make $(DEFINES) CFLAGS="$(CFLAGS) -O4"

# Millions of Intel BSD's (many are really BSE's, with incredibly archaic
#			development tools and libs, although it's slowly getting better):
#			cc is gcc except when it isn't.  Most are still using a.out,
#			although some are slowly going to ELF, which we can autodetect by
#			checking whether the compiler defines __ELF__.  If the compiler
#			check doesn't work then [ `uname -r | cut -f 1 -d '.'` -ge 4 ]
#			(for FreeBSD) and -ge 2 (for OpenBSD) should usually work.
#
#			NetBSD for many years (up until around 1999-2000) used an
#			incredibly old version of as that didn't handle 486 opcodes (!!),
#			so the asm code was disabled by default.  In addition it used an
#			equally archaic version of gcc, requiring manual fiddling with
#			the compiler type and options.  If you're still using one of
#			these ancient versions, you'll have to change the entry below to
#			handle it.  In addition the rule is currently hardwired to assume
#			x86 due to lack of access to a non-x86 box, if you're building on
#			a different architecture you'll have to change the entry slightly
#			to detect x86 vs. whatever you're currently using, see the Linux
#			entry for an example.
#
#			For the newer BSDs, the optimisation level is set via the
#			ccopts.sh script.

BSD386:
	@./tools/buildasm.sh $(AS) $(OBJPATH)
	@make $(DEFINES) EXTRAOBJS="$(ASMOBJS)" CFLAGS="$(CFLAGS) -DUSE_ASM \
		-fomit-frame-pointer -O3"
iBSD:
	@./tools/buildasm.sh $(AS) $(OBJPATH)
	@make $(DEFINES) EXTRAOBJS="$(ASMOBJS)" CFLAGS="$(CFLAGS) -DUSE_ASM \
		-fomit-frame-pointer -O3"
BSD/OS:
	@./tools/buildasm.sh $(AS) $(OBJPATH)
	@make $(DEFINES) EXTRAOBJS="$(ASMOBJS)" CFLAGS="$(CFLAGS) -DUSE_ASM \
		-fomit-frame-pointer -O3"

FreeBSD:
	@if uname -m | grep "i[3,4,5,6]86" > /dev/null; then \
		./tools/buildasm.sh $(AS) $(OBJPATH) ; \
		make $(DEFINES) EXTRAOBJS="$(ASMOBJS)" CFLAGS="$(CFLAGS) -DUSE_ASM \
			-fomit-frame-pointer -pthread" ; \
	else \
		make $(DEFINES) CFLAGS="$(CFLAGS) -fomit-frame-pointer -pthread" ; \
	fi

NetBSD:
	@if uname -m | grep "i[3,4,5,6]86" > /dev/null; then \
		./tools/buildasm.sh $(AS) $(OBJPATH) ; \
		make $(DEFINES) EXTRAOBJS="$(ASMOBJS)" CFLAGS="$(CFLAGS) -DUSE_ASM \
			-fomit-frame-pointer -pthread" ; \
	else \
		make $(DEFINES) CFLAGS="$(CFLAGS) -fomit-frame-pointer -pthread" ; \
	fi

OpenBSD:
	@if uname -m | grep "i[3,4,5,6]86" > /dev/null; then \
		./tools/buildasm.sh $(AS) $(OBJPATH) ; \
		make $(DEFINES) EXTRAOBJS="$(ASMOBJS)" CFLAGS="$(CFLAGS) -DUSE_ASM \
			-fomit-frame-pointer" ; \
	else \
		make $(DEFINES) CFLAGS="$(CFLAGS) -fomit-frame-pointer" ; \
	fi

# Convex:

Convex:
	@make $(DEFINES) CFLAGS="$(CFLAGS) -O4"

# Cray Unicos: The Cray compiler complains about char * vs. unsigned char
#			   passed to functions, there's no way to disable this directly
#			   so the best that we can do is disable warnings:
#				cc-256 Function call argument or assignment has incompatible type
#				cc-265 Function call argument has incompatible type

CRAY:
	@make $(DEFINES) CFLAGS="$(CFLAGS) -h nomessage=256:265 -O2"

# Cygwin: cc is gcc.

CYGWIN_NT-5.1:
	@./tools/buildasm.sh $(AS) $(OBJPATH)
	@make $(DEFINES) EXTRAOBJS="$(ASMOBJS)" CFLAGS="$(CFLAGS) -DUSE_ASM \
		-fomit-frame-pointer -O3 -D__CYGWIN__ -I/usr/local/include"

CYGWIN_NT-6.1:
	@./tools/buildasm.sh $(AS) $(OBJPATH)
	@make $(DEFINES) EXTRAOBJS="$(ASMOBJS)" CFLAGS="$(CFLAGS) -DUSE_ASM \
		-fomit-frame-pointer -O3 -D__CYGWIN__ -I/usr/local/include"

# DGUX: cc is a modified gcc.

dgux:
	@make $(DEFINES) CFLAGS="$(CFLAGS) -ansi -fomit-frame-pointer -O3"

# PHUX: A SYSVR2 layer with a SYSVR3 glaze on top of an adapted BSD 4.2
#		kernel.  Use cc, the exact incantation varies somewhat depending on
#		which version of PHUX you're running.  For 9.x you need to use
#		'-Aa -D_HPUX_SOURCE' to get the compiler into ANSI mode, in 10.x this
#		changed to just '-Ae', and after 10.30 -Ae was the default mode.
#		With PA-RISC 2 you should probably also define +DD64 to compile in
#		64-bit mode under PHUX 11.x, under even newer versions this becomes
#		+DA2.0w (note that building 64-bit versions of anything will probably
#		cause various build problems arising from the compiler and linker
#		because although the CPU may be 64 bit the software development tools
#		really, really want to give you 32-bit versions of everything and it
#		takes quite some cajoling to actually get them to spit out a 64-bit
#		result).  In addition the PHUX compilers don't recognise -On like the
#		rest of the universe but use +On instead so we adjust things based
#		on the compiler we're using.  In addition we only build the asm code
#		under 11 since it doesn't like 10.x and earlier systems.
#
#		Newer compilers can use +Oall to apply all optimisations (even the
#		dodgy ones).  Typically going from +O2 -> +O3 -> +O4 gives a ~10-15%
#		improvement at each step.  Finally, when making the shared lib you
#		can only use +O2, not +O3, because it gives the compiler the speed
#		wobbles.  In theory we could also use +ESlit to force const data
#		into a read-only segment, but this is defeated by a compiler bug
#		that doesn't initialise non-explicitly-initialised struct elements
#		to zero any more when this option is enabled (this is a double-bug
#		that violates two C rules because if there are insufficient
#		initialisers the remaining elements should be set to zero, and for
#		static objects they should be set to zero even if there are no
#		initialisers).
#
#		Note that the PHUX compilers (especially the earlier ones) are
#		horribly broken and will produce all sorts of of bogus warnings of
#		non-problems, eg:
#
#			/usr/ccs/bin/ld: (Warning) Quadrant change in relocatable
#							 expression in subspace $CODE$
#
#		(translation: Klingons off the starboard bow!).  The code contains
#		workarounds for non-errors (for example applying a cast to anything
#		magically turns it into an rvalue), but it's not worth fixing the
#		warnings for an OS as broken as this.  In addition most of the HP
#		compilers are incapable of handling whitespace before a preprocessor
#		directive, so you need to either (a) get a non-broken compiler or
#		(b) run each file through sed to strip the whitespace, something like:
#
#		#! /bin/csh -f
#		foreach file (*.h *.c)
#		  sed -e 's/  #/#/g' -e 's/	#/#/g' -e 's/	  #/#/g' $file > tmp
#		  mv tmp $file
#		end
#
#		Again, it isn't worth changing every single source file just to
#		accomodate this piece of compiler braindamage.
#
#		The asm bignum asm code is for PA-RISC 2.0, so we have to make sure
#		that we're building a PA-RISC 2.0 version if we use the asm code.
#		This can be detected with "getconf CPU_VERSION", if the result is >=
#		532 (equal to the symbolic define CPU_PA_RISC2_0) it's PA-RISC 2.0.
#		We need to explicitly check the architecture rather than the OS
#		since although PHUX 10.20 first supported PA-RISC 2.0, it wasn't
#		until PHUX 11.00 that the 64-bit capabilities were first supported
#		(previously it was treated as PA-RISC 1.x, 32-bit, or a 1.x/2.0
#		hybrid).  Because of the not-quite PA-RISC 2.0 support in PHUX 10.x,
#		we'd need to check the kernel with "file /stand/vmunix" for that,
#		which will report "ELF-64 executable object file - PA-RISC 2.0
#		(LP64)" for PA-RISC 2.0.
#
#		Even then, this may not necessarily work, depending on the phase of
#		the moon and a few other variables.  If testlib dumps core right at
#		the start (in the internal self-test), disable the use of the asm
#		code and rebuild.
#
#		In addition pa_risc2.s is written using the HP as syntax rather than
#		gas syntax, so we can only build it if we're using the PHUX native
#		development tools.
#
#		The HP compilers emit bogus warnings about (signed) char <->
#		unsigned char conversion, to get rid of these we use +W 563,604
#		to disable warning 563 (Argument is not the correct type) and 604
#		(Pointers are not assignment-compatible).
#
#		Finally, the default PHUX system ships with a non-C compiler (C++)
#		with most of the above bugs, but that can't process standard C code
#		either.  To detect this we feed it a C-compiler option and check for
#		a non-C-compiler error message, in this case +O3 which yields "The
#		+O3 option is available only with the C/ANSI C product; ignored".
#
#		The PHUX compiler bugs comment is really starting to give the SCO
#		one a run for its money.

HP-UX:
	@if [ `$(CC) +O3 ./tools/endian.c -o /dev/null 2>&1 | grep -c "ANSI C product"` = '1' ] ; then \
		echo "Warning: This system appears to be running the HP bundled C++ compiler as" ; \
		echo "         its cc.  You need to install a proper C compiler to build cryptlib." ; \
		echo "" ; \
		fi
	@rm -f a.out
	@case `./tools/osversion.sh HP-UX` in \
		8|9) \
			make $(DEFINES) CFLAGS="$(CFLAGS) -Aa -D_HPUX_SOURCE +O3" ;; \
		10) \
			if [ $(CC) = "gcc" ] ; then \
				make $(DEFINES) CFLAGS="$(CFLAGS) -O3" ; \
			else \
				make $(DEFINES) CFLAGS="$(CFLAGS) -Ae +O3" ; \
			fi ;; \
		11) \
			if [ $(CC) = "gcc" ] ; then \
				make $(DEFINES) CFLAGS="$(CFLAGS) -O3 -D_REENTRANT" ; \
			else \
				if [ `getconf CPU_VERSION` -ge 532 ] ; then \
					./tools/buildasm.sh $(AS) $(OBJPATH) ; \
					make $(DEFINES) CFLAGS="$(CFLAGS) +O3 +ESlit +DA2.0 +DS2.0 -Ae +W 563,604 -D_REENTRANT" ; \
				else \
					make $(DEFINES) CFLAGS="$(CFLAGS) +O3 +ESlit -Ae +W 563,604 -D_REENTRANT" ; \
				fi ; \
			fi ;; \
	esac

# Irix: Use cc.

IRIX:
	@./tools/buildasm.sh $(AS) $(OBJPATH)
	@make $(DEFINES) CFLAGS="$(CFLAGS) -O3"
IRIX64:
	@./tools/buildasm.sh $(AS) $(OBJPATH)
	@make $(DEFINES) CFLAGS="$(CFLAGS) -O3"

# ISC Unix: Use gcc.

ISC:
	@./tools/buildasm.sh $(AS) $(OBJPATH)
	@make $(DEFINES) CC=gcc CFLAGS="$(CFLAGS) -fomit-frame-pointer -O3 -mcpu=pentium"

# Linux: cc is gcc.  Optimisation level is set via the ccopts.sh script.

Linux:
	@if uname -m | grep "i[3,4,5,6]86" > /dev/null; then \
		./tools/buildasm.sh $(AS) $(OBJPATH) ; \
		make $(DEFINES) EXTRAOBJS="$(ASMOBJS)" CFLAGS="$(CFLAGS) -DUSE_ASM \
			-fomit-frame-pointer -D_REENTRANT" ; \
	else \
		make $(DEFINES) CFLAGS="$(CFLAGS) -fomit-frame-pointer -D_REENTRANT" ; \
	fi

# Mac OS X: BSD variant.  Optimisation level is set via the ccopts.sh script.
#			If you want to build a universal binary you can use the a command
#			a bit like the following (with the path to your SDK install
#			substituted for the one in the command-lines below):
#
# make LDFLAGS='-isysroot /Developer/SDKs/MacOSX10.5.sdk' CFLAGS='-c -isysroot \
# /Developer/SDKs/MacOSX10.5.sdk -Os -mmacosx-version-min=10.5 -arch ppc -arch \
# ppc64 -arch i386 -arch x86_64 -DOSX_UNIVERSAL_BINARY -D__UNIX__ -DNDEBUG -I.'
#
# make LDFLAGS='-arch i386 -arch x86_64' CFLAGS='-c -O2 -mmacosx-version-min=10.5 \
# -arch i386 -arch x86_64 -D__UNIX__ -DNDEBUG -I.'
#
#			This will also required adding $(LDFLAGS) to the dylib build rule,
#			and removing LDFLAGS="-object -s" from the overall OS X build rule.
#
#			This build method is rather trouble-prone because the low-level
#			crypto code has to configure itself for CPU endianness and word
#			size for the algorithms that require low-level bit fiddling, and
#			uses different code strategies depending on the CPU architecture
#			and bit width.  This single-pass build for multiple architectures
#			often causes problems, and you're more or less on your own if you
#			decide to try it.

Darwin:
	@make $(DEFINES) CFLAGS="$(CFLAGS) -fomit-frame-pointer" LDFLAGS="-object -s"

# MinGW: cc is gcc.  Note that we have to use the cross-compile flags
# XCFLAGS rather than CFLAGS because the latter implies a native Unix
# build, and we also need to execute the target-init rule in order to
# reconfigure ourselves to use the Win32 randomness-polling
# system rather than the Unix one, and we need to use the pre-built
# Win32 COFF object files because the assembler included with MinGW
# is a rather minimal one that seems to be intended mostly as a back-
# end for MinGW's gcc.

MINGW32_NT-5.1:
	@make OSNAME=win32 target-init
	@make $(DEFINES) CFLAGS="$(XCFLAGS) -fomit-frame-pointer -O3"

MINGW32_NT-6.1:
	@make OSNAME=win32 target-init
	@make $(DEFINES) EXTRAOBJS="$(WIN32ASMOBJS)" \
		CFLAGS="$(XCFLAGS) -fomit-frame-pointer -O3 \
		-Wl,--subsystem,windows,--output-def,cl32.def"

# NCR MP-RAS: Use the NCR cc.  The "-DNCR_UST" is needed to enable threading
#			  (User-Space Threads).

UNIX_SV:
	@make $(DEFINES) ARMETHOD=rcs CFLAGS="$(CFLAGS) -D_MPRAS -DNCR_UST \
		-O2 -Xa -Hnocopyr -K xpg42 -K catchnull '-Hpragma=Offwarn(39)' \
		'-Hpragma=Offwarn(73)'"

# NeXT 3.0:

NeXT:
	@make $(DEFINES) LDFLAGS="-object -s"

# OSF 1: If you're using the OSF1 cc you need to use "-std1" to force ANSI
#		 compliance and "-msg_disable ptrmismatch1" to turn off vast numbers
#		 of bogus warnings about things like unsigned chars passed to string
#		 functions.

OSF1:
	@make $(DEFINES) CC=gcc CFLAGS="$(CFLAGS) -fomit-frame-pointer -O3 -D_REENTRANT"

# QNX: Older versions of QNX (4.x) use braindamaged old 16-bit MSDOS-era
#	   Watcom tools that can't handle Unix-style code (or behaviour).
#	   The handling of compiler flags is particularly painful, in order to
#	   save space under DOS the compiler uses variable-size enums, in theory
#	   there's a compiler option -ei to make them the same size as an int
#	   but because the system 'cc' is just a wrapper for the DOS-style wcc386
#	   compiler we need to first use '-Wc' to tell the wrapper that an option
#	   for the compiler follows and then '-ei' for the compiler option itself.
#	   In addition to these problems the tools can't handle either ELF or
#	   a.out asm formats so we can't use the asm code unless we're building
#	   with gcc.

QNX:
	@if gcc -v > /dev/null 2>&1 ; then \
		./tools/buildasm.sh $(AS) $(OBJPATH) ; \
		make $(DEFINES) EXTRAOBJS="$(ASMOBJS)" CFLAGS="$(CFLAGS) -DUSE_ASM \
			-fomit-frame-pointer -O3 -D_REENTRANT"; \
	else \
		if [ `./tools/osversion.sh QNX` = '4' ] ; then \
			make $(DEFINES) CFLAGS="$(CFLAGS) -O4 -Wc,-ei -zc" ; \
		else \
			make $(DEFINES) CFLAGS="$(CFLAGS) -O4 -ei -zc" ; \
		fi ; \
	fi

# SCO: Unlike the entire rest of the world, SCO doesn't use -On, although it
#	   does recognise -O3 to mean "turn off pass 3 optimization".  The SCO cc
#	   is in fact a mutant version of Microsoft C 6.0, so we use the usual
#	   MSC optimization options except for the unsafe ones.  -Olx is
#	   equivalent to -Oegilt.  Unless SCO rewrote half the compiler when
#	   no-one was looking, you won't be getting much optimization for your
#	   -O.
#
#	   Actually it turns out that the only thing you get with -Olx is
#	   compiler bugs, so we only use -O, and even with that you get internal
#	   compiler faults that it traps and forces a compiler restart on,
#	   presumably with optimisations disabled.
#
#	   SCO is basically too braindamaged to support any of the asm builds.
#	   as won't take input from stdin and dumps core on the crypto .S files,
#	   and cc/as barf on bni80386.s.  Even compiling the straight C code
#	   gives a whole slew of internal compiler errors/failed assertions.  If
#	   you have a setup that works (i.e.with GNU tools installed) then you
#	   can call buildasm.sh to use the asm routines.
#
#	   For another taste of the wonderful SCO compiler, take the trivial lex
#	   example from the dragon book, lex it, and compile it.  Either the
#	   compiler will core dump from a SIGSEGV or the resulting program will
#	   from a SIGILL, depending on what level of optimization you use (a
#	   compiler that'll produce illegal code as output is pretty impressive).
#
#	   In addition the SCO cc ignores the path for output files and dumps the
#	   whole mess in the same directory as the source files.  This means you
#	   need to set STATIC_OBJ_PATH = . in order for the library to be built,
#	   however the following rule does this for you by forwarding down the
#	   $(TARGET) define rather than $(DEFINES), which also includes the
#	   output path.
#
#	   If you're building the shared version after building the static one
#	   you need to manually remove all the object files before trying to
#	   build it.
#
#	   The SCO/UnixWare sockets libraries are extraordinarily buggy, make
#	   sure that you've got the latest patches installed if you plan to use
#	   cryptlib's secure session interface.  Note that some bugs reappear in
#	   later patches, so you should make sure that you really do have the
#	   very latest patch installed ("SCO - Where Quality is Job #9" -
#	   unofficial company motto following a SCO employee survey).
#
#	   In terms of straight compiling of code, UnixWare (SCO 7.x) is only
#	   marginally better.  as now finally accepts input from stdin if '-' is
#	   specified as a command-line arg, but it doesn't recognise 486
#	   instructions yet (they've only been with us for over a decade for
#	   crying out loud), even using the BSDI-format kludge doesn't quite
#	   work since as just terminates with an internal error.
#
#	   The compiler breaks when processing the aestab.c file, if you want to
#	   use the SCO cc to build cryptlib you'll have to do without AES (or
#	   use gcc, see below).
#
#	   UnixWare also finally supports threads, but it may not be possible to
#	   build cryptlib with threading support under older versions because of
#	   a compiler bug in which the preprocessor sprays random spaces around
#	   any code in which token-pasting is used.  Although having foo##->mutex
#	   turn into "certInfo -> mutex" is OK, foo##.mutex turns into
#	   "certInfo. mutex" which the compiler chokes on (the appearances of
#	   spaces in different places doesn't seem to follow any pattern, the
#	   quoted strings above are exactly as output by the preprocessor).
#
#	   To avoid this mess, you can build the code using the SCO-modified gcc
#	   which has been hacked to work with cc-produced libraries (the code
#	   below tries this by default, falling back to the SCO compiler only if
#	   it can't find gcc).
#
#	   Cool, the SCO comment is now longer than the comments for all the
#	   other Unix variants put together.

SCO:
	@if gcc -v > /dev/null 2>&1 ; then \
		./tools/buildasm.sh $(AS) $(OBJPATH) ; \
		make $(DEFINES) EXTRAOBJS="$(ASMOBJS)" CFLAGS="$(CFLAGS) -DUSE_ASM \
			-fomit-frame-pointer -O3 -D_REENTRANT"; \
	else \
		echo "Please read the entry for SCO in the makefile before continuing." ; \
		make $(TARGET) CFLAGS="$(CFLAGS) -O" ; \
	fi

UnixWare:
	@if gcc -v > /dev/null 2>&1 ; then \
		./tools/buildasm.sh $(AS) $(OBJPATH) ; \
		make $(DEFINES) EXTRAOBJS="$(ASMOBJS)" CFLAGS="$(CFLAGS) -DUSE_ASM \
			-fomit-frame-pointer -O3 -D_REENTRANT"; \
	else \
		echo "Please read the entry for UnixWare in the makefile before continuing." ; \
		make $(DEFINES) CFLAGS="$(CFLAGS) -O -Xa -Khost -Kthread" ; \
	fi

itgoaway:
	@echo "You poor bastard."

# Sun/Slowaris: An OS named after the 1972 Andrei Tarkovsky film about a space
#				station that drives people who work there mad.  Use gcc, but
#				fall back to the SUNSwspro compiler if necessary (in the c
#				checks below, the '-' is necessary because one of the checks
#				returns a nonzero status somewhere that causes make to bail
#				out, and the error suppression is necessary to avoid dozens of
#				bogus warnings about signed vs.unsigned chars).
#
#				In addition we can only safely use -O2 (-xO2 in SUNwspro-
#				speak) because -O3 introduces too many problems due to
#				optimiser bugs, while it's possible to (eventually) eliminate
#				them through the judicious sprinkling of 'asm("");' in
#				appropriate locations to disable optimisation within that
#				code block it becomes a pain having to track them down
#				whenever the code changes, and -O2 isn't really much
#				different than -O3 anyway.
#
#				We only build the asm code if we're doing a 32-bit build,
#				since it's 32-bit only.

SunOS:
	@if [ $(CC) = "gcc" ] ; then \
		make SunOS-gcc $(DEFINES) CFLAGS="$(CFLAGS)" ; \
	else \
		make SunOS-SunPro $(DEFINES) CFLAGS="$(CFLAGS)" ; \
	fi

SunOS-gcc:
	@- if [ `uname -m` = 'i86pc' -a `echo $(CFLAGS) | grep -c -- '-m64'` = 0 ] ; then \
		./tools/buildasm.sh $(AS) $(OBJPATH) ; \
		make $(DEFINES) EXTRAOBJS="$(ASMOBJS)" CC=gcc \
			CFLAGS="$(CFLAGS) -fomit-frame-pointer -O3 -DUSE_ASM -D_REENTRANT" ; \
	else \
		make $(DEFINES) CC=gcc CFLAGS="$(CFLAGS) -fomit-frame-pointer \
			-O3 -D_REENTRANT" ; \
	fi

SunOS-SunPro:
	@- if [ `uname -m` = 'i86pc' -a `echo $(CFLAGS) | grep -c -- '-m64'` = 0 ] ; then \
		./tools/buildasm.sh $(AS) $(OBJPATH) ; \
		make $(DEFINES) EXTRAOBJS="$(ASMOBJS)" \
			CFLAGS="$(CFLAGS) -erroff=E_ARG_INCOMPATIBLE_WITH_ARG \
			-xO2 -DUSE_ASM -D_REENTRANT" ; \
	else \
		make $(DEFINES) CFLAGS="$(CFLAGS) -erroff=E_ARG_INCOMPATIBLE_WITH_ARG \
			-xO2 -D_REENTRANT" ; \
	fi

# SVR4: Better results can be obtained by upgrading your OS to 4.4 BSD.
#		A few SVR4 unames don't report the OS name properly (Olivetti Unix)
#		so it's necessary to specify the SVR4 target on the command line.

SVR4:
	@make $(DEFINES) CFLAGS="$(CFLAGS) -O3"

# Ultrix: Use vcc or gcc.

ULTRIX:
	@./tools/buildasm.sh $(AS) $(OBJPATH)
	@make $(DEFINES) CC=gcc CFLAGS="$(CFLAGS) -fomit-frame-pointer -O3"

# Amdahl UTS 4:

UTS4:
	@make $(DEFINES) CFLAGS="$(CFLAGS) -Xc -O4"

#****************************************************************************
#*																			*
#*								Other OS Targets							*
#*																			*
#****************************************************************************

# BeOS: By default we use the newer BeOS development environment, which uses
#		gcc.  Since BeOS doesn't use the default Unix environment, we use
#		XCFLAGS and insert __BEOS__ as the OS.
#
#		The older BeOS development environment can still be used with:
#
#	@make $(DEFINES) CC=mwcc AR="mwcc -xml -o" LD="mwcc -xms -f crypt.exp"

BeOS:
	@if grep "unix\.o" makefile > /dev/null ; then \
		sed s/unix\.o/beos\.o/g makefile | sed s/unix\.c/beos\.c/g > makefile.tmp || exit 1 ; \
		mv -f makefile.tmp makefile || exit 1 ; \
	fi
	@if [ `uname -m` = 'BePC' ]; then \
		make asm_elf OBJPATH=$(OBJPATH) ; \
		make $(DEFINES) EXTRAOBJS="$(ASMOBJS)" CFLAGS="$(XCFLAGS) -DUSE_ASM \
			-D__BEOS__ -fomit-frame-pointer -O3 -D_REENTRANT" ; \
	else \
		make $(DEFINES) CFLAGS="$(CFLAGS) -U__UNIX__ -D__BEOS__ \
			-fomit-frame-pointer -O3 -D_REENTRANT" ; \
	fi

# EPOC: Cross-compilation requires custom code paths to build using the
#		Symbian SDK rather than the native compiler.  The following defines
#		are for Symbian OS 7.x as the SDK and ARM as the architecture.  A
#		cross-compile config for a more modern toolset (Carbide) is given
#		further down.
#
# EPOC		= /usr/local/symbian/7.0
# CXX		= ${EPOC}/bin/arm-epoc-pe-g++
# CC		= ${EPOC}/bin/arm-epoc-pe-gcc
# AR		= ${EPOC}/bin/arm-epoc-pe-ar
# LD		= ${EPOC}/bin/arm-epoc-pe-ld
# CPP		= ${EPOC}/bin/arm-epoc-pe-cpp
# RANLIB	= ${EPOC}/bin/arm-epoc-pe-ranlib
# STRIP		= ${EPOC}/bin/arm-epoc-pe-strip
# INCS		= -I$(EPOC)/include/libc

EPOC:
	@make CFLAGS="$(XCFLAGS) -D__EPOC__" $(DEFINES)

# IBM MVS (a.k.a.OS/390, z/OS): File naming behaviour is controlled by the
#								DDNAME_IO define.
#
#	DDNAME_IO defined: Use ddnames for all I/O.  User options will be saved
#		in dynamically allocated datasets userid.CRYPTLIB.filename.
#
#	DDNAME_IO not defined: Use HFS for all I/O.  User options will be saved
#		in directory $HOME/.cryptlib.

OS/390:
	@./tools/buildasm.sh $(AS) $(OBJPATH)
	@if grep "unix\.o" makefile > /dev/null ; then \
		sed s/unix\.o/mvs\.o/g makefile | sed s/unix\.c/mvs\.c/g > makefile.tmp || exit 1 ; \
	fi
	@make $(DEFINES) EXTRAOBJS="$(OBJPATH)mvsent.o" CFLAGS="$(XCFLAGS) -O2 \
		-W c,'langlvl(extended) csect rent roc ros targ(osv2r7) enum(4)' \
		-W c,'CONVLIT(ISO8859-1)' -D_OPEN_THREADS -D_XOPEN_SOURCE_EXTENDED=1"

# Tandem NSK/OSS: Use c89.  There are two variants of the OS here, OSS
#				  (Posix-like layer over NSK) and NSK hosted on OSS (many
#				  of the Posix functions aren't available).  The following
#				  builds for the OSS target (the default), to build for
#				  NSK use "-Wsystype=guardian".  For optimisation there's
#				  only -O, which is equivalent to the Tandem-specific
#				  -Woptimize=2 setting.  We need to enable extensions with
#				  -Wextensions for the networking code or many of the
#				  networking header data types are NOP'ed out.
#
#				  The compiler is pretty picky, we turn off warnings for:
#
#					Nested comments (106)
#					Unreachable code (203, usually for failsafe defaults
#						after a case statement)
#					Unsigned char vs. char (232)
#					Char vs. unsigned char (252)
#					Int vs. static int functions (257, the STATIC_FN
#						issue)
#					Mixing enum and int (272)
#					Char vs. unsigned char (611),
#					Variable initialised but never used (770, mostly in
#						OpenSSL code)
#					Int vs. unsigned int (1506)

NONSTOP_KERNEL:
	@if grep "unix\.o" makefile > /dev/null ; then \
		sed s/unix\.o/tandem\.o/g makefile | sed s/unix\.c/tandem\.c/g > makefile.tmp || exit 1 ; \
		mv -f makefile.tmp makefile || exit 1 ; \
	fi
	@make $(DEFINES) CC=c89 CFLAGS="$(CFLAGS) -O -Wextensions -Wnowarn=106,203,232,252,257,272,611,770,1506"

#****************************************************************************
#*																			*
#*							Cross-Compilation Targets						*
#*																			*
#****************************************************************************

# Generic entry for cross-compilation.  You need to provide at least the
# following:
#
#	-DCONFIG_DATA_LITTLEENDIAN/-DCONFIG_DATA_BIGENDIAN
#		Override endianness auto-detection.
#
#	-DOSVERSION=major_version
#		OS major version number.
#
#	$(OSNAME)
#		The target OS name, to select the appropriate compiler/link
#		options further down.
#
# For further options, see the cryptlib manual.  A general template for an
# entry is:
#
# target-X:
#	@make directories
#	make $(DEFINES) OSNAME=target-X CC=target-cc CFLAGS="$(XCFLAGS) \
#		-DCONFIG_DATA_xxxENDIAN -DOSVERSION=major_version \
#		-fomit-frame-pointer -O3 -D_REENTRANT"
#
# Since we're cross-compiling here, we use $(XCFLAGS) and $(XDEFINES) instead
# if the usual $(CFLAGS) and $(DEFINES), which assume that the target is a
# Unix system.

target-init:
	@make directories
	@if grep "unix\.o" makefile > /dev/null ; then \
		sed s/unix\.o/$(OSNAME)\.o/g makefile | sed s/unix\.c/$(OSNAME)\.c/g > makefile.tmp || exit 1 ; \
		mv -f makefile.tmp makefile || exit 1 ; \
	fi

# (Kadak) AMX: Gnu toolchain under Unix or Cygwin or ARM cc for ARM CPUs.

target-amx-arm:
	@make OSNAME=amx target-init
	make $(XDEFINES) OSNAME=AMX CC=armcc CFLAGS="$(XCFLAGS) \
		-DCONFIG_DATA_LITTLEENDIAN -D__AMX__ -O2"

target-amx-mips:
	@make OSNAME=amx target-init
	make $(XDEFINES) OSNAME=AMX CC=mips-elf-gcc CFLAGS="$(XCFLAGS) \
		-DCONFIG_DATA_LITTLEENDIAN -D__AMX__ -O2 \
		`./tools/ccopts-crosscompile.sh $(CC)`"

target-amx-ppc:
	@make OSNAME=amx target-init
	make $(XDEFINES) OSNAME=AMX CC=powerpc-eabi-gcc CFLAGS="$(XCFLAGS) \
		-DCONFIG_DATA_BIGENDIAN -D__AMX__ -O2 \
		`./tools/ccopts-crosscompile.sh $(CC)`"

target-amx-x86:
	@make OSNAME=amx target-init
	make $(XDEFINES) OSNAME=AMX CC=i386-elf-gcc CFLAGS="$(XCFLAGS) \
		-DCONFIG_DATA_LITTLEENDIAN -D__AMX__ -O2 \
		`./tools/ccopts-crosscompile.sh $(CC)`"

# Atmel ARM7 TDMI: Little-endian, no OS, maximum restrictions on resource
# usage since it's running on the bare metal.

target-atmel:
	@make OSNAME=atmel target-init
	make $(XDEFINES) OSNAME=Atmel CFLAGS="$(XCFLAGS) \
		-DCONFIG_DATA_LITTLEENDIAN -DCONFIG_NO_STDIO -DCONFIG_CONSERVE_MEMORY \
		-DCONFIG_NO_DYNALLOC -fomit-frame-pointer -O3"

# ChorusOS: Generic toolchain for various architectures.

target-chorus:
	@make OSNAME=chorus target-init
	make $(XDEFINES) OSNAME=CHORUS CFLAGS="$(XCFLAGS) \
		-DCONFIG_DATA_LITTLEENDIAN -D__CHORUS__ -O3"

# eCOS: Gnu toolchain under Unix.  For a standard install you also need
# to change the XCFLAGS define at the start of this makefile to
# XCFLAGS = -c -DNDEBUG -I. -I$(ECOS_INSTALL_DIR)/include.

target-ecos-arm:
	@make OSNAME=ecos target-init
	make $(XDEFINES) OSNAME=eCOS CC=arm-elf-gcc CFLAGS="$(XCFLAGS) \
		-DCONFIG_DATA_LITTLEENDIAN -D__ECOS__ -O2 \
		`./tools/ccopts-crosscompile.sh $(CC)`"

target-ecos-ppc:
	@make OSNAME=ecos target-init
	make $(XDEFINES) OSNAME=eCOS CC=powerpc-eabi-gcc CFLAGS="$(XCFLAGS) \
		-DCONFIG_DATA_BIGENDIAN -D__ECOS__ -O2 \
		`./tools/ccopts-crosscompile.sh $(CC)`"

target-ecos-sh:
	@make OSNAME=ecos target-init
	make $(XDEFINES) OSNAME=eCOS CC=sh-elf-gcc CFLAGS="$(XCFLAGS) \
		-DCONFIG_DATA_LITTLEENDIAN -D__ECOS__ -O2 \
		`./tools/ccopts-crosscompile.sh $(CC)`"

target-ecos-x86:
	@make OSNAME=ecos target-init
	make $(XDEFINES) OSNAME=eCOS CC=i386-elf-gcc CFLAGS="$(XCFLAGS) \
		-DCONFIG_DATA_LITTLEENDIAN -D__ECOS__ -O2 \
		`./tools/ccopts-crosscompile.sh $(CC)`"

# FreeRTOS/OpenRTOS: Gnu toolchain under Cygwin.

target-freertos-arm:
	@make OSNAME=X target-init
	make $(XDEFINES) OSNAME=FreeRTOS CC=arm-elf-gcc CFLAGS="$(XCFLAGS) \
		-DCONFIG_DATA_LITTLEENDIAN -D__FREERTOS__ -O2 \
		`./tools/ccopts-crosscompile.sh $(CC)`"

target-freertos-mb:
	@make OSNAME=X target-init
	make $(XDEFINES) OSNAME=FreeRTOS CC=mb-elf-gcc CFLAGS="$(XCFLAGS) \
		-DCONFIG_DATA_BIGENDIAN -D__FREERTOS__ -O2 \
		`./tools/ccopts-crosscompile.sh $(CC)`"

target-freertos-ppc:
	@make OSNAME=X target-init
	make $(XDEFINES) OSNAME=FreeRTOS CC=powerpc-eabi-gcc CFLAGS="$(XCFLAGS) \
		-DCONFIG_DATA_BIGENDIAN -D__FREERTOS__ -O2 \
		`./tools/ccopts-crosscompile.sh $(CC)`"

# Apple iOS hosted on OS X.

target-ios:
	@make directories
	@make OSNAME=iOS target-init
	make $(XDEFINES) OSNAME=iOS CC=/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/cc \
		CFLAGS="$(XCFLAGS) -DCONFIG_DATA_LITTLEENDIAN -fomit-frame-pointer -O2 \
		-D_REENTRANT -arch armv7 \
		-isysroot /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS5.0.sdk"
	make $(SLIBNAME) OBJPATH=$(OBJPATH) CROSSCOMPILE=1 OSNAME=iOS

# Embedded Linux.  Note that we don't have to perform the 'make target-init'
# as for the other systems since the target environment is the same as the
# source environment, and we use $(CFLAGS) rather than $(XCFLAGS) for the
# same reason.

target-linux-arm:
	@make directories
	make $(XDEFINES) OSNAME=Linux CC=arm-linux-gnueabi-gcc AR=arm-linux-gnueabi-ar \
		CFLAGS="$(CFLAGS) -DCONFIG_DATA_LITTLEENDIAN -fomit-frame-pointer \
		-O2 -D_REENTRANT"
	make $(SLIBNAME) OBJPATH=$(OBJPATH) LD=arm-linux-gnueabi-ld \
		STRIP=arm-linux-gnueabi-strip CROSSCOMPILE=1 OSNAME=Linux

target-linux-sh4:
	@make directories
	make $(XDEFINES) OSNAME=Linux CC=sh4-linux-gnu-gcc AR=sh4-linux-gnu-ar \
		CFLAGS="$(CFLAGS) -DCONFIG_DATA_LITTLEENDIAN -fomit-frame-pointer \
		-O2 -D_REENTRANT"
	make $(SLIBNAME) OBJPATH=$(OBJPATH) LD=sh4-linux-gnu-ld \
		STRIP=sh4-linux-gnu-strip CROSSCOMPILE=1 OSNAME=Linux

target-linux-ppc:
	@make directories
	make $(XDEFINES) OSNAME=Linux CC=powerpc-eabi-gcc AR=powerpc-eabi-ar \
		CFLAGS="$(CFLAGS) -DCONFIG_DATA_BIGENDIAN -fomit-frame-pointer \
		-O2 -D_REENTRANT"
	make $(SLIBNAME) OBJPATH=$(OBJPATH) LD=powerpc-eabi-ld \
		STRIP=powerpc-eabi-strip CROSSCOMPILE=1 OSNAME=Linux

# MIPS running Linux: Little-endian, 2.x kernel.  Note that we use $(CFLAGS)
# rather than $(XCFLAGS) since this is a Unix system, just not the same as
# the source one.

target-mips:
	@make directories
	make $(XDEFINES) OSNAME=Linux CFLAGS="$(CFLAGS) \
		-DCONFIG_DATA_LITTLEENDIAN -DOSVERSION=2 \
		-fomit-frame-pointer -O3 -D_REENTRANT"

# PalmOS on ARM: Little-endian.  The first target is for the Palm tools, the
# second for the PRC tools package.  The latter needs to have assorted extra
# defines that are automatically set by the Palm tools set manually.  The
# optimisation level for the Palm compiler is left at the default -O, which is
# equivalent to -O3.  -O4 and -O5 are somewhat flaky.
#
# The toolchains can require a bit of tweaking to get running due to problems
# with finding include directories.  The PRC tools using gcc expect to find
# standard ARM headers as a fallback from the PalmOS ones, using
# #include_next to pull in the next headers.  For a standard install this
# requires specifying the additional include file paths with
# "-idirafter /usr/lib/gcc-lib/arm-palmos/...".  The Palm tools under Cygwin
# are even more problematic, and may require manual instruction on where to
# find their include files for both the Palm and ANSI/ISO C standard headers.
#
# The PalmOS compiler sets an idiotic -wall by default, requiring that we
# manually turn off a pile of the more annoying warnings, although the worst
# one (used before initialised) can't be turned off.  For the warnings that
# we can turn off:
#
#	112 = unreachable code
#	187 = comparison of unsigned type for < 0
#	189 = enumerated type mixed with another type (== int)

palm-sld:		cryptlib.sld
	pslib -inDef cryptlib.sld -outObjStartup $(OBJPATH)cryptsld.o \
	-outObjStub palmcl.obj -outEntryNums palmcl.h

target-palmos:
	@make OSNAME=palmos target-init
	@make palm-sld
	make $(XDEFINES) OSNAME=PalmOS CC=pacc CFLAGS="$(XCFLAGS) \
		-I$(PALMSDK_PATH)/headers/ \
		-I$(PALMSDK_PATH)/headers/posix/ \
		-nologo -D__PALMOS_KERNEL__ -DBUILD_TYPE=BUILD_TYPE_RELEASE \
		-DCONFIG_DATA_LITTLEENDIAN -O -wd112 -wd187 -wd189"

target-palmos-prc:
	@make OSNAME=palmos target-init
	make $(XDEFINES) OSNAME=PalmOS-PRC CC=arm-palmos-gcc CFLAGS="$(XCFLAGS) \
		-idirafter /usr/lib/gcc-lib/arm-palmos/3.2.2/include/ \
		-D__PALMOS_KERNEL__ -D__PALMSOURCE__ -DBUILD_TYPE=BUILD_TYPE_RELEASE \
		-DCONFIG_DATA_LITTLEENDIAN -fomit-frame-pointer -O3 \
		`./tools/ccopts-crosscompile.sh $(CC)`"

# RTEMS: Gnu toolchain under Cygwin.

target-rtems-arm:
	@make OSNAME=rtems target-init
	make $(XDEFINES) OSNAME=RTEMS CC=arm-elf-gcc CFLAGS="$(XCFLAGS) \
		-DCONFIG_DATA_LITTLEENDIAN -D__RTEMS__ -O2 \
		`./tools/ccopts-crosscompile.sh $(CC)`"

target-rtems-mips:
	@make OSNAME=rtems target-init
	make $(XDEFINES) OSNAME=RTEMS CC=mips-elf-gcc CFLAGS="$(XCFLAGS) \
		-DCONFIG_DATA_LITTLEENDIAN -D__RTEMS__ -O2 \
		`./tools/ccopts-crosscompile.sh $(CC)`"

target-rtems-ppc:
	@make OSNAME=rtems target-init
	make $(XDEFINES) OSNAME=RTEMS CC=powerpc-eabi-gcc CFLAGS="$(XCFLAGS) \
		-DCONFIG_DATA_LITTLEENDIAN -D__RTEMS__ -O2 \
		`./tools/ccopts-crosscompile.sh $(CC)`"

target-rtems-x86:
	@make OSNAME=rtems target-init
	make $(XDEFINES) OSNAME=RTEMS CC=i386-elf-gcc CFLAGS="$(XCFLAGS) \
		-DCONFIG_DATA_LITTLEENDIAN -D__RTEMS__ -O2 \
		`./tools/ccopts-crosscompile.sh $(CC)`"

# Symbian: Carbide toolchain under Windows or Unix.  This builds either for
# the ARM target or for the x86 Symbian emulator, strictly speaking the
# latter isn't really a cross-compile but we have to treat it as such
# because we're building for an OS other than the native one.
#
# The handling of this is a bit of a mess, the emulator build is done using
# the ex-Metrowerks CodeWarrior compiler (restricted to only produce x86
# output) and the ARM builds are done using either gcc or the ARM compiler.
# Since the only preprocessor indicator that the emulator compiler defines
# is __EMU_SYMBIAN_OS__ (as well as __INTEL__, a Metrowerks define that's
# hardcoded on because none of the other Metrowerks target types are
# available any more) we have to manually define __SYMBIAN32__ ourselves
# for the emulator build.

CARBIDE_PATH			= "C:\Carbide.c++ v2.3"
CARBIDE_INCLUDE_PATH	= "$(CARBIDE_PATH)\x86Build\Symbian_Support\MSL\MSL_C\MSL_Common\Include\"

target-symbian:
	@make OSNAME=symbian target-init
	make $(XDEFINES) OSNAME=Symbian CC=arm-none-symbianelf-gcc CFLAGS="$(XCFLAGS) \
		-O2 -I$(CARBIDE_INCLUDE_PATH)"

target-symbian-emulator:
	@make OSNAME=symbian target-init
	make $(XDEFINES) OSNAME=Symbian CC=mwccsym2 CFLAGS="$(XCFLAGS) \
		-D__SYMBIAN32__ -O2 -I$(CARBIDE_INCLUDE_PATH)"

# ThreadX: Usually the Gnu toolchain under Cygwin or Unix (with occasional
# exceptions for vendor-specific compilers, in the rules below the ones that
# invoke ccopts-crosscompile.sh are for the Gnu toolchain).  The front-end
# when gcc is used is usually Eclipse, but it's not really needed for
# building cryptlib.

THREADX_IAR_PATH = "C:/Program Files (x86)/IAR Systems/Embedded Workbench 6.4"
THREADX_INCLUDE_PATH = "../../../uk_smets_integ_01_crypto/projects/threadx"

target-threadx-arm:
	@make OSNAME=threadx target-init
	make $(XDEFINES) OSNAME=ThreadX CC=armcc CFLAGS="$(XCFLAGS) \
		-DCONFIG_DATA_LITTLEENDIAN -D__ThreadX__ -O2"

target-threadx-mb:
	@make OSNAME=threadx target-init
	make $(XDEFINES) OSNAME=ThreadX CC=mb-elf-gcc CFLAGS="$(XCFLAGS) \
		-DCONFIG_DATA_BIGENDIAN -D__ThreadX__ -O2 \
		`./tools/ccopts-crosscompile.sh $(CC)`"

target-threadx-mips:
	@make OSNAME=threadx target-init
	make $(XDEFINES) OSNAME=ThreadX CC=mips-elf-gcc CFLAGS="$(XCFLAGS) \
		-DCONFIG_DATA_LITTLEENDIAN -D__ThreadX__ -O2 \
		`./tools/ccopts-crosscompile.sh $(CC)`"

target-threadx-ppc:
	@make OSNAME=threadx target-init
	make $(XDEFINES) OSNAME=ThreadX CC=powerpc-eabi-gcc CFLAGS="$(XCFLAGS) \
		-DCONFIG_DATA_BIGENDIAN -D__ThreadX__ -O2 \
		`./tools/ccopts-crosscompile.sh $(CC)`"

target-threadx-rx:
	@make OSNAME=threadx target-init
	make $(XDEFINES) OSNAME=ThreadX CC=iccrx CFLAGS="$(XCFLAGS) \
		-D__ThreadX__ -DDEBUG_DIAGNOSTIC_ENABLE -DCONFIG_DEBUG_MALLOC \
		-DCONFIG_DATA_LITTLEENDIAN -DOPENSSL_NO_FP_API -DCONFIG_NO_STDIO \
		-Ohs --core RX610 -r -I $(THREADX_INCLUDE_PATH) \
		--dlib_config '$(THREADX_IAR_PATH)/rx/LIB/dlrxfllf.h'"

target-threadx-x86:
	@make OSNAME=threadx target-init
	make $(XDEFINES) OSNAME=ThreadX CC=i386-elf-gcc CFLAGS="$(XCFLAGS) \
		-DCONFIG_DATA_LITTLEENDIAN -D__ThreadX__ -O2 \
		`./tools/ccopts-crosscompile.sh $(CC)`"

# uC/OS-II: Generic toolchain for various architectures.

target-ucos-arm:
	@make OSNAME=ucos target-init
	make $(XDEFINES) OSNAME=UCOS CC=armcc CFLAGS="$(XCFLAGS) \
		-DCONFIG_DATA_LITTLEENDIAN -D__UCOS__ -O3"

target-ucos-ppc:
	@make OSNAME=ucos target-init
	make $(XDEFINES) OSNAME=UCOS CC=powerpc-eabi-gcc CFLAGS="$(XCFLAGS) \
		-DCONFIG_DATA_BIGENDIAN -D__UCOS__ -O3 \
		`./tools/ccopts-crosscompile.sh $(CC)`"

target-ucos-x86:
	@make OSNAME=ucos target-init
	make $(XDEFINES) OSNAME=UCOS CC=i386-elf-gcc CFLAGS="$(XCFLAGS) \
		-DCONFIG_DATA_LITTLEENDIAN -D__UCOS__ -O3 \
		`./tools/ccopts-crosscompile.sh $(CC)`"

# ucLinux on ARM: Little-endian.  Note that we use $(CFLAGS) rather than
# $(XCFLAGS) since this is a Unix system, just not the same as the source
# one (in particular we need __UNIX__ defined for the build).

target-uclinux:
	@make directories
	make $(XDEFINES) OSNAME=Linux CC=arm-elf-gcc CFLAGS="$(CFLAGS) \
		-DCONFIG_DATA_LITTLEENDIAN -DCONFIG_CONSERVE_MEMORY \
		-fomit-frame-pointer -O3 `./tools/ccopts-crosscompile.sh $(CC)`"

# VDK: AD-provided toolchain under Windows.

target-vdk:
	@make OSNAME=vdk target-init
	make $(XDEFINES) OSNAME=VDK CC=blackfin-gcc CFLAGS="$(XCFLAGS) \
		-DCONFIG_DATA_LITTLEENDIAN -D__VDK__ -O2 \
		`./tools/ccopts-crosscompile.sh $(CC)`"

# VxWorks: VxWorks toolchain under Windows.

target-vxworks-arm:
	@make OSNAME=vxworks target-init
	make $(XDEFINES) OSNAME=VxWorks CC=arm-elf-gcc CFLAGS="$(XCFLAGS) \
		-DCONFIG_DATA_LITTLEENDIAN -D__VXWORKS__ -O3 \
		`./tools/ccopts-crosscompile.sh $(CC)`"

target-vxworks-mb:
	@make OSNAME=vxworks target-init
	make $(XDEFINES) OSNAME=VxWorks CC=mb-elf-gcc CFLAGS="$(XCFLAGS) \
		-DCONFIG_DATA_BIGENDIAN -D__VXWORKS__ -O3 \
		`./tools/ccopts-crosscompile.sh $(CC)`"

target-vxworks-mips:
	@make OSNAME=vxworks target-init
	make $(XDEFINES) OSNAME=VxWorks CC=mips-elf-gcc CFLAGS="$(XCFLAGS) \
		-DCONFIG_DATA_LITTLEENDIAN -D__VXWORKS__ -O3 \
		`./tools/ccopts-crosscompile.sh $(CC)`"

target-vxworks-ppc:
	@make OSNAME=vxworks target-init
	make $(XDEFINES) OSNAME=VxWorks CC=powerpc-eabi-gcc CFLAGS="$(XCFLAGS) \
		-DCONFIG_DATA_BIGENDIAN -D__VXWORKS__ -O3 \
		`./tools/ccopts-crosscompile.sh $(CC)`"

target-vxworks-x86:
	@make OSNAME=vxworks target-init
	make $(XDEFINES) OSNAME=VxWorks CC=i386-elf-gcc CFLAGS="$(XCFLAGS) \
		-DCONFIG_DATA_LITTLEENDIAN -D__VXWORKS__ -O3 \
		`./tools/ccopts-crosscompile.sh $(CC)`"

# Xilinx XMK: Gnu toolchain under Unix or Cygwin.  There are two possible
# compilers, gcc for MicroBlaze (Xilinx custom RISC core) or for PPC.  The
# MB gcc doesn't predefine any symbols allowing us to autoconfigure
# ourselves so we manually define __mb__.  It may also be necessary to use
# the MicroBlaze-specific mb-ar instead of the standard ar.
#
# Note that the MB cores are highly reconfigurable and may have all sorts
# of capabilities enabled or disabled.  You'll need to edit the 'xl'
# options below based on your config.

target-xmk-mb:
	@make OSNAME=xmk target-init
	@echo See the comments by the MicroBlaze entry in the makefile before
	@echo building for this core.
	make $(XDEFINES) OSNAME=XMK CC=mb-gcc CFLAGS="$(XCFLAGS) \
		-DCONFIG_DATA_BIGENDIAN -DCONFIG_CONSERVE_MEMORY -D__XMK__ \
		-D__mb__ -mno-xl-soft-mul -mxl-barrel-shift -mno-xl-soft-div \
		-fomit-frame-pointer -O2 -I../microblaze_0/include \
		`./tools/ccopts-crosscompile.sh $(CC)`"

target-xmk-ppc:
	@make OSNAME=xmk target-init
	make $(XDEFINES) OSNAME=XMK CC=powerpc-eabi-gcc CFLAGS="$(XCFLAGS) \
		-DCONFIG_DATA_LITTLEENDIAN -D__XMK__ -fomit-frame-pointer -O3 \
		`./tools/ccopts-crosscompile.sh $(CC)`"

# Non-OS target or proprietary OS.  This will trigger #errors at various
# locations in the code where you need to fill in the blanks for whatever
# your OS (or non-OS) requires.

target-68k:
	@make OSNAME=moto68 target-init

	make $(XDEFINES) OSNAME=moto68 CC=mcc68k.exe CFLAGS="$(XCFLAGS) \
		-DCONFIG_DATA_BIGENDIAN -D__m68k__ -pCPU32 -c -g -zc -Mca -Md5 \
		-Kha5 -NScode -nKf -Oc -S -DTVA_68K=1 -DIS_COOPERATIVE=1 \
		-DTVA_FUNCTION_FRAME_SUPPORT=0 -Os -Oe -OXc2 -OXcs2 -OXk2 -OXrep1 \
		-nOj -OXrf -OXsr \
		`./tools/ccopts-crosscompile.sh $(CC)`"

#****************************************************************************
#*																			*
#*						Clean up after make has finished					*
#*																			*
#****************************************************************************

# The removal of the some files and directories is silenced since they
# may not exist and we don't want unnecessary error messages arising from
# trying to remove them

clean:
	rm -f *.o core testlib stestlib tools/endian $(LIBNAME) $(SLIBNAME)
	@if [ -d $(STATIC_OBJ_PATH) ] ; then \
		rm -f $(STATIC_OBJ_PATH)*.o ; \
		rmdir $(STATIC_OBJ_DIR) ; \
	fi
	@if [ -d $(SHARED_OBJ_DIR) ] ; then \
		rm -f $(SHARED_OBJ_PATH)*.o ; \
		rmdir $(SHARED_OBJ_DIR) ; \
	fi
	@if [ -d ./clang_output ] ; then \
		rm -r ./clang_output/* ; \
		rmdir clang_output ; \
	fi
	@if [ `uname -s` = 'CYGWIN_NT-5.0' ] ; then rm -f *.exe; fi
	@if [ `uname -s` = 'HP-UX' ] ; then rm -f lib$(PROJ).sl; fi
