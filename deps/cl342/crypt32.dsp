# Microsoft Developer Studio Project File - Name="Crypt32" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=Crypt32 - Win32 Crosscompile
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "crypt32.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "crypt32.mak" CFG="Crypt32 - Win32 Crosscompile"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Crypt32 - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "Crypt32 - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "Crypt32 - Win32 Purify" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "Crypt32 - Win32 Crosscompile" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Crypt32 - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\Release"
# PROP BASE Intermediate_Dir ".\Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\binaries32_vc6"
# PROP Intermediate_Dir ".\release32_vc6"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
F90=fl32.exe
# ADD BASE F90 /I "Release/"
# ADD F90 /I "Release/"
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MD /W3 /O2 /I ".\\" /D "NDEBUG" /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib advapi32.lib /nologo /subsystem:windows /dll /pdb:none /machine:I386 /out:".\binaries32_vc6/cl32.dll"

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\Debug"
# PROP BASE Intermediate_Dir ".\Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\binaries32_vc6"
# PROP Intermediate_Dir ".\debug32_vc6"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
F90=fl32.exe
# ADD BASE F90 /I "Debug/"
# ADD F90 /I "Debug/"
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MD /W4 /Gm /Zi /Od /I ".\\" /FD /c
# SUBTRACT CPP /Fr /YX /Yc /Yu
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /win32
# SUBTRACT MTL /mktyplib203
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386
# ADD LINK32 kernel32.lib user32.lib advapi32.lib /nologo /subsystem:windows /dll /pdb:none /debug /machine:I386 /out:".\binaries32_vc6/cl32.dll"

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Purify"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Crypt32___Win32_Purify"
# PROP BASE Intermediate_Dir "Crypt32___Win32_Purify"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\binaries32_vc6"
# PROP Intermediate_Dir ".\debug32_purify"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
F90=fl32.exe
# ADD BASE F90 /I "Debug/"
# ADD F90 /I "Debug/"
# ADD BASE CPP /nologo /MD /W4 /Gm /Zi /Od /I ".\\" /D "NO_ASM" /FD /c
# SUBTRACT BASE CPP /Fr /YX
# ADD CPP /nologo /MD /W4 /Gm /Zi /Od /I ".\\" /D "NO_ASM" /FD /c
# SUBTRACT CPP /Fr /YX
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# SUBTRACT BASE MTL /mktyplib203
# ADD MTL /nologo /D "_DEBUG" /win32
# SUBTRACT MTL /mktyplib203
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib advapi32.lib /nologo /subsystem:windows /dll /pdb:none /debug /machine:I386 /out:".\binaries/cl32.dll"
# ADD LINK32 kernel32.lib user32.lib advapi32.lib /nologo /subsystem:windows /dll /pdb:none /debug /machine:I386 /out:".\binaries32_vc6/cl32.dll"

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Crosscompile"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Crypt32___Win32_Crosscompile"
# PROP BASE Intermediate_Dir "Crypt32___Win32_Crosscompile"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Crosscompile"
# PROP Intermediate_Dir "Crosscompile"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
F90=fl32.exe
# ADD BASE F90 /I "Debug/"
# ADD F90 /I "Debug/"
# ADD BASE CPP /nologo /MD /W4 /Gm /Zi /Od /I ".\\" /D "NO_ASM" /FD /c
# SUBTRACT BASE CPP /Fr /YX
# ADD CPP /nologo /MD /W4 /Gm /Zi /Od /I ".\\" /I ".\docs\\" /D "__i386__" /D "NO_ASM" /D "CROSSCOMPILE" /FD /c
# SUBTRACT CPP /Fr /YX
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# SUBTRACT BASE MTL /mktyplib203
# ADD MTL /nologo /D "_DEBUG" /win32
# SUBTRACT MTL /mktyplib203
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib advapi32.lib /nologo /subsystem:windows /dll /pdb:none /debug /machine:I386 /out:".\binaries32_vc6/cl32.dll"
# ADD LINK32 kernel32.lib user32.lib advapi32.lib /nologo /subsystem:windows /dll /pdb:none /debug /machine:I386 /out:".\binaries32_vc6/cl32.dll"

!ENDIF 

# Begin Target

# Name "Crypt32 - Win32 Release"
# Name "Crypt32 - Win32 Debug"
# Name "Crypt32 - Win32 Purify"
# Name "Crypt32 - Win32 Crosscompile"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;hpj;bat;for;f90"
# Begin Group "Bignum library"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\bn\bn_add.c
# End Source File
# Begin Source File

