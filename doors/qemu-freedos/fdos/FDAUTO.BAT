@ECHO OFF
@REM Optimized fdauto.bat for running BBS DOORs.
set DOSDIR=C:\FDOS
set LANG=EN
set TZ=UTC
set PATH=%dosdir%\BIN
set NLSPATH=%dosdir%\NLS
set HELPPATH=%dosdir%\HELP
set TEMP=%dosdir%\TEMP
set TMP=%TEMP%
set OS_NAME=FreeDOS
set OS_VERSION=1.3-RC4
alias halt=fdapm pureoff

LH FDAPM APMDOS
LH SHARE

if exist E:\DOOR.BAT goto DOOR
goto END

:DOOR
E:
cd\
CALL E:\DOOR.BAT
HALT

:END

