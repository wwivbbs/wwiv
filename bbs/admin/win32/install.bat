@echo off
cls
ECHO.
ECHO                     WWIV v5.00 Installation Batch
ECHO.
ECHO    IT IS HIGHLY RECOMMENDED THAT YOU MAKE A COMPLETE BACKUP OF YOUR
ECHO          DATA PRIOR TO INSTALLING AND RUNNNING THIS SOFTWARE.
ECHO.
ECHO  WWIV SOFTWARE SERVICES DISCLAIMS ANY AND ALL RESPONSIBILITY AND
ECHO  LIABILITY FOR ANY HARDWARE OR SOFTWARE DAMAGE, CORRUPTION OR LOSS OF
ECHO             DATA AS A RESULT OF YOUR USE OF THIS SOFTWARE.
ECHO.
ECHO         ******************************************************
ECHO.
ECHO.
rem ******************************* run INIT to convert data files to
rem ******************************* v5.00 format
ECHO Press Control-C to abort
pause
init.exe ,1
goto CLOSE

rem Exit Batch file
:CLOSE
cls
echo.
echo.
echo All done!  Examples of known working commands are in wwivbat.sam.

:END
echo.
echo.
echo.
