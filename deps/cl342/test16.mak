# Microsoft Visual C++ generated build script - Do not modify

PROJ = TEST16
DEBUG = 1
PROGTYPE = 3
CALLER = 
ARGS = 
DLLS = 
D_RCDEFINES = -d_DEBUG
R_RCDEFINES = -dNDEBUG
ORIGIN = MSVC
ORIGIN_VER = 1.00
PROJPATH = D:\WORK\CRYPT\
USEMFC = 0
CC = cl
CPP = cl
CXX = cl
CCREATEPCHFLAG = 
CPPCREATEPCHFLAG = 
CUSEPCHFLAG = 
CPPUSEPCHFLAG = 
FIRSTC = CERTS.C     
FIRSTCPP =             
RC = rc
CFLAGS_D_WTTY = /nologo /G2 /Mq /W3 /Zi /ALu /Od /D "_DEBUG" /I ".\." /Fd"TEST16.PDB"
CFLAGS_R_WTTY = /nologo /Gs /G2 /Mq /W3 /AM /Ox /D "NDEBUG" /FR 
LFLAGS_D_WTTY = /NOLOGO /NOD /PACKC:57344 /ALIGN:16 /ONERROR:NOEXE /CO 
LFLAGS_R_WTTY = /NOLOGO /NOD /PACKC:57344 /ALIGN:16 /ONERROR:NOEXE 
LIBS_D_WTTY = oldnames libw llibcewq cl16 
LIBS_R_WTTY = oldnames libw mlibcewq 
RCFLAGS = /nologo
RESFLAGS = /nologo
RUNFLAGS = 
OBJS_EXT = 
LIBS_EXT = 
!if "$(DEBUG)" == "1"
CFLAGS = $(CFLAGS_D_WTTY)
LFLAGS = $(LFLAGS_D_WTTY)
LIBS = $(LIBS_D_WTTY)
MAPFILE = nul
RCDEFINES = $(D_RCDEFINES)
DEFFILE=C:\MSVC\BIN\CL.DEF
!else
CFLAGS = $(CFLAGS_R_WTTY)
LFLAGS = $(LFLAGS_R_WTTY)
LIBS = $(LIBS_R_WTTY)
MAPFILE = nul
RCDEFINES = $(R_RCDEFINES)
DEFFILE=C:\MSVC\BIN\CL.DEF
!endif
!if [if exist MSVC.BND del MSVC.BND]
!endif
SBRS = CERTS.SBR \
		ENVELOPE.SBR \
		HIGHLVL.SBR \
		KEYDBX.SBR \
		KEYFILE.SBR \
		LOWLVL.SBR \
		SCERT.SBR \
		SREQRESP.SBR \
		SSH.SBR \
		SSL.SBR \
		TESTLIB.SBR \
		UTILS.SBR \
		LOADKEY.SBR


CERTS_DEP = d:\work\crypt\cryptlib.h \
	d:\work\crypt\test\test.h \
	d:\work\crypt\test\filename.h \
	d:\work\crypt\test/filename.h \
	d:\work\crypt\test/test.h \
	d:\work\crypt\test/\filename.h


ENVELOPE_DEP = d:\work\crypt\cryptlib.h \
	d:\work\crypt\test\test.h \
	d:\work\crypt\test\filename.h \
	d:\work\crypt\test/filename.h \
	d:\work\crypt\test/test.h \
	d:\work\crypt\test/\filename.h


HIGHLVL_DEP = d:\work\crypt\cryptlib.h \
	d:\work\crypt\test\test.h \
	d:\work\crypt\test\filename.h \
	d:\work\crypt\test/filename.h \
	d:\work\crypt\test/test.h \
	d:\work\crypt\test/\filename.h


KEYDBX_DEP = d:\work\crypt\cryptlib.h \
	d:\work\crypt\test\test.h \
	d:\work\crypt\test\filename.h \
	d:\work\crypt\test/filename.h \
	d:\work\crypt\test/test.h \
	d:\work\crypt\test/\filename.h


KEYFILE_DEP = d:\work\crypt\cryptlib.h \
	d:\work\crypt\test\test.h \
	d:\work\crypt\test\filename.h \
	d:\work\crypt\test/filename.h \
	d:\work\crypt\test/test.h \
	d:\work\crypt\test/\filename.h


LOWLVL_DEP = d:\work\crypt\cryptlib.h \
	d:\work\crypt\test\test.h \
	d:\work\crypt\test\filename.h \
	d:\work\crypt\test/filename.h \
	d:\work\crypt\test/test.h \
	d:\work\crypt\test/\filename.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h


SCERT_DEP = d:\work\crypt\cryptlib.h \
	d:\work\crypt\test\test.h \
	d:\work\crypt\test\filename.h \
	d:\work\crypt\test/filename.h \
	d:\work\crypt\test/test.h \
	d:\work\crypt\test/\filename.h


