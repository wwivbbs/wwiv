# Microsoft Developer Studio Project File - Name="WWIV50" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=WWIV50 - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "WWIV50.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "WWIV50.mak" CFG="WWIV50 - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "WWIV50 - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "WWIV50 - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""$/WWIV50", LBAAAAAA"
# PROP Scc_LocalPath "."
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "WWIV50 - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX"wwiv.h" /FD /EHsc /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ws2_32.lib /nologo /subsystem:console /machine:I386 /out:"C:\bbs\wwiv50nd.exe"

!ELSEIF  "$(CFG)" == "WWIV50 - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GR /GX /ZI /Od /I "platform/" /I "platform/WIN32/" /I "." /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /FR /YX"wwiv.h" /FD /GZ /EHsc /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 ws2_32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /out:"C:\bbs\wwiv50.exe" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "WWIV50 - Win32 Release"
# Name "WWIV50 - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "Platform Source"

# PROP Default_Filter ""
# Begin Group "Win32"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\platform\WIN32\exec.cpp
# End Source File
# Begin Source File

SOURCE=.\platform\WIN32\filesupp.cpp
# End Source File
# Begin Source File

SOURCE=.\platform\WIN32\reboot.cpp
# End Source File
# Begin Source File

SOURCE=.\platform\WIN32\utility2.cpp
# End Source File
# Begin Source File

SOURCE=.\platform\WIN32\WFile.cpp
# End Source File
# Begin Source File

SOURCE=.\platform\WIN32\wfndfile.cpp
# End Source File
# Begin Source File

SOURCE=.\platform\WIN32\Wios.cpp
# End Source File
# Begin Source File

SOURCE=.\platform\WIN32\Wiot.cpp
# End Source File
# Begin Source File

SOURCE=.\platform\WIN32\WLocalIO.cpp
# End Source File
# Begin Source File

SOURCE=.\platform\WIN32\wshare.cpp
# End Source File
# End Group
# Begin Group "linux"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\platform\linux\filestuff.cpp
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\platform\linux\reboot.cpp
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\platform\linux\stringstuff.cpp
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\platform\linux\utility2.cpp
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\platform\linux\wfndfile.cpp
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\platform\linux\Wiou.cpp
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\platform\linux\WLocalIO.cpp
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\platform\linux\wshare.cpp
# PROP Exclude_From_Build 1
# End Source File
# End Group
# Begin Group "common platform source"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\platform\DateTime.cpp
# End Source File
# Begin Source File

SOURCE=.\platform\wutil.cpp
# End Source File
# End Group
# End Group
# Begin Group "Generic Source"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\asv.cpp
# End Source File
# Begin Source File

SOURCE=.\attach.cpp
# End Source File
# Begin Source File

SOURCE=.\automsg.cpp
# End Source File
# Begin Source File

SOURCE=.\BATCH.CPP
# End Source File
# Begin Source File

SOURCE=.\BBS.CPP
# End Source File
# Begin Source File

SOURCE=.\bbslist.cpp
# End Source File
# Begin Source File

SOURCE=.\BBSOVL1.CPP
# End Source File
# Begin Source File

SOURCE=.\BBSOVL2.CPP

!IF  "$(CFG)" == "WWIV50 - Win32 Release"

!ELSEIF  "$(CFG)" == "WWIV50 - Win32 Debug"

# ADD CPP /W4

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\BBSOVL3.CPP
# End Source File
# Begin Source File

SOURCE=.\BBSUTL.CPP
# End Source File
# Begin Source File

SOURCE=.\BBSUTL1.CPP
# End Source File
# Begin Source File

SOURCE=.\BBSUTL2.CPP
# End Source File
# Begin Source File

SOURCE=.\bgetch.cpp
# End Source File
# Begin Source File

SOURCE=.\bputch.cpp
# End Source File
# Begin Source File

SOURCE=.\CALLBACK.CPP
# End Source File
# Begin Source File

SOURCE=.\chains.cpp
# End Source File
# Begin Source File

SOURCE=.\CHAT.CPP
# End Source File
# Begin Source File

SOURCE=.\CHNEDIT.CPP
# End Source File
# Begin Source File

SOURCE=.\colors.cpp
# End Source File
# Begin Source File

SOURCE=.\COM.CPP
# End Source File
# Begin Source File

SOURCE=.\CONF.CPP
# End Source File
# Begin Source File

SOURCE=.\confutil.cpp
# End Source File
# Begin Source File

SOURCE=.\CONNECT1.CPP
# End Source File
# Begin Source File

SOURCE=.\CRC.CPP
# End Source File
# Begin Source File

SOURCE=.\prot\crctab.cpp
# End Source File
# Begin Source File

