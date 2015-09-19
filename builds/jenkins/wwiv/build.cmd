@rem **************************************************************************
@rem WWIV 5.0 Build Script.
@rem
@rem Required Variables:
@rem WORKSPACE - wwiv-svn root directory
@rem BUILD_NUMBER - Jenkins Build number
@rem SVN_REVISION - subversion build number
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
echo TextTransform: %TEXT_TRANSFORM%
echo Workspace:     %WORKSPACE%         
echo Build Number:  %BUILD_NUMBER%
echo Archive:       %RELEASE_ZIP%

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
echo * Building CORE
cd %WORKSPACE%\core
msbuild core.vcxproj /t:Build /p:Configuration=Release || exit /b

echo:
echo * Building BBS
cd %WORKSPACE%\bbs
msbuild bbs_lib.vcxproj /t:Build /p:Configuration=Release || exit /b
msbuild bbs.vcxproj /t:Build /p:Configuration=Release || exit /b

echo:
echo * Building WWIV5TelnetServer
cd %WORKSPACE%\WWIV5TelnetServer\WWIV5TelnetServer
msbuild WWIV5TelnetServer.csproj /t:Build /p:Configuration=Release || exit /b

echo:
echo * Building INITLIB
cd %WORKSPACE%\initlib
msbuild initlib.vcxproj /t:Build /p:Configuration=Release || exit /b

echo:
echo * Building INIT
cd %WORKSPACE%\init
msbuild init.vcxproj /t:Build /p:Configuration=Release || exit /b

echo:
echo * Building NETWORKB
cd %WORKSPACE%\networkb
msbuild networkb.vcxproj /t:Build /p:Configuration=Release || exit /b

echo:
echo * Building NETWORK
cd %WORKSPACE%\network
msbuild network.vcxproj /t:Build /p:Configuration=Release || exit /b

echo:
echo * Building FIX
cd %WORKSPACE%\fix
msbuild fix.vcxproj /t:Build /p:Configuration=Release /property:EnableEnhancedInstructionSet=NoExtensions

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


@rem build DEPS
echo:
echo * Building INFOZIP (zip/unzip)
cd %WORKSPACE%\deps\infozip

msbuild unzip60\win32\vc8\unzip.vcxproj /t:Build /p:Configuration=Release || exit /b
msbuild zip30\win32\vc6\zip.vcxproj /t:Build /p:Configuration=Release || exit /b

cd %WORKSPACE%\
if not exist %WORKSPACE%\release (
  echo Creating %WORKSPACE\release
  mkdir %WORKSPACE%\release
)
del /q %WORKSPACE%\release
del wwiv-build-*.zip

echo:
echo * Creating Menus (EN)
cd %WORKSPACE%\bbs\admin\menus\en
%ZIP_EXE% a -tzip -r %WORKSPACE%\release\en-menus.zip *

echo:
echo * Creating Regions
cd %WORKSPACE%\bbs\admin
%ZIP_EXE% a -tzip -r %WORKSPACE%\release\zip-city.zip zip-city\*
%ZIP_EXE% a -tzip -r %WORKSPACE%\release\regions.zip regions\*

cd %WORKSPACE%\
echo:
echo * Copying BBS files to staging directory.
copy /v/y %WORKSPACE%\bbs\Release\bbs.exe %WORKSPACE%\release\bbs.exe
copy /v/y %WORKSPACE%\WWIV5TelnetServer\WWIV5TelnetServer\bin\release\WWIV5TelnetServer.exe %WORKSPACE%\release\WWIV5TelnetServer.exe
copy /v/y %WORKSPACE%\init\Release\init.exe %WORKSPACE%\release\init.exe
copy /v/y %WORKSPACE%\network\Release\network.exe %WORKSPACE%\release\network.exe
copy /v/y %WORKSPACE%\networkb\Release\networkb.exe %WORKSPACE%\release\networkb.exe
copy /v/y %WORKSPACE%\fix\Release\fix.exe %WORKSPACE%\release\fix.exe
copy /v/y %WORKSPACE%\bbs\admin\* %WORKSPACE%\release\

echo:
echo * Copying WINS files to staging area
set WINS=%WORKSPACE%\wins
copy /v/y %WINS%\exp\Release\exp.exe %WORKSPACE%\release\exp.exe
copy /v/y %WINS%\networkp\Release\networkp.exe %WORKSPACE%\release\networkp.exe
copy /v/y %WINS%\news\Release\news.exe %WORKSPACE%\release\news.exe
copy /v/y %WINS%\pop\Release\pop.exe %WORKSPACE%\release\pop.exe
copy /v/y %WINS%\pppurge\Release\pppurge.exe %WORKSPACE%\release\pppurge.exe
copy /v/y %WINS%\ppputil\Release\ppputil.exe %WORKSPACE%\release\ppputil.exe
copy /v/y %WINS%\qotd\Release\qotd.exe %WORKSPACE%\release\qotd.exe
copy /v/y %WINS%\uu\Release\uu.exe %WORKSPACE%\release\uu.exe

echo:
echo * Copying WINS sample filesto staging area
copy /v/y %WINS%\admin\* %WORKSPACE%\release\

echo:
echo * Copying INFOZIP files to staging area
set INFOZIP=%WORKSPACE%\deps\infozip
dir %INFOZIP%\unzip60\win32\vc8\unzip__Win32_Release\unzip.exe
dir %INFOZIP%\zip30\win32\vc6\zip___Win32_Release\zip.exe
copy /v/y %INFOZIP%\unzip60\win32\vc8\unzip__Win32_Release\unzip.exe %WORKSPACE%\release\unzip.exe
copy /v/y %INFOZIP%\zip30\win32\vc6\zip___Win32_Release\zip.exe %WORKSPACE%\release\zip.exe


echo:
echo * Creating build.nfo file
echo Build URL %BUILD_URL% > release\build.nfo
echo Subversion Build: %BUILD_NUMBER% >> release\build.nfo

echo:
echo * Creating release archive: %RELEASE_ZIP%
cd %WORKSPACE%\release
%ZIP_EXE% a -tzip -y %RELEASE_ZIP%

echo:
echo:
echo: **** SUCCESS ****
echo:
echo ** Archive File: %RELEASE_ZIP%
echo ** Archive contents:
%ZIP_EXE% l %RELEASE_ZIP%
endlocal