SOURCE=.\bn\bn_asm.c
# End Source File
# Begin Source File

SOURCE=.\bn\bn_ctx.c
# End Source File
# Begin Source File

SOURCE=.\bn\bn_div.c
# End Source File
# Begin Source File

SOURCE=.\bn\bn_exp.c
# End Source File
# Begin Source File

SOURCE=.\bn\bn_exp2.c
# End Source File
# Begin Source File

SOURCE=.\bn\bn_gcd.c
# End Source File
# Begin Source File

SOURCE=.\bn\bn_lib.c
# End Source File
# Begin Source File

SOURCE=.\bn\bn_mod.c
# End Source File
# Begin Source File

SOURCE=.\bn\bn_mont.c
# End Source File
# Begin Source File

SOURCE=.\bn\bn_mul.c
# End Source File
# Begin Source File

SOURCE=.\bn\bn_recp.c
# End Source File
# Begin Source File

SOURCE=.\bn\bn_shift.c
# End Source File
# Begin Source File

SOURCE=.\bn\bn_sqr.c
# End Source File
# Begin Source File

SOURCE=.\bn\bn_word.c
# End Source File
# Begin Source File

SOURCE=.\bn\ec_kron.c
# End Source File
# Begin Source File

SOURCE=.\bn\ec_lib.c
# End Source File
# Begin Source File

SOURCE=.\bn\ec_mult.c
# End Source File
# Begin Source File

SOURCE=.\bn\ec_rand.c
# End Source File
# Begin Source File

SOURCE=.\bn\ec_sqrt.c
# End Source File
# Begin Source File

SOURCE=.\bn\ecp_mont.c
# End Source File
# Begin Source File

SOURCE=.\bn\ecp_smpl.c
# End Source File
# Begin Source File

SOURCE=".\bn\bn-win32.obj"

!IF  "$(CFG)" == "Crypt32 - Win32 Release"

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Debug"

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Purify"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Crosscompile"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# End Group
# Begin Group "Certificates"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\cert\certrev.c
# End Source File
# Begin Source File

SOURCE=.\cert\certschk.c
# End Source File
# Begin Source File

SOURCE=.\cert\certsign.c
# End Source File
# Begin Source File

SOURCE=.\cert\certval.c
# End Source File
# Begin Source File

SOURCE=.\cert\chain.c
# End Source File
# Begin Source File

SOURCE=.\cert\chk_cert.c
# End Source File
# Begin Source File

SOURCE=.\cert\chk_chn.c
# End Source File
# Begin Source File

SOURCE=.\cert\chk_use.c
# End Source File
# Begin Source File

SOURCE=.\cert\comp_cert.c
# End Source File
# Begin Source File

SOURCE=.\cert\comp_curs.c
# End Source File
# Begin Source File

SOURCE=.\cert\comp_del.c
# End Source File
# Begin Source File

SOURCE=.\cert\comp_get.c
# End Source File
# Begin Source File

SOURCE=.\cert\comp_gets.c
# End Source File
# Begin Source File

SOURCE=.\cert\comp_set.c
# End Source File
# Begin Source File

SOURCE=.\cert\dn.c
# End Source File
# Begin Source File

SOURCE=.\cert\dn_rw.c
# End Source File
# Begin Source File

SOURCE=.\cert\dnstring.c
# End Source File
# Begin Source File

SOURCE=.\cert\ext.c
# End Source File
# Begin Source File

SOURCE=.\cert\ext_add.c
# End Source File
# Begin Source File

SOURCE=.\cert\ext_chk.c
# End Source File
# Begin Source File

SOURCE=.\cert\ext_copy.c
# End Source File
# Begin Source File

SOURCE=.\cert\ext_def.c
# End Source File
# Begin Source File

SOURCE=.\cert\ext_rd.c
# End Source File
# Begin Source File

SOURCE=.\cert\ext_wr.c
# End Source File
# Begin Source File

SOURCE=.\cert\imp_chk.c
# End Source File
# Begin Source File

SOURCE=.\cert\imp_exp.c
# End Source File
# Begin Source File

SOURCE=.\cert\read.c
# End Source File
# Begin Source File

SOURCE=.\cert\trustmgr.c
# End Source File
# Begin Source File

SOURCE=.\cert\write.c
# End Source File
# Begin Source File

SOURCE=.\cert\write_pre.c
# End Source File
# End Group
# Begin Group "Contexts"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\context\ctx_3des.c
# End Source File
# Begin Source File

