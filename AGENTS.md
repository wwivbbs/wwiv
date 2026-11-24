# AGENTS.md

WWIV is a C++23 BBS with its own networking stack (WWIVnet, FidoNet/FTN, BinkP). Builds on Windows (32-bit x86 MSVC only), Linux (GCC 14+), and partially OS/2.

## Build and test

Required submodules (`vcpkg`, `deps/my_basic`, `deps/pdcurses`, `deps/cryptlib`): `git submodule update --init --recursive`. First configure builds vcpkg deps (fmt, cereal, cpp-httplib, nlohmann-json, gtest) and takes several minutes—not hung.

```bash
cmake --preset linux-debug                 # configure into _build/linux-debug
cmake --build --preset linux-debug         # build everything
cmake --build --preset linux-debug --target bbs_tests   # build one target
ctest --preset linux-debug                 # run all tests
ctest --preset linux-debug -LE integration # skip slow SSH integration test
ctest --preset linux-debug -R 'EmailTest'  # run tests matching a regex
_build/linux-debug/sdk/sdk_tests --gtest_filter='NamesTest.*'   # run a gtest binary directly
```

Windows uses `windows-x86-debug` / `windows-x86-release` from an *x86* Native Tools prompt (x64 causes link errors). Don't run `cmake` builds concurrently across multiple subagents—it saturates the VM CPU.

- Test binaries (`_build/<preset>/<dir>/`): `core_tests`, `sdk_tests`, `common_tests`, `bbs_tests`, `fsed_tests`, `net_core_tests`, `binkp_tests`, `networkf_tests`, `wwivconfig_tests`, `wwivd_tests`, plus `wwivbasic_tests` (requires `WWIV_WWIVBASIC_NEXT=ON`).
- Unit tests sit next to code as `*_test.cpp` and must be added to that directory's `CMakeLists.txt`. When running `core_tests` or `sdk_tests` directly, pass `--wwiv_testdata=<repo>/<dir>/testdata` (ctest passes it; without it, testdata tests skip).
- Key CMake options: `WWIV_SSH_CRYPTLIB=OFF` (skip cryptlib/SSH), `WWIV_WWIVBASIC_NEXT=ON` (build experimental `wwivbasic/` and `bbs/basic/next/`), `WWIV_INSTALL=ON` + `WWIV_ZIP_INSTALL_FILES=ON` (packaging and `data.zip`/`inifiles.zip` for wwivconfig; on in `*-release` presets).
- CI (`.github/workflows/pr-build.yml`) builds and tests `linux-debug` and `windows-x86-debug` with `-DWWIV_WWIVBASIC_NEXT=ON -DWWIVBASIC_GENERATE_PARSER=OFF`. Windows is 32-bit x86—watch `size_t`/`int64_t` assumptions.
- Style & gotchas: `.clang-format` (LLVM, 2-space indent, 100 cols, left-aligned pointers); most targets use `-Wall -Wextra` / `/W4` (`set_max_warnings`). Include `"core/wwiv_windows.h"` instead of `<windows.h>`. Preserve each file's existing line endings—some files (e.g. `CMakeLists.txt`, `core/command_line.h`) use CRLF.

## Architecture

Includes are rooted at the repo root (`#include "core/strings.h"`). Libraries, roughly bottom-up:

- **`core/`**: platform-independent utilities (no WWIV domain knowledge): `File`/`TextFile`, strings, logging, `command_line`, ini/JSON, sockets, `Clock`/`FakeClock`, expression parser. Platform files use `*_unix.cpp` / `*_win32.cpp` (or `*_win.cpp`) / `*_os2.cpp`. Shared gtest fixtures live in `core/test/`.
- **`localui/`, `local_io/`**: local console UI and I/O (curses/PDCurses, plus native Win32 console in `local_io`).
- **`sdk/`**: WWIV data model and on-disk formats. `vardec.h` defines `#pragma pack(1)` legacy structs—do not change their binary layout. Users (`user.lst`) and names (`names.lst`) use these binary records; config, subs, dirs, chains, gfiles, menus, networks, and conferences serialize with cereal (`*_cereal.h`). Subdirs: `msgapi/` (`type2_text`, `email_wwiv`), `files/`, `fido/`, `net/`, `acs/`, `menus/`, `conf/`. Tools should access BBS data through `sdk/`.
- **`common/`**: session layer shared by `bbs` and tools: `Context`/`SessionContext` (`context.h`), `Output`/`Input` (globals `bout`/`bin`), remote I/O (socket/pipe), pipe-code expressions, printfile, menu helpers (`menus/`), message editor data.
- **`bbs/`**: built as `bbs_lib` + `bbs_main.cpp` (`bbs`). Global session `Application` (`application.h`) via `a()`; I/O via `bout`/`bin`. Most code is free functions in DOS-overlay-named files (`bbsovl1/2/3`, `bbsutl*`, `xferovl*`)—search by symbol, not filename. Subdirs: `menus/`, `qwk/`, `basic/`, `prot/` (legacy C zmodem/xmodem).
- **`fsed/`**: full-screen editor library used by `bbs` and standalone `wwivfsed/`.
- **`net_core/`, `binkp/`, `network*/`, `lnet/`**: networking. `network` dispatches; `networkb` handles BinkP (via `binkp/`); `networkc` drives inbound processing; `network1` splits pending packets into outbound/`local.net`; `network2` imports mail/posts; `network3` rebuilds node/routing files; `networkf` handles FTN import/export; `networkt` processes FTN TIC echoes; `lnet` is a packet viewer.
- **`wwivconfig/`** (sysop config TUI), **`wwivd/`** (telnet/SSH daemon launching `bbs`), **`wwivutil/`** (CLI subcommands).
- **Scripting**: `bbs/basic/` wraps `deps/my_basic`. `wwivbasic/` is the experimental ANTLR-based replacement—read `wwivbasic/AGENTS.md` before working there. It may depend only on `core/`; its BBS bindings live in `bbs/basic/next/`.
- **`install/`** (packaged data/menus/gfiles/scripts/platform files), **`builds/`** (CI/release scripts and prereq checkers).

## Direction for `bbs/` changes

See `todo-claude.md` and `todo-c-isms.md`. When touching `bbs/`:

- Pass `Context&` (and an output/input interface) to new/refactored functions instead of calling `a()`, `bout`, or `bin`—incrementally, file by file.
- Don't extend `bbs/` copies of `sdk/` data-layer code (`message_file.cpp`/`subacc.cpp` vs `sdk/msgapi/type2_text`; `readmail.cpp`/`email.cpp`/`attach.cpp`/`multmail.cpp` vs `sdk/msgapi/email_wwiv`). Migrate callers onto `sdk/msgapi`, and keep raw `Read(&rec, sizeof rec)` of `vardec.h` structs inside `sdk/`.
- When changing how `sdk/msgapi` (or the BBS's reader) reads message text, keep `bbs/msgapi_legacy_compare_test.cpp` passing, and if you have a real BBS, run `_build/<preset>/bbs/msgcompare <bbsdir>`. It reads every post with both the BBS's reader and `sdk/msgapi` and reports where they differ. It works on a copy, so `<bbsdir>` is never modified.
- Add characterization tests before refactoring a `bbs/` module.
- Replace C-isms (`strcpy`/`sprintf` into fixed buffers, `calloc`/`BbsAllocA`, raw `new`) with `std::string`, `fmt::format`, and RAII containers as you go.
