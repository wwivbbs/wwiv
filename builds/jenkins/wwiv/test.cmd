call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x86
echo "Subversion revision: %SVN_REVISION%"
echo "Workpace: %WORKSPACE%"
echo "WWIV_TEST_TEMPDIR: %WWIV_TEST_TEMPDIR%"

cd %WORKSPACE%\deps\gtest-1.7.0\msvc
msbuild gtest.vcxproj /t:Build /p:Configuration=Debug || exit /b

cd %WORKSPACE%\core
msbuild core.vcxproj /t:Build /p:Configuration=Debug || exit /b

cd %WORKSPACE%\bbs
msbuild bbs_lib.vcxproj /t:Build /p:Configuration=Debug || exit /b

cd %WORKSPACE%\core_test
msbuild core_fixtures.vcxproj /t:Build /p:Configuration=Debug || exit /b

cd %WORKSPACE%\core_test
msbuild core_test.vcxproj /t:Build /p:Configuration=Debug || exit /b

cd %WORKSPACE%\bbs_test
msbuild bbs_test.vcxproj /t:Build /p:Configuration=Debug || exit /b

cd %WORKSPACE%\sdk_test
msbuild sdk_test.vcxproj /t:Build /p:Configuration=Debug || exit /b

cd %WORKSPACE%\networkb_test
msbuild networkb_test.vcxproj /t:Build /p:Configuration=Debug || exit /b

cd %WORKSPACE%\core_test\Debug
del result.xml
dir
core_test.exe --gtest_output=xml:result-core.xml

cd %WORKSPACE%\bbs_test\Debug
del result.xml
dir
bbs_test.exe --gtest_output=xml:result-bbs.xml

cd %WORKSPACE%\sdk_test\Debug
del result.xml
dir
sdk_test.exe --gtest_output=xml:result-sdk.xml

cd %WORKSPACE%\networkb_test\Debug
del result.xml
dir
networkb_test.exe --gtest_output=xml:result-networkb.xml