SOURCE=.\context\ctx_aes.c
# End Source File
# Begin Source File

SOURCE=.\context\ctx_attr.c
# End Source File
# Begin Source File

SOURCE=.\context\ctx_bf.c
# End Source File
# Begin Source File

SOURCE=.\context\ctx_cast.c
# End Source File
# Begin Source File

SOURCE=.\context\ctx_des.c
# End Source File
# Begin Source File

SOURCE=.\context\ctx_dh.c
# End Source File
# Begin Source File

SOURCE=.\context\ctx_dsa.c
# End Source File
# Begin Source File

SOURCE=.\context\ctx_ecdh.c
# End Source File
# Begin Source File

SOURCE=.\context\ctx_ecdsa.c
# End Source File
# Begin Source File

SOURCE=.\context\ctx_elg.c
# End Source File
# Begin Source File

SOURCE=.\context\ctx_generic.c
# End Source File
# Begin Source File

SOURCE=.\context\ctx_hmd5.c
# End Source File
# Begin Source File

SOURCE=.\context\ctx_hrmd.c
# End Source File
# Begin Source File

SOURCE=.\context\ctx_hsha.c
# End Source File
# Begin Source File

SOURCE=.\context\ctx_hsha2.c
# End Source File
# Begin Source File

SOURCE=.\context\ctx_idea.c
# End Source File
# Begin Source File

SOURCE=.\context\ctx_md5.c
# End Source File
# Begin Source File

SOURCE=.\context\ctx_misc.c
# End Source File
# Begin Source File

SOURCE=.\context\ctx_rc2.c
# End Source File
# Begin Source File

SOURCE=.\context\ctx_rc4.c
# End Source File
# Begin Source File

SOURCE=.\context\ctx_rc5.c
# End Source File
# Begin Source File

SOURCE=.\context\ctx_ripe.c
# End Source File
# Begin Source File

SOURCE=.\context\ctx_rsa.c
# End Source File
# Begin Source File

SOURCE=.\context\ctx_sha.c
# End Source File
# Begin Source File

SOURCE=.\context\ctx_sha2.c
# End Source File
# Begin Source File

SOURCE=.\context\key_id.c
# End Source File
# Begin Source File

SOURCE=.\context\key_rd.c
# End Source File
# Begin Source File

SOURCE=.\context\key_wr.c
# End Source File
# Begin Source File

SOURCE=.\context\keyload.c
# End Source File
# Begin Source File

SOURCE=.\context\kg_dlp.c
# End Source File
# Begin Source File

SOURCE=.\context\kg_ecc.c
# End Source File
# Begin Source File

SOURCE=.\context\kg_prime.c
# End Source File
# Begin Source File

SOURCE=.\context\kg_rsa.c
# End Source File
# End Group
# Begin Group "Crypt/Hash algorithms"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\crypt\aes_modes.c
# End Source File
# Begin Source File

SOURCE=.\crypt\aescrypt.c
# End Source File
# Begin Source File

SOURCE=.\crypt\aeskey.c
# End Source File
# Begin Source File

SOURCE=.\crypt\aestab.c
# End Source File
# Begin Source File

SOURCE=.\crypt\bfecb.c
# End Source File
# Begin Source File

SOURCE=.\crypt\bfenc.c
# End Source File
# Begin Source File

SOURCE=.\crypt\bfskey.c
# End Source File
# Begin Source File

SOURCE=.\crypt\castecb.c
# End Source File
# Begin Source File

SOURCE=.\crypt\castenc.c
# End Source File
# Begin Source File

SOURCE=.\crypt\castskey.c
# End Source File
# Begin Source File

SOURCE=.\crypt\descbc.c
# End Source File
# Begin Source File

SOURCE=.\crypt\desecb.c
# End Source File
# Begin Source File

SOURCE=.\crypt\desecb3.c
# End Source File
# Begin Source File

SOURCE=.\crypt\desenc.c
# End Source File
# Begin Source File

SOURCE=.\crypt\desskey.c
# End Source File
# Begin Source File

SOURCE=.\crypt\gcm.c
# End Source File
# Begin Source File

SOURCE=.\crypt\gf128mul.c
# End Source File
# Begin Source File

SOURCE=.\crypt\icbc.c
# End Source File
# Begin Source File

SOURCE=.\crypt\iecb.c
# End Source File
# Begin Source File

SOURCE=.\crypt\iskey.c
# End Source File
# Begin Source File

