# Microsoft Developer Studio Project File - Name="Test32" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=Test32 - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Test32.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Test32.mak" CFG="Test32 - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Test32 - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "Test32 - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Test32 - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\test32"
# PROP BASE Intermediate_Dir ".\test32"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\binaries32_vc6"
# PROP Intermediate_Dir ".\binaries32_vc6"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
F90=fl32.exe
# ADD BASE F90 /I "test32/"
# ADD F90 /I "test32/"
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /YX /c
# ADD CPP /nologo /MD /W3 /O2 /I ".\\" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x1409 /d "NDEBUG"
# ADD RSC /l 0x1409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 shell32.lib kernel32.lib user32.lib wsock32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# SUBTRACT LINK32 /incremental:yes

!ELSEIF  "$(CFG)" == "Test32 - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\test32"
# PROP BASE Intermediate_Dir ".\test32"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\binaries32_vc6"
# PROP Intermediate_Dir ".\binaries32_vc6"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
F90=fl32.exe
# ADD BASE F90 /I "test32/"
# ADD F90 /I "test32/"
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /YX /c
# ADD CPP /nologo /MDd /W3 /Gm /Zi /Od /I ".\\" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x1409 /d "_DEBUG"
# ADD RSC /l 0x1409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386
# ADD LINK32 kernel32.lib user32.lib wsock32.lib odbccp32.lib /nologo /subsystem:console /incremental:no /debug /machine:I386 /fixed:no
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "Test32 - Win32 Release"
# Name "Test32 - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;hpj;bat;for;f90"
# Begin Source File

SOURCE=.\test\certimp.c
# End Source File
# Begin Source File

SOURCE=.\test\certproc.c
# End Source File
# Begin Source File

SOURCE=.\test\certs.c
# End Source File
# Begin Source File

SOURCE=.\test\devices.c
# End Source File
# Begin Source File

SOURCE=.\test\envelope.c
# End Source File
# Begin Source File

SOURCE=.\test\highlvl.c
# End Source File
# Begin Source File

SOURCE=.\test\keydbx.c
# End Source File
# Begin Source File

SOURCE=.\test\keyfile.c
# End Source File
# Begin Source File

SOURCE=.\test\loadkey.c
# End Source File
# Begin Source File

SOURCE=.\test\lowlvl.c
# End Source File
# Begin Source File

SOURCE=.\test\s_cmp.c
# End Source File
# Begin Source File

SOURCE=.\test\s_scep.c
# End Source File
# Begin Source File

SOURCE=.\test\sreqresp.c
# End Source File
# Begin Source File

SOURCE=.\test\ssh.c
# End Source File
# Begin Source File

SOURCE=.\test\ssl.c
# End Source File
# Begin Source File

SOURCE=.\test\stress.c
# End Source File
# Begin Source File

SOURCE=.\test\suiteb.c
# End Source File
# Begin Source File

SOURCE=.\test\testfunc.c
# End Source File
# Begin Source File

SOURCE=.\test\testlib.c
# End Source File
# Begin Source File

SOURCE=.\test\utils.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Source File

SOURCE=.\cryptlib.h
# End Source File
# Begin Source File

SOURCE=.\test\filename.h
# End Source File
# Begin Source File

SOURCE=.\test\test.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;cnt;rtf;gif;jpg;jpeg;jpe"
# End Group
# Begin Source File

SOURCE=.\binaries32_vc6\cl32.lib
# End Source File
# End Target
# End Project
