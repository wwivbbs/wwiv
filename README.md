WWIV BBS
========

# Building WWIV BBS
***

## Requirements

| Platform | Compiler | Tools |
| -------- | -------- | ----- |
| Windows  | Visual Studio 2022 (17.2) or newer, with the *Desktop development with C++* workload | Git, plus the CMake and Ninja that ship with Visual Studio |
| Linux    | GCC 10 or newer | Git, CMake 3.21+ (3.22+ on arm64), Ninja, make, ncurses and zlib dev packages |

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

To run the tests, copy the cryptlib DLL next to the build first. The tests need it
and the build doesn't copy it yet:

```bat
copy install\platform\win32\cl32.dll _build\windows-x86-debug\
ctest --preset windows-x86-debug
```

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
g++ | 10 or later (`build-essential` on Debian/Ubuntu)
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

### Troubleshooting

* **"Missing ... the git submodules have not been checked out"**: run
  `git submodule update --init --recursive` and configure again.
* **"wwiv requires an out of source build"**: you ran CMake from inside the source
  directory. Delete the `CMakeCache.txt` file and `CMakeFiles` directory it
  created there, then use a preset or build from `_build`.
* **Windows: wrong architecture or link errors about x64**: make sure you are in
  the *x86* Native Tools prompt, not the x64 one. Then delete
  `_build/<preset>` and configure again.

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