SOURCE=.\crypt\md5dgst.c
# End Source File
# Begin Source File

SOURCE=.\crypt\rc2cbc.c
# End Source File
# Begin Source File

SOURCE=.\crypt\rc2ecb.c
# End Source File
# Begin Source File

SOURCE=.\crypt\rc2skey.c
# End Source File
# Begin Source File

SOURCE=.\crypt\rc4skey.c
# End Source File
# Begin Source File

SOURCE=.\crypt\rc5ecb.c
# End Source File
# Begin Source File

SOURCE=.\crypt\rc5enc.c
# End Source File
# Begin Source File

SOURCE=.\crypt\rc5skey.c
# End Source File
# Begin Source File

SOURCE=.\crypt\rmddgst.c
# End Source File
# Begin Source File

SOURCE=.\crypt\sha1dgst.c
# End Source File
# Begin Source File

SOURCE=.\crypt\sha2.c
# End Source File
# Begin Source File

SOURCE=".\crypt\s1-win32.obj"

!IF  "$(CFG)" == "Crypt32 - Win32 Release"

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Debug"

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Purify"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Crosscompile"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=".\crypt\d-win32.obj"

!IF  "$(CFG)" == "Crypt32 - Win32 Release"

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Debug"

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Purify"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Crosscompile"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=".\crypt\m5-win32.obj"

!IF  "$(CFG)" == "Crypt32 - Win32 Release"

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Debug"

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Purify"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Crosscompile"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=".\crypt\r4-win32.obj"

!IF  "$(CFG)" == "Crypt32 - Win32 Release"

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Debug"

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Purify"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Crosscompile"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=".\crypt\r5-win32.obj"

!IF  "$(CFG)" == "Crypt32 - Win32 Release"

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Debug"

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Purify"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Crosscompile"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=".\crypt\rm-win32.obj"

!IF  "$(CFG)" == "Crypt32 - Win32 Release"

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Debug"

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Purify"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Crosscompile"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=".\crypt\b-win32.obj"

!IF  "$(CFG)" == "Crypt32 - Win32 Release"

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Debug"

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Purify"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Crosscompile"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\crypt\aescrypt2.obj

!IF  "$(CFG)" == "Crypt32 - Win32 Release"

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Debug"

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Purify"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Crosscompile"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=".\crypt\c-win32.obj"
# End Source File
# End Group
# Begin Group "Devices"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\device\dev_attr.c
# End Source File
# Begin Source File

SOURCE=.\device\hardware.c
# End Source File
# Begin Source File

SOURCE=.\device\hw_dummy.c
# End Source File
# Begin Source File

SOURCE=.\device\ms_capi.c

!IF  "$(CFG)" == "Crypt32 - Win32 Release"

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Debug"

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Purify"

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Crosscompile"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\device\pkcs11.c
# End Source File
# Begin Source File

SOURCE=.\device\pkcs11_init.c
# End Source File
# Begin Source File

SOURCE=.\device\pkcs11_pkc.c
# End Source File
# Begin Source File

SOURCE=.\device\pkcs11_rd.c
# End Source File
# Begin Source File

SOURCE=.\device\pkcs11_wr.c
# End Source File
# Begin Source File

SOURCE=.\device\system.c
# End Source File
# End Group
# Begin Group "Encode/Decode"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\enc_dec\asn1_algid.c
# End Source File
# Begin Source File

SOURCE=.\enc_dec\asn1_chk.c
# End Source File
# Begin Source File

SOURCE=.\enc_dec\asn1_ext.c
# End Source File
# Begin Source File

SOURCE=.\enc_dec\asn1_rd.c
# End Source File
# Begin Source File

SOURCE=.\enc_dec\asn1_wr.c
# End Source File
# Begin Source File

SOURCE=.\enc_dec\base64.c
# End Source File
# Begin Source File

SOURCE=.\enc_dec\base64_id.c
# End Source File
# Begin Source File

SOURCE=.\enc_dec\misc_rw.c
# End Source File
# Begin Source File

SOURCE=.\enc_dec\pgp_rw.c
# End Source File
# End Group
# Begin Group "Envelopes"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\envelope\cms_denv.c
# End Source File
# Begin Source File

SOURCE=.\envelope\cms_env.c
# End Source File
# Begin Source File

SOURCE=.\envelope\cms_envpre.c
# End Source File
# Begin Source File

SOURCE=.\envelope\decode.c
# End Source File
# Begin Source File

