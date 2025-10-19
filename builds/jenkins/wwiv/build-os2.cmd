@rem **************************************************************************
@rem WWIV Build Script.
@rem
@rem Required Variables:
@rem WORKSPACE - git root directory
@rem BUILD_NUMBER - Jenkins Build number
@rem
@rem Installed Software (all via apt)
@rem   GCC 9.2
@rem   7-Zip [C:\usr\bin\7z.exe]
@rem   cmake [in PATH]
@rem 
@rem **************************************************************************

@echo off
del wwiv-*.zip

set WWIV_ARCH=x86
set WWIV_DISTRO=os2
set ZIP_EXE=7z.exe
set WWIV_RELEASE=5.7.2
set WWIV_RELEASE_ARCHIVE_FILE=wwiv-%WWIV_DISTRO%-%WWIV_RELEASE%.%BUILD_NUMBER%.zip
set CMAKE_BINARY_DIR=%WORKSPACE%\_build
set WWIV_RELEASE_DIR=%CMAKE_BINARY_DIR%\release

@echo =============================================================================
@echo Workspace:            %WORKSPACE% 
@echo WWIV_ARCHitecture:    %WWIV_ARCH%
@echo WWIV_DISTRO:          %WWIV_DISTRO%
@echo WWIV Release:         %WWIV_RELEASE%        
@echo Build Number:         %BUILD_NUMBER%
@echo WWIV CMake Root:      %CMAKE_BINARY_DIR%
@echo WWIV_ARCHive:         %WWIV_RELEASE_ARCHIVE_FILE%
@echo Release Dir:          %WWIV_RELEASE_DIR%
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
cmake -DCMAKE_BUILD_TYPE=Release ^
      -DWWIV_RELEASE=%WWIV_RELEASE% ^
      -DWWIV_ARCH=%WWIV_ARCH%  ^
      -DWWIV_BUILD_NUMBER=%BUILD_NUMBER% ^
      -DWWIV_DISTRO=%WWIV_DISTRO% ^
      %WORKSPACE%

cmake --build . --config Release

@echo =============================================================================
@echo                           **** RUNNING TESTS ****
@echo =============================================================================
ctest --no-compress-output --output-on-failure -T Test 

echo * Creating release Archive: %WWIV_RELEASE_ARCHIVE_FILE%
cpack -G ZIP
