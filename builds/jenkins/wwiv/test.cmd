@rem **************************************************************************
@rem WWIV Test Script.
@rem
@rem **************************************************************************

set WWIV_CMAKE_DIR=%WORKSPACE%\_build
if not exist %WWIV_CMAKE_DIR% (
  echo Creating %WWIV_CMAKE_DIR%
  mkdir %WWIV_CMAKE_DIR%
)

if not exist %WWIV_TEST_TEMPDIR% (
  echo Creating %WWIV_TEST_TEMPDIR%
  mkdir %WWIV_TEST_TEMPDIR%
)

echo "Build Number:    %BUILD_NUMBER%"
echo "Workpace: %WORKSPACE%"
echo "WWIV_TEST_TEMPDIR: %WWIV_TEST_TEMPDIR%"
echo "WWIV CMake Root: %WWIV_CMAKE_DIR%"

@if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" (
  call "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x86
)

@if exist "%ProgramFiles%\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" (
  call "%ProgramFiles%\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x86
)

@rem build Cryptlib 1st.
echo:
echo * Building Cryptlib (zip/unzip)
cd %WORKSPACE%\deps\cl342
msbuild crypt32.vcxproj  /t:Build /p:Configuration=Release /p:Platform=Win32 || exit /b
set CL32_DLL="%WORKSPACE%\deps\cl342\Release\cl32.dll"

echo:
echo * Building WWIV Tests
cd %WWIV_CMAKE_DIR%
cmake -G "Ninja" -DCMAKE_BUILD_TYPE:STRING=Debug %WORKSPACE%   || exit /b
cmake --build .   || exit /b

cd %WWIV_CMAKE_DIR%\core_test
del result.xml
dir
core_tests.exe --gtest_output=xml:result-core.xml

cd %WWIV_CMAKE_DIR%\bbs_test
copy /y/v %CL32_DLL% .
del result.xml
dir
bbs_tests.exe --gtest_output=xml:result-bbs.xml

cd %WWIV_CMAKE_DIR%\sdk_test
del result.xml
dir
sdk_tests.exe --gtest_output=xml:result-sdk.xml

cd %WWIV_CMAKE_DIR%\networkb_test
del result.xml
dir
networkb_tests.exe --gtest_output=xml:result-networkb.xml