SOURCE=.\envelope\encode.c
# End Source File
# Begin Source File

SOURCE=.\envelope\env_attr.c
# End Source File
# Begin Source File

SOURCE=.\envelope\pgp_denv.c
# End Source File
# Begin Source File

SOURCE=.\envelope\pgp_env.c
# End Source File
# Begin Source File

SOURCE=.\envelope\res_actn.c
# End Source File
# Begin Source File

SOURCE=.\envelope\res_denv.c
# End Source File
# Begin Source File

SOURCE=.\envelope\res_env.c
# End Source File
# End Group
# Begin Group "I/O"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\io\dns.c
# End Source File
# Begin Source File

SOURCE=.\io\dns_srv.c
# End Source File
# Begin Source File

SOURCE=.\io\file.c
# End Source File
# Begin Source File

SOURCE=.\io\http_parse.c
# End Source File
# Begin Source File

SOURCE=.\io\http_rd.c
# End Source File
# Begin Source File

SOURCE=.\io\http_wr.c
# End Source File
# Begin Source File

SOURCE=.\io\memory.c
# End Source File
# Begin Source File

SOURCE=.\io\net.c
# End Source File
# Begin Source File

SOURCE=.\io\net_proxy.c
# End Source File
# Begin Source File

SOURCE=.\io\net_trans.c
# End Source File
# Begin Source File

SOURCE=.\io\net_url.c
# End Source File
# Begin Source File

SOURCE=.\io\stream.c
# End Source File
# Begin Source File

SOURCE=.\io\tcp.c
# End Source File
# End Group
# Begin Group "Kernel"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\kernel\attr_acl.c
# End Source File
# Begin Source File

SOURCE=.\kernel\certm_acl.c
# End Source File
# Begin Source File

SOURCE=.\kernel\init.c
# End Source File
# Begin Source File

SOURCE=.\kernel\int_msg.c
# End Source File
# Begin Source File

SOURCE=.\kernel\key_acl.c
# End Source File
# Begin Source File

SOURCE=.\kernel\mech_acl.c
# End Source File
# Begin Source File

SOURCE=.\kernel\msg_acl.c
# End Source File
# Begin Source File

SOURCE=.\kernel\obj_acc.c
# End Source File
# Begin Source File

SOURCE=.\kernel\objects.c
# End Source File
# Begin Source File

SOURCE=.\kernel\sec_mem.c
# End Source File
# Begin Source File

SOURCE=.\kernel\selftest.c
# End Source File
# Begin Source File

SOURCE=.\kernel\semaphore.c
# End Source File
# Begin Source File

SOURCE=.\kernel\sendmsg.c
# End Source File
# End Group
# Begin Group "Keysets"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\keyset\http.c
# End Source File
# Begin Source File

SOURCE=.\keyset\key_attr.c
# End Source File
# Begin Source File

SOURCE=.\keyset\ldap.c

!IF  "$(CFG)" == "Crypt32 - Win32 Release"

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Debug"

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Purify"

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Crosscompile"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\keyset\pgp.c
# End Source File
# Begin Source File

SOURCE=.\keyset\pgp_rd.c
# End Source File
# Begin Source File

SOURCE=.\keyset\pkcs12.c
# End Source File
# Begin Source File

SOURCE=.\keyset\pkcs12_rd.c
# End Source File
# Begin Source File

SOURCE=.\keyset\pkcs12_rdo.c
# End Source File
# Begin Source File

SOURCE=.\keyset\pkcs12_wr.c
# End Source File
# End Group
# Begin Group "Keysets - DBMS"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\keyset\ca_add.c
# End Source File
# Begin Source File

SOURCE=.\keyset\ca_clean.c
# End Source File
# Begin Source File

SOURCE=.\keyset\ca_issue.c
# End Source File
# Begin Source File

SOURCE=.\keyset\ca_misc.c
# End Source File
# Begin Source File

SOURCE=.\keyset\ca_rev.c
# End Source File
# Begin Source File

SOURCE=.\keyset\dbms.c
# End Source File
# Begin Source File

SOURCE=.\keyset\dbx_misc.c
# End Source File
# Begin Source File

SOURCE=.\keyset\dbx_rd.c
# End Source File
# Begin Source File

SOURCE=.\keyset\dbx_wr.c
# End Source File
# Begin Source File

SOURCE=.\keyset\odbc.c
# End Source File
# End Group
# Begin Group "Keysets - PKCS15"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\keyset\pkcs15.c
# End Source File
# Begin Source File

