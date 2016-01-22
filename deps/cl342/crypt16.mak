# Microsoft Visual C++ generated build script - Do not modify

PROJ = CL16
DEBUG = 1
PROGTYPE = 1
CALLER = test16.exe
ARGS = 
DLLS = 
D_RCDEFINES = /d_DEBUG
R_RCDEFINES = /dNDEBUG
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
FIRSTC = BN_ADD.C    
FIRSTCPP =             
RC = rc
CFLAGS_D_WDLL = /nologo /G2 /W3 /Gf /Zi /ALw /Od /D "_DEBUG" /D "WIN16" /D "CONFIG_NO_SESSIONS" /D "CONFIG_NO_DEVICES" /I ".\." /GD /Fd"CL16.PDB"
CFLAGS_R_WDLL = /nologo /G3 /W3 /Gf /ALw /O2 /D "NDEBUG" /D "WIN16" /GD 
LFLAGS_D_WDLL = /NOLOGO /NOD /NOE /PACKC:61440 /SEG:256 /ALIGN:32 /ONERROR:NOEXE /CO  
LFLAGS_R_WDLL = /NOLOGO /NOD /NOE /PACKC:61440 /SEG:256 /ALIGN:16 /ONERROR:NOEXE  
LIBS_D_WDLL = oldnames libw ldllcew toolhelp.lib 
LIBS_R_WDLL = oldnames libw ldllcew toolhelp.lib 
RCFLAGS = /nologo
RESFLAGS = /nologo
RUNFLAGS = 
DEFFILE = CRYPT.DEF
OBJS_EXT = 
LIBS_EXT = 
!if "$(DEBUG)" == "1"
CFLAGS = $(CFLAGS_D_WDLL)
LFLAGS = $(LFLAGS_D_WDLL)
LIBS = $(LIBS_D_WDLL)
MAPFILE = nul
RCDEFINES = $(D_RCDEFINES)
!else
CFLAGS = $(CFLAGS_R_WDLL)
LFLAGS = $(LFLAGS_R_WDLL)
LIBS = $(LIBS_R_WDLL)
MAPFILE = nul
RCDEFINES = $(R_RCDEFINES)
!endif
!if [if exist MSVC.BND del MSVC.BND]
!endif
SBRS = BN_ADD.SBR \
		BN_ASM.SBR \
		BN_CTX.SBR \
		BN_DIV.SBR \
		BN_EXP.SBR \
		BN_EXP2.SBR \
		BN_GCD.SBR \
		BN_LIB.SBR \
		BN_MOD.SBR \
		BN_MONT.SBR \
		BN_MUL.SBR \
		BN_RECP.SBR \
		BN_SHIFT.SBR \
		BN_SQR.SBR \
		BN_WORD.SBR \
		CERTREV.SBR \
		CERTSIGN.SBR \
		CERTVAL.SBR \
		CHAIN.SBR \
		CHK_CERT.SBR \
		CHK_CHN.SBR \
		CHK_USE.SBR \
		COMP_GET.SBR \
		COMP_SET.SBR \
		DN.SBR \
		DNSTRING.SBR \
		EXT.SBR \
		EXT_ADD.SBR \
		EXT_CHK.SBR \
		EXT_COPY.SBR \
		EXT_DEF.SBR \
		EXT_RD.SBR \
		EXT_WR.SBR \
		IMP_EXP.SBR \
		TRUSTMGR.SBR \
		READ.SBR \
		WRITE.SBR \
		CTX_3DES.SBR \
		CTX_AES.SBR \
		CTX_BF.SBR \
		CTX_CAST.SBR \
		CTX_DES.SBR \
		CTX_DH.SBR \
		CTX_DSA.SBR \
		CTX_ELG.SBR \
		CTX_HMD5.SBR \
		CTX_HRMD.SBR \
		CTX_HSHA.SBR \
		CTX_MD5.SBR \
		CTX_MISC.SBR \
		CTX_RIPE.SBR \
		CTX_RSA.SBR \
		CTX_SHA.SBR \
		CTX_SHA2.SBR \
		KEY_RD.SBR \
		KEY_WR.SBR \
		KEYLOAD.SBR \
		KG_DLP.SBR \
		KG_PRIME.SBR \
		KG_RSA.SBR \
		AESCRYPT.SBR \
		AESKEY.SBR \
		AESTAB.SBR \
		BFECB.SBR \
		BFENC.SBR \
		BFSKEY.SBR \
		CASTECB.SBR \
		CASTENC.SBR \
		CASTSKEY.SBR \
		DESCBC.SBR \
		DESECB.SBR \
		DESECB3.SBR \
		DESENC.SBR \
		DESSKEY.SBR \
		MD5DGST.SBR \
		RMDDGST.SBR \
		SHA1DGST.SBR \
		SHA2.SBR \
		SYSTEM.SBR \
		CMS_DENV.SBR \
		CMS_ENV.SBR \
		DECODE.SBR \
		ENCODE.SBR \
		PGP_DENV.SBR \
		PGP_ENV.SBR \
		PGP_MISC.SBR \
		RES_DENV.SBR \
		RES_ENV.SBR \
		DNS.SBR \
		FILE.SBR \
		HTTP_RD.SBR \
		HTTP_WR.SBR \
		MEMORY.SBR \
		NET.SBR \
		STREAM.SBR \
		TCP.SBR \
		ATTR_ACL.SBR \
		CERTM_~1.SBR \
		INIT.SBR \
		INT_MSG.SBR \
		KEY_ACL.SBR \
		MECH_ACL.SBR \
		MSG_ACL.SBR \
		OBJ_ACC.SBR \
		OBJECTS.SBR \
		SEC_MEM.SBR \
		SEMAPH~1.SBR \
		SENDMSG.SBR \
		HTTP.SBR \
		PGP.SBR \
		PKCS15.SBR \
		PKCS15~1.SBR \
		PKCS15~2.SBR \
		KEYEX.SBR \
		KEYEX_RW.SBR \
		MECH_DRV.SBR \
		MECH_ENC.SBR \
		MECH_SIG.SBR \
		MECH_WRP.SBR \
		OBJ_QRY.SBR \
		SIGN.SBR \
		SIGN_RW.SBR \
		ASN1_CHK.SBR \
		ASN1_EXT.SBR \
		ASN1_RD.SBR \
		ASN1_WR.SBR \
		BASE64.SBR \
		INT_API.SBR \
		INT_ATTR.SBR \
		INT_ENV.SBR \
		MISC_RW.SBR \
		OS_SPEC.SBR \
		RANDOM.SBR \
		WIN16.SBR \
		CRYPTAPI.SBR \
		CRYPTCFG.SBR \
		CRYPTCRT.SBR \
		CRYPTCTX.SBR \
		CRYPTDEV.SBR \
		CRYPTENV.SBR \
		CRYPTKEY.SBR \
		CRYPTLIB.SBR \
		CRYPTSES.SBR \
		CRYPTUSR.SBR \
		ACE_MO~1.SBR


CRYPT_RCDEP = 

BN_ADD_DEP = d:\work\crypt\bn\bn_lcl.h \
	d:\work\crypt\bn\bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\bn/bn_lcl.h \
	d:\work\crypt\bn/\bn.h


BN_ASM_DEP = d:\work\crypt\bn\bn_lcl.h \
	d:\work\crypt\bn\bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\bn/bn_lcl.h \
	d:\work\crypt\bn/\bn.h


BN_CTX_DEP = d:\work\crypt\bn\bn_lcl.h \
	d:\work\crypt\bn\bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\bn/bn_lcl.h \
	d:\work\crypt\bn/\bn.h


BN_DIV_DEP = d:\work\crypt\bn\bn_lcl.h \
	d:\work\crypt\bn\bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\bn/bn_lcl.h \
	d:\work\crypt\bn/\bn.h


BN_EXP_DEP = d:\work\crypt\bn\bn_lcl.h \
	d:\work\crypt\bn\bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\bn/bn_lcl.h \
	d:\work\crypt\bn/\bn.h


BN_EXP2_DEP = d:\work\crypt\bn\bn_lcl.h \
	d:\work\crypt\bn\bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\bn/bn_lcl.h \
	d:\work\crypt\bn/\bn.h


BN_GCD_DEP = d:\work\crypt\bn\bn_lcl.h \
	d:\work\crypt\bn\bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\bn/bn_lcl.h \
	d:\work\crypt\bn/\bn.h


BN_LIB_DEP = d:\work\crypt\bn\bn_lcl.h \
	d:\work\crypt\bn\bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\bn/bn_lcl.h \
	d:\work\crypt\bn/\bn.h


BN_MOD_DEP = d:\work\crypt\bn\bn_lcl.h \
	d:\work\crypt\bn\bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\bn/bn_lcl.h \
	d:\work\crypt\bn/\bn.h


BN_MONT_DEP = d:\work\crypt\bn\bn_lcl.h \
	d:\work\crypt\bn\bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\bn/bn_lcl.h \
	d:\work\crypt\bn/\bn.h


