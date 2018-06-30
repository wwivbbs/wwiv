@rem **************************************************************************
@rem WWIV Build Script.
@rem
@rem Required Variables:
@rem WORKSPACE - git root directory
@rem BUILD_NUMBER - Jenkins Build number
@rem
@rem Installed Software:
@rem   7-Zip [C:\Program Files\7-Zip\7z.exe]
@rem   VS 2013 [C:\Program Files (x86)\Microsoft Visual Studio 12.0]
@rem   msbuild [in PATH, set by vcvarsall.bat]
@rem   sed [in PATH]
@rem 
@rem **************************************************************************

setlocal

del wwiv-*.zip

if /I "%LABEL%"=="win-x86" (
	set ARCH="x86"
	set NUM_BITS="32"
)
if /I "%LABEL%"=="win-x64" (
	set ARCH="x64"
	set NUM_BITS="64"
)

set ZIP_EXE="C:\Program Files\7-Zip\7z.exe"
set WWIV_RELEASE=5.4
set WWIV_FULL_RELEASE=5.4.0
set RELEASE_ZIP=%WORKSPACE%\wwiv-win-%ARCH%-%WWIV_RELEASE%.%BUILD_NUMBER%.zip
set STAGE_DIR=%WORKSPACE%\staging
set WWIV_CMAKE_DIR=%WORKSPACE%\_build
echo =============================================================================
echo Workspace:         %WORKSPACE% 
echo Label:             %LABEL%
echo Architecture:      %ARCH%
echo WWIV Full Release: %WWIV_FULL_RELEASE%        
echo WWIV Release:      %WWIV_RELEASE%        
echo Build Number:      %BUILD_NUMBER%
echo WWIV CMake Root:   %WWIV_CMAKE_DIR%
echo Archive:           %RELEASE_ZIP%
echo Staging Dir:       %STAGE_DIR%
echo =============================================================================
echo Release Notes:
echo %RELEASE_NOTES%
echo =============================================================================

@if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" (
  call "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" %ARCH%
)

@if exist "%ProgramFiles%\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" (
  call "%ProgramFiles%\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" %ARCH%
)

if not exist %WWIV_CMAKE_DIR% (
  echo Creating %WWIV_CMAKE_DIR%
  mkdir %WWIV_CMAKE_DIR%
)

@rem build Cryptlib 1st.
echo:
echo * Building Cryptlib (zip/unzip)
cd %WORKSPACE%\deps\cl342
set cryptlib_platform=Win32
if /i "%arch%"=="x64" (set cryptlib_platform="x64")
msbuild crypt32.vcxproj /t:Build /p:Configuration=Release /p:Platform=%cryptlib_platform% || exit /b

@rem Build BBS, wwivconfig, telnetserver
echo:
echo * Updating the Build Number in version.cpp
cd %WORKSPACE%\core

%SED% -i -e "s@.development@.%BUILD_NUMBER%@" version.cpp

echo:
echo * Building WWIV
cd %WWIV_CMAKE_DIR%
cmake -G "Ninja" -DCMAKE_BUILD_TYPE:STRING=MinSizeRel %WORKSPACE% || exit /b
cmake --build .   || exit /b

@rem Building bits from the build tree.
@rem build InfoZIP Zip/UnZip
echo:
echo * Building INFOZIP (zip/unzip)
cd %WORKSPACE%\deps\infozip

@rem
@rem Always build this as 32-bit binaries nomatter what. 
@rem
msbuild unzip60\win32\vc8\unzip.vcxproj /t:Build /p:Configuration=Release /p:Platform=Win32 || exit /b
msbuild zip30\win32\vc6\zip.vcxproj /t:Build /p:Configuration=Release /p:Platform=Win32 || exit /b

cd %WORKSPACE%\
if not exist %STAGE_DIR% (
  echo Creating %STAGE_DIR%
  mkdir %STAGE_DIR%
)
del /q %STAGE_DIR%
del wwiv-*.zip

echo:
echo * Creating inifiles.zip
cd %WORKSPACE%\bbs\admin\inifiles
%ZIP_EXE% a -tzip -r %STAGE_DIR%\inifiles.zip *

