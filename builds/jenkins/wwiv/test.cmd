@echo off
@rem **************************************************************************
@rem WWIV Test Script.
@rem
@rem **************************************************************************

if /I "%LABEL%"=="win-x86" (
	@echo "Setting x86 (32-bit) Architecture"
	set ARCH=x86
	set NUM_BITS=32
)
if /I "%LABEL%"=="win-x64" (
	@echo "Setting x64 (64-bit) Architecture"
	set ARCH=x64
	set NUM_BITS=64
)

rem TODO(rushfan) Change to 2019 once it works for command line builds.
set VS_VERSION=2017
set VS_BUILDTOOLS_DIR=Microsoft Visual Studio\%VS_VERSION%\BuildTools\VC\Auxiliary\Build\
set VS_COMMUNITY_DIR=Microsoft Visual Studio\%VS_VERSION%\Community\VC\Auxiliary\Build\

echo =============================================================================
echo "Build Number:         %BUILD_NUMBER%"
echo "Label:                %LABEL%"
echo "Architecture:         %ARCH%"
echo "Workpace:             %WORKSPACE%"
echo "WWIV_TEST_TEMPDIR:    %WWIV_TEST_TEMPDIR%"
echo "WWIV CMake Root:      %WWIV_CMAKE_DIR%"
echo "Visual Studio Ver:    %VS_VERSION%"
echo "Visual Studio Ed:     %VS_EDITION%"
echo "Visual Studio DIR:    %VS_INSTALL_DIR%"
echo "WindowsSdkVerBinPath  %WindowsSdkVerBinPath%"
echo "WindowsLibPath        %WindowsLibPath%"
echo "INCLUDE               %INCLUDE%"
echo =============================================================================

rem ===============================================================================

@if exist "%ProgramFiles(x86)%\%VS_BUILDTOOLS_DIR%\vcvarsall.bat" (
  echo "%ProgramFiles(x86)%\%VS_BUILDTOOLS_DIR%\vcvarsall.bat" %ARCH%
  call "%ProgramFiles(x86)%\%VS_BUILDTOOLS_DIR%\vcvarsall.bat" %ARCH%
  set VS_EDITION="BuildTools"
  set VS_INSTALL_DIR=%VS_BUILDTOOLS_DIR%
)

@if exist "%ProgramFiles%\%VS_BUILDTOOLS_DIR%\vcvarsall.bat" (
  echo "%ProgramFiles%\%VS_BUILDTOOLS_DIR%\vcvarsall.bat" %ARCH%
  call "%ProgramFiles%\%VS_BUILDTOOLS_DIR%\vcvarsall.bat" %ARCH%
  set VS_EDITION="BuildTools"
  set VS_INSTALL_DIR=%VS_BUILDTOOLS_DIR%
)

@if exist "%ProgramFiles(x86)%\%VS_COMMUNITY_DIR%\vcvarsall.bat" (
  echo "%ProgramFiles(x86)%\%VS_COMMUNITY_DIR%\vcvarsall.bat" %ARCH%
  call "%ProgramFiles(x86)%\%VS_COMMUNITY_DIR%\vcvarsall.bat" %ARCH%
  set VS_EDITION="Community"
  set VS_INSTALL_DIR=%VS_COMMUNITY_DIR%
)

@if exist "%ProgramFiles%\%VS_COMMUNITY_DIR%\vcvarsall.bat" (
  echo "%ProgramFiles%\%VS_COMMUNITY_DIR%\vcvarsall.bat" %ARCH%
  call "%ProgramFiles%\%VS_COMMUNITY_DIR%\vcvarsall.bat" %ARCH%
  set VS_EDITION="Community"
  set VS_INSTALL_DIR=%VS_COMMUNITY_DIR%
)

set WWIV_CMAKE_DIR=%WORKSPACE%\_build
if not exist %WWIV_CMAKE_DIR% (
  echo Creating %WWIV_CMAKE_DIR%
  mkdir %WWIV_CMAKE_DIR%
)

if not exist %WWIV_TEST_TEMPDIR% (
  echo Creating %WWIV_TEST_TEMPDIR%
  mkdir %WWIV_TEST_TEMPDIR%
)


@echo on
@rem build Cryptlib 1st.
echo * Building Cryptlib (zip/unzip)
cd %WORKSPACE%\deps\cl342
msbuild crypt32.vcxproj  /t:Build /p:Configuration=Release /p:Platform=Win32 || exit /b
set CL32_DLL="%WORKSPACE%\deps\cl342\Release\cl32.dll"

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

