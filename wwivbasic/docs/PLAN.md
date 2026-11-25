# Plan: replacing my_basic in WWIV

This file is the current plan; tick items off here as they land. (An earlier
draft is in the shared doc "WWIVbasic: Replacing my_basic — Plan".)

## Goal

Replace the my_basic interpreter behind `RunBasicScript` in WWIV with
wwivbasic, then open up most of WWIV's SDK to scripts, so that someone could
write, for example, a message viewer in WWIVbasic. Existing scripts should
mostly run unchanged; intentional breaks are documented in
[PORTING.md](../PORTING.md).

Done when:

- `install/scripts/hello.bas` and `oneliners.bas` run, unchanged or with only
  edits that PORTING.md documents.
- The `wwiv`, `wwiv.io`, `wwiv.io.file`, `wwiv.data`, `wwiv.os` and
  `wwiv.time` modules exist with the same names and behaviour.
- WWIV runs scripts through wwivbasic by default and `deps/my_basic` is
  removed.
- Scripts can use SDK bindings (`wwiv.msg`, `wwiv.user`, `wwiv.files`).

Out of scope for the first release: GOTO/GOSUB, classes defined in BASIC,
lambdas, coroutines.

## Architecture

```
RunBasicScript(name)                       (WWIV)
   └─ ScriptEngine, picked at runtime
        ├─ LegacyEngine → bbs/basic/Basic (my_basic)          [always built]
        └─ NextEngine   → wwivbasic + bindings in WWIV        [WWIV_WWIVBASIC_NEXT]
```

- **wwivbasic** (`wwivbasic/`) is the language: parser, interpreter, standard
  library, and a stable embedding API (public headers for `Context`, `Module`,
  `Value` and the binding helpers). It has no dependency on WWIV's `sdk/` or
  `bbs/`.
- **`Console`** is the one interface the interpreter needs from its embedder:
  writing text, newlines, reading a line or a key, clearing the screen. WWIV
  implements it with `bout`/`bin`; tests use a fake; `basicrun` uses
  stdin/stdout.
- **Bindings** are modules the embedder registers through the embedding API.
  The `wwiv.*` modules and future SDK bindings live in WWIV next to the code
  they wrap (for example `bbs/basic/next/`), and are tested with WWIV's test
  helpers. Adding a binding never requires changing the interpreter.
- **Building and choosing the engine.** A CMake option (`WWIV_WWIVBASIC_NEXT`,
  off at first, on by default since M4 except on OS/2) decides whether the new engine is built at all, so WWIV builds
  without the ANTLR runtime until it's ready (the generated parser is checked
  in, so Java is never needed). In builds that have both, a runtime
  setting chooses the engine (the `script_flag_engine_next` bit in
  `config.json`'s `script_flags`, "Use New Engine" in wwivconfig's scripting
  settings, plus `bbs --basic_engine=legacy|next` for testing), so sysops
  can try it and fall back without rebuilding, and scripts can be run on both
  and their output compared.
- **Repo layout.** wwivbasic lives in the WWIV repo under `wwivbasic/`, a
  library next to `core/` and `sdk/` that depends only on `core/`, the ANTLR
  runtime and fmt. Its public headers are at the top of `wwivbasic/`
  (`#include "wwivbasic/interpreter.h"`), and its internals in
  `wwivbasic/internal/`. The engine switch and the `wwiv.*` bindings live in
  `bbs/basic/` (the new ones in `bbs/basic/next/`). It was developed in its
  own repo (github.com/wwiv/wwivbasic) through M3 and moved in at the start
  of M4, since from M4 on most changes touch both the interpreter and WWIV.

## Compatibility policy

Keep compatible: `'` comments, `WHILE/WEND`, `NOT`, `PRINT a, b` / `PRINT a; b`
without parentheses, `import "@wwiv.io"`, a trailing `;` on a statement,
module/function names and behaviour, case-insensitivity, and `wwiv.data`'s
on-disk format.

Intentional breaks (proposed): no GOTO/GOSUB, classes, lambdas or coroutines;
no dicts yet.

## Milestones

### M0: Build and test baseline

Done when the project builds with one command and the tests pass.

- [x] Build with current vcpkg, ANTLR 4.13.2 and fmt (`cmake --preset linux-debug`)
- [x] Golden-test runner: run each `.bas`, compare with `.expected` (wired into CTest)
- [x] Turn `bas/*.bas` into golden tests (`tests/golden/`)
- [x] Parse errors report file:line:column and exit non-zero
- [x] CI workflow (`.github/workflows/build.yml`) green on Linux and Windows

### M1: The existing scripts parse

Done when `hello.bas` and `oneliners.bas` parse without errors.

- [x] `'` comments, including at end of line
- [x] `WHILE expr ... WEND` (and `DO ... UNTIL expr`)
- [x] Unary `NOT` and unary minus (and fixed operator precedence)
- [x] `PRINT` statement with `,` and `;` separators
- [x] Accept and ignore `;` at the end of a statement
- [x] `import "@name"` is a module import
- [x] Unqualified lookup in imported modules
- [x] `BREAK` / `EXIT` out of loops; `END` stops the script
- [x] Negative / expression `STEP`

