rem ************************************************************************************************
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
rem ************************************************************************************************

call "C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\vcvarsall.bat" x86
echo "Subversion revision: %SVN_REVISION%"

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

cd %WORKSPACE%
mkdir release
del /q release
mkdir archives
del /q archives
copy bbs\Release\bbs.exe release\
copy bbs\readme.txt release\readme-bbs.txt
copy bbs\whatsnew.txt release\whatsnew.txt
copy WWIV5TelnetServer\WWIV5TelnetServer\bin\release\WWIV5TelnetServer.exe release\
copy init\Release\init.exe release\
copy fix\Release\fix.exe release\

echo Build URL %BUILD_URL% > release\build.nfo
echo Subversion Build: %SVN_REVISION% >> release\build.nfo

cd release
"C:\Program Files\7-Zip\7z.exe" a -tzip -y %WORKSPACE%\archives\wwiv-build-%SVN_REVISION%-%BUILD_NUMBER%.zip *

echo "Archive contents:"
"C:\Program Files\7-Zip\7z.exe" l %WORKSPACE%\archives\wwiv-build-%SVN_REVISION%-%BUILD_NUMBER%.zip
cd %WORKSPACE%