SREQRESP_DEP = d:\work\crypt\cryptlib.h \
	d:\work\crypt\test\test.h \
	d:\work\crypt\test\filename.h \
	d:\work\crypt\test/filename.h \
	d:\work\crypt\test/test.h \
	d:\work\crypt\test/\filename.h


SSH_DEP = d:\work\crypt\cryptlib.h \
	d:\work\crypt\test\test.h \
	d:\work\crypt\test\filename.h \
	d:\work\crypt\test/filename.h \
	d:\work\crypt\test/test.h \
	d:\work\crypt\test/\filename.h \
	d:\work\crypt\misc/misc_rw.c \
	d:\work\crypt\crypt.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\misc/\misc_rw.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\misc/misc_rw.h \
	d:\work\crypt\envelope/pgp.h


SSL_DEP = d:\work\crypt\cryptlib.h \
	d:\work\crypt\test\test.h \
	d:\work\crypt\test\filename.h \
	d:\work\crypt\test/filename.h \
	d:\work\crypt\test/test.h \
	d:\work\crypt\test/\filename.h \
	c:\msvc\include\winsock.h


TESTLIB_DEP = d:\work\crypt\cryptlib.h \
	d:\work\crypt\test\test.h \
	d:\work\crypt\test\filename.h \
	d:\work\crypt\test/filename.h \
	d:\work\crypt\test/test.h \
	d:\work\crypt\test/\filename.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h


UTILS_DEP = d:\work\crypt\cryptlib.h \
	d:\work\crypt\test\test.h \
	d:\work\crypt\test\filename.h \
	d:\work\crypt\test/filename.h \
	d:\work\crypt\test/test.h \
	d:\work\crypt\test/\filename.h


LOADKEY_DEP = d:\work\crypt\cryptlib.h \
	d:\work\crypt\test\test.h \
	d:\work\crypt\test\filename.h \
	d:\work\crypt\test/filename.h \
	d:\work\crypt\test/test.h \
	d:\work\crypt\test/\filename.h


all:	$(PROJ).EXE

CERTS.OBJ:	TEST\CERTS.C $(CERTS_DEP)
	$(CC) $(CFLAGS) $(CCREATEPCHFLAG) /c TEST\CERTS.C

ENVELOPE.OBJ:	TEST\ENVELOPE.C $(ENVELOPE_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c TEST\ENVELOPE.C

HIGHLVL.OBJ:	TEST\HIGHLVL.C $(HIGHLVL_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c TEST\HIGHLVL.C

KEYDBX.OBJ:	TEST\KEYDBX.C $(KEYDBX_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c TEST\KEYDBX.C

KEYFILE.OBJ:	TEST\KEYFILE.C $(KEYFILE_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c TEST\KEYFILE.C

LOWLVL.OBJ:	TEST\LOWLVL.C $(LOWLVL_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c TEST\LOWLVL.C

SCERT.OBJ:	TEST\SCERT.C $(SCERT_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c TEST\SCERT.C

SREQRESP.OBJ:	TEST\SREQRESP.C $(SREQRESP_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c TEST\SREQRESP.C

SSH.OBJ:	TEST\SSH.C $(SSH_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c TEST\SSH.C

SSL.OBJ:	TEST\SSL.C $(SSL_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c TEST\SSL.C

TESTLIB.OBJ:	TEST\TESTLIB.C $(TESTLIB_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c TEST\TESTLIB.C

UTILS.OBJ:	TEST\UTILS.C $(UTILS_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c TEST\UTILS.C

LOADKEY.OBJ:	TEST\LOADKEY.C $(LOADKEY_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c TEST\LOADKEY.C


$(PROJ).EXE::	CERTS.OBJ ENVELOPE.OBJ HIGHLVL.OBJ KEYDBX.OBJ KEYFILE.OBJ LOWLVL.OBJ \
	SCERT.OBJ SREQRESP.OBJ SSH.OBJ SSL.OBJ TESTLIB.OBJ UTILS.OBJ LOADKEY.OBJ $(OBJS_EXT) $(DEFFILE)
	echo >NUL @<<$(PROJ).CRF
CERTS.OBJ +
ENVELOPE.OBJ +
HIGHLVL.OBJ +
KEYDBX.OBJ +
KEYFILE.OBJ +
LOWLVL.OBJ +
SCERT.OBJ +
SREQRESP.OBJ +
SSH.OBJ +
SSL.OBJ +
TESTLIB.OBJ +
UTILS.OBJ +
LOADKEY.OBJ +
$(OBJS_EXT)
$(PROJ).EXE
$(MAPFILE)
c:\msvc\lib\+
$(LIBS)
$(DEFFILE);
<<
	link $(LFLAGS) @$(PROJ).CRF
	$(RC) $(RESFLAGS) $@


run: $(PROJ).EXE
	$(PROJ) $(RUNFLAGS)


$(PROJ).BSC: $(SBRS)
	bscmake @<<
/o$@ $(SBRS)
<<