SOURCE=.\DEFAULTS.CPP
# End Source File
# Begin Source File

SOURCE=.\DIREDIT.CPP
# End Source File
# Begin Source File

SOURCE=.\dirlist.cpp
# End Source File
# Begin Source File

SOURCE=.\dropfile.cpp
# End Source File
# Begin Source File

SOURCE=.\dupphone.cpp
# End Source File
# Begin Source File

SOURCE=.\EVENTS.CPP
# End Source File
# Begin Source File

SOURCE=.\execexternal.cpp
# End Source File
# Begin Source File

SOURCE=.\extract.cpp
# End Source File
# Begin Source File

SOURCE=.\EXTRN.CPP
# End Source File
# Begin Source File

SOURCE=.\EXTRN1.CPP
# End Source File
# Begin Source File

SOURCE=.\GFILES.CPP
# End Source File
# Begin Source File

SOURCE=.\GFLEDIT.CPP
# End Source File
# Begin Source File

SOURCE=.\hop.cpp
# End Source File
# Begin Source File

SOURCE=.\inetmsg.cpp
# End Source File
# Begin Source File

SOURCE=.\INI.CPP
# End Source File
# Begin Source File

SOURCE=.\inmsg.cpp
# End Source File
# Begin Source File

SOURCE=.\input.cpp
# End Source File
# Begin Source File

SOURCE=.\INSTMSG.CPP
# End Source File
# Begin Source File

SOURCE=.\interpret.cpp
# End Source File
# Begin Source File

SOURCE=.\LILO.CPP
# End Source File
# Begin Source File

SOURCE=.\LISTPLUS.CPP
# End Source File
# Begin Source File

SOURCE=.\lpfunc.cpp
# End Source File
# Begin Source File

SOURCE=.\memory.cpp
# End Source File
# Begin Source File

SOURCE=.\MENU.CPP
# End Source File
# Begin Source File

SOURCE=.\MENUEDIT.CPP
# End Source File
# Begin Source File

SOURCE=.\MENUSPEC.CPP
# End Source File
# Begin Source File

SOURCE=.\MENUSUPP.CPP
# End Source File
# Begin Source File

SOURCE=.\MISCCMD.CPP
# End Source File
# Begin Source File

SOURCE=.\MODEM.CPP
# End Source File
# Begin Source File

SOURCE=.\MSGBASE.CPP
# End Source File
# Begin Source File

SOURCE=.\MSGBASE1.CPP
# End Source File
# Begin Source File

SOURCE=.\msgscan.cpp
# End Source File
# Begin Source File

SOURCE=.\MULTINST.CPP
# End Source File
# Begin Source File

SOURCE=.\MULTMAIL.CPP
# End Source File
# Begin Source File

SOURCE=.\NETSUP.CPP
# End Source File
# Begin Source File

SOURCE=.\NEWUSER.CPP
# End Source File
# Begin Source File

SOURCE=.\normupld.cpp
# End Source File
# Begin Source File

SOURCE=.\pause.cpp
# End Source File
# Begin Source File

SOURCE=.\printfile.cpp
# End Source File
# Begin Source File

SOURCE=.\quote.cpp
# End Source File
# Begin Source File

SOURCE=.\READMAIL.CPP
# End Source File
# Begin Source File

SOURCE=.\shortmsg.cpp
# End Source File
# Begin Source File

SOURCE=.\SR.CPP
# End Source File
# Begin Source File

SOURCE=.\SRRCV.CPP
# End Source File
# Begin Source File

SOURCE=.\SRSEND.CPP
# End Source File
# Begin Source File

SOURCE=.\status.cpp
# End Source File
# Begin Source File

SOURCE=.\STRINGS.CPP
# End Source File
# Begin Source File

SOURCE=.\stuffin.cpp
# End Source File
# Begin Source File

SOURCE=.\SUBACC.CPP
# End Source File
# Begin Source File

SOURCE=.\SUBEDIT.CPP
# End Source File
# Begin Source File

SOURCE=.\sublist.cpp
# End Source File
# Begin Source File

SOURCE=.\SUBREQ.CPP
# End Source File
# Begin Source File

SOURCE=.\SUBXTR.CPP
# End Source File
# Begin Source File

SOURCE=.\syschat.cpp
# End Source File
# Begin Source File

SOURCE=.\SYSOPF.CPP
# End Source File
# Begin Source File

SOURCE=.\sysoplog.cpp
# End Source File
# Begin Source File

SOURCE=.\trytoul.cpp
# End Source File
# Begin Source File

SOURCE=.\UEDIT.CPP
# End Source File
# Begin Source File

