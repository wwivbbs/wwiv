<#
  WWIV Version 5.x
  Copyright (C)2014-2026, WWIV Software Services

  Licensed  under the  Apache License, Version  2.0 (the "License");
  you may not use this  file  except in compliance with the License.
  You may obtain a copy of the License at
  http://www.apache.org/licenses/LICENSE-2.0

  Unless  required  by  applicable  law  or agreed to  in  writing,
  software  distributed  under  the  License  is  distributed on an
  "AS IS"  BASIS, WITHOUT  WARRANTIES  OR  CONDITIONS OF ANY  KIND,
  either  express  or implied.  See  the  License for  the specific
  language governing permissions and limitations under the License.

.SYNOPSIS
  Checks that the prerequisites for building WWIV on Windows are installed.

.DESCRIPTION
  The Windows counterpart of builds/linux/install-prereqs.sh and the checks
  in cmake-config.sh. It checks for:
    * Visual Studio 2022 (17.2) or newer, or the Build Tools, with the
      C++ x86/x64 build tools (VC.Tools.x86.x64) installed
    * CMake (on PATH or bundled with Visual Studio), new enough for
      CMakePresets.json
    * Ninja (on PATH or bundled with Visual Studio)
    * Git
    * Initialised git submodules (vcpkg, deps/*)
  It prints a PASS/FAIL line for each, with a hint on how to fix failures,
  and exits with a non-zero status if anything failed.

  Works with Windows PowerShell 5.1 and PowerShell 7.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File builds\win\check-prereqs.ps1
#>

$ErrorActionPreference = 'Stop'

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$script:Failures = 0

function Write-Pass([string]$Name, [string]$Detail) {
  Write-Host ('[PASS] {0,-14} {1}' -f $Name, $Detail) -ForegroundColor Green
}

function Write-Fail([string]$Name, [string]$Detail, [string[]]$Fix) {
  $script:Failures++
  Write-Host ('[FAIL] {0,-14} {1}' -f $Name, $Detail) -ForegroundColor Red
  foreach ($line in $Fix) {
    Write-Host ('       {0}' -f $line) -ForegroundColor Yellow
  }
}

# Minimum CMake version: the larger of CMakePresets.json's cmakeMinimumRequired
# and CMakeLists.txt's cmake_minimum_required.
# TODO(5.10): when we branch for 5.10, also derive a minimum from the
# CMakePresets.json schema "version" (2->3.20, 3->3.21, 4->3.23, 5->3.24,
# 6->3.25, 7->3.27, 8->3.28, 9->3.30, 10->3.31) and use the largest of the
# three. Schema 4 needs CMake 3.23, which this check doesn't catch yet. Also
# default a missing major/minor/patch in cmakeMinimumRequired to 0, instead
# of letting the [version] cast throw and silently dropping that minimum.
function Get-MinCMakeVersion {
  $min = [version]'3.15'
  $lists = Join-Path $RepoRoot 'CMakeLists.txt'
  if (Test-Path $lists) {
    $m = Select-String -Path $lists -Pattern 'cmake_minimum_required\s*\(\s*VERSION\s+([0-9.]+)' |
      Select-Object -First 1
    if ($m) { $min = [version]$m.Matches[0].Groups[1].Value }
  }
  $presets = Join-Path $RepoRoot 'CMakePresets.json'
  if (Test-Path $presets) {
    try {
      $p = Get-Content -Raw $presets | ConvertFrom-Json
      $r = $p.cmakeMinimumRequired
      if ($r) {
        $v = [version]('{0}.{1}.{2}' -f $r.major, $r.minor, $r.patch)
        if ($v -gt $min) { $min = $v }
      }
    } catch {
      # Fall back to the CMakeLists.txt version.
    }
  }
  return $min
}

# Runs a native command with stderr discarded and returns its stdout.
# In Windows PowerShell 5.1, with $ErrorActionPreference = 'Stop', any stderr
# output from a native command (even redirected with 2>$null) becomes a
# terminating NativeCommandError, so relax the preference in this scope only.
# Callers check $LASTEXITCODE.
function Invoke-Native([string]$Exe, [string[]]$Arguments) {
  $ErrorActionPreference = 'Continue'
  & $Exe @Arguments 2>$null
}

# Returns the first tool found on PATH, else the copy bundled with Visual Studio.
function Find-Tool([string]$Exe, [string]$VsRelPath) {
  $cmd = Get-Command $Exe -CommandType Application -ErrorAction SilentlyContinue |
    Select-Object -First 1
  if ($cmd) { return $cmd.Source }
  if ($script:VsPath -and $VsRelPath) {
    $bundled = Join-Path $script:VsPath $VsRelPath
    if (Test-Path $bundled) { return $bundled }
  }
  return $null
}

Write-Host "Checking WWIV Windows build prerequisites in $RepoRoot"
Write-Host ''

# --- Visual Studio with the C++ workload ------------------------------------
$VcTools = 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64'
$VsWinget = 'winget install --id Microsoft.VisualStudio.2022.Community -e --override "--passive --wait --add Microsoft.VisualStudio.Workload.NativeDesktop --includeRecommended"'
$script:VsPath = $null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) {
  Write-Fail 'Visual Studio' 'not found (no vswhere.exe)' @(
    'Install Visual Studio 2022 or newer with the "Desktop development with C++" workload:',
    $VsWinget)
} else {
  # -products * includes the Build Tools as well as the IDE editions.
  $found = & $vswhere -latest -products * -version '[17.2,' -requires $VcTools `
    -property installationPath -nologo
  if ($found) {
    $script:VsPath = ($found | Select-Object -First 1).Trim()
    $name = & $vswhere -path $script:VsPath -property displayName -nologo
    $ver = & $vswhere -path $script:VsPath -property catalog_productDisplayVersion -nologo
    Write-Pass 'Visual Studio' ('{0} {1} ({2})' -f $name, $ver, $script:VsPath)
  } else {
    $any = & $vswhere -latest -products * -property displayName -nologo
    if ($any) {
      # Still use this install to look for its bundled CMake and Ninja.
      $script:VsPath = & $vswhere -latest -products * -property installationPath -nologo |
        Select-Object -First 1
      Write-Fail 'Visual Studio' "$any is installed, but not 17.2+ with the C++ x86/x64 tools" @(
        'Use the Visual Studio Installer to add the "Desktop development with C++" workload,',
        "or update to 17.2 or newer. (Component: $VcTools)")
    } else {
      Write-Fail 'Visual Studio' 'not found' @(
        'Install Visual Studio 2022 or newer with the "Desktop development with C++" workload:',
        $VsWinget)
    }
  }
}

# --- CMake --------------------------------------------------------------------
$MinCMake = Get-MinCMakeVersion
$cmake = Find-Tool 'cmake.exe' 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not $cmake) {
  Write-Fail 'CMake' "not found (need $MinCMake or newer)" @(
    'It comes with the Visual Studio C++ workload, or install it with:',
    'winget install --id Kitware.CMake -e')
} else {
  $out = Invoke-Native $cmake @('--version') | Select-Object -First 1
  $cmakeVer = $null
  if ($out -match '(\d+\.\d+\.\d+)') { $cmakeVer = [version]$Matches[1] }
  if (-not $cmakeVer) {
    Write-Fail 'CMake' "could not read the version of $cmake" @(
      'winget install --id Kitware.CMake -e')
  } elseif ($cmakeVer -lt $MinCMake) {
    Write-Fail 'CMake' "$cmakeVer is too old, need $MinCMake or newer ($cmake)" @(
      'winget upgrade --id Kitware.CMake -e  (or install it if it is not from winget)')
  } else {
    Write-Pass 'CMake' "$cmakeVer >= $MinCMake ($cmake)"
  }
}

# --- Ninja --------------------------------------------------------------------
$ninja = Find-Tool 'ninja.exe' 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
if ($ninja) {
  $nv = Invoke-Native $ninja @('--version') | Select-Object -First 1
  Write-Pass 'Ninja' "$nv ($ninja)"
} else {
  Write-Fail 'Ninja' 'not found' @(
    'It comes with the Visual Studio C++ workload, or install it with:',
    'winget install --id Ninja-build.Ninja -e')
}

# --- Git ----------------------------------------------------------------------
$git = Find-Tool 'git.exe' $null
if ($git) {
  $gv = Invoke-Native $git @('--version') | Select-Object -First 1
  Write-Pass 'Git' "$gv ($git)"
} else {
  Write-Fail 'Git' 'not found' @('winget install --id Git.Git -e')
}

# --- Submodules ---------------------------------------------------------------
$SubFix = 'Run: git submodule update --init --recursive'
if (-not $git) {
  Write-Fail 'Submodules' 'cannot check without git' @('Install git first, then: git submodule update --init --recursive')
} else {
  $status = Invoke-Native $git @('-C', $RepoRoot, 'submodule', 'status')
  if ($LASTEXITCODE -ne 0) {
    Write-Fail 'Submodules' "'git submodule status' failed in $RepoRoot" @(
      'Make sure this is a git clone of WWIV (git clone --recurse-submodules ...)')
  } elseif (-not $status) {
    Write-Pass 'Submodules' 'none required'
  } else {
    foreach ($line in @($status)) {
      # Format: <state char><sha1> <path> [(<describe>)]
      # '-' = not initialised, '+' = different commit checked out, 'U' = conflicts.
      $state = $line.Substring(0, 1)
      $path = ($line.Substring(1).Trim() -split '\s+')[1]
      if ($state -eq '-') {
        Write-Fail 'Submodule' "$path - not initialised" @($SubFix)
      } elseif ($state -eq 'U') {
        Write-Fail 'Submodule' "$path - has merge conflicts" @('Resolve the conflicts in the submodule, then: git submodule update --init --recursive')
      } elseif (-not (Test-Path (Join-Path $RepoRoot (Join-Path $path '*')))) {
        Write-Fail 'Submodule' "$path - directory is empty" @($SubFix)
      } elseif ($state -eq '+') {
        Write-Pass 'Submodule' "$path (checked out at a different commit than recorded)"
      } else {
        Write-Pass 'Submodule' $path
      }
    }
  }
}

Write-Host ''
if ($script:Failures -gt 0) {
  Write-Host "$($script:Failures) check(s) failed. Fix the above and run this script again." -ForegroundColor Red
  exit 1
}
Write-Host 'All prerequisites found. See README.md for how to build.' -ForegroundColor Green
exit 0
