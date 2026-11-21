# Third-party dependencies
***

WWIV gets its third-party code from three places:

1. **vcpkg** (see `vcpkg.json` at the top of the tree): fmt, cereal,
   cpp-httplib, nlohmann-json and googletest (gtest). These are built by the
   `vcpkg` submodule the first time you configure with CMake. Nothing for
   them lives in this directory.
2. **Git submodules** in this directory (see `.gitmodules`).
3. **Code checked directly into this directory.**

Run `git submodule update --init --recursive` after cloning so that the
submodules are present.

## What's in deps/

### My-Basic
* dir: `my_basic` (submodule)
* fork: https://github.com/wwivbbs/my_basic
* upstream: https://github.com/paladin-t/my_basic
* license: MIT
* Used by the BBS's BASIC script support (`bbs/basic`). The `my_basic`
  static library target is defined in `deps/CMakeLists.txt` (outside the
  submodule) and linked into `bbs_lib`.

### PDCurses
* dir: `pdcurses` (submodule)
* fork: https://github.com/wwivbbs/PDCurses
* upstream: https://github.com/wmcbrine/PDCurses
* license: Public Domain
* Only built on Windows and OS/2. Linux uses the system ncurses.

### Cryptlib
* dir: `cl345` (checked in, not a submodule)
* url: https://github.com/cryptlib/cryptlib
* license: see `cl345/COPYING`
* Provides SSH support when `WWIV_SSH_CRYPTLIB` is on. On Windows the
  prebuilt `cl32.lib`/`cl32.dll` are used; elsewhere it is built with its
  own makefile.

## How to add a new dependency

Prefer vcpkg if the library is available there: add it to `vcpkg.json` and
use `find_package(...)` in CMake.

Otherwise, add it as a submodule:

1. Fork the repo to GitHub under wwivbbs. We use this as a way to version
   the dependencies we use, and can test them from there.
2. Add a submodule, usually like this:
   ```git submodule add https://github.com/wwivbbs/module deps/module```
3. Add a CMake target for it. If the project has its own usable
   `CMakeLists.txt`, `add_subdirectory()` it from the top level
   `CMakeLists.txt`; otherwise define the target in `deps/CMakeLists.txt`,
   the way `my_basic` is, so the submodule itself stays unmodified.
4. Add it to the list above and commit the change.
