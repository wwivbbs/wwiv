# AGENTS.md

Guidance for coding agents working in this repo.

## What this is

wwivbasic is a BASIC interpreter written in C++23 on top of an ANTLR 4 grammar. The goal is to
replace the my_basic interpreter behind `RunBasicScript` in WWIV (the BBS). Most existing
scripts should keep running unchanged.

- `docs/PLAN.md` lists the milestones (M0–M4) and open decisions. Tick items off there when
  they land.
- `PORTING.md` records every intentional difference from my_basic. If a change breaks
  compatibility with my_basic, add an entry (old form, new form, why) in the same change.

## Layout

| Path | Contents |
| --- | --- |
| `BasicLexer.g4`, `BasicParser.g4` | Grammar. The C++ lexer and parser are generated into `_build/<preset>/antlr4_generated_src/` (namespace `wwivbasic`). |
| `src/` | The interpreter: `context.*` (scopes, variables, modules, function lookup), `executor.*` (`ExecutionVisitor`, which walks the parse tree), `function_def_visitor.*` (collects `DEF`s before execution), `interpreter.*` (`run_file`/`run_source`: parse, run, collect errors), `value.*` (`Value`: BOOLEAN/INTEGER/STRING, and `BasicError`), `utils.*` |
| `src/stdlib/` | Built-in functions (`strings`, `numbers`, `common`) |
| `src/basicrun.cpp` | CLI that runs one `.bas` file: `--check`/`-c` parses only, `--show_parsetree`/`-t` prints the parse tree, `--trace`/`-v` logs execution to stderr. Script output goes to stdout, errors to stderr as `file:line[:col]: error: ...`, exit code 1 on error. |
| `core/` | A vendored copy of WWIV's `core/` library (files, strings, logging, command line and so on). Treat it as upstream code: avoid edits unless they're required, and keep them minimal. It will be replaced by WWIV's own `core/` in M4. |
| `tests/golden/` | Golden tests: each `NAME.bas` is run by `basicrun` and its stdout compared with `NAME.expected`. A first line `#exit N` in the expected file sets the expected exit code. |
| `tests/run_golden.cmake` | The golden-test runner used by CTest. |
| `antlr/antlr-4.13.2-complete.jar` | ANTLR tool used by the build (needs Java on `PATH`). Keep it at the same version as the vcpkg antlr4 runtime. |

## Build

Dependencies come from vcpkg in manifest mode (`vcpkg.json`): antlr4, fmt, cereal, gtest,
cpp-httplib, nlohmann-json. Versions are pinned by `builtin-baseline`; when updating it, keep
`antlr/antlr-*-complete.jar` at the same version as the antlr4 runtime. You also need Java for
ANTLR code generation. CMake finds vcpkg from `CMAKE_TOOLCHAIN_FILE`, then `VCPKG_ROOT`, then
`../wwiv/vcpkg`, so with WWIV checked out alongside no setup is needed:

```sh
cmake --preset linux-debug          # or windows-x64-debug
cmake --build --preset linux-debug
```

Editing a `.g4` file regenerates the parser on the next build.

## Test

```sh
ctest --preset linux-debug          # unit tests and golden tests
_build/linux-debug/basicrun tests/golden/loops.bas
```

- Unit tests live in `src/*_test.cpp` and `src/stdlib/*_test.cpp`; `src/interpreter_test.cpp`
  runs small scripts through `run_source` and checks output and errors. New test files have to
  be added to the `wwivbasic_tests` target in the top-level `CMakeLists.txt`. New interpreter
  sources go in `wwivbasic_interpreter`.
- To add a golden test, drop `NAME.bas` and `NAME.expected` into `tests/golden/` (picked up on
  the next configure). Check the expected output by hand; don't just capture whatever runs.
- `core/` tests are only built with `-DWWIV_BUILD_TESTS=ON`. That option comes from the
  WWIV tree and isn't wired up here.
- To inspect a parse visually, use the ANTLR GUI: `a.cmd <file>` (Windows), or the equivalent
  `java -cp antlr/antlr-4.13.2-complete.jar org.antlr.v4.gui.Interpreter ...`.

Before you finish a change, build it and make sure `ctest` passes.

## Code style

- C++23 (GCC 14 or newer, or a current MSVC; CMake 3.20 or newer). Format with the repo's
  `.clang-format` (LLVM base, 2-space indent, 100 columns, `int* p` pointer alignment).
- Under C++20 and later, fmt checks format strings at compile time, so a `{}` without a
  matching argument is a build error. Use `fmt::runtime(s)` only for genuinely dynamic formats.
- Code lives in namespace `wwivbasic` (stdlib functions in `wwivbasic::stdlib`). Reuse the
  helpers in `core/` (for example `wwiv::strings`, `wwiv::stl::ci_less`) and `fmt` instead
  of adding new dependencies.
- BASIC keywords and identifiers are case-insensitive. Use case-insensitive comparison and
  containers (`ci_less`) for names.
- Keep the `wwiv.*` module names and behaviour compatible with my_basic (see
  "Compatibility policy" in `docs/PLAN.md`).

## Git

- Main branch: `main`. Do work on a feature branch.
- Commit messages are short and imperative ("Add support for TRUE, FALSE").
- Never commit `_build/`, `build/`, `out/` or `*.log`.
