@ECHO OFF
set DOSDIR=C:\FDOS
set LANG=EN
set TZ=UTC
set PATH=%dosdir%\BIN
set NLSPATH=%dosdir%\NLS
set HELPPATH=%dosdir%\HELP
set TEMP=%dosdir%\TEMP
set TMP=%TEMP%
set BLASTER=A220 I5 D1 H5 P330
set DIRCMD=/P /OGN /Y
set COPYCMD=/-Y
set OS_NAME=FreeDOS
set OS_VERSION=1.3-RC4
set autofile=C:\FDAUTO.BAT
set cfgfile=C:\FDCONFIG.SYS
alias halt=fdapm pureoff

LH FDAPM APMDOS
LH SHARE

if exist D:\DOOR.BAT goto DOOR
goto END

:DOOR
E:
cd\
CALL E:\DOOR.BAT
HALT

:END

