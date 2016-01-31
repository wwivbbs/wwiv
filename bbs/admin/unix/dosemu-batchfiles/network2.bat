@echo off
REM network2.bat

break=off
lredir e: linux\fsREPLACE-WWIVBASE

e:
cd \
network2.exe %1
exitemu