### M2: Runtime parity

Done when both scripts run against stub WWIV modules and match golden output.

- [x] List type in `Value` (reference semantics)
- [x] `list()`, `len`, `push`, `remove`, `get`, indexing, and the rest of my_basic's list functions
- [x] `FOR x IN l` (my_basic's syntax) and `iterator`/`move_next`/`get`
- [x] Runtime errors for unknown functions/modules and bad arity
- [x] `import "file.bas"` loads the file (relative to the importing script, once per file)
- [x] stdlib gaps the scripts need (`STR`, `LEN` on lists)
- [x] Stub `wwiv`, `wwiv.io`, `wwiv.data` modules; `hello.bas` and `oneliners.bas` run unchanged as golden tests

### M3: Embedding API (wwivbasic)

Done when a binding can expose a C++ class to scripts, and a fake-backed test
binding shows `obj.property`, `obj.method()` and `FOR x IN obj.items()`
working.

- [x] Public headers for the embedding API (`Interpreter`, `Module`, `Value`,
      native functions, errors) at the top of `wwivbasic/`; internals in
      `wwivbasic/internal/`
- [x] `Console` interface; `PRINT` and the console go through it; a fake for
      tests and a stdin/stdout one for `basicrun`
- [x] Object values: a `Value` that wraps a C++ object, shared by reference
      like lists
- [x] Class binding helper, roughly
      `m.class_<T>("name").property(...).method(...)`
- [x] Member access in the grammar: `.` becomes an operator and `a.b.c` is
      resolved at runtime (a module path if there is one, otherwise members
      of a variable), so `wwiv.io.outstr()` keeps working and
      `area.headers().count` parses
- [x] Floating-point numbers (`REAL`); `wwiv.data` files can contain REALs
- [x] Build options for embedding: use the embedder's `core` target when one
      exists, and skip `basicrun` and the tests
- [x] Check in the generated parser (`generated/`), regenerated when the
      grammar changes and Java is available, so WWIV's build doesn't need Java

### M4: Integrate into WWIV (behind a CMake option)

Done when, with `WWIV_WWIVBASIC_NEXT=ON`, WWIV runs `hello.bas` and
`oneliners.bas` on either engine (chosen at runtime) with the same output, and
WWIV's existing `bbs/basic` tests pass against the new engine.

- [x] Move wwivbasic into the WWIV repo under `wwivbasic/` (with its
      history); CMake option `WWIV_WWIVBASIC_NEXT` (default OFF), which also
      enables the `wwivbasic` vcpkg feature (antlr4); build against WWIV's
      `core/`, dropping the vendored copy
- [x] `core`: `CommandLine::set_dot_args_positional()`, so `basicrun ./x.bas`
      works without changing how other programs treat `.N` arguments
- [x] `ScriptEngine` interface in WWIV (`bbs/basic/script_engine.h`); today's
      `Basic` becomes the legacy engine; `RunBasicScript` picks the engine
      from `config.json` (`script_flags` bit `0x0008`) and
      `bbs --basic_engine=legacy|next`. With `--debug`, `NextEngine`
      (`bbs/basic/next/`) warns and runs without the debugger until M6
- [x] `Console` implemented with `bout`/`bin` (`bbs/basic/next/bbs_console.*`);
      `NextEngine` runs scripts, without the `wwiv.*` modules yet
- [x] `wwiv.*` bindings in WWIV (`bbs/basic/next/`), same names and
      behaviour: `wwiv`, `wwiv.io`, `wwiv.data` (same on-disk format),
      `wwiv.time`, and `wwiv.io.file` / `wwiv.os` only when the sysop enables
      them. `wwiv.io.file` gains `WRITE`, `WRITELINE` and `CLOSE`
- [x] Port WWIV's `bbs/basic/basic_test.cpp` cases to run against the new
      engine (each case runs on every engine in the build)
- [x] Side-by-side check: run scripts on both engines and diff the output
      (`bbs/basic/next/side_by_side_test.cpp`: `hello.bas` matches apart from
      the documented PRINT layout, `oneliners.bas` matches exactly, and the
      engines read each other's `wwiv.data` files)
- [x] WWIV CI builds with the option on (both jobs), and fails if the
      checked-in parser doesn't match the grammar
- [x] Build the new engine by default (`WWIV_WWIVBASIC_NEXT` defaults to ON,
      except on OS/2); scripts still run on the legacy engine unless
      `config.json` or `--basic_engine` picks the new one

### M5: SDK bindings (WWIV)

Done when a message viewer written in WWIVbasic can list areas, list message
headers and read messages.

- [x] `wwiv.msg`: message areas, headers and text (read-only first;
      `bbs/basic/next/wwiv_msg.cpp`, documented in [BINDINGS.md](BINDINGS.md)).
      It reads through a `MessageBase` interface (`BbsMessageBase` in the
      BBS, a fake in tests)
- [ ] `wwiv.msg` writing: post, and change or delete posts, with the BBS's
      rules for who may (BINDINGS.md says it's read-only for now)
- [x] `wwiv.user`: the current user, and user lookups (read-only;
      `bbs/basic/next/wwiv_user.cpp`, documented in [BINDINGS.md](BINDINGS.md)).
      Other users show only their name, number and last logon
- [ ] `wwiv.files`: file areas and file records
- [ ] Permissions: access follows the user's own rights; write access is
      separate from read access; each package can be enabled by the sysop.
      Done for reading messages: the user's subs (`usub`), co-sysop rights
      for unvalidated and deleted posts, and the anonymous post rules, as in
      the BBS's reader; on unless the sysop turns it off
      (`script_flag_disable_msg`)
- [ ] Reference documentation for the bindings ([BINDINGS.md](BINDINGS.md): `wwiv.msg` so far)
- [x] A sample message viewer script (`install/scripts/msgview.bas`; run in
      `bbs_tests` by `MsgViewTest`)

### M5b: Full-screen UI

Done when a script can draw a full-screen, lightbar-style UI like the BBS's
own full-screen message reader: `msgview.bas` gets a full-screen mode.

Names follow WWIV's own where WWIV has one (4.24's documented helpers and
today's C++: `okansi`, `ansic`, `prt`, `pla`, `onek`, `nsl`), with `GOTOXY`
for `goxy`; otherwise QBasic/VB names for strings and Synchronet's for the
terminal. All of it is new-engine only.

- [x] Stdlib strings: `UCASE`, `LCASE`, `SPACE`, `STRING`, `TRIM`, `LTRIM`,
      `RTRIM`, `INSTR` (positions count from 0, as in `MID`; -1 if not found)
- [x] `wwiv.io` primitives: `OKANSI`, `GOTOXY`, `CLREOL`, `ANSIC`, `PRT`,
      `PLA`, `ONEK`, `READKEY` (special keys as names: `"UP"`, `"PGDN"`,
      `"F1"`...), `SCREEN_COLUMNS`, `SCREEN_ROWS`, `STRIP_COLORS`,
      `LEN_COLORS`, `PAD_COLORS`, `PAUSING` (the name may change), `NSL`.
      Screen functions do nothing without ANSI; input returns `""` once the
      caller hangs up
- [x] wwivbasic: `Interpreter::call(name, args)`, so native code can call a
      script's `DEF` function while the script runs (needed for callbacks)
- [ ] `wwiv.screen` (always on): `FULLSCREEN(header_lines)`, a layout over
      `common::FullScreenView` that turns pausing off while it's open, and
      `LIGHTBAR(items, top, height)`, a scrolling list in C++ that redraws
      only the lines that change. `RUN(["fn"])` handles the movement keys and
      returns the first other key; with a callback, `fn(key, selected)` is
      called for each such key and returning `TRUE` keeps the list running
      (an experiment)
- [ ] Full-screen `msgview.bas`: a lightbar title list and a scrolling
      reader, keeping the line UI when `OKANSI()` is false

### M6: Graduate

Done when wwivbasic is WWIV's only BASIC engine.

- [ ] Debugger with the same `/debug/v1` HTTP API
- [ ] Make the new engine the default; publish the porting guide
- [ ] Remove `deps/my_basic`, the legacy engine and the CMake option

## Open decisions

- Unqualified names after import — implemented as recommended (script and root
  functions win over imported ones); revisit if needed
- Iterators: resolved, no break needed. my_basic already has `FOR x IN l`, so
  both it and `iterator`/`move_next`/`get` are supported
- Floating-point numbers: decided, added in M3 (needed by `wwiv.data` and the
  SDK)
- Engine selection: decided, a CMake option to build the new engine plus a
  runtime setting to choose it: a bit in `config.json`'s `script_flags`
  (default legacy), overridden by `bbs --basic_engine`. If `config.json` asks
  for the new engine in a build without it, WWIV logs a warning and uses
  legacy; `--basic_engine=next` in such a build is an error
- Repo layout: decided, moved into the WWIV repo under `wwivbasic/` at the
  start of M4 (was going to be a submodule until M6); bindings live in
  `bbs/basic/`
- Dicts: not implemented. Do any scripts use `dict()`? The SDK may need dicts
  or records — M3/M5
- `wwiv.io.file.open(..., "W")`: decided, add `WRITE(file, text...)`,
  `WRITELINE(file, text...)` and `CLOSE(file)` (M4). `OPEN_OPTIONS("MENUS")`
  now reads from the current menu set's gfiles directory and can't be opened
  for writing (legacy used the BBS's gfiles)
- Debugger: decided, required before removing my_basic (M6). Move it earlier
  if it's needed day to day
- Generated parser: decided, checked in under `generated/`; builds with Java
  regenerate it when the grammar changes, builds without Java use it as is
- C++ standard: resolved, WWIV builds as C++23