SOURCE=.\keyset\pkcs15_add.c
# End Source File
# Begin Source File

SOURCE=.\keyset\pkcs15_adpb.c
# End Source File
# Begin Source File

SOURCE=.\keyset\pkcs15_adpr.c
# End Source File
# Begin Source File

SOURCE=.\keyset\pkcs15_atrd.c
# End Source File
# Begin Source File

SOURCE=.\keyset\pkcs15_atwr.c
# End Source File
# Begin Source File

SOURCE=.\keyset\pkcs15_get.c
# End Source File
# Begin Source File

SOURCE=.\keyset\pkcs15_getp.c
# End Source File
# Begin Source File

SOURCE=.\keyset\pkcs15_rd.c
# End Source File
# Begin Source File

SOURCE=.\keyset\pkcs15_set.c
# End Source File
# Begin Source File

SOURCE=.\keyset\pkcs15_wr.c
# End Source File
# End Group
# Begin Group "Mechanisms"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\mechs\keyex.c
# End Source File
# Begin Source File

SOURCE=.\mechs\keyex_int.c
# End Source File
# Begin Source File

SOURCE=.\mechs\keyex_rw.c
# End Source File
# Begin Source File

SOURCE=.\mechs\mech_cwrap.c
# End Source File
# Begin Source File

SOURCE=.\mechs\mech_drv.c
# End Source File
# Begin Source File

SOURCE=.\mechs\mech_int.c
# End Source File
# Begin Source File

SOURCE=.\mechs\mech_pkwrap.c
# End Source File
# Begin Source File

SOURCE=.\mechs\mech_privk.c
# End Source File
# Begin Source File

SOURCE=.\mechs\mech_sig.c
# End Source File
# Begin Source File

SOURCE=.\mechs\obj_qry.c
# End Source File
# Begin Source File

SOURCE=.\mechs\sign.c
# End Source File
# Begin Source File

SOURCE=.\mechs\sign_cms.c
# End Source File
# Begin Source File

SOURCE=.\mechs\sign_int.c
# End Source File
# Begin Source File

SOURCE=.\mechs\sign_pgp.c
# End Source File
# Begin Source File

SOURCE=.\mechs\sign_rw.c
# End Source File
# Begin Source File

SOURCE=.\mechs\sign_x509.c
# End Source File
# End Group
# Begin Group "Misc"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\crypt32.def
# End Source File
# Begin Source File

SOURCE=.\crypt32.rc
# End Source File
# Begin Source File

SOURCE=.\misc\int_api.c
# End Source File
# Begin Source File

SOURCE=.\misc\int_attr.c
# End Source File
# Begin Source File

SOURCE=.\misc\int_debug.c
# End Source File
# Begin Source File

SOURCE=.\misc\int_env.c
# End Source File
# Begin Source File

SOURCE=.\misc\int_err.c
# End Source File
# Begin Source File

SOURCE=.\misc\int_mem.c
# End Source File
# Begin Source File

SOURCE=.\misc\int_string.c
# End Source File
# Begin Source File

SOURCE=.\misc\int_time.c
# End Source File
# Begin Source File

SOURCE=.\bindings\java_jni.c
# End Source File
# Begin Source File

SOURCE=.\misc\os_spec.c
# End Source File
# Begin Source File

SOURCE=.\misc\pgp_misc.c
# End Source File
# Begin Source File

SOURCE=.\random\rand_x917.c
# End Source File
# Begin Source File

SOURCE=.\random\random.c
# End Source File
# Begin Source File

SOURCE=.\misc\user.c
# End Source File
# Begin Source File

SOURCE=.\misc\user_attr.c
# End Source File
# Begin Source File

SOURCE=.\misc\user_cfg.c
# End Source File
# Begin Source File

SOURCE=.\misc\user_rw.c
# End Source File
# Begin Source File

SOURCE=.\random\win32.c

!IF  "$(CFG)" == "Crypt32 - Win32 Release"

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Debug"

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Purify"

!ELSEIF  "$(CFG)" == "Crypt32 - Win32 Crosscompile"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# End Group
# Begin Group "Sessions"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\session\scorebrd.c
# End Source File
# Begin Source File

SOURCE=.\session\sess_attr.c
# End Source File
# Begin Source File

SOURCE=.\session\sess_iattr.c
# End Source File
# Begin Source File

SOURCE=.\session\sess_rw.c
# End Source File
# Begin Source File

