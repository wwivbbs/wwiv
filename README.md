WWIV BBS
========

# Building WWIV BBS
***

## Requirements

| Platform | Compiler | Tools |
| -------- | -------- | ----- |
| Windows  | Visual Studio 2022 (17.2) or newer, with the *Desktop development with C++* workload | Git, plus the CMake and Ninja that ship with Visual Studio |
| Linux    | GCC 14 or newer | Git, CMake 3.21+ (3.22+ on arm64), Ninja, make, ncurses and zlib dev packages |

CMake 3.21 or newer is needed to use the presets described below.

### A note on dependencies

WWIV pulls its third-party code in two ways:

* **Git submodules** (`vcpkg`, `deps/my_basic`, `deps/pdcurses`). You must clone
  with `--recurse-submodules`, or run `git submodule update --init --recursive`
  afterwards. CMake stops with a clear error if they are missing.
* **vcpkg** (fmt, cereal, cpp-httplib, nlohmann-json, gtest; see `vcpkg.json`).
  The vcpkg submodule is used automatically. **The first configure downloads and
  builds all of these, which can take several minutes.** That's normal, it hasn't
  hung, and later configures reuse the results.

## Getting the source

We prefer contributors to [fork](https://help.github.com/articles/fork-a-repo/)
the `wwivbbs/wwiv` repository and work from their fork:

```bash
# Use your GitHub username instead of YOUR-USERNAME
git clone --recurse-submodules -j8 https://github.com/YOUR-USERNAME/wwiv.git
cd wwiv
```

See [contributing.md](contributing.md) for how to keep your fork up to date.

## Building with CMake presets (recommended)

`CMakePresets.json` defines one preset per platform and build type:

| Preset | Platform |
| ------ | -------- |
| `windows-x86-debug`, `windows-x86-release` | Windows, 32-bit MSVC |
| `linux-debug`, `linux-release` | Linux, GCC |

Run `cmake --list-presets` to see the ones available on your machine. Each
preset builds into `_build/<preset-name>`.

### Windows

WWIV is built as a 32-bit (x86) program on Windows.

**Installing the prerequisites:** if you don't have them yet, you can install
Git and Visual Studio 2022 Community with the C++ workload (which includes CMake
and Ninja) using `winget`:

```bat
winget install --id Git.Git -e
winget install --id Microsoft.VisualStudio.2022.Community -e --override "--passive --wait --add Microsoft.VisualStudio.Workload.NativeDesktop --includeRecommended"
```

If you'd rather not use the ones bundled with Visual Studio, CMake and Ninja are
`winget install --id Kitware.CMake -e` and `winget install --id Ninja-build.Ninja -e`.

**Checking the prerequisites:** from the `wwiv` folder, run:

```bat
powershell -ExecutionPolicy Bypass -File builds\win\check-prereqs.ps1
```

It checks for Visual Studio 2022 (17.2) or newer (or the Build Tools) with the
C++ tools, CMake (new enough for the presets), Ninja, Git and the git
submodules. It prints a PASS or FAIL line for each, with how to fix any
failures, and exits non-zero if anything is missing.

**From Visual Studio:** choose *File → Open → Folder...* and pick your `wwiv`
folder. Visual Studio reads `CMakePresets.json`; pick `Windows x86 Debug` in the
configuration drop-down on the toolbar, wait for the configure to finish (the
first one is slow, see above), then use *Build → Build All*.

**From the command line:** open the *x86 Native Tools Command Prompt for VS 2022*
(the x86 one, not x64) and run:

```bat
cd path\to\wwiv
cmake --preset windows-x86-debug
cmake --build --preset windows-x86-debug
```

To run the tests:

```bat
ctest --preset windows-x86-debug
```

The build copies cryptlib's DLL (`cl32.dll`, from `deps\cryptlib`) next to every
program that needs it (`bbs.exe` and `bbs_tests.exe`), so there is nothing to copy
by hand.

### Linux

**NOTE:** Build as a non-root user; your BBS user is easiest from a file
permissions point of view. Never use root to compile.

Install the prerequisites. On Debian or Ubuntu you can run
`sudo builds/linux/install-prereqs.sh`, or install them yourself:

Package | Comments
------- | ----------
git | to get the source code
cmake | 3.21 or later (3.22 or later on arm64/aarch64)
ninja-build | 1.8 or later
g++ | 14 or later (`build-essential` on Debian/Ubuntu)
make | used to build cryptlib
ncurses | `libncurses-dev`, `ncurses-devel`, etc. depending on your distro
zlib | `zlib1g-dev` or `zlib-devel` depending on your distro
pkg-config, zip | used by the build and packaging

Then, from the `wwiv` directory:

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

If your CMake is older than 3.21, the older helper script still works:

```bash
mkdir _build && cd _build
../cmake-config.sh
cmake --build .
```

The script checks for the submodules and for Ninja before configuring. On
aarch64 it also sets `VCPKG_FORCE_SYSTEM_BINARIES=1`, which vcpkg needs there.
If you configure by hand on arm64, set that variable yourself.

### What gets built

Each program ends up in its own subdirectory of the build directory, e.g.
`_build/linux-debug/bbs/bbs`. The main ones are:

* `bbs/bbs`
* `wwivconfig/wwivconfig`
* `wwivd/wwivd`
* `wwivutil/wwivutil`
* `wwivfsed/wwivfsed`
* `network/network`, `networkb/networkb`, `networkc/networkc`,
  `networkf/networkf`, `networkt/networkt`, `network1/network1`,
  `network2/network2`, `network3/network3`

This only builds the binaries. It does not include the BBS data files. To set up
a BBS, follow the [Linux Installation](http://docs.wwivbbs.org/en/latest/linux_installation/)
instructions under *Steps to install the software* first, then point your
install at the binaries you built. For example (as the user that owns the
source, with the source in `$HOME/git/wwiv`):

```bash
cd /opt/wwiv
ln -s $HOME/git/wwiv/builds/tools/linux/use-built-bin.sh
./use-built-bin.sh $HOME/git/wwiv/_build/linux-debug bbs wwivd wwivconfig network network?
```

### Packaging (release builds)

Packaging is off by default, so the debug presets and plain `cmake` configures
only build the programs. Two CMake options turn it on:

Option | What it does
------ | ------------
`WWIV_INSTALL` | Adds the `install` rules (binaries, platform files, docs) that `cmake --install` and `cpack` use.
`WWIV_ZIP_INSTALL_FILES` | Also builds `data.zip`, `gfiles.zip`, `menus.zip`, etc. into `<build>/release`. Needs `WWIV_INSTALL`.

The `windows-x86-release` and `linux-release` presets, and the release scripts in
`builds/jenkins/wwiv` (used by CI), turn both on. To package any other build, add
`-DWWIV_INSTALL=ON -DWWIV_ZIP_INSTALL_FILES=ON` when you configure, then run
`cpack -G ZIP` (or `-G TGZ`) in the build directory. Without `WWIV_INSTALL`,
`cpack` fails with `CPack Error: CPack project name not specified`.

You also need the zip files to set up a new test BBS from your build:
wwivconfig unpacks `data.zip` and `inifiles.zip` when it creates a new BBS, so
configure that build directory with both options first.

These are only defaults for new build directories. CMake keeps the values
already in an existing `CMakeCache.txt`, so a `_build` or `out` directory
configured before this change still has both ON. Delete the cache, or pass
`-DWWIV_INSTALL=OFF -DWWIV_ZIP_INSTALL_FILES=OFF`, to pick up the new defaults.

### Faster builds

The build uses these automatically when they are installed; nothing else to
configure. Re-run the configure step after installing one.

Tool | What it does
---- | ------------
[ccache](https://ccache.dev/) or [sccache](https://github.com/mozilla/sccache) | Caches compiled objects, so rebuilding unchanged files (after switching branches, or in a fresh build directory) is nearly instant. On Linux: `sudo apt install ccache`. With MSVC this needs CMake 3.25 or newer, and the build then puts debug info in the object files (`/Z7`) instead of a shared PDB.
[mold](https://github.com/rui314/mold) or lld | A much faster linker than the default GNU `ld` (Linux only). On Debian/Ubuntu: `sudo apt install mold` (needs GCC 12+) or `sudo apt install lld`.

To turn them off, configure with `-DWWIV_USE_COMPILER_CACHE=OFF` or
`-DWWIV_USE_FAST_LINKER=OFF`. The release build scripts in `builds/jenkins/wwiv`
turn the compiler cache off, so released binaries are always compiled from
scratch. Linux releases and the PR builds both link with mold (the CI jobs
install it), so CI links the same way the releases do. Building only what you
are working on also helps, e.g. `cmake --build --preset linux-debug --target bbs`.

### Troubleshooting

* **"Missing ... the git submodules have not been checked out"**: run
  `git submodule update --init --recursive` and configure again.
* **"wwiv requires an out of source build"**: you ran CMake from inside the source
  directory. Delete the `CMakeCache.txt` file and `CMakeFiles` directory it
  created there, then use a preset or build from `_build`.
* **Windows: wrong architecture or link errors about x64**: make sure you are in
  the *x86* Native Tools prompt, not the x64 one. Then delete
  `_build/<preset>` and configure again.
* **Problems with cryptlib**: cryptlib is only used for SSH support in the BBS.
  You can build without it by turning it off when you configure, e.g.
  `cmake --preset windows-x86-debug -DWWIV_SSH_CRYPTLIB=OFF` (or
  `cmake --preset linux-debug -DWWIV_SSH_CRYPTLIB=OFF`). Everything else builds
  and works as normal; the BBS just won't accept SSH connections. On Linux this
  also skips the cryptlib `make` step.

## Building on ChromeOS/ChromeBook 

* Tested successfully on
  * ChromeOS versions 99.x.x and higher
  * CPU/Arch 64bit (i3, i5, aarch64, mediatek arm64)
  * 4GB+ Memory

* Tested un-successful on (Was unable to get build tools installed)
  * ChromeOS version below 99.x.x
  * CPU/Arch 32bit
  * Under 4GB 


* Notes: 
  * Not really for production, but great for developing and testing.
  * Use Linux build directions
  * Telnet/SSH/Status Ports must be above 10,000 due to limitations on ChromeOS
  * If running on arm64, or aarch64 CPU, use cmake 3.22.x or higher
  * Recommend installing under your primary user instead of "wwiv" for simplicity sake. (Again not recommended for production use)
    * If you choose to use a new user be sure to restart the entire linux sub-system after install. 

***
Installation and SysOp Instructions
====================

All the installation and SysOp administration information you 
need is in the [WWIV Documentation](https://docs.wwivbbs.org/)

***

Get Involved
====================

If you want to help out with WWIV BBS:

* Read the [Contributors Guidelines](contributing.md)
* Check out the [WWIV BBS Homepage](https://www.wwivbbs.org) and find us on IRC.
* Jump into the [Issues List](https://github.com/wwivbbs/wwiv/issues).