BN_MUL_DEP = d:\work\crypt\bn\bn_lcl.h \
	d:\work\crypt\bn\bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\bn/bn_lcl.h \
	d:\work\crypt\bn/\bn.h


BN_RECP_DEP = d:\work\crypt\bn\bn_lcl.h \
	d:\work\crypt\bn\bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\bn/bn_lcl.h \
	d:\work\crypt\bn/\bn.h


BN_SHIFT_DEP = d:\work\crypt\bn\bn_lcl.h \
	d:\work\crypt\bn\bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\bn/bn_lcl.h \
	d:\work\crypt\bn/\bn.h


BN_SQR_DEP = d:\work\crypt\bn\bn_lcl.h \
	d:\work\crypt\bn\bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\bn/bn_lcl.h \
	d:\work\crypt\bn/\bn.h


BN_WORD_DEP = d:\work\crypt\bn\bn_lcl.h \
	d:\work\crypt\bn\bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\bn/bn_lcl.h \
	d:\work\crypt\bn/\bn.h


CERTREV_DEP = d:\work\crypt\cert\cert.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\misc/asn1_ext.h \
	d:\work\crypt\cert/cert.h


CERTSIGN_DEP = d:\work\crypt\cert\cert.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\cert/cert.h


CERTVAL_DEP = d:\work\crypt\cert\cert.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\misc/asn1_ext.h \
	d:\work\crypt\cert/cert.h


CHAIN_DEP = d:\work\crypt\cert\cert.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\misc/asn1_ext.h \
	d:\work\crypt\misc/misc_rw.h \
	d:\work\crypt\cert/cert.h


CHK_CERT_DEP = d:\work\crypt\cert\cert.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\cert\certattr.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\misc/asn1_ext.h \
	d:\work\crypt\cert/cert.h \
	d:\work\crypt\cert/certattr.h


CHK_CHN_DEP = d:\work\crypt\cert\cert.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\cert/cert.h


CHK_USE_DEP = d:\work\crypt\cert\cert.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\cert\certattr.h \
	d:\work\crypt\cert/cert.h \
	d:\work\crypt\cert/certattr.h


COMP_GET_DEP = d:\work\crypt\cert\cert.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\cert\certattr.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\misc/asn1_ext.h \
	d:\work\crypt\cert/cert.h \
	d:\work\crypt\cert/certattr.h


COMP_SET_DEP = d:\work\crypt\cert\cert.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\cert\certattr.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\misc/asn1_ext.h \
	d:\work\crypt\cert/cert.h \
	d:\work\crypt\cert/certattr.h


DN_DEP = d:\work\crypt\cert\cert.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\cert/cert.h


DNSTRING_DEP = d:\work\crypt\cert\cert.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\cert/cert.h


EXT_DEP = d:\work\crypt\cert\cert.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\cert\certattr.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\cert/cert.h \
	d:\work\crypt\cert/certattr.h


EXT_ADD_DEP = d:\work\crypt\cert\cert.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\cert\certattr.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\cert/cert.h \
	d:\work\crypt\cert/certattr.h


EXT_CHK_DEP = d:\work\crypt\cert\cert.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\cert\certattr.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\misc/asn1_ext.h \
	d:\work\crypt\cert/cert.h \
	d:\work\crypt\cert/certattr.h


EXT_COPY_DEP = d:\work\crypt\cert\cert.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\cert\certattr.h \
	d:\work\crypt\cert/cert.h \
	d:\work\crypt\cert/certattr.h


EXT_DEF_DEP = d:\work\crypt\cert\cert.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\cert\certattr.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\misc/asn1_ext.h \
	d:\work\crypt\cert/cert.h \
	d:\work\crypt\cert/certattr.h


EXT_RD_DEP = d:\work\crypt\cert\cert.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\cert\certattr.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\misc/asn1_ext.h \
	d:\work\crypt\cert/cert.h \
	d:\work\crypt\cert/certattr.h


EXT_WR_DEP = d:\work\crypt\cert\cert.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\cert\certattr.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\misc/asn1_ext.h \
	d:\work\crypt\cert/cert.h \
	d:\work\crypt\cert/certattr.h


IMP_EXP_DEP = d:\work\crypt\cert\cert.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\misc/asn1_ext.h \
	d:\work\crypt\cert/cert.h


TRUSTMGR_DEP = d:\work\crypt\cert\cert.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\cert\trustmgr.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\cert/cert.h \
	d:\work\crypt\cert/trustmgr.h


READ_DEP = d:\work\crypt\cert\cert.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\misc/asn1_ext.h \
	d:\work\crypt\cert/cert.h


WRITE_DEP = d:\work\crypt\cert\cert.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\misc/asn1_ext.h \
	d:\work\crypt\cert/cert.h


CTX_3DES_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\context\context.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\device/capabil.h \
	d:\work\crypt\crypt/des.h \
	d:\work\crypt\context/context.h \
	d:\work\crypt\crypt/testdes.h


CTX_AES_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\context\context.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\device/capabil.h \
	d:\work\crypt\crypt/aes.h \
	d:\work\crypt\crypt/\aes_tdef.h \
	d:\work\crypt\crypt/aes_tdef.h \
	d:\work\crypt\context/context.h


CTX_BF_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\context\context.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\device/capabil.h \
	d:\work\crypt\crypt/blowfish.h \
	d:\work\crypt\context/context.h


CTX_CAST_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\context\context.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\device/capabil.h \
	d:\work\crypt\crypt/cast.h \
	d:\work\crypt\context/context.h


CTX_DES_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\context\context.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\device/capabil.h \
	d:\work\crypt\crypt/des.h \
	d:\work\crypt\context/context.h \
	d:\work\crypt\crypt/testdes.h


CTX_DH_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\context\context.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\device/capabil.h \
	d:\work\crypt\context/context.h


CTX_DSA_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\context\context.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\device/capabil.h \
	d:\work\crypt\context/context.h


CTX_ELG_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\context\context.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\device/capabil.h \
	d:\work\crypt\context/context.h


CTX_HMD5_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\context\context.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\device/capabil.h \
	d:\work\crypt\crypt/md5.h \
	d:\work\crypt\context/context.h


CTX_HRMD_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\context\context.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\device/capabil.h \
	d:\work\crypt\crypt/ripemd.h \
	d:\work\crypt\context/context.h


CTX_HSHA_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\context\context.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\device/capabil.h \
	d:\work\crypt\crypt/sha.h \
	d:\work\crypt\context/context.h


CTX_MD5_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\context\context.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\device/capabil.h \
	d:\work\crypt\crypt/md5.h \
	d:\work\crypt\context/context.h


CTX_MISC_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\context\context.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\device/capabil.h \
	d:\work\crypt\context/context.h


CTX_RIPE_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\context\context.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\device/capabil.h \
	d:\work\crypt\crypt/ripemd.h \
	d:\work\crypt\context/context.h


CTX_RSA_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\context\context.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\device/capabil.h \
	d:\work\crypt\context/context.h


CTX_SHA_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\context\context.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\device/capabil.h \
	d:\work\crypt\crypt/sha.h \
	d:\work\crypt\context/context.h


CTX_SHA2_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\context\context.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\device/capabil.h \
	d:\work\crypt\crypt/sha2.h \
	d:\work\crypt\crypt/\aes_tdef.h \
	d:\work\crypt\crypt/aes_tdef.h \
	d:\work\crypt\context/context.h


KEY_RD_DEP = d:\work\crypt\context\context.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\device/capabil.h \
	d:\work\crypt\envelope/pgp.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\misc/asn1_ext.h \
	d:\work\crypt\misc/misc_rw.h \
	d:\work\crypt\context/context.h


KEY_WR_DEP = d:\work\crypt\context\context.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\device/capabil.h \
	d:\work\crypt\envelope/pgp.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\misc/asn1_ext.h \
	d:\work\crypt\misc/misc_rw.h \
	d:\work\crypt\context/context.h


KEYLOAD_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\context\context.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\device/capabil.h \
	d:\work\crypt\context/context.h


KG_DLP_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\context\context.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\device/capabil.h \
	d:\work\crypt\context\keygen.h \
	d:\work\crypt\context/context.h \
	d:\work\crypt\context/keygen.h


KG_PRIME_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\context\context.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\device/capabil.h \
	d:\work\crypt\context\keygen.h \
	d:\work\crypt\context/context.h \
	d:\work\crypt\context/keygen.h \
	d:\work\crypt\bn/bn_prime.h


KG_RSA_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\context\context.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\device/capabil.h \
	d:\work\crypt\context\keygen.h \
	d:\work\crypt\context/context.h \
	d:\work\crypt\context/keygen.h


AESCRYPT_DEP = d:\work\crypt\crypt\aesopt.h \
	d:\work\crypt\crypt\aes.h \
	d:\work\crypt\crypt\aes_tdef.h \
	d:\work\crypt\crypt/aes_tdef.h \
	d:\work\crypt\crypt\aes_edef.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\vcfix\sys/endian.h \
	d:\vcfix\machine/endian.h \
	d:\work\crypt\crypt/aes.h \
	d:\work\crypt\crypt/\aes_tdef.h \
	d:\work\crypt\crypt/aes_edef.h \
	d:\work\crypt\crypt\aestab.h \
	d:\work\crypt\crypt/aesopt.h \
	d:\work\crypt\crypt/\aes.h \
	d:\work\crypt\crypt/\aes_edef.h \
	d:\work\crypt\crypt/aestab.h


