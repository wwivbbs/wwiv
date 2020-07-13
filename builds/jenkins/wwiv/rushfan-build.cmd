SET WORKSPACE=W:\wwiv
SET BUILD_NUMBER=2112
set label=win-x86
pushd %WORKSPACE%
call %WORKSPACE%\builds\jenkins\wwiv\build.cmd || echo "Build FAILED!"
popd

