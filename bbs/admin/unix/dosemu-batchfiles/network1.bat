@echo off
REM network1.bat

break=off
lredir e: linux\fsREPLACE-WWIVBASE

e:
cd \
network1.exe %1
exitemu