SOURCE=.\user.cpp
# End Source File
# Begin Source File

SOURCE=.\UTILITY.CPP
# End Source File
# Begin Source File

SOURCE=.\valscan.cpp
# End Source File
# Begin Source File

SOURCE=.\VERSION.CPP
# End Source File
# Begin Source File

SOURCE=.\vote.cpp
# End Source File
# Begin Source File

SOURCE=.\VOTEEDIT.CPP
# End Source File
# Begin Source File

SOURCE=.\WComm.cpp
# End Source File
# Begin Source File

SOURCE=.\wfc.cpp
# End Source File
# Begin Source File

SOURCE=.\wqscn.cpp
# End Source File
# Begin Source File

SOURCE=.\WSession.cpp
# End Source File
# Begin Source File

SOURCE=.\WStringUtils.cpp
# End Source File
# Begin Source File

SOURCE=.\WUser.cpp
# End Source File
# Begin Source File

SOURCE=.\XFER.CPP
# End Source File
# Begin Source File

SOURCE=.\XFEROVL.CPP
# End Source File
# Begin Source File

SOURCE=.\XFEROVL1.CPP
# End Source File
# Begin Source File

SOURCE=.\XFERTMP.CPP
# End Source File
# Begin Source File

SOURCE=.\XINIT.CPP
# End Source File
# Begin Source File

SOURCE=.\prot\zmodem.cpp
# End Source File
# Begin Source File

SOURCE=.\prot\zmodemcrc.cpp
# End Source File
# Begin Source File

SOURCE=.\prot\zmodemr.cpp
# End Source File
# Begin Source File

SOURCE=.\prot\zmodemt.cpp
# End Source File
# Begin Source File

SOURCE=.\prot\zmutil.cpp
# End Source File
# Begin Source File

SOURCE=.\prot\zmwwiv.cpp
# End Source File
# End Group
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Group "Generic Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\bbs.h
# End Source File
# Begin Source File

SOURCE=.\COMMON.H
# End Source File
# Begin Source File

SOURCE=.\FCNS.H
# End Source File
# Begin Source File

SOURCE=.\INI.H
# End Source File
# Begin Source File

SOURCE=.\INSTMSG.H
# End Source File
# Begin Source File

SOURCE=.\LISTPLUS.H
# End Source File
# Begin Source File

SOURCE=.\MENU.H
# End Source File
# Begin Source File

SOURCE=.\NET.H
# End Source File
# Begin Source File

SOURCE=.\SUBXTR.H
# End Source File
# Begin Source File

SOURCE=.\platform\testos.h
# End Source File
# Begin Source File

SOURCE=.\VARDEC.H
# End Source File
# Begin Source File

SOURCE=.\VARDEC1.H
# End Source File
# Begin Source File

SOURCE=.\VARS.H
# End Source File
# Begin Source File

SOURCE=.\wwivassert.h
# End Source File
# Begin Source File

SOURCE=.\WSession.h
# End Source File
# Begin Source File

SOURCE=.\WStringUtils.h
# End Source File
# Begin Source File

SOURCE=.\WUser.h
# End Source File
# Begin Source File

SOURCE=.\platform\wutil.h
# End Source File
# Begin Source File

SOURCE=.\wwiv.h
# End Source File
# End Group
# Begin Group "platform Headers"

# PROP Default_Filter ""
# Begin Group "Win32 platform Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\platform\WIN32\Wios.h
# End Source File
# Begin Source File

SOURCE=.\platform\WIN32\Wiot.h
# End Source File
# Begin Source File

SOURCE=.\platform\WIN32\wshare.h
# End Source File
# End Group
# Begin Group "common platform headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\platform\incl1.h
# End Source File
# Begin Source File

SOURCE=.\platform\incl2.h
# End Source File
# Begin Source File

SOURCE=.\platform\incl3.h
# End Source File
# Begin Source File

SOURCE=.\platform\platformfcns.h
# End Source File
# Begin Source File

SOURCE=.\wcomm.h
# End Source File
# Begin Source File

SOURCE=.\platform\WFile.h
# End Source File
# Begin Source File

SOURCE=.\platform\wfndfile.h
# End Source File
# Begin Source File

SOURCE=.\platform\WLocalIO.h
# End Source File
# Begin Source File

SOURCE=.\wtypes.h
# End Source File
# End Group
# Begin Group "linux platform headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\platform\linux\linuxplatform.h
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\platform\linux\Wiou.h
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\platform\linux\wshare.h
# PROP Exclude_From_Build 1
# End Source File
# End Group
# End Group
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\README.TXT
# End Source File
# Begin Source File

SOURCE=.\todo.txt
# End Source File
# End Group
# End Target
# End Project