echo:
echo * Creating data.zip
cd %WORKSPACE%\bbs\admin\data
%ZIP_EXE% a -tzip -r %STAGE_DIR%\data.zip *

echo:
echo * Creating gfiles.zip
cd %WORKSPACE%\bbs\admin\gfiles
%ZIP_EXE% a -tzip -r %STAGE_DIR%\gfiles.zip *

cd %WORKSPACE%\
echo:
echo * Copying BBS files to staging directory.
copy /v/y %WORKSPACE%\deps\cl342\Release\cl%NUM_BITS%.dll %STAGE_DIR%\cl%NUM_BITS%.dll || exit /b
copy /v/y %WWIV_CMAKE_DIR%\bbs\bbs.exe %STAGE_DIR%\bbs.exe || exit /b
copy /v/y %WWIV_CMAKE_DIR%\wwivconfig\wwivconfig.exe %STAGE_DIR%\wwivconfig.exe || exit /b
copy /v/y %WWIV_CMAKE_DIR%\network\network.exe %STAGE_DIR%\network.exe || exit /b
copy /v/y %WWIV_CMAKE_DIR%\network1\network1.exe %STAGE_DIR%\network1.exe || exit /b
copy /v/y %WWIV_CMAKE_DIR%\network2\network2.exe %STAGE_DIR%\network2.exe || exit /b
copy /v/y %WWIV_CMAKE_DIR%\network3\network3.exe %STAGE_DIR%\network3.exe || exit /b
copy /v/y %WWIV_CMAKE_DIR%\networkb\networkb.exe %STAGE_DIR%\networkb.exe || exit /b
copy /v/y %WWIV_CMAKE_DIR%\networkc\networkc.exe %STAGE_DIR%\networkc.exe || exit /b
copy /v/y %WWIV_CMAKE_DIR%\networkf\networkf.exe %STAGE_DIR%\networkf.exe || exit /b
copy /v/y %WWIV_CMAKE_DIR%\wwivd\wwivd.exe %STAGE_DIR%\wwivd.exe || exit /b
copy /v/y %WWIV_CMAKE_DIR%\wwivutil\wwivutil.exe %STAGE_DIR%\wwivutil.exe || exit /b
copy /v/y %WORKSPACE%\bbs\admin\* %STAGE_DIR%\
copy /v/y %WORKSPACE%\bbs\admin\win32\* %STAGE_DIR%\

echo:
echo * Copying INFOZIP files to staging area
set INFOZIP=%WORKSPACE%\deps\infozip
dir %INFOZIP%\unzip60\win32\vc8\unzip__Win32_Release\unzip.exe
dir %INFOZIP%\zip30\win32\vc6\zip___Win32_Release\zip.exe
copy /v/y %INFOZIP%\unzip60\win32\vc8\unzip__Win32_Release\unzip.exe %STAGE_DIR%\unzip.exe
copy /v/y %INFOZIP%\zip30\win32\vc6\zip___Win32_Release\zip.exe %STAGE_DIR%\zip.exe

echo:
echo * Creating scripts.zip
cd %WORKSPACE%\bbs\admin\scripts
%ZIP_EXE% a -tzip -r %STAGE_DIR%\scripts.zip *

echo:
echo * Creating Regions
cd %WORKSPACE%\bbs\admin
%ZIP_EXE% a -tzip -r %STAGE_DIR%\zip-city.zip zip-city\*
%ZIP_EXE% a -tzip -r %STAGE_DIR%\regions.zip regions\*

echo:
echo * Creating build.nfo file
echo Build URL %BUILD_URL% > release\build.nfo
echo Build: %WWIV_FULL_RELEASE%.%BUILD_NUMBER% >> release\build.nfo

echo:
echo * Creating release archive: %RELEASE_ZIP%
cd %STAGE_DIR%
%ZIP_EXE% a -tzip -y %RELEASE_ZIP%

echo:
echo:
echo: **** SUCCESS ****
echo:
echo ** Archive File: %RELEASE_ZIP%
echo ** Archive contents:
%ZIP_EXE% l %RELEASE_ZIP%
endlocal
