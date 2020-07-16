SET WORKSPACE=W:\wwiv
SET BUILD_NUMBER=2112
set label=win-x86
set VCVARS_ALL="C:\Program Files (x86)\Microsoft Visual Studio\2019\Preview\VC\Auxiliary\Build\\vcvarsall.bat"
pushd %WORKSPACE%
call %WORKSPACE%\builds\jenkins\wwiv\build.cmd || echo "Build FAILED!"
popd