AESKEY_DEP = d:\work\crypt\crypt\aesopt.h \
	d:\work\crypt\crypt\aes.h \
	d:\work\crypt\crypt\aes_tdef.h \
	d:\work\crypt\crypt/aes_tdef.h \
	d:\work\crypt\crypt\aes_edef.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\vcfix\sys/endian.h \
	d:\vcfix\machine/endian.h \
	d:\work\crypt\crypt/aes.h \
	d:\work\crypt\crypt/\aes_tdef.h \
	d:\work\crypt\crypt/aes_edef.h \
	d:\work\crypt\crypt\aestab.h \
	d:\work\crypt\crypt\via_ace.h \
	d:\work\crypt\crypt/aesopt.h \
	d:\work\crypt\crypt/\aes.h \
	d:\work\crypt\crypt/\aes_edef.h \
	d:\work\crypt\crypt/aestab.h \
	d:\work\crypt\crypt/via_ace.h


AESTAB_DEP = d:\work\crypt\crypt\aes.h \
	d:\work\crypt\crypt\aes_tdef.h \
	d:\work\crypt\crypt/aes_tdef.h \
	d:\work\crypt\crypt\aesopt.h \
	d:\work\crypt\crypt\aes_edef.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\vcfix\sys/endian.h \
	d:\vcfix\machine/endian.h \
	d:\work\crypt\crypt/aes.h \
	d:\work\crypt\crypt/\aes_tdef.h \
	d:\work\crypt\crypt/aes_edef.h \
	d:\work\crypt\crypt/aesopt.h \
	d:\work\crypt\crypt/\aes.h \
	d:\work\crypt\crypt/\aes_edef.h \
	d:\work\crypt\crypt\aestab.h \
	d:\work\crypt\crypt/aestab.h


BFECB_DEP = d:\work\crypt\crypt\osconfig.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\crypt\blowfish.h \
	d:\work\crypt\crypt\bflocl.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\crypt/blowfish.h \
	d:\work\crypt\crypt/bflocl.h


BFENC_DEP = d:\work\crypt\crypt\osconfig.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\crypt\blowfish.h \
	d:\work\crypt\crypt\bflocl.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\crypt/blowfish.h \
	d:\work\crypt\crypt/bflocl.h


BFSKEY_DEP = d:\work\crypt\crypt\osconfig.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\crypt\blowfish.h \
	d:\work\crypt\crypt\bflocl.h \
	d:\work\crypt\crypt\bfpi.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\crypt/blowfish.h \
	d:\work\crypt\crypt/bflocl.h \
	d:\work\crypt\crypt/bfpi.h


CASTECB_DEP = d:\work\crypt\crypt\osconfig.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\crypt\cast.h \
	d:\work\crypt\crypt\castlcl.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\crypt/cast.h \
	d:\work\crypt\crypt/castlcl.h


CASTENC_DEP = d:\work\crypt\crypt\osconfig.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\crypt\cast.h \
	d:\work\crypt\crypt\castlcl.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\crypt/cast.h \
	d:\work\crypt\crypt/castlcl.h


CASTSKEY_DEP = d:\work\crypt\crypt\osconfig.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\crypt\cast.h \
	d:\work\crypt\crypt\castlcl.h \
	d:\work\crypt\crypt\castsbox.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\crypt/cast.h \
	d:\work\crypt\crypt/castsbox.h


DESCBC_DEP = d:\work\crypt\crypt\osconfig.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\crypt\des.h \
	d:\work\crypt\crypt\deslocl.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\crypt/des.h \
	d:\work\crypt\crypt/deslocl.h \
	d:\work\crypt\crypt/\osconfig.h \
	d:\work\crypt\crypt/\des.h


DESECB_DEP = d:\work\crypt\crypt\osconfig.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\crypt\des.h \
	d:\work\crypt\crypt\deslocl.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\crypt/des.h \
	d:\work\crypt\crypt\spr.h \
	d:\work\crypt\crypt/deslocl.h \
	d:\work\crypt\crypt/\osconfig.h \
	d:\work\crypt\crypt/\des.h \
	d:\work\crypt\crypt/spr.h


DESECB3_DEP = d:\work\crypt\crypt\osconfig.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\crypt\des.h \
	d:\work\crypt\crypt\deslocl.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\crypt/des.h \
	d:\work\crypt\crypt/deslocl.h \
	d:\work\crypt\crypt/\osconfig.h \
	d:\work\crypt\crypt/\des.h


DESENC_DEP = d:\work\crypt\crypt\osconfig.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\crypt\des.h \
	d:\work\crypt\crypt\deslocl.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\crypt/des.h \
	d:\work\crypt\crypt/deslocl.h \
	d:\work\crypt\crypt/\osconfig.h \
	d:\work\crypt\crypt/\des.h


DESSKEY_DEP = d:\work\crypt\crypt\osconfig.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\crypt\des.h \
	d:\work\crypt\crypt\deslocl.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\crypt/des.h \
	d:\work\crypt\crypt/deslocl.h \
	d:\work\crypt\crypt/\osconfig.h \
	d:\work\crypt\crypt/\des.h


MD5DGST_DEP = d:\work\crypt\crypt\osconfig.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\crypt\md5locl.h \
	d:\work\crypt\crypt\md5.h \
	d:\work\crypt\crypt/md5.h \
	d:\work\crypt\crypt\md32com.h \
	d:\work\crypt\crypt/md32com.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\crypt/md5locl.h \
	d:\work\crypt\crypt/\md5.h \
	d:\work\crypt\crypt/\md32com.h


RMDDGST_DEP = d:\work\crypt\crypt\osconfig.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\crypt\rmdlocl.h \
	d:\work\crypt\crypt\ripemd.h \
	d:\work\crypt\crypt/ripemd.h \
	d:\work\crypt\crypt\md32com.h \
	d:\work\crypt\crypt/md32com.h \
	d:\work\crypt\crypt\rmdconst.h \
	d:\work\crypt\crypt/rmdconst.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\crypt/rmdlocl.h \
	d:\work\crypt\crypt/\ripemd.h \
	d:\work\crypt\crypt/\md32com.h \
	d:\work\crypt\crypt/\rmdconst.h


SHA1DGST_DEP = d:\work\crypt\crypt\osconfig.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\crypt\sha1locl.h \
	d:\work\crypt\crypt\sha.h \
	d:\work\crypt\crypt/sha.h \
	d:\work\crypt\crypt\md32com.h \
	d:\work\crypt\crypt/md32com.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\crypt/sha1locl.h \
	d:\work\crypt\crypt/\sha.h \
	d:\work\crypt\crypt/\md32com.h


SHA2_DEP = d:\work\crypt\crypt\sha2.h \
	d:\work\crypt\crypt\aes_tdef.h \
	d:\work\crypt\crypt/aes_tdef.h \
	d:\work\crypt\crypt\aes_edef.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\vcfix\sys/endian.h \
	d:\vcfix\machine/endian.h \
	d:\work\crypt\crypt/sha2.h \
	d:\work\crypt\crypt/\aes_tdef.h \
	d:\work\crypt\crypt/aes_edef.h


SYSTEM_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\device\capabil.h \
	d:\work\crypt\device\device.h \
	d:\work\crypt\device/capabil.h \
	d:\work\crypt\device/device.h


CMS_DENV_DEP = d:\work\crypt\envelope\envelope.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\zlib/zlib.h \
	d:\work\crypt\zlib/\zconf.h \
	d:\work\crypt\zlib/zconf.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\misc/asn1_ext.h \
	d:\work\crypt\envelope/envelope.h


CMS_ENV_DEP = d:\work\crypt\envelope\envelope.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\zlib/zlib.h \
	d:\work\crypt\zlib/\zconf.h \
	d:\work\crypt\zlib/zconf.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\misc/asn1_ext.h \
	d:\work\crypt\envelope/envelope.h


DECODE_DEP = d:\work\crypt\envelope\envelope.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\zlib/zlib.h \
	d:\work\crypt\zlib/\zconf.h \
	d:\work\crypt\zlib/zconf.h \
	d:\work\crypt\envelope\pgp.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\misc/misc_rw.h \
	d:\work\crypt\envelope/envelope.h \
	d:\work\crypt\envelope/pgp.h


ENCODE_DEP = d:\work\crypt\envelope\envelope.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\zlib/zlib.h \
	d:\work\crypt\zlib/\zconf.h \
	d:\work\crypt\zlib/zconf.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\envelope/envelope.h


PGP_DENV_DEP = d:\work\crypt\envelope\envelope.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\zlib/zlib.h \
	d:\work\crypt\zlib/\zconf.h \
	d:\work\crypt\zlib/zconf.h \
	d:\work\crypt\envelope\pgp.h \
	d:\work\crypt\misc/misc_rw.h \
	d:\work\crypt\envelope/envelope.h \
	d:\work\crypt\envelope/pgp.h


