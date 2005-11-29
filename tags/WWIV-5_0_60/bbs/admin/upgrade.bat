@ECHO OFF
REM  Commandline must include the MAIN BBS destination directory for the
REM  upgrade with *NO* trailing slash i.e. C:\WWIV

CLS
ECHO.
ECHO.
ECHO                      WWIV v5.00 Upgrade Batch
ECHO.

if %1a == a goto HELP

ECHO    IT IS HIGHLY RECOMMENDED THAT YOU MAKE A COMPLETE BACKUP OF YOUR
ECHO          DATA PRIOR TO INSTALLING AND RUNNNING THIS SOFTWARE.
ECHO.
ECHO  WWIV SOFTWARE SERVICES DISCLAIMS ANY AND ALL RESPONSIBILITY AND
ECHO  LIABILITY FOR ANY HARDWARE OR SOFTWARE DAMAGE, CORRUPTION OR LOSS OF
ECHO             DATA AS A RESULT OF YOUR USE OF THIS SOFTWARE.
ECHO.
ECHO.
ECHO Press Control-C to Cancel
pause

rem ******************************* display license agreement
cls
type license.agr | more
ECHO.
ECHO.
ECHO.
rem ******************************* run INIT to convert data files to
rem ******************************* v5.00 format
ECHO Press Control-C to Cancel
pause

CLS
ECHO.
ECHO.
ECHO                      WWIV v5.00 Upgrade Batch
ECHO.
ECHO  If your system does not use a standard WWIV directory tree, you
ECHO  must edit this file prior to upgrading to account for directories
ECHO  or files that are in non-standard locations.
ECHO.
ECHO  You must manually merge the changes from WWIVINI.500 with your
ECHO  WWIV.INI file.  Settings in your current file that are not in the
ECHO  new file should be removed.  They will be ignored in any case, but
ECHO  will increase read times if not removed.
ECHO.
ECHO                  ***WARNING***WARNING***WARNING***
ECHO.
ECHO  Running the BBS prior to running INIT to convert the data files
ECHO  will corrupt the files making it impossible to convert them.  Make
ECHO  sure you have a current backup!  If you included a trailing
ECHO  backslash on your install directory, this upgrade will fail!!
ECHO.
ECHO Upgrade to WWIV v5.00 now?
ECHO.
ECHO Press Control-C to Cancel
pause

:UPGRADE
ECHO Upgrading to WWIV v5.00...  Please wait....
ECHO.
ECHO.
REM ******************************* Change the attributes if set
ECHO Changing file attributes if set...
attrib -r %1\init.exe
attrib -r %1\bbs.exe
attrib -r %1\fix.exe
attrib -r %1\return.exe

REM ******************************* copy EXE's
ECHO Copying EXE's...
copy init.exe %1
copy bbs.exe %1
attrib +r %1\bbs.exe
copy fix.exe %1
copy return.exe %1
copy stredit.exe %1

REM ******************************* copy STR files
ECHO Copying String files...
copy english.str %1\gfiles\bbs.str
copy sysoplog.str %1\gfiles\sysoplog.str
copy chat.str %1\gfiles\chat.str
copy ini.str %1\gfiles\ini.str
copy yes.str %1\gfiles\yes.str
copy no.str %1\gfiles\no.str

REM ******************************* copy INI's and text files
ECHO Copying INI, data, and text files...
copy chat.ini %1
copy wwivini.500 %1
copy bbsads.txt %1
copy revision.txt %1
copy readme.500 %1
copy menu.doc %1
copy *.sam %1
copy modems.500 %1\DATA
copy menucmds.dat %1\DATA
copy regions.dat %1\DATA
copy wfc.dat %1\DATA

REM ******************************* unzip menus regions and zip code files
ECHO Decompressing archives. Please wait....
if %TZ%a == a goto DOTZ
unzip -qq -o en-menus.zip -d%1\gfiles
unzip -qq -o regions.zip -d%1\data
unzip -qq -o zip-city.zip -d%1\data
goto TZEND
:DOTZ
SET TZ=EST5EDT
unzip -qq -o en-menus.zip -d%1\gfiles
unzip -qq -o regions.zip -d%1\data
unzip -qq -o zip-city.zip -d%1\data
SET TZ=
:TZEND

REM ******************************* run INIT to convert data files to
REM ******************************* v5.00 format
CLS
cd %1
type license.agr | more
ECHO.
ECHO.
ECHO You must now run INIT to convert your data files to v5.00 format.
ECHO.
ECHO.
ECHO.
ECHO Run INIT ,1 now?
ECHO Press Control-C to Cancel
pause
cd %1
init ,1
goto CLOSE

REM Display Help
:HELP
ECHO.
ECHO.
ECHO Commandline must include destination directory!
ECHO.
ECHO DO NOT include a trailing backslash!
ECHO.
ECHO.
ECHO  i.e.  UPGRADE.BAT C:\WWIV
goto END

REM Exit Batch file
:CLOSE
CLS
ECHO.
ECHO.
ECHO                      WWIV v5.00 Upgrade Batch
ECHO.
ECHO If INIT.EXE faile to run, you will need to run it manually to complete
ECHO the upgrade process.  Be forewarened that if INIT.EXE is not run once
ECHO prior to running the BBS, your data files will be permanently
ECHO corrupted.  It would be wise to run INIT.EXE again now just to be
ECHO sure.
ECHO.
ECHO You will then need to manually merge WWIVINI.500 with your current
ECHO WWIV.INI file.  This is also imperative as some new settings must be
ECHO included for v5.00 to run.  Be sure to read the comments thoroughly.
ECHO They should be more than sufficient to get you up and running for the
ECHO first time.  Consult the documentation for advanced settings.
ECHO.
ECHO The new file, MODEMS.500, has been copied to your DATA directory.
ECHO If your current modem setup is working, you should continue to use it.
ECHO There are some new definitions in this file that are specialized for
ECHO telnet use with COM/IP or NetModem32.
ECHO.

:END
ECHO.
