SET TEXT_TRANSFORM="C:\Program Files\Common Files\microsoft shared\TextTemplating\14.0\TextTransform.exe"
SET WORKSPACE=W:\wwiv
SET BUILD_NUMBER=2112
pushd %WORKSPACE%
copy %WORKSPACE%\core\version.cpp %WORKSPACE%\core\version.cpp.saved
call %WORKSPACE%\builds\jenkins\wwiv\build.cmd || echo "Build FAILED!"
copy %WORKSPACE%\core\version.cpp.saved %WORKSPACE%\core\version.cpp
popd

