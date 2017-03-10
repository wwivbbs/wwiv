echo "Subversion revision: %SVN_REVISION%"
echo "Workpace: %WORKSPACE%"
echo "WWIV_TEST_TEMPDIR: %WWIV_TEST_TEMPDIR%"

@if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" (
  call "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x86
)

@if exist "%ProgramFiles%\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" (
  call "%ProgramFiles%\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x86
)

cd %WORKSPACE%
msbuild WWIV.sln /t:Build /p:Configuration=Debug /p:Platform=Win32 || exit /b

cd %WORKSPACE%\Debug
del result.xml
dir
core_test.exe --gtest_output=xml:result-core.xml

cd %WORKSPACE%\Debug
del result.xml
dir
bbs_test.exe --gtest_output=xml:result-bbs.xml

cd %WORKSPACE%\Debug
del result.xml
dir
sdk_test.exe --gtest_output=xml:result-sdk.xml

cd %WORKSPACE%\Debug
del result.xml
dir
networkb_test.exe --gtest_output=xml:result-networkb.xml