SOURCE=.\session\session.c
# End Source File
# End Group
# Begin Group "Sessions - SSH"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\session\ssh.c
# End Source File
# Begin Source File

SOURCE=.\session\ssh1.c
# End Source File
# Begin Source File

SOURCE=.\session\ssh2.c
# End Source File
# Begin Source File

SOURCE=.\session\ssh2_authc.c
# End Source File
# Begin Source File

SOURCE=.\session\ssh2_auths.c
# End Source File
# Begin Source File

SOURCE=.\session\ssh2_chn.c
# End Source File
# Begin Source File

SOURCE=.\session\ssh2_cli.c
# End Source File
# Begin Source File

SOURCE=.\session\ssh2_cry.c
# End Source File
# Begin Source File

SOURCE=.\session\ssh2_msg.c
# End Source File
# Begin Source File

SOURCE=.\session\ssh2_msgc.c
# End Source File
# Begin Source File

SOURCE=.\session\ssh2_msgs.c
# End Source File
# Begin Source File

SOURCE=.\session\ssh2_rd.c
# End Source File
# Begin Source File

SOURCE=.\session\ssh2_svr.c
# End Source File
# Begin Source File

SOURCE=.\session\ssh2_wr.c
# End Source File
# End Group
# Begin Group "Sessions - SSL"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\session\ssl.c
# End Source File
# Begin Source File

SOURCE=.\session\ssl_cli.c
# End Source File
# Begin Source File

SOURCE=.\session\ssl_cry.c
# End Source File
# Begin Source File

SOURCE=.\session\ssl_ext.c
# End Source File
# Begin Source File

SOURCE=.\session\ssl_hs.c
# End Source File
# Begin Source File

SOURCE=.\session\ssl_hsc.c
# End Source File
# Begin Source File

SOURCE=.\session\ssl_kmgmt.c
# End Source File
# Begin Source File

SOURCE=.\session\ssl_rd.c
# End Source File
# Begin Source File

SOURCE=.\session\ssl_suites.c
# End Source File
# Begin Source File

SOURCE=.\session\ssl_svr.c
# End Source File
# Begin Source File

SOURCE=.\session\ssl_wr.c
# End Source File
# End Group
# Begin Group "Sessions - PKI"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\session\certstore.c
# End Source File
# Begin Source File

SOURCE=.\session\cmp.c
# End Source File
# Begin Source File

SOURCE=.\session\cmp_cli.c
# End Source File
# Begin Source File

SOURCE=.\session\cmp_cry.c
# End Source File
# Begin Source File

SOURCE=.\session\cmp_err.c
# End Source File
# Begin Source File

SOURCE=.\session\cmp_rd.c
# End Source File
# Begin Source File

SOURCE=.\session\cmp_rdmsg.c
# End Source File
# Begin Source File

SOURCE=.\session\cmp_svr.c
# End Source File
# Begin Source File

SOURCE=.\session\cmp_wr.c
# End Source File
# Begin Source File

SOURCE=.\session\cmp_wrmsg.c
# End Source File
# Begin Source File

SOURCE=.\session\ocsp.c
# End Source File
# Begin Source File

SOURCE=.\session\pnppki.c
# End Source File
# Begin Source File

SOURCE=.\session\rtcs.c
# End Source File
# Begin Source File

SOURCE=.\session\scep.c
# End Source File
# Begin Source File

SOURCE=.\session\scep_cli.c
# End Source File
# Begin Source File

SOURCE=.\session\scep_svr.c
# End Source File
# Begin Source File

SOURCE=.\session\tsp.c
# End Source File
# End Group
# Begin Group "Zlib"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\zlib\adler32.c
# End Source File
# Begin Source File

SOURCE=.\zlib\deflate.c
# End Source File
# Begin Source File

SOURCE=.\zlib\inffast.c
# End Source File
# Begin Source File

SOURCE=.\zlib\inflate.c
# End Source File
# Begin Source File

SOURCE=.\zlib\inftrees.c
# End Source File
# Begin Source File

SOURCE=.\zlib\trees.c
# End Source File
# Begin Source File

SOURCE=.\zlib\zutil.c
# End Source File
# Begin Source File

SOURCE=.\zlib\match686.obj
# End Source File
# Begin Source File

SOURCE=.\zlib\inffas32.obj
# End Source File
# End Group
# Begin Source File

SOURCE=.\cryptapi.c
# End Source File
# Begin Source File

SOURCE=.\cryptcrt.c
# End Source File
# Begin Source File

