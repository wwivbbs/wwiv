SET TEXT_TRANSFORM="C:\Program Files\Common Files\microsoft shared\TextTemplating\14.0\TextTransform.exe"
SET WORKSPACE=W:\wwiv
SET BUILD_NUMBER=0
SET SVN_REVISION=0
pushd %WORKSPACE%
call %WORKSPACE%\builds\jenkins\wwiv\build.cmd
popd

