@rem **************************************************************************
@rem WWIV Build Script.
@rem
@rem Required Variables:
@rem WORKSPACE - git root directory
@rem BUILD_NUMBER - Jenkins Build number
@rem
@rem Installed Software:
@rem   7-Zip [C:\Program Files\7-Zip\7z.exe]
@rem   Visual Studio [C:\Program Files (x86)\Microsoft Visual Studio\VER]
@rem   cmake [in PATH, set by vcvarsall.bat]
@rem 
@rem **************************************************************************

setlocal
@echo off

del wwiv-*.zip

if /I "%LABEL%"=="win-x86" (
	@echo "Setting x86 (32-bit) Architecture"
	set WWIV_ARCH=x86
)
if /I "%LABEL%"=="win-x64" (
	@echo "Setting x64 (64-bit) Architecture"
	set WWIV_ARCH=x64
)
set WWIV_DISTRO=%LABEL%

set ZIP_EXE="C:\Program Files\7-Zip\7z.exe"
set WWIV_RELEASE=5.7.1
set WWIV_FULL_RELEASE=%WWIV_RELEASE%.%BUILD_NUMBER%
set WWIV_RELEASE_ARCHIVE_FILE=wwiv-%WWIV_DISTRO%-%WWIV_FULL_RELEASE%.zip
set CMAKE_BINARY_DIR=%WORKSPACE%\_build
set WWIV_RELEASE_DIR=%CMAKE_BINARY_DIR%\release

@rem ===============================================================================

call %VCVARS_ALL% %WWIV_ARCH%

@echo =============================================================================
@echo Workspace:            %WORKSPACE% 
@echo Label:                %LABEL%
@echo WWIV_ARCHitecture:    %WWIV_ARCH%
@echo WWIV_DISTRO:          %WWIV_DISTRO%
@echo WWIV Release:         %WWIV_RELEASE%        
@echo Build Number:         %BUILD_NUMBER%
@echo WWIV CMake Root:      %CMAKE_BINARY_DIR%
@echo WWIV_ARCHive:         %WWIV_RELEASE_ARCHIVE_FILE%
@echo Release Dir:          %WWIV_RELEASE_DIR%
@echo Visual Studio Shell:  %VCVARS_ALL%
@echo WindowsSdkVerBinPath  %WindowsSdkVerBinPath%
@echo WindowsLibPath        %WindowsLibPath%
@echo INCLUDE               %INCLUDE%
@echo =============================================================================

mkdir %CMAKE_BINARY_DIR%
del %CMAKE_BINARY_DIR%\CMakeCache.txt
rmdir /s/q %CMAKE_BINARY_DIR%\CMakeFiles
rmdir /s/q %CMAKE_BINARY_DIR%\Testing

cd %WORKSPACE%
mkdir %WWIV_RELEASE_DIR%
del /q %WWIV_RELEASE_DIR%
del /q wwiv-*.zip
del /q wwiv-*.exe

echo * Building WWIV
cd %CMAKE_BINARY_DIR%
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release ^
    -DWWIV_RELEASE=%WWIV_RELEASE% ^
    -DWWIV_ARCH=%WWIV_ARCH%  ^
    -DWWIV_BUILD_NUMBER=%BUILD_NUMBER% ^
    -DWWIV_DISTRO=%WWIV_DISTRO% ^
    %WORKSPACE% || exit /b

echo "Copy CL32.DLL so tests can use it in post-config"
copy /y/v %WORKSPACE%\install\platform\win32\cl32.dll %CMAKE_BINARY_DIR%
copy /y/v %WORKSPACE%\install\platform\win32\cl32.dll %CMAKE_BINARY_DIR%\bbs_test

cmake --build . --config Release || exit /b

@echo =============================================================================
@echo                           **** RUNNING TESTS ****
@echo =============================================================================
ctest --no-compress-output --output-on-failure -T Test 

echo * Creating release Archive: %WWIV_RELEASE_ARCHIVE_FILE%
cpack -G ZIP || exit /b 

cd %WORKSPACE%
copy /y/v %CMAKE_BINARY_DIR%\%WWIV_RELEASE_ARCHIVE_FILE% %WORKSPACE%\%WWIV_RELEASE_ARCHIVE_FILE%

echo **** SUCCESS ****
echo ** Archive File: %WORKSPACE%\%WWIV_RELEASE_ARCHIVE_FILE%
echo ** Archive contents:
%ZIP_EXE% l %WORKSPACE%\%WWIV_RELEASE_ARCHIVE_FILE%
endlocal
