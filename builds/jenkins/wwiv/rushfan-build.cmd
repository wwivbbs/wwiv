SET TEXT_TRANSFORM="C:\Program Files\Common Files\microsoft shared\TextTemplating\14.0\TextTransform.exe"
SET WORKSPACE=W:\wwiv
SET BUILD_NUMBER=2112
pushd %WORKSPACE%
call %WORKSPACE%\builds\jenkins\wwiv\build.cmd
popd