PGP_ENV_DEP = d:\work\crypt\envelope\envelope.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\zlib/zlib.h \
	d:\work\crypt\zlib/\zconf.h \
	d:\work\crypt\zlib/zconf.h \
	d:\work\crypt\envelope\pgp.h \
	d:\work\crypt\misc/misc_rw.h \
	d:\work\crypt\envelope/envelope.h \
	d:\work\crypt\envelope/pgp.h


PGP_MISC_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\envelope\pgp.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\misc/misc_rw.h \
	d:\work\crypt\envelope/pgp.h


RES_DENV_DEP = d:\work\crypt\envelope\envelope.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\zlib/zlib.h \
	d:\work\crypt\zlib/\zconf.h \
	d:\work\crypt\zlib/zconf.h \
	d:\work\crypt\envelope\pgp.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\misc/asn1_ext.h \
	d:\work\crypt\envelope/envelope.h \
	d:\work\crypt\envelope/pgp.h


RES_ENV_DEP = d:\work\crypt\envelope\envelope.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\zlib/zlib.h \
	d:\work\crypt\zlib/\zconf.h \
	d:\work\crypt\zlib/zconf.h \
	d:\work\crypt\envelope\pgp.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\misc/asn1_ext.h \
	d:\work\crypt\envelope/envelope.h \
	d:\work\crypt\envelope/pgp.h


DNS_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\io\stream.h \
	d:\work\crypt\io\tcp.h \
	d:\vcfix\sys/socket.h \
	d:\vcfix\netinet/in.h \
	d:\vcfix\arpa/inet.h \
	d:\vcfix\arpa/nameser.h \
	d:\vcfix\netinet/tcp.h \
	d:\vcfix\sys/select.h \
	c:\msvc\include\winsock.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\io/tcp.h


FILE_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\io\stream.h \
	d:\work\crypt\io\file.h \
	d:\vcfix\sys/file.h \
	d:\vcfix\sys/mode.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\io/file.h


HTTP_RD_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\io\http.h \
	d:\work\crypt\io\stream.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\misc/misc_rw.h \
	d:\work\crypt\io/http.h \
	d:\work\crypt\io/\stream.h


HTTP_WR_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\io\http.h \
	d:\work\crypt\io\stream.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\io/http.h \
	d:\work\crypt\io/\stream.h


MEMORY_DEP = d:\work\crypt\io\stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\io/stream.h


NET_DEP = d:\work\crypt\io\stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\io/stream.h


STREAM_DEP = d:\work\crypt\io\stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\io/stream.h


TCP_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\io\stream.h \
	d:\work\crypt\io\tcp.h \
	d:\vcfix\sys/socket.h \
	d:\vcfix\netinet/in.h \
	d:\vcfix\arpa/inet.h \
	d:\vcfix\arpa/nameser.h \
	d:\vcfix\netinet/tcp.h \
	d:\vcfix\sys/select.h \
	c:\msvc\include\winsock.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\io/tcp.h


ATTR_ACL_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\kernel\acl.h \
	d:\work\crypt\kernel\kernel.h \
	d:\work\crypt\kernel\thread.h \
	d:\vcfix\kernel/os.h \
	d:\vcfix\exec/chexec.h \
	d:\vcfix\cyg/hal/hal_arch.h \
	d:\vcfix\cyg/kernel/kapi.h \
	d:\vcfix\sys/timer.h \
	d:\work\crypt\kernel/thread.h \
	d:\work\crypt\kernel/acl.h \
	d:\work\crypt\kernel/kernel.h \
	d:\work\crypt\kernel/\thread.h


CERTM_~1_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\kernel\acl.h \
	d:\work\crypt\kernel\kernel.h \
	d:\work\crypt\kernel\thread.h \
	d:\vcfix\kernel/os.h \
	d:\vcfix\exec/chexec.h \
	d:\vcfix\cyg/hal/hal_arch.h \
	d:\vcfix\cyg/kernel/kapi.h \
	d:\vcfix\sys/timer.h \
	d:\work\crypt\kernel/thread.h \
	d:\work\crypt\kernel/acl.h \
	d:\work\crypt\kernel/kernel.h \
	d:\work\crypt\kernel/\thread.h


INIT_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\kernel\acl.h \
	d:\work\crypt\kernel\kernel.h \
	d:\work\crypt\kernel\thread.h \
	d:\vcfix\kernel/os.h \
	d:\vcfix\exec/chexec.h \
	d:\vcfix\cyg/hal/hal_arch.h \
	d:\vcfix\cyg/kernel/kapi.h \
	d:\vcfix\sys/timer.h \
	d:\work\crypt\kernel/thread.h \
	d:\work\crypt\kernel/acl.h \
	d:\work\crypt\kernel/kernel.h \
	d:\work\crypt\kernel/\thread.h \
	d:\work\crypt\crypt/des.h \
	d:\work\crypt\crypt/testdes.h


INT_MSG_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\kernel\acl.h \
	d:\work\crypt\kernel\kernel.h \
	d:\work\crypt\kernel\thread.h \
	d:\vcfix\kernel/os.h \
	d:\vcfix\exec/chexec.h \
	d:\vcfix\cyg/hal/hal_arch.h \
	d:\vcfix\cyg/kernel/kapi.h \
	d:\vcfix\sys/timer.h \
	d:\work\crypt\kernel/thread.h \
	d:\work\crypt\kernel/acl.h \
	d:\work\crypt\kernel/kernel.h \
	d:\work\crypt\kernel/\thread.h


KEY_ACL_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\kernel\acl.h \
	d:\work\crypt\kernel\kernel.h \
	d:\work\crypt\kernel\thread.h \
	d:\vcfix\kernel/os.h \
	d:\vcfix\exec/chexec.h \
	d:\vcfix\cyg/hal/hal_arch.h \
	d:\vcfix\cyg/kernel/kapi.h \
	d:\vcfix\sys/timer.h \
	d:\work\crypt\kernel/thread.h \
	d:\work\crypt\kernel/acl.h \
	d:\work\crypt\kernel/kernel.h \
	d:\work\crypt\kernel/\thread.h


MECH_ACL_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\kernel\acl.h \
	d:\work\crypt\kernel\kernel.h \
	d:\work\crypt\kernel\thread.h \
	d:\vcfix\kernel/os.h \
	d:\vcfix\exec/chexec.h \
	d:\vcfix\cyg/hal/hal_arch.h \
	d:\vcfix\cyg/kernel/kapi.h \
	d:\vcfix\sys/timer.h \
	d:\work\crypt\kernel/thread.h \
	d:\work\crypt\kernel/acl.h \
	d:\work\crypt\kernel/kernel.h \
	d:\work\crypt\kernel/\thread.h


MSG_ACL_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\kernel\acl.h \
	d:\work\crypt\kernel\kernel.h \
	d:\work\crypt\kernel\thread.h \
	d:\vcfix\kernel/os.h \
	d:\vcfix\exec/chexec.h \
	d:\vcfix\cyg/hal/hal_arch.h \
	d:\vcfix\cyg/kernel/kapi.h \
	d:\vcfix\sys/timer.h \
	d:\work\crypt\kernel/thread.h \
	d:\work\crypt\kernel/acl.h \
	d:\work\crypt\kernel/kernel.h \
	d:\work\crypt\kernel/\thread.h


OBJ_ACC_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\kernel\acl.h \
	d:\work\crypt\kernel\kernel.h \
	d:\work\crypt\kernel\thread.h \
	d:\vcfix\kernel/os.h \
	d:\vcfix\exec/chexec.h \
	d:\vcfix\cyg/hal/hal_arch.h \
	d:\vcfix\cyg/kernel/kapi.h \
	d:\vcfix\sys/timer.h \
	d:\work\crypt\kernel/thread.h \
	d:\work\crypt\kernel/acl.h \
	d:\work\crypt\kernel/kernel.h \
	d:\work\crypt\kernel/\thread.h \
	d:\work\crypt\context/context.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\device/capabil.h


OBJECTS_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\kernel\acl.h \
	d:\work\crypt\kernel\kernel.h \
	d:\work\crypt\kernel\thread.h \
	d:\vcfix\kernel/os.h \
	d:\vcfix\exec/chexec.h \
	d:\vcfix\cyg/hal/hal_arch.h \
	d:\vcfix\cyg/kernel/kapi.h \
	d:\vcfix\sys/timer.h \
	d:\work\crypt\kernel/thread.h \
	d:\work\crypt\kernel/acl.h \
	d:\work\crypt\kernel/kernel.h \
	d:\work\crypt\kernel/\thread.h


SEC_MEM_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\kernel\acl.h \
	d:\work\crypt\kernel\kernel.h \
	d:\work\crypt\kernel\thread.h \
	d:\vcfix\kernel/os.h \
	d:\vcfix\exec/chexec.h \
	d:\vcfix\cyg/hal/hal_arch.h \
	d:\vcfix\cyg/kernel/kapi.h \
	d:\vcfix\sys/timer.h \
	d:\work\crypt\kernel/thread.h \
	d:\work\crypt\kernel/acl.h \
	d:\work\crypt\kernel/kernel.h \
	d:\work\crypt\kernel/\thread.h \
	d:\vcfix\sys/mman.h \
	d:\vcfix\mem/chmem.h


