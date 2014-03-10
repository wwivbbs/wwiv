call "C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\vcvarsall.bat" x86
echo "Subversion revision: %SVN_REVISION%"
cd %WORKSPACE%\bbs
%TEXT_TRANSFORM% -a !!version!%SVN_REVISION% version.template
msbuild bbs.vcxproj /t:Rebuild /p:Configuration=Release /detailedsummary
cd %WORKSPACE%\telsrv
msbuild WWIVTelnetServer.vcxproj /t:Rebuild /p:Configuration=Release /detailedsummary /property:EnableEnhancedInstructionSet=NoExtensions
cd %WORKSPACE%\WWIV5TelnetServer\WWIV5TelnetServer
msbuild WWIV5TelnetServer.csproj /t:Rebuild /p:Configuration=Release /detailedsummary
cd %WORKSPACE%\init
msbuild init.vcxproj /t:Rebuild /p:Configuration=Release /detailedsummary

cd %WORKSPACE%
mkdir release
del /q release
mkdir archives
del /q archives
copy bbs\Release\bbs.exe release\
copy bbs\readme.txt release\readme-bbs.txt
copy bbs\whatsnew.txt release\whatsnew.txt
copy bbs\telsrv\Release\WWIVTelnetServer.exe release\
copy bbs\telsrv\README.txt release\readme-telsrv.txt
copy WWIV5TelnetServer\WWIV5TelnetServer\bin\release\WWIV5TelnetServer.exe release\
copy init\Release\init.exe release\

echo Build URL %BUILD_URL% > release\build.nfo
echo Subversion Build: %SVN_REVISION% >> release\build.nfo

cd release
"C:\Program Files\7-Zip\7z.exe" a -tzip -y %WORKSPACE%\archives\wwiv-build-%SVN_REVISION%-%BUILD_NUMBER%.zip *
cd %WORKSPACE%
rem C:\bin\zip -j %WORKSPACE%\archives\wwiv-build-%SVN_REVISION%-%BUILD_NUMBER%.zip release\*
