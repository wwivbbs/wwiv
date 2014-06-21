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

@if exist "%ProgramFiles(x86)%\Microsoft Visual Studio 12.0\VC\vcvarsall.bat" (
  call "%ProgramFiles(x86)%\Microsoft Visual Studio 12.0\VC\vcvarsall.bat" x86
)

@if exist "%ProgramFiles%\Microsoft Visual Studio 12.0\VC\vcvarsall.bat" (
  call "%ProgramFiles%\Microsoft Visual Studio 12.0\VC\vcvarsall.bat" x86
)

set ZIP_EXE="C:\Program Files\7-Zip\7z.exe"
set RELEASE_ZIP=%WORKSPACE%\archives\wwiv-build-%SVN_REVISION%-%BUILD_NUMBER%.zip
echo Workspace: %WORKSPACE%         
echo Revision:  %SVN_REVISION%
echo Archive:   %RELEASE_ZIP%

@rem Build BBS, init, telnetserver
cd %WORKSPACE%\bbs
%TEXT_TRANSFORM% -a !!version!%SVN_REVISION% version.template
msbuild bbs_lib.vcxproj /t:Build /p:Configuration=Release /detailedsummary
msbuild bbs.vcxproj /t:Build /p:Configuration=Release /detailedsummary

cd %WORKSPACE%\WWIV5TelnetServer\WWIV5TelnetServer
msbuild WWIV5TelnetServer.csproj /t:Rebuild /p:Configuration=Release /detailedsummary

cd %WORKSPACE%\init
msbuild init.vcxproj /t:Build /p:Configuration=Release /detailedsummary

cd %WORKSPACE%\fix
msbuild fix.vcxproj /t:Build /p:Configuration=Release /detailedsummary /property:EnableEnhancedInstructionSet=NoExtensions

@rem build WINS
cd %WORKSPACE%\wins

msbuild exp\exp.vcxproj /t:Build /p:Configuration=Release /detailedsummary
msbuild network\network.vcxproj /t:Build /p:Configuration=Release /detailedsummary
msbuild news\news.vcxproj /t:Build /p:Configuration=Release /detailedsummary
msbuild ntime\ntime.vcxproj /t:Build /p:Configuration=Release /detailedsummary
msbuild pop\pop.vcxproj /t:Build /p:Configuration=Release /detailedsummary
msbuild pppurge\pppurge.vcxproj /t:Build /p:Configuration=Release /detailedsummary
msbuild ppputil\ppputil.vcxproj /t:Build /p:Configuration=Release /detailedsummary
msbuild qotd\qotd.vcxproj /t:Build /p:Configuration=Release /detailedsummary
msbuild uu\uu.vcxproj /t:Build /p:Configuration=Release /detailedsummary

cd %WORKSPACE%\
if not exist %WORKSPACE%\release (
  echo Creating %WORKSPACE\release
  mkdir %WORKSPACE%\release
)
del /q %WORKSPACE%\release
if not exist %WORKSPACE%\archives (
   echo Creating %WORKSPACE%\archives
   mkdir %WORKSPACE%\archives
)
del /q %WORKSPACE%\archives


echo Create Menus (EN)
cd %WORKSPACE%\bbs\admin\menus\en
%ZIP_EXE% a -tzip -r %WORKSPACE%\release\en-menus.zip *

cd %WORKSPACE%\bbs\admin
%ZIP_EXE% a -tzip -r %WORKSPACE%\release\zip-city.zip zip-city\*
%ZIP_EXE% a -tzip -r %WORKSPACE%\release\regions.zip regions\*

cd %WORKSPACE%\
echo Copying BBS files to staging directory.
copy /v/y %WORKSPACE%\bbs\Release\bbs.exe %WORKSPACE%\release\bbs.exe
copy /v/y %WORKSPACE%\bbs\readme.txt %WORKSPACE%\release\readme-bbs.txt
copy /v/y %WORKSPACE%\WWIV5TelnetServer\WWIV5TelnetServer\bin\release\WWIV5TelnetServer.exe %WORKSPACE%\release\WWIV5TelnetServer.exe
copy /v/y %WORKSPACE%\init\Release\init.exe %WORKSPACE%\release\init.exe
copy /v/y %WORKSPACE%\fix\Release\fix.exe %WORKSPACE%\release\fix.exe
copy /v/y %WORKSPACE%\bbs\admin\* %WORKSPACE%\release\

echo Copying WINS files to staging area
set WINS=%WORKSPACE%\wins
copy /v/y %WINS%\exp\Release\exp.exe %WORKSPACE%\release\exp.exe
copy /v/y %WINS%\network\Release\network.exe %WORKSPACE%\release\network.exe
copy /v/y %WINS%\news\Release\news.exe %WORKSPACE%\release\news.exe
copy /v/y %WINS%\ntime\Release\ntime.exe %WORKSPACE%\release\ntime.exe
copy /v/y %WINS%\pop\Release\pop.exe %WORKSPACE%\release\pop.exe
copy /v/y %WINS%\pppurge\Release\pppurge.exe %WORKSPACE%\release\pppurge.exe
copy /v/y %WINS%\ppputil\Release\ppputil.exe %WORKSPACE%\release\ppputil.exe
copy /v/y %WINS%\qotd\Release\qotd.exe %WORKSPACE%\release\qotd.exe
copy /v/y %WINS%\uu\Release\uu.exe %WORKSPACE%\release\uu.exe

echo Creating build.nfo file
echo Build URL %BUILD_URL% > release\build.nfo
echo Subversion Build: %SVN_REVISION% >> release\build.nfo

echo Creating release archive: %RELEASE_ZIP%
cd %WORKSPACE%\release
%ZIP_EXE% a -tzip -y %RELEASE_ZIP%

echo Archive File: %RELEASE_ZIP%
echo Archive contents:
%ZIP_EXE% l %RELEASE_ZIP%