SEMAPH~1_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\kernel\acl.h \
	d:\work\crypt\kernel\kernel.h \
	d:\work\crypt\kernel\thread.h \
	d:\vcfix\kernel/os.h \
	d:\vcfix\exec/chexec.h \
	d:\vcfix\cyg/hal/hal_arch.h \
	d:\vcfix\cyg/kernel/kapi.h \
	d:\vcfix\sys/timer.h \
	d:\work\crypt\kernel/thread.h \
	d:\work\crypt\kernel/acl.h \
	d:\work\crypt\kernel/kernel.h \
	d:\work\crypt\kernel/\thread.h


SENDMSG_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\kernel\acl.h \
	d:\work\crypt\kernel\kernel.h \
	d:\work\crypt\kernel\thread.h \
	d:\vcfix\kernel/os.h \
	d:\vcfix\exec/chexec.h \
	d:\vcfix\cyg/hal/hal_arch.h \
	d:\vcfix\cyg/kernel/kapi.h \
	d:\vcfix\sys/timer.h \
	d:\work\crypt\kernel/thread.h \
	d:\work\crypt\kernel/acl.h \
	d:\work\crypt\kernel/kernel.h \
	d:\work\crypt\kernel/\thread.h


HTTP_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\keyset\keyset.h \
	d:\vcfix\mfc/sqltypes.h \
	d:\work\crypt\io/stream.h \
	d:\vcfix\sys/param.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\keyset/keyset.h


PGP_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\keyset\keyset.h \
	d:\vcfix\mfc/sqltypes.h \
	d:\work\crypt\io/stream.h \
	d:\vcfix\sys/param.h \
	d:\work\crypt\envelope/pgp.h \
	d:\work\crypt\misc/misc_rw.h \
	d:\work\crypt\keyset/keyset.h


PKCS15_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\keyset\keyset.h \
	d:\vcfix\mfc/sqltypes.h \
	d:\work\crypt\io/stream.h \
	d:\vcfix\sys/param.h \
	d:\work\crypt\keyset\pkcs15.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\misc/asn1_ext.h \
	d:\work\crypt\keyset/keyset.h \
	d:\work\crypt\keyset/pkcs15.h


PKCS15~1_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\keyset\keyset.h \
	d:\vcfix\mfc/sqltypes.h \
	d:\work\crypt\io/stream.h \
	d:\vcfix\sys/param.h \
	d:\work\crypt\keyset\pkcs15.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\misc/asn1_ext.h \
	d:\work\crypt\keyset/keyset.h \
	d:\work\crypt\keyset/pkcs15.h


PKCS15~2_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\keyset\keyset.h \
	d:\vcfix\mfc/sqltypes.h \
	d:\work\crypt\io/stream.h \
	d:\vcfix\sys/param.h \
	d:\work\crypt\keyset\pkcs15.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\misc/asn1_ext.h \
	d:\work\crypt\keyset/keyset.h \
	d:\work\crypt\keyset/pkcs15.h


KEYEX_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\mechs\mech.h \
	d:\work\crypt\envelope/pgp.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\misc/asn1_ext.h \
	d:\work\crypt\misc/misc_rw.h \
	d:\work\crypt\mechs/mech.h


KEYEX_RW_DEP = d:\work\crypt\mechs\mech.h \
	d:\work\crypt\envelope/pgp.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\misc/asn1_ext.h \
	d:\work\crypt\misc/misc_rw.h \
	d:\work\crypt\mechs/mech.h


MECH_DRV_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\envelope/pgp.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\misc/asn1.h


MECH_ENC_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\envelope/pgp.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\misc/asn1.h


MECH_SIG_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\misc/asn1_ext.h


MECH_WRP_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\envelope/pgp.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\misc/misc_rw.h


OBJ_QRY_DEP = d:\work\crypt\mechs\mech.h \
	d:\work\crypt\envelope/pgp.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\misc/asn1_ext.h \
	d:\work\crypt\misc/misc_rw.h \
	d:\work\crypt\mechs/mech.h


SIGN_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\mechs\mech.h \
	d:\work\crypt\envelope/pgp.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\misc/asn1_ext.h \
	d:\work\crypt\misc/misc_rw.h \
	d:\work\crypt\mechs/mech.h


SIGN_RW_DEP = d:\work\crypt\mechs\mech.h \
	d:\work\crypt\envelope/pgp.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\misc/asn1_ext.h \
	d:\work\crypt\misc/misc_rw.h \
	d:\work\crypt\mechs/mech.h


ASN1_CHK_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\misc\asn1.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\misc/asn1.h


ASN1_EXT_DEP = d:\work\crypt\misc\asn1.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\misc\asn1_ext.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\misc/asn1_ext.h


ASN1_RD_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\misc\asn1.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\misc/asn1.h


ASN1_WR_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\misc\asn1.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\misc/asn1.h


BASE64_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\io/stream.h


INT_API_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\crypt/md2.h \
	d:\work\crypt\crypt/md5.h \
	d:\work\crypt\crypt/ripemd.h \
	d:\work\crypt\crypt/sha.h \
	d:\work\crypt\crypt/sha2.h \
	d:\work\crypt\crypt/\aes_tdef.h \
	d:\work\crypt\crypt/aes_tdef.h \
	d:\work\crypt\io/stream.h


INT_ATTR_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h


INT_ENV_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h


MISC_RW_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\misc\misc_rw.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\misc/misc_rw.h \
	d:\work\crypt\envelope/pgp.h


OS_SPEC_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h


RANDOM_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\crypt/des.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\crypt/testdes.h


WIN16_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h


CRYPTAPI_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\misc/rpc.h


CRYPTCFG_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\cert/trustmgr.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\io/stream.h


CRYPTCRT_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\cert/cert.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\misc/asn1_ext.h


CRYPTCTX_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\context/context.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\bn/bn.h \
	d:\work\crypt\crypt/osconfig.h \
	d:\work\crypt\device/capabil.h \
	d:\work\crypt\misc/asn1.h


CRYPTDEV_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\device/device.h


CRYPTENV_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\envelope/envelope.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\zlib/zlib.h


CRYPTKEY_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\keyset/keyset.h \
	d:\vcfix\mfc/sqltypes.h \
	d:\work\crypt\io/stream.h \
	d:\vcfix\sys/param.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\misc/asn1_ext.h \
	d:\work\crypt\envelope/pgp.h \
	d:\work\crypt\misc/misc_rw.h


CRYPTLIB_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h


CRYPTSES_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\session/session.h


CRYPTUSR_DEP = d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\work\crypt\cert/trustmgr.h \
	d:\work\crypt\misc/asn1.h \
	d:\work\crypt\io/stream.h \
	d:\work\crypt\misc/asn1_ext.h


ACE_MO~1_DEP = d:\work\crypt\crypt/aesopt.h \
	d:\work\crypt\crypt/\aes.h \
	d:\work\crypt\crypt/\aes_tdef.h \
	d:\work\crypt\crypt/aes_tdef.h \
	d:\work\crypt\crypt/\aes_edef.h \
	d:\work\crypt\crypt.h \
	d:\work\crypt\cryptlib.h \
	d:\work\crypt\misc/os_spec.h \
	d:\work\crypt\misc/\config.h \
	d:\work\crypt\misc/config.h \
	d:\work\crypt\cryptkrn.h \
	d:\work\crypt\misc/consts.h \
	d:\work\crypt\misc/int_api.h \
	d:\vcfix\sys/endian.h \
	d:\vcfix\machine/endian.h \
	d:\work\crypt\crypt/aes.h \
	d:\work\crypt\crypt/aes_edef.h \
	d:\work\crypt\crypt/via_ace.h


all:	$(PROJ).DLL

CRYPT.RES:	CRYPT.RC $(CRYPT_RCDEP)
	$(RC) $(RCFLAGS) $(RCDEFINES) -r CRYPT.RC

BN_ADD.OBJ:	BN\BN_ADD.C $(BN_ADD_DEP)
	$(CC) $(CFLAGS) $(CCREATEPCHFLAG) /c BN\BN_ADD.C

