# AGENTS.md

Guidance for coding agents working in this repo.

## What this is

wwivbasic is a BASIC interpreter written in C++23 on top of an ANTLR 4 grammar. The goal is to
replace the my_basic interpreter behind `RunBasicScript` in WWIV (the BBS). Most existing
scripts should keep running unchanged.

- `docs/PLAN.md` lists the milestones (M0–M6), the architecture (engine switch, bindings, repo layout) and open decisions. Tick items off there when
  they land.
- `PORTING.md` records every intentional difference from my_basic. If a change breaks
  compatibility with my_basic, add an entry (old form, new form, why) in the same change.

## Layout

| Path | Contents |
| --- | --- |
| `BasicLexer.g4`, `BasicParser.g4` | Grammar. The C++ lexer and parser (namespace `wwivbasic`) are generated into `generated/` and checked in. |
| `generated/` | ANTLR output; never edit by hand. `grammar.sha256` is the hash of the grammar it was generated from. Our code includes it through `src/antlr.h` only (fmt must come before ANTLR, which `#undef`s `EOF`). |
| `cmake/` | `generate_parser.cmake` (runs ANTLR and writes `grammar.sha256`), `grammar_hash.cmake`. |
| `include/wwivbasic/` | The public embedding API: `interpreter.h` (`Interpreter`, `RunOptions`, `RunResult`), `module.h` (`Module`, `native_function`, `class_`, argument conversion), `object.h` (`ClassBinding<T>`: properties, methods, `wrap`/`unwrap`), `value.h` (`Value`: BOOLEAN, INTEGER, REAL, STRING, LIST, ITERATOR, OBJECT; `BasicError`), `console.h` (`Console`), `stdio_console.h`. These must not include ANTLR or anything from `src/`; `src/embedding_test.cpp` uses only these, including a fake message base bound as objects. |
| `src/` | The interpreter internals: `context.*` (scopes, variables, modules, function lookup, parsed sources), `executor.*` (`ExecutionVisitor`, which walks the parse tree), `function_def_visitor.*` (collects `DEF`s before execution), `interpreter.cpp` (`Interpreter`: parse, run, collect errors), `stdio_console.cpp`, `value.cpp`, `wwiv_stub.*` (stand-in `wwiv`, `wwiv.io`, `wwiv.data` modules used by `basicrun`, written against the public API; the real ones are bindings in WWIV, see M4), `testing/fake_console.h` (a `Console` for tests), `utils.*` |
| `src/stdlib/` | Built-in functions (`strings`, `numbers`, `common`, `collections`: my_basic's list functions) |
| `src/basicrun.cpp` | CLI that runs one `.bas` file: `--check`/`-c` parses only, `--show_parsetree`/`-t` prints the parse tree, `--trace`/`-v` logs execution to stderr. Script output goes to stdout, errors to stderr as `file:line[:col]: error: ...`, exit code 1 on error. |
| `core/` | A vendored copy of WWIV's `core/` library (files, strings, logging, command line and so on). Treat it as upstream code: avoid edits unless they're required, and keep them minimal. Inside WWIV's build (`add_subdirectory` with a `core` target already defined), WWIV's own `core/` is used instead, and `basicrun` and the tests aren't built. |
| `tests/golden/` | Golden tests: each `NAME.bas` is run by `basicrun` and its stdout compared with `NAME.expected`. A first line `#exit N` in the expected file sets the expected exit code; `NAME.input`, if present, is the script's stdin. `lib/` holds files imported by tests (not tests themselves). `wwiv_*.bas` are unchanged copies of scripts WWIV ships; keep them identical. |
| `tests/run_golden.cmake` | The golden-test runner used by CTest. |
| `antlr/antlr-4.13.2-complete.jar` | ANTLR tool that regenerates `generated/` (needs Java 11+). Keep it at the same version as the vcpkg antlr4 runtime. |

## Build

Dependencies come from vcpkg in manifest mode (`vcpkg.json`): antlr4, fmt, cereal, gtest,
cpp-httplib, nlohmann-json. Versions are pinned by `builtin-baseline`; when updating it, keep
`antlr/antlr-*-complete.jar` at the same version as the antlr4 runtime. CMake finds vcpkg from `CMAKE_TOOLCHAIN_FILE`, then `VCPKG_ROOT`, then
`../wwiv/vcpkg`, so with WWIV checked out alongside no setup is needed:

```sh
cmake --preset linux-debug          # or windows-x64-debug
cmake --build --preset linux-debug
```

Editing a `.g4` file regenerates `generated/` on the next build (needs Java 11+). Commit the
regenerated files in the same change as the grammar; CI fails if they are out of date. Without
Java, configuring fails if the grammar doesn't match `generated/grammar.sha256`.

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
- To debug the grammar, use `basicrun --show_parsetree file.bas`, or the ANTLR tool directly:
  `java -cp antlr/antlr-4.13.2-complete.jar org.antlr.v4.gui.Interpreter BasicLexer.g4 BasicParser.g4 main -tokens -tree file.bas`
  (no rebuild needed). `basicrun --trace` logs calls and assignments. See "Tools for working on
  the grammar" in README.md.

Before you finish a change, build it and make sure `ctest` passes.

## Code style

- C++23 (GCC 14 or newer, or a current MSVC; CMake 3.20 or newer). Our code builds with
  `-Wall -Wextra` (`/W4` on MSVC); keep it warning-free. Generated ANTLR code and `core/` are
  excluded. Format with the repo's
  `.clang-format` (LLVM base, 2-space indent, 100 columns, `int* p` pointer alignment).
- Under C++20 and later, fmt checks format strings at compile time, so a `{}` without a
  matching argument is a build error. Use `fmt::runtime(s)` only for genuinely dynamic formats.
- Code lives in namespace `wwivbasic` (stdlib functions in `wwivbasic::stdlib`). Reuse the
  helpers in `core/` (for example `wwiv::strings`, `wwiv::stl::ci_less`) and `fmt` instead
  of adding new dependencies.
- BASIC keywords and identifiers are case-insensitive. Use case-insensitive comparison and
  containers (`ci_less`) for names.
- Switch statements over enums list every case instead of using `default:`, so the compiler
  flags a new enum value that isn't handled.
- Keep the `wwiv.*` module names and behaviour compatible with my_basic (see
  "Compatibility policy" in `docs/PLAN.md`).

## Git

- Main branch: `main`. Do work on a feature branch.
- Commit messages are short and imperative ("Add support for TRUE, FALSE").
- Never commit `_build/`, `build/`, `out/` or `*.log`.
