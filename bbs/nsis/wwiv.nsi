; Script generated with the Venis Install Wizard

; Define your application name
!define PRODUCT_NAME "WWIV"
!define PRODUCT_BUILD "49"
!define PRODUCT_VERSION "Beta1"
!define PRODUCT_WEB_SITE "http://www.wwiv.com"
!define PRODUCT_PUBLISHER "WWIV Software Services"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"

; Modern interface settings
!include "MUI.nsh"

!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\WWIVTelnetServer.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Start WWIV Telnet Server"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\admin\LICENSE.AGR"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; Main Install settings
Name "${PRODUCT_NAME}"
InstallDir "C:\WWIV"
OutFile "WWIV50_beta1_${PRODUCT_BUILD}.exe"
InstallDirRegKey HKLM "Software\${APPNAME}" ""

Section "-WWIV 430 Core" Section1
	; Set Section properties
	SetOverwrite on
	SetOutPath "$INSTDIR\"
	File "..\admin\unzip.exe"
	File "..\admin\*.ini"
	File "..\admin\*.txt"
	File "..\admin\docs\*.doc"
        File /oname="wwiv.ini" "..\admin\wwivini.500"
	File "..\admin\readme.500"
	File "..\admin\license.agr"
	File "..\admin\regions.zip"
	File "..\admin\zip-city.zip"
	File "..\admin\wfc.dat"
	
        File /oname="wwiv50.exe" "..\bin\wwiv50.exe"
; This next line assumes that "FIX.EXE", "INIT.EXE", "STREDIT.EXE" have been placed in the vin folder
        File "..\bin\*.exe"

	SetOutPath "$INSTDIR\data"
	File "..\admin\modems.500"
	File "..\admin\menucmds.dat"	
	File "..\admin\regions.dat"	

	SetOutPath "$INSTDIR\gfiles"
	File "..\admin\menus\en\*"
	
	SetOutPath "$INSTDIR\gfiles\menus"
	File "..\admin\menus\en\menus\*"
	
	SetOutPath "$INSTDIR\gfiles\menus\wwiv"
	File "..\admin\menus\en\menus\wwiv\*"

	CreateDirectory "$INSTDIR\data"	
	ExecWait "$INSTDIR\unzip -qq -o $INSTDIR\regions.zip -d $INSTDIR\data"
	ExecWait "$INSTDIR\unzip -qq -o $INSTDIR\zip-city.zip -d $INSTDIR\data"

	Delete $INSTDIR\regions.zip
	Delete $INSTDIR\zip-city.zip	
SectionEnd

Section "WWIV Telnet Server" Section2
	; Set Section properties
	SetOverwrite on
	; Set Section Files and Shortcuts
	SetOutPath "$INSTDIR\"
	File "..\bin\WWIVTelnetServer.exe"
	File "..\telsrv\WWIVTelnetServer.exe.manifest"
	File "..\telsrv\WWIVTelnetServer.cnt"
	File "..\telsrv\WWIVTelnetServer.HLP"
	File /oname="TelnetServer_README.TXT" "..\telsrv\README.TXT"	
	CreateShortCut "$DESKTOP\WWIV Telnet Server.lnk" "$INSTDIR\WWIVTelnetServer.exe"
	CreateDirectory "$SMPROGRAMS\WWIV"
	CreateShortCut "$SMPROGRAMS\WWIV\WWIV Telnet Server.lnk" "$INSTDIR\WWIVTelnetServer.exe"
	CreateShortCut "$SMPROGRAMS\WWIV\WWIV Telnet Server Help.lnk" "$INSTDIR\WWIVTELNETSERVER.HLP"
SectionEnd

Section "Synchronet Fossil" Section3
	; Set Section properties
	SetOverwrite on
	; Set Section Files and Shortcuts
	SetOutPath "$INSTDIR\"	
	File "..\bin\sync\*"
SectionEnd

Section -FinishSection
  WriteUninstaller "$INSTDIR\uninstall.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninstall.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
  ExecWait "$INSTDIR\init.exe ,1"
  CreateDirectory "$SMPROGRAMS\WWIV"
  CreateShortCut "$SMPROGRAMS\WWIV\Uninstall.lnk" "$INSTDIR\uninstall.exe"
  CreateShortCut "$SMPROGRAMS\WWIV\Sysop Node.lnk" "$INSTDIR\wwiv50.exe" "-m -u0"
SectionEnd

; Modern install component descriptions
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
	!insertmacro MUI_DESCRIPTION_TEXT ${Section1} ""
	!insertmacro MUI_DESCRIPTION_TEXT ${Section2} ""
	!insertmacro MUI_DESCRIPTION_TEXT ${Section3} ""
!insertmacro MUI_FUNCTION_DESCRIPTION_END

;Uninstall section
Section Uninstall
	;Remove from registry...
	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
	DeleteRegKey HKLM "SOFTWARE\${PRODUCT_NAME}"

	; Delete self
	Delete "$INSTDIR\uninstall.exe"

	; Delete Shortcuts
	Delete "$DESKTOP\WWIV Telnet Server.lnk"
	Delete "$SMPROGRAMS\WWIV\WWIV Telnet Server.lnk"
	Delete "$SMPROGRAMS\WWIV\Uninstall.lnk"

	; Delete all installed files
	Delete "$INSTDIR\*"
	Delete "$INSTDIR\data\*"
	Delete "$INSTDIR\data\regions\*"
	Delete "$INSTDIR\data\zip-city\*"
	Delete "$INSTDIR\gfiles\menus\wwiv\*"
	Delete "$INSTDIR\gfiles\menus\*"
	Delete "$INSTDIR\gfiles\*"
	Delete "$INSTDIR\attach\*"
	Delete "$INSTDIR\msgs\*"
	Delete "$INSTDIR\temp1\*"
	Delete "$INSTDIR\temp2\*"
	Delete "$INSTDIR\dloads\misc\*"
	Delete "$INSTDIR\dloads\sysop\*"
	Delete "$INSTDIR\dloads\*"
	; Remove remaining directories
	RMDir "$INSTDIR\data\zip-city"
	RMDir "$INSTDIR\data\regions"
	RMDir "$INSTDIR\data"
	RMDir "$INSTDIR\gfiles\menus\wwiv"
	RMDir "$INSTDIR\gfiles\menus"
	RMDir "$INSTDIR\gfiles"
	RMDir "$INSTDIR\attach"
	RMDir "$INSTDIR\msgs"
	RMDir "$INSTDIR\temp1"
	RMDir "$INSTDIR\temp2"
	RMDir "$INSTDIR\dloads\misc"
	RMDir "$INSTDIR\dloads\sysop"
	RMDir "$INSTDIR\dloads"
	RMDir "$INSTDIR\"
	RMDir "$SMPROGRAMS\WWIV"
SectionEnd

; eof