BN_ASM.OBJ:	BN\BN_ASM.C $(BN_ASM_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c BN\BN_ASM.C

BN_CTX.OBJ:	BN\BN_CTX.C $(BN_CTX_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c BN\BN_CTX.C

BN_DIV.OBJ:	BN\BN_DIV.C $(BN_DIV_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c BN\BN_DIV.C

BN_EXP.OBJ:	BN\BN_EXP.C $(BN_EXP_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c BN\BN_EXP.C

BN_EXP2.OBJ:	BN\BN_EXP2.C $(BN_EXP2_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c BN\BN_EXP2.C

BN_GCD.OBJ:	BN\BN_GCD.C $(BN_GCD_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c BN\BN_GCD.C

BN_LIB.OBJ:	BN\BN_LIB.C $(BN_LIB_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c BN\BN_LIB.C

BN_MOD.OBJ:	BN\BN_MOD.C $(BN_MOD_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c BN\BN_MOD.C

BN_MONT.OBJ:	BN\BN_MONT.C $(BN_MONT_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c BN\BN_MONT.C

BN_MUL.OBJ:	BN\BN_MUL.C $(BN_MUL_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c BN\BN_MUL.C

BN_RECP.OBJ:	BN\BN_RECP.C $(BN_RECP_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c BN\BN_RECP.C

BN_SHIFT.OBJ:	BN\BN_SHIFT.C $(BN_SHIFT_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c BN\BN_SHIFT.C

BN_SQR.OBJ:	BN\BN_SQR.C $(BN_SQR_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c BN\BN_SQR.C

BN_WORD.OBJ:	BN\BN_WORD.C $(BN_WORD_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c BN\BN_WORD.C

CERTREV.OBJ:	CERT\CERTREV.C $(CERTREV_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CERT\CERTREV.C

CERTSIGN.OBJ:	CERT\CERTSIGN.C $(CERTSIGN_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CERT\CERTSIGN.C

CERTVAL.OBJ:	CERT\CERTVAL.C $(CERTVAL_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CERT\CERTVAL.C

CHAIN.OBJ:	CERT\CHAIN.C $(CHAIN_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CERT\CHAIN.C

CHK_CERT.OBJ:	CERT\CHK_CERT.C $(CHK_CERT_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CERT\CHK_CERT.C

CHK_CHN.OBJ:	CERT\CHK_CHN.C $(CHK_CHN_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CERT\CHK_CHN.C

CHK_USE.OBJ:	CERT\CHK_USE.C $(CHK_USE_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CERT\CHK_USE.C

COMP_GET.OBJ:	CERT\COMP_GET.C $(COMP_GET_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CERT\COMP_GET.C

COMP_SET.OBJ:	CERT\COMP_SET.C $(COMP_SET_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CERT\COMP_SET.C

DN.OBJ:	CERT\DN.C $(DN_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CERT\DN.C

DNSTRING.OBJ:	CERT\DNSTRING.C $(DNSTRING_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CERT\DNSTRING.C

EXT.OBJ:	CERT\EXT.C $(EXT_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CERT\EXT.C

EXT_ADD.OBJ:	CERT\EXT_ADD.C $(EXT_ADD_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CERT\EXT_ADD.C

EXT_CHK.OBJ:	CERT\EXT_CHK.C $(EXT_CHK_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CERT\EXT_CHK.C

EXT_COPY.OBJ:	CERT\EXT_COPY.C $(EXT_COPY_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CERT\EXT_COPY.C

EXT_DEF.OBJ:	CERT\EXT_DEF.C $(EXT_DEF_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CERT\EXT_DEF.C

EXT_RD.OBJ:	CERT\EXT_RD.C $(EXT_RD_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CERT\EXT_RD.C

EXT_WR.OBJ:	CERT\EXT_WR.C $(EXT_WR_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CERT\EXT_WR.C

IMP_EXP.OBJ:	CERT\IMP_EXP.C $(IMP_EXP_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CERT\IMP_EXP.C

TRUSTMGR.OBJ:	CERT\TRUSTMGR.C $(TRUSTMGR_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CERT\TRUSTMGR.C

READ.OBJ:	CERT\READ.C $(READ_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CERT\READ.C

WRITE.OBJ:	CERT\WRITE.C $(WRITE_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CERT\WRITE.C

CTX_3DES.OBJ:	CONTEXT\CTX_3DES.C $(CTX_3DES_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CONTEXT\CTX_3DES.C

CTX_AES.OBJ:	CONTEXT\CTX_AES.C $(CTX_AES_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CONTEXT\CTX_AES.C

CTX_BF.OBJ:	CONTEXT\CTX_BF.C $(CTX_BF_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CONTEXT\CTX_BF.C

CTX_CAST.OBJ:	CONTEXT\CTX_CAST.C $(CTX_CAST_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CONTEXT\CTX_CAST.C

CTX_DES.OBJ:	CONTEXT\CTX_DES.C $(CTX_DES_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CONTEXT\CTX_DES.C

CTX_DH.OBJ:	CONTEXT\CTX_DH.C $(CTX_DH_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CONTEXT\CTX_DH.C

CTX_DSA.OBJ:	CONTEXT\CTX_DSA.C $(CTX_DSA_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CONTEXT\CTX_DSA.C

CTX_ELG.OBJ:	CONTEXT\CTX_ELG.C $(CTX_ELG_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CONTEXT\CTX_ELG.C

CTX_HMD5.OBJ:	CONTEXT\CTX_HMD5.C $(CTX_HMD5_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CONTEXT\CTX_HMD5.C

CTX_HRMD.OBJ:	CONTEXT\CTX_HRMD.C $(CTX_HRMD_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CONTEXT\CTX_HRMD.C

CTX_HSHA.OBJ:	CONTEXT\CTX_HSHA.C $(CTX_HSHA_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CONTEXT\CTX_HSHA.C

CTX_MD5.OBJ:	CONTEXT\CTX_MD5.C $(CTX_MD5_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CONTEXT\CTX_MD5.C

CTX_MISC.OBJ:	CONTEXT\CTX_MISC.C $(CTX_MISC_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CONTEXT\CTX_MISC.C

CTX_RIPE.OBJ:	CONTEXT\CTX_RIPE.C $(CTX_RIPE_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CONTEXT\CTX_RIPE.C

CTX_RSA.OBJ:	CONTEXT\CTX_RSA.C $(CTX_RSA_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CONTEXT\CTX_RSA.C

CTX_SHA.OBJ:	CONTEXT\CTX_SHA.C $(CTX_SHA_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CONTEXT\CTX_SHA.C

CTX_SHA2.OBJ:	CONTEXT\CTX_SHA2.C $(CTX_SHA2_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CONTEXT\CTX_SHA2.C

KEY_RD.OBJ:	CONTEXT\KEY_RD.C $(KEY_RD_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CONTEXT\KEY_RD.C

KEY_WR.OBJ:	CONTEXT\KEY_WR.C $(KEY_WR_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CONTEXT\KEY_WR.C

KEYLOAD.OBJ:	CONTEXT\KEYLOAD.C $(KEYLOAD_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CONTEXT\KEYLOAD.C

KG_DLP.OBJ:	CONTEXT\KG_DLP.C $(KG_DLP_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CONTEXT\KG_DLP.C

KG_PRIME.OBJ:	CONTEXT\KG_PRIME.C $(KG_PRIME_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CONTEXT\KG_PRIME.C

KG_RSA.OBJ:	CONTEXT\KG_RSA.C $(KG_RSA_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CONTEXT\KG_RSA.C

AESCRYPT.OBJ:	CRYPT\AESCRYPT.C $(AESCRYPT_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CRYPT\AESCRYPT.C

AESKEY.OBJ:	CRYPT\AESKEY.C $(AESKEY_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CRYPT\AESKEY.C

AESTAB.OBJ:	CRYPT\AESTAB.C $(AESTAB_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CRYPT\AESTAB.C

BFECB.OBJ:	CRYPT\BFECB.C $(BFECB_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CRYPT\BFECB.C

BFENC.OBJ:	CRYPT\BFENC.C $(BFENC_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CRYPT\BFENC.C

BFSKEY.OBJ:	CRYPT\BFSKEY.C $(BFSKEY_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CRYPT\BFSKEY.C

CASTECB.OBJ:	CRYPT\CASTECB.C $(CASTECB_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CRYPT\CASTECB.C

CASTENC.OBJ:	CRYPT\CASTENC.C $(CASTENC_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CRYPT\CASTENC.C

CASTSKEY.OBJ:	CRYPT\CASTSKEY.C $(CASTSKEY_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CRYPT\CASTSKEY.C

DESCBC.OBJ:	CRYPT\DESCBC.C $(DESCBC_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CRYPT\DESCBC.C

DESECB.OBJ:	CRYPT\DESECB.C $(DESECB_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CRYPT\DESECB.C

DESECB3.OBJ:	CRYPT\DESECB3.C $(DESECB3_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CRYPT\DESECB3.C

DESENC.OBJ:	CRYPT\DESENC.C $(DESENC_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CRYPT\DESENC.C

DESSKEY.OBJ:	CRYPT\DESSKEY.C $(DESSKEY_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CRYPT\DESSKEY.C

MD5DGST.OBJ:	CRYPT\MD5DGST.C $(MD5DGST_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CRYPT\MD5DGST.C

RMDDGST.OBJ:	CRYPT\RMDDGST.C $(RMDDGST_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CRYPT\RMDDGST.C

SHA1DGST.OBJ:	CRYPT\SHA1DGST.C $(SHA1DGST_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CRYPT\SHA1DGST.C

SHA2.OBJ:	CRYPT\SHA2.C $(SHA2_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CRYPT\SHA2.C

SYSTEM.OBJ:	DEVICE\SYSTEM.C $(SYSTEM_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c DEVICE\SYSTEM.C

CMS_DENV.OBJ:	ENVELOPE\CMS_DENV.C $(CMS_DENV_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c ENVELOPE\CMS_DENV.C

CMS_ENV.OBJ:	ENVELOPE\CMS_ENV.C $(CMS_ENV_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c ENVELOPE\CMS_ENV.C

DECODE.OBJ:	ENVELOPE\DECODE.C $(DECODE_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c ENVELOPE\DECODE.C

ENCODE.OBJ:	ENVELOPE\ENCODE.C $(ENCODE_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c ENVELOPE\ENCODE.C

PGP_DENV.OBJ:	ENVELOPE\PGP_DENV.C $(PGP_DENV_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c ENVELOPE\PGP_DENV.C

PGP_ENV.OBJ:	ENVELOPE\PGP_ENV.C $(PGP_ENV_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c ENVELOPE\PGP_ENV.C

PGP_MISC.OBJ:	ENVELOPE\PGP_MISC.C $(PGP_MISC_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c ENVELOPE\PGP_MISC.C

RES_DENV.OBJ:	ENVELOPE\RES_DENV.C $(RES_DENV_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c ENVELOPE\RES_DENV.C

RES_ENV.OBJ:	ENVELOPE\RES_ENV.C $(RES_ENV_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c ENVELOPE\RES_ENV.C

DNS.OBJ:	IO\DNS.C $(DNS_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c IO\DNS.C

FILE.OBJ:	IO\FILE.C $(FILE_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c IO\FILE.C

HTTP_RD.OBJ:	IO\HTTP_RD.C $(HTTP_RD_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c IO\HTTP_RD.C

HTTP_WR.OBJ:	IO\HTTP_WR.C $(HTTP_WR_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c IO\HTTP_WR.C

MEMORY.OBJ:	IO\MEMORY.C $(MEMORY_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c IO\MEMORY.C

NET.OBJ:	IO\NET.C $(NET_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c IO\NET.C

STREAM.OBJ:	IO\STREAM.C $(STREAM_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c IO\STREAM.C

TCP.OBJ:	IO\TCP.C $(TCP_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c IO\TCP.C

ATTR_ACL.OBJ:	KERNEL\ATTR_ACL.C $(ATTR_ACL_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c KERNEL\ATTR_ACL.C

CERTM_~1.OBJ:	KERNEL\CERTM_~1.C $(CERTM_~1_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c KERNEL\CERTM_~1.C

INIT.OBJ:	KERNEL\INIT.C $(INIT_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c KERNEL\INIT.C

INT_MSG.OBJ:	KERNEL\INT_MSG.C $(INT_MSG_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c KERNEL\INT_MSG.C

KEY_ACL.OBJ:	KERNEL\KEY_ACL.C $(KEY_ACL_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c KERNEL\KEY_ACL.C

MECH_ACL.OBJ:	KERNEL\MECH_ACL.C $(MECH_ACL_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c KERNEL\MECH_ACL.C

MSG_ACL.OBJ:	KERNEL\MSG_ACL.C $(MSG_ACL_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c KERNEL\MSG_ACL.C

OBJ_ACC.OBJ:	KERNEL\OBJ_ACC.C $(OBJ_ACC_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c KERNEL\OBJ_ACC.C

OBJECTS.OBJ:	KERNEL\OBJECTS.C $(OBJECTS_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c KERNEL\OBJECTS.C

SEC_MEM.OBJ:	KERNEL\SEC_MEM.C $(SEC_MEM_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c KERNEL\SEC_MEM.C

SEMAPH~1.OBJ:	KERNEL\SEMAPH~1.C $(SEMAPH~1_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c KERNEL\SEMAPH~1.C

SENDMSG.OBJ:	KERNEL\SENDMSG.C $(SENDMSG_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c KERNEL\SENDMSG.C

HTTP.OBJ:	KEYSET\HTTP.C $(HTTP_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c KEYSET\HTTP.C

PGP.OBJ:	KEYSET\PGP.C $(PGP_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c KEYSET\PGP.C

PKCS15.OBJ:	KEYSET\PKCS15.C $(PKCS15_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c KEYSET\PKCS15.C

PKCS15~1.OBJ:	KEYSET\PKCS15~1.C $(PKCS15~1_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c KEYSET\PKCS15~1.C

PKCS15~2.OBJ:	KEYSET\PKCS15~2.C $(PKCS15~2_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c KEYSET\PKCS15~2.C

KEYEX.OBJ:	MECHS\KEYEX.C $(KEYEX_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c MECHS\KEYEX.C

KEYEX_RW.OBJ:	MECHS\KEYEX_RW.C $(KEYEX_RW_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c MECHS\KEYEX_RW.C

MECH_DRV.OBJ:	MECHS\MECH_DRV.C $(MECH_DRV_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c MECHS\MECH_DRV.C

MECH_ENC.OBJ:	MECHS\MECH_ENC.C $(MECH_ENC_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c MECHS\MECH_ENC.C

MECH_SIG.OBJ:	MECHS\MECH_SIG.C $(MECH_SIG_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c MECHS\MECH_SIG.C

MECH_WRP.OBJ:	MECHS\MECH_WRP.C $(MECH_WRP_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c MECHS\MECH_WRP.C

OBJ_QRY.OBJ:	MECHS\OBJ_QRY.C $(OBJ_QRY_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c MECHS\OBJ_QRY.C

SIGN.OBJ:	MECHS\SIGN.C $(SIGN_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c MECHS\SIGN.C

SIGN_RW.OBJ:	MECHS\SIGN_RW.C $(SIGN_RW_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c MECHS\SIGN_RW.C

ASN1_CHK.OBJ:	MISC\ASN1_CHK.C $(ASN1_CHK_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c MISC\ASN1_CHK.C

ASN1_EXT.OBJ:	MISC\ASN1_EXT.C $(ASN1_EXT_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c MISC\ASN1_EXT.C

ASN1_RD.OBJ:	MISC\ASN1_RD.C $(ASN1_RD_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c MISC\ASN1_RD.C

ASN1_WR.OBJ:	MISC\ASN1_WR.C $(ASN1_WR_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c MISC\ASN1_WR.C

BASE64.OBJ:	MISC\BASE64.C $(BASE64_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c MISC\BASE64.C

INT_API.OBJ:	MISC\INT_API.C $(INT_API_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c MISC\INT_API.C

INT_ATTR.OBJ:	MISC\INT_ATTR.C $(INT_ATTR_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c MISC\INT_ATTR.C

INT_ENV.OBJ:	MISC\INT_ENV.C $(INT_ENV_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c MISC\INT_ENV.C

MISC_RW.OBJ:	MISC\MISC_RW.C $(MISC_RW_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c MISC\MISC_RW.C

OS_SPEC.OBJ:	MISC\OS_SPEC.C $(OS_SPEC_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c MISC\OS_SPEC.C

RANDOM.OBJ:	RANDOM\RANDOM.C $(RANDOM_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c RANDOM\RANDOM.C

WIN16.OBJ:	RANDOM\WIN16.C $(WIN16_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c RANDOM\WIN16.C

CRYPTAPI.OBJ:	CRYPTAPI.C $(CRYPTAPI_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CRYPTAPI.C

CRYPTCFG.OBJ:	CRYPTCFG.C $(CRYPTCFG_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CRYPTCFG.C

CRYPTCRT.OBJ:	CRYPTCRT.C $(CRYPTCRT_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CRYPTCRT.C

CRYPTCTX.OBJ:	CRYPTCTX.C $(CRYPTCTX_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CRYPTCTX.C

CRYPTDEV.OBJ:	CRYPTDEV.C $(CRYPTDEV_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CRYPTDEV.C

CRYPTENV.OBJ:	CRYPTENV.C $(CRYPTENV_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CRYPTENV.C

CRYPTKEY.OBJ:	CRYPTKEY.C $(CRYPTKEY_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CRYPTKEY.C

CRYPTLIB.OBJ:	CRYPTLIB.C $(CRYPTLIB_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CRYPTLIB.C

CRYPTSES.OBJ:	CRYPTSES.C $(CRYPTSES_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CRYPTSES.C

CRYPTUSR.OBJ:	CRYPTUSR.C $(CRYPTUSR_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CRYPTUSR.C

ACE_MO~1.OBJ:	CRYPT\ACE_MO~1.C $(ACE_MO~1_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c CRYPT\ACE_MO~1.C


$(PROJ).DLL::	CRYPT.RES

$(PROJ).DLL::	BN_ADD.OBJ BN_ASM.OBJ BN_CTX.OBJ BN_DIV.OBJ BN_EXP.OBJ BN_EXP2.OBJ \
	BN_GCD.OBJ BN_LIB.OBJ BN_MOD.OBJ BN_MONT.OBJ BN_MUL.OBJ BN_RECP.OBJ BN_SHIFT.OBJ \
	BN_SQR.OBJ BN_WORD.OBJ CERTREV.OBJ CERTSIGN.OBJ CERTVAL.OBJ CHAIN.OBJ CHK_CERT.OBJ \
	CHK_CHN.OBJ CHK_USE.OBJ COMP_GET.OBJ COMP_SET.OBJ DN.OBJ DNSTRING.OBJ EXT.OBJ EXT_ADD.OBJ \
	EXT_CHK.OBJ EXT_COPY.OBJ EXT_DEF.OBJ EXT_RD.OBJ EXT_WR.OBJ IMP_EXP.OBJ TRUSTMGR.OBJ \
	READ.OBJ WRITE.OBJ CTX_3DES.OBJ CTX_AES.OBJ CTX_BF.OBJ CTX_CAST.OBJ CTX_DES.OBJ CTX_DH.OBJ \
	CTX_DSA.OBJ CTX_ELG.OBJ CTX_HMD5.OBJ CTX_HRMD.OBJ CTX_HSHA.OBJ CTX_MD5.OBJ CTX_MISC.OBJ \
	CTX_RIPE.OBJ CTX_RSA.OBJ CTX_SHA.OBJ CTX_SHA2.OBJ KEY_RD.OBJ KEY_WR.OBJ KEYLOAD.OBJ \
	KG_DLP.OBJ KG_PRIME.OBJ KG_RSA.OBJ AESCRYPT.OBJ AESKEY.OBJ AESTAB.OBJ BFECB.OBJ BFENC.OBJ \
	BFSKEY.OBJ CASTECB.OBJ CASTENC.OBJ CASTSKEY.OBJ DESCBC.OBJ DESECB.OBJ DESECB3.OBJ \
	DESENC.OBJ DESSKEY.OBJ MD5DGST.OBJ RMDDGST.OBJ SHA1DGST.OBJ SHA2.OBJ SYSTEM.OBJ CMS_DENV.OBJ \
	CMS_ENV.OBJ DECODE.OBJ ENCODE.OBJ PGP_DENV.OBJ PGP_ENV.OBJ PGP_MISC.OBJ RES_DENV.OBJ \
	RES_ENV.OBJ DNS.OBJ FILE.OBJ HTTP_RD.OBJ HTTP_WR.OBJ MEMORY.OBJ NET.OBJ STREAM.OBJ \
	TCP.OBJ ATTR_ACL.OBJ CERTM_~1.OBJ INIT.OBJ INT_MSG.OBJ KEY_ACL.OBJ MECH_ACL.OBJ MSG_ACL.OBJ \
	OBJ_ACC.OBJ OBJECTS.OBJ SEC_MEM.OBJ SEMAPH~1.OBJ SENDMSG.OBJ HTTP.OBJ PGP.OBJ PKCS15.OBJ \
	PKCS15~1.OBJ PKCS15~2.OBJ KEYEX.OBJ KEYEX_RW.OBJ MECH_DRV.OBJ MECH_ENC.OBJ MECH_SIG.OBJ \
	MECH_WRP.OBJ OBJ_QRY.OBJ SIGN.OBJ SIGN_RW.OBJ ASN1_CHK.OBJ ASN1_EXT.OBJ ASN1_RD.OBJ \
	ASN1_WR.OBJ BASE64.OBJ INT_API.OBJ INT_ATTR.OBJ INT_ENV.OBJ MISC_RW.OBJ OS_SPEC.OBJ \
	RANDOM.OBJ WIN16.OBJ CRYPTAPI.OBJ CRYPTCFG.OBJ CRYPTCRT.OBJ CRYPTCTX.OBJ CRYPTDEV.OBJ \
	CRYPTENV.OBJ CRYPTKEY.OBJ CRYPTLIB.OBJ CRYPTSES.OBJ CRYPTUSR.OBJ ACE_MO~1.OBJ $(OBJS_EXT) $(DEFFILE)
	echo >NUL @<<$(PROJ).CRF
BN_ADD.OBJ +
BN_ASM.OBJ +
BN_CTX.OBJ +
BN_DIV.OBJ +
BN_EXP.OBJ +
BN_EXP2.OBJ +
BN_GCD.OBJ +
BN_LIB.OBJ +
BN_MOD.OBJ +
BN_MONT.OBJ +
BN_MUL.OBJ +
BN_RECP.OBJ +
BN_SHIFT.OBJ +
BN_SQR.OBJ +
BN_WORD.OBJ +
CERTREV.OBJ +
CERTSIGN.OBJ +
CERTVAL.OBJ +
CHAIN.OBJ +
CHK_CERT.OBJ +
CHK_CHN.OBJ +
CHK_USE.OBJ +
COMP_GET.OBJ +
COMP_SET.OBJ +
DN.OBJ +
DNSTRING.OBJ +
EXT.OBJ +
EXT_ADD.OBJ +
EXT_CHK.OBJ +
EXT_COPY.OBJ +
EXT_DEF.OBJ +
EXT_RD.OBJ +
EXT_WR.OBJ +
IMP_EXP.OBJ +
TRUSTMGR.OBJ +
READ.OBJ +
WRITE.OBJ +
CTX_3DES.OBJ +
CTX_AES.OBJ +
CTX_BF.OBJ +
CTX_CAST.OBJ +
CTX_DES.OBJ +
CTX_DH.OBJ +
CTX_DSA.OBJ +
CTX_ELG.OBJ +
CTX_HMD5.OBJ +
CTX_HRMD.OBJ +
CTX_HSHA.OBJ +
CTX_MD5.OBJ +
CTX_MISC.OBJ +
CTX_RIPE.OBJ +
CTX_RSA.OBJ +
CTX_SHA.OBJ +
CTX_SHA2.OBJ +
KEY_RD.OBJ +
KEY_WR.OBJ +
KEYLOAD.OBJ +
KG_DLP.OBJ +
KG_PRIME.OBJ +
KG_RSA.OBJ +
AESCRYPT.OBJ +
AESKEY.OBJ +
AESTAB.OBJ +
BFECB.OBJ +
BFENC.OBJ +
BFSKEY.OBJ +
CASTECB.OBJ +
CASTENC.OBJ +
CASTSKEY.OBJ +
DESCBC.OBJ +
DESECB.OBJ +
DESECB3.OBJ +
DESENC.OBJ +
DESSKEY.OBJ +
MD5DGST.OBJ +
RMDDGST.OBJ +
SHA1DGST.OBJ +
SHA2.OBJ +
SYSTEM.OBJ +
CMS_DENV.OBJ +
CMS_ENV.OBJ +
DECODE.OBJ +
ENCODE.OBJ +
PGP_DENV.OBJ +
PGP_ENV.OBJ +
PGP_MISC.OBJ +
RES_DENV.OBJ +
RES_ENV.OBJ +
DNS.OBJ +
FILE.OBJ +
HTTP_RD.OBJ +
HTTP_WR.OBJ +
MEMORY.OBJ +
NET.OBJ +
STREAM.OBJ +
TCP.OBJ +
ATTR_ACL.OBJ +
CERTM_~1.OBJ +
INIT.OBJ +
INT_MSG.OBJ +
KEY_ACL.OBJ +
MECH_ACL.OBJ +
MSG_ACL.OBJ +
OBJ_ACC.OBJ +
OBJECTS.OBJ +
SEC_MEM.OBJ +
SEMAPH~1.OBJ +
SENDMSG.OBJ +
HTTP.OBJ +
PGP.OBJ +
PKCS15.OBJ +
PKCS15~1.OBJ +
PKCS15~2.OBJ +
KEYEX.OBJ +
KEYEX_RW.OBJ +
MECH_DRV.OBJ +
MECH_ENC.OBJ +
MECH_SIG.OBJ +
MECH_WRP.OBJ +
OBJ_QRY.OBJ +
SIGN.OBJ +
SIGN_RW.OBJ +
ASN1_CHK.OBJ +
ASN1_EXT.OBJ +
ASN1_RD.OBJ +
ASN1_WR.OBJ +
BASE64.OBJ +
INT_API.OBJ +
INT_ATTR.OBJ +
INT_ENV.OBJ +
MISC_RW.OBJ +
OS_SPEC.OBJ +
RANDOM.OBJ +
WIN16.OBJ +
CRYPTAPI.OBJ +
CRYPTCFG.OBJ +
CRYPTCRT.OBJ +
CRYPTCTX.OBJ +
CRYPTDEV.OBJ +
CRYPTENV.OBJ +
CRYPTKEY.OBJ +
CRYPTLIB.OBJ +
CRYPTSES.OBJ +
CRYPTUSR.OBJ +
ACE_MO~1.OBJ +
$(OBJS_EXT)
$(PROJ).DLL
$(MAPFILE)
c:\msvc\lib\+
$(LIBS)
$(DEFFILE);
<<
	link $(LFLAGS) @$(PROJ).CRF
	$(RC) $(RESFLAGS) CRYPT.RES $@
	@copy $(PROJ).CRF MSVC.BND
	implib /nowep $(PROJ).LIB $(PROJ).DLL

$(PROJ).DLL::	CRYPT.RES
	if not exist MSVC.BND 	$(RC) $(RESFLAGS) CRYPT.RES $@

run: $(PROJ).DLL
	$(PROJ) $(RUNFLAGS)


$(PROJ).BSC: $(SBRS)
	bscmake @<<
/o$@ $(SBRS)
<<
