@echo off
REM network3.bat

break=off
lredir e: linux\fsREPLACE-WWIVBASE

e:
cd \
network3.exe y %1
exitemu
