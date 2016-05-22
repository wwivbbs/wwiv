SET TEXT_TRANSFORM="C:\Program Files\Common Files\microsoft shared\TextTemplating\14.0\TextTransform.exe"
SET WORKSPACE=W:\wwiv
SET WWIV_TEST_TEMPDIR=W:\wwiv\test_tempdir
SET BUILD_NUMBER=2112
pushd %WORKSPACE%
call %WORKSPACE%\builds\jenkins\wwiv\test.cmd || echo "Tests FAILED!"
popd

