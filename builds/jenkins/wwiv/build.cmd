rem ****************************************************************************
rem WWIV 5.0 Build Script.
rem
rem Required Variables:
rem WORKSPACE - wwiv-svn root directory
rem BUILD_NUMBER - Jenkins Build number (arbitrary and monotonically increasing)
rem SVN_REVISION - subversion build number.
rem TEXT_TRANSFORM - path to text transform tool from visual studio
rem
rem Installed Software:
rem   7-Zip [C:\Program Files\7-Zip\7z.exe]
rem   VS 2013 [C:\Program Files (x86)\Microsoft Visual Studio 12.0]
rem   msbuild [in PATH, set by vcvarsall.bat]
rem   
rem 
rem ****************************************************************************

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
echo  Archive:  %RELEASE_ZIP%

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
copy %WORKSPACE%\bbs\Release\bbs.exe %WORKSPACE%\release\
copy %WORKSPACE%\bbs\readme.txt %WORKSPACE%\release\readme-bbs.txt
copy %WORKSPACE%\bbs\whatsnew.txt %WORKSPACE%\release\whatsnew.txt
copy %WORKSPACE%\WWIV5TelnetServer\WWIV5TelnetServer\bin\release\WWIV5TelnetServer.exe %WORKSPACE%\release\
copy %WORKSPACE%\init\Release\init.exe %WORKSPACE%\release\
copy %WORKSPACE%\fix\Release\fix.exe %WORKSPACE%\release\

echo Build URL %BUILD_URL% > release\build.nfo
echo Subversion Build: %SVN_REVISION% >> release\build.nfo

cd %WORKSPACE%\release
%ZIP_EXE% a -tzip -y %WORKSPACE%\archives\wwiv-build-%SVN_REVISION%-%BUILD_NUMBER%.zip *

echo Archive File: %RELEASE_ZIP%
echo Archive contents:
%ZIP_EXE% l %RELEASE_ZIP%
cd %WORKSPACE%\

