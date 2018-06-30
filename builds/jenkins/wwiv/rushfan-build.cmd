SET WORKSPACE=W:\wwiv
SET BUILD_NUMBER=2112
SET SED="C:\Program Files\Git\usr\bin\sed.exe"
set label=win-x86
pushd %WORKSPACE%
copy %WORKSPACE%\core\version.cpp %WORKSPACE%\core\version.cpp.saved
call %WORKSPACE%\builds\jenkins\wwiv\build.cmd || echo "Build FAILED!"
copy %WORKSPACE%\core\version.cpp.saved %WORKSPACE%\core\version.cpp
popd

