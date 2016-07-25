@rem **************************************************************************
@rem WWIV Build Script.
@rem
@rem Required Variables:
@rem WORKSPACE - git root directory
@rem BUILD_NUMBER - Jenkins Build number
@rem TEXT_TRANSFORM - path to text transform tool from visual studio
@rem
@rem Installed Software:
@rem   7-Zip [C:\Program Files\7-Zip\7z.exe]
@rem   VS 2013 [C:\Program Files (x86)\Microsoft Visual Studio 12.0]
@rem   msbuild [in PATH, set by vcvarsall.bat]
@rem 
@rem **************************************************************************

setlocal

@if exist "%ProgramFiles(x86)%\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" (
  call "%ProgramFiles(x86)%\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x86
)

@if exist "%ProgramFiles%\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" (
  call "%ProgramFiles%\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x86
)

set ZIP_EXE="C:\Program Files\7-Zip\7z.exe"
set RELEASE_ZIP=%WORKSPACE%\wwiv-build-win-%BUILD_NUMBER%.zip
set STAGE_DIR=%WORKSPACE%\staging
echo TextTransform: %TEXT_TRANSFORM%
echo Workspace:     %WORKSPACE%         
echo Build Number:  %BUILD_NUMBER%
echo Archive:       %RELEASE_ZIP%
echo Staging Dir:   %STAGE_DIR%

@rem Build BBS, init, telnetserver
echo:
echo * Updating the Build Number in version.cpp
cd %WORKSPACE%\core

setlocal
rem
rem When LIB contains paths that do not exist, then TextTransform
rem fails, so we'll clear it out while running TEXT_TRANSFORM
rem
set LIB=
%TEXT_TRANSFORM% -a !!version!%BUILD_NUMBER% version.template || exit /b
endlocal

echo:
echo * Building WWIV
cd %WORKSPACE%
msbuild WWIV.sln /t:Build /p:Configuration=Release || exit /b

@rem build WINS
echo:
echo * Building WINS
cd %WORKSPACE%\wins

msbuild exp\exp.vcxproj /t:Build /p:Configuration=Release || exit /b
msbuild networkp\networkp.vcxproj /t:Build /p:Configuration=Release || exit /b
msbuild news\news.vcxproj /t:Build /p:Configuration=Release || exit /b
msbuild pop\pop.vcxproj /t:Build /p:Configuration=Release || exit /b
msbuild pppurge\pppurge.vcxproj /t:Build /p:Configuration=Release || exit /b
msbuild ppputil\ppputil.vcxproj /t:Build /p:Configuration=Release || exit /b
msbuild qotd\qotd.vcxproj /t:Build /p:Configuration=Release || exit /b
msbuild uu\uu.vcxproj /t:Build /p:Configuration=Release || exit /b


@rem build InfoZIP Zip/UnZip
echo:
echo * Building INFOZIP (zip/unzip)
cd %WORKSPACE%\deps\infozip

msbuild unzip60\win32\vc8\unzip.vcxproj /t:Build /p:Configuration=Release || exit /b
msbuild zip30\win32\vc6\zip.vcxproj /t:Build /p:Configuration=Release || exit /b

cd %WORKSPACE%\
if not exist %STAGE_DIR% (
  echo Creating %STAGE_DIR%
  mkdir %STAGE_DIR%
)
del /q %STAGE_DIR%
del wwiv-build-*.zip

echo:
echo * Creating Menus (EN)
cd %WORKSPACE%\bbs\admin\menus\en
%ZIP_EXE% a -tzip -r %STAGE_DIR%\en-menus.zip *

echo:
echo * Creating Regions
cd %WORKSPACE%\bbs\admin
%ZIP_EXE% a -tzip -r %STAGE_DIR%\zip-city.zip zip-city\*
%ZIP_EXE% a -tzip -r %STAGE_DIR%\regions.zip regions\*

cd %WORKSPACE%\
echo:
echo * Copying BBS files to staging directory.
copy /v/y %WORKSPACE%\Release\cl32.dll %STAGE_DIR%\cl32.dll || exit /b
copy /v/y %WORKSPACE%\Release\bbs.exe %STAGE_DIR%\bbs.exe || exit /b
copy /v/y %WORKSPACE%\WWIV5TelnetServer\WWIV5TelnetServer\bin\Release\WWIVServer.exe %STAGE_DIR%\WWIVServer.exe || exit /b
copy /v/y %WORKSPACE%\windows-wwiv-update\bin\Release\wwiv-update.exe %STAGE_DIR%\wwiv-update.exe || exit /b
copy /v/y %WORKSPACE%\Release\init.exe %STAGE_DIR%\init.exe || exit /b
copy /v/y %WORKSPACE%\Release\network.exe %STAGE_DIR%\network.exe || exit /b
copy /v/y %WORKSPACE%\Release\networkb.exe %STAGE_DIR%\networkb.exe || exit /b
copy /v/y %WORKSPACE%\Release\wwivutil.exe %STAGE_DIR%\wwivutil.exe || exit /b
copy /v/y %WORKSPACE%\bbs\admin\* %STAGE_DIR%\
copy /v/y %WORKSPACE%\bbs\admin\win32\* %STAGE_DIR%\

echo:
echo * Copying WINS files to staging area
set WINS=%WORKSPACE%\wins
copy /v/y %WINS%\exp\Release\exp.exe %STAGE_DIR%\exp.exe || exit /b
copy /v/y %WINS%\networkp\Release\networkp.exe %STAGE_DIR%\networkp.exe || exit /b
copy /v/y %WINS%\news\Release\news.exe %STAGE_DIR%\news.exe || exit /b
copy /v/y %WINS%\pop\Release\pop.exe %STAGE_DIR%\pop.exe || exit /b
copy /v/y %WINS%\pppurge\Release\pppurge.exe %STAGE_DIR%\pppurge.exe || exit /b
copy /v/y %WINS%\ppputil\Release\ppputil.exe %STAGE_DIR%\ppputil.exe || exit /b
copy /v/y %WINS%\qotd\Release\qotd.exe %STAGE_DIR%\qotd.exe || exit /b
copy /v/y %WINS%\uu\Release\uu.exe %STAGE_DIR%\uu.exe || exit /b

echo:
echo * Copying WINS sample filesto staging area
copy /v/y %WINS%\admin\* %STAGE_DIR%\

echo:
echo * Copying INFOZIP files to staging area
set INFOZIP=%WORKSPACE%\deps\infozip
dir %INFOZIP%\unzip60\win32\vc8\unzip__Win32_Release\unzip.exe
dir %INFOZIP%\zip30\win32\vc6\zip___Win32_Release\zip.exe
copy /v/y %INFOZIP%\unzip60\win32\vc8\unzip__Win32_Release\unzip.exe %STAGE_DIR%\unzip.exe
copy /v/y %INFOZIP%\zip30\win32\vc6\zip___Win32_Release\zip.exe %STAGE_DIR%\zip.exe


echo:
echo * Creating build.nfo file
echo Build URL %BUILD_URL% > release\build.nfo
echo Build: 5.1.0.%BUILD_NUMBER% >> release\build.nfo

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