SOURCE=.\cryptctx.c
# End Source File
# Begin Source File

SOURCE=.\cryptdev.c
# End Source File
# Begin Source File

SOURCE=.\cryptenv.c
# End Source File
# Begin Source File

SOURCE=.\cryptkey.c
# End Source File
# Begin Source File

SOURCE=.\cryptlib.c
# End Source File
# Begin Source File

SOURCE=.\cryptses.c
# End Source File
# Begin Source File

SOURCE=.\cryptusr.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Group "Certificates - Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\cert\cert.h
# End Source File
# Begin Source File

SOURCE=.\cert\certattr.h
# End Source File
# Begin Source File

SOURCE=.\cert\certfn.h
# End Source File
# Begin Source File

SOURCE=.\cert\dn.h
# End Source File
# Begin Source File

SOURCE=.\cert\trustmgr.h
# End Source File
# End Group
# Begin Group "Devices - Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\device\capabil.h
# End Source File
# Begin Source File

SOURCE=.\device\device.h
# End Source File
# Begin Source File

SOURCE=.\device\hardware.h
# End Source File
# Begin Source File

SOURCE=.\device\pkcs11_api.h
# End Source File
# End Group
# Begin Group "Encode/Decode - Folder"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\enc_dec\asn1.h
# End Source File
# Begin Source File

SOURCE=.\enc_dec\asn1_ext.h
# End Source File
# Begin Source File

SOURCE=.\enc_dec\asn1_oids.h
# End Source File
# Begin Source File

SOURCE=.\enc_dec\misc_rw.h
# End Source File
# Begin Source File

SOURCE=.\enc_dec\pgp_rw.h
# End Source File
# End Group
# Begin Group "I/O - Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\io\file.h
# End Source File
# Begin Source File

SOURCE=.\io\http.h
# End Source File
# Begin Source File

SOURCE=.\io\stream.h
# End Source File
# Begin Source File

SOURCE=.\io\stream_int.h
# End Source File
# Begin Source File

SOURCE=.\io\tcp.h
# End Source File
# End Group
# Begin Group "Kernel - Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\kernel\acl.h
# End Source File
# Begin Source File

SOURCE=.\kernel\acl_perm.h
# End Source File
# Begin Source File

SOURCE=.\cryptkrn.h
# End Source File
# Begin Source File

SOURCE=.\kernel\kernel.h
# End Source File
# Begin Source File

SOURCE=.\kernel\thread.h
# End Source File
# End Group
# Begin Group "Keysets - Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\keyset\dbms.h
# End Source File
# Begin Source File

SOURCE=.\keyset\keyset.h
# End Source File
# Begin Source File

SOURCE=.\keyset\pgp_key.h
# End Source File
# Begin Source File

SOURCE=.\keyset\pkcs12.h
# End Source File
# Begin Source File

SOURCE=.\keyset\pkcs15.h
# End Source File
# End Group
# Begin Group "Sessions - Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\session\certstore.h
# End Source File
# Begin Source File

SOURCE=.\session\cmp.h
# End Source File
# Begin Source File

SOURCE=.\session\scep.h
# End Source File
# Begin Source File

SOURCE=.\session\scorebrd.h
# End Source File
# Begin Source File

SOURCE=.\session\session.h
# End Source File
# Begin Source File

SOURCE=.\session\ssh.h
# End Source File
# Begin Source File

SOURCE=.\session\ssh_dhkeys.h
# End Source File
# Begin Source File

SOURCE=.\session\ssl.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\misc\analyse.h
# End Source File
# Begin Source File

SOURCE=.\misc\config.h
# End Source File
# Begin Source File

SOURCE=.\misc\consts.h
# End Source File
# Begin Source File

SOURCE=.\context\context.h
# End Source File
# Begin Source File

SOURCE=.\crypt.h
# End Source File
# Begin Source File

SOURCE=.\cryptlib.h
# End Source File
# Begin Source File

SOURCE=.\misc\debug.h
# End Source File
# Begin Source File

SOURCE=.\envelope\envelope.h
# End Source File
# Begin Source File

SOURCE=.\misc\int_api.h
# End Source File
# Begin Source File

SOURCE=.\mechs\mech.h
# End Source File
# Begin Source File

SOURCE=.\misc\os_spec.h
# End Source File
# Begin Source File

SOURCE=.\misc\pgp.h
# End Source File
# Begin Source File

SOURCE=.\misc\user.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;cnt;rtf;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\crypt32.ico
# End Source File
# End Group
# End Target
# End Project
