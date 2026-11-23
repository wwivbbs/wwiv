# AGENTS.md

Guidance for coding agents working on wwivbasic (`wwivbasic/` in the WWIV tree).

## What this is

wwivbasic is a BASIC interpreter written in C++23 on top of an ANTLR 4 grammar. The goal is to
replace the my_basic interpreter behind `RunBasicScript` in WWIV (the BBS). Most existing
scripts should keep running unchanged.

- `docs/PLAN.md` lists the milestones (M0–M6), the architecture (engine switch, bindings, repo layout) and open decisions. Tick items off there when
  they land.
- `PORTING.md` records every intentional difference from my_basic. If a change breaks
  compatibility with my_basic, add an entry (old form, new form, why) in the same change.

## Layout

Everything is under `wwivbasic/` in the WWIV tree, which is on the include path, so includes
are `#include "wwivbasic/..."` like `core/...`.

| Path | Contents |
| --- | --- |
| `BasicLexer.g4`, `BasicParser.g4` | Grammar. The C++ lexer and parser (namespace `wwivbasic`) are generated into `generated/` and checked in. |
| `generated/` | ANTLR output; never edit by hand. `grammar.sha256` is the hash of the grammar it was generated from. Our code includes it through `internal/antlr.h` only (fmt must come before ANTLR, which `#undef`s `EOF`). |
| `cmake/` | `generate_parser.cmake` (runs ANTLR and writes `grammar.sha256`), `grammar_hash.cmake`. |
| `*.h` (top level) | The public embedding API: `interpreter.h` (`Interpreter`, `RunOptions`, `RunResult`), `module.h` (`Module`, `native_function`, `class_`, argument conversion), `object.h` (`ClassBinding<T>`: properties, methods, `wrap`/`unwrap`), `value.h` (`Value`: BOOLEAN, INTEGER, REAL, STRING, LIST, ITERATOR, OBJECT; `BasicError`), `console.h` (`Console`), `stdio_console.h`. These must not include ANTLR or anything from `internal/`; `embedding_test.cpp` uses only these, including a fake message base bound as objects. Their implementations (`interpreter.cpp`, `value.cpp`, `stdio_console.cpp`) sit next to them. |
| `internal/` | The interpreter internals, not for use outside `wwivbasic/`: `context.*` (scopes, variables, modules, function lookup, parsed sources), `executor.*` (`ExecutionVisitor`, which walks the parse tree), `function_def_visitor.*` (collects `DEF`s before execution), `antlr.h`, `utils.*` |
| `stdlib/` | Built-in functions (`strings`, `numbers`, `common`, `collections`: my_basic's list functions) |
| `testing/fake_console.h` | A `Console` for tests |
| `basicrun/` | CLI that runs one `.bas` file: `--check`/`-c` parses only, `--show_parsetree`/`-t` prints the parse tree, `--trace`/`-v` logs execution to stderr. Script output goes to stdout, errors to stderr as `file:line[:col]: error: ...`, exit code 1 on error. `wwiv_stub.*` are stand-in `wwiv`, `wwiv.io`, `wwiv.data` modules written against the public API; the real ones are bindings in `bbs/basic/` (M4). |
| `tests/golden/` | Golden tests: each `NAME.bas` is run by `basicrun` and its stdout compared with `NAME.expected`. A first line `#exit N` in the expected file sets the expected exit code; `NAME.input`, if present, is the script's stdin. `lib/` holds files imported by tests (not tests themselves). `wwiv_*.bas` are unchanged copies of scripts WWIV ships; keep them identical. |
| `tests/run_golden.cmake` | The golden-test runner used by CTest. |
| `antlr/antlr-4.13.2-complete.jar` | ANTLR tool that regenerates `generated/` (needs Java 11+). Keep it at the same version as the vcpkg antlr4 runtime. |

wwivbasic depends only on WWIV's `core/`, the ANTLR runtime and fmt. It must not depend on
`bbs/`, `sdk/` or `common/`; bindings that need those live in `bbs/basic/`.

## Build

wwivbasic is built only with `-DWWIV_WWIVBASIC_NEXT=ON`, which also turns on the `wwivbasic`
feature in WWIV's `vcpkg.json` (the antlr4 runtime). When updating vcpkg's `builtin-baseline`,
keep `antlr/antlr-*-complete.jar` at the same version as the antlr4 runtime. From the top of
the WWIV tree:

```sh
cmake --preset linux-debug -DWWIV_WWIVBASIC_NEXT=ON
cmake --build --preset linux-debug
```

Editing a `.g4` file regenerates `generated/` on the next build (needs Java 11+). Commit the
regenerated files in the same change as the grammar. Without Java (and in CI, which passes
`-DWWIVBASIC_GENERATE_PARSER=OFF`), configuring fails if the grammar doesn't match
`generated/grammar.sha256`.

## Test

```sh
ctest --preset linux-debug                            # all of WWIV's tests, including these
_build/linux-debug/wwivbasic/wwivbasic_tests          # wwivbasic's unit tests
_build/linux-debug/wwivbasic/basicrun wwivbasic/tests/golden/loops.bas
```

- Unit tests live next to the code in `*_test.cpp`; `interpreter_test.cpp` runs small scripts
  through `run_source` and checks output and errors. New test files have to be added to the
  `wwivbasic_tests` target in `wwivbasic/CMakeLists.txt`. New interpreter sources go in
  `wwivbasic_interpreter`.
- To add a golden test, drop `NAME.bas` and `NAME.expected` into `tests/golden/` (picked up on
  the next configure). Check the expected output by hand; don't just capture whatever runs.
- To debug the grammar, use `basicrun --show_parsetree file.bas`, or the ANTLR tool directly
  from `wwivbasic/`:
  `java -cp antlr/antlr-4.13.2-complete.jar org.antlr.v4.gui.Interpreter BasicLexer.g4 BasicParser.g4 main -tokens -tree file.bas`
  (no rebuild needed). `basicrun --trace` logs calls and assignments. See "Tools for working on
  the grammar" in README.md.

Before you finish a change, build it and make sure `ctest` passes.

## Working on M4 (integrating with the BBS)

M4 in `docs/PLAN.md` is the current milestone; its checklist says what's done. Decisions
already made, so don't revisit them without asking:

- **Where code goes.** The my_basic engine stays where it is (`bbs/basic/basic.*`,
  `bbs/basic/wwiv_*.cpp`, the debugger); don't move or restructure it, since M6 deletes it. The
  engine interface goes in `bbs/basic/` (`script_engine.h`), and everything for the new engine in
  `bbs/basic/next/`: the engine adapter, a `Console` over `bout`/`bin`, and the `wwiv.*`
  bindings. Build `bbs/basic/next/` only with `WWIV_WWIVBASIC_NEXT`; the BBS must still build and
  behave exactly as before without it.
- **Dependencies.** `wwivbasic/` depends only on `core/`, antlr4 and fmt. Bindings that need
  `bbs/`, `sdk/` or `common/` live in `bbs/basic/next/` and use only wwivbasic's public headers
  (`wwivbasic/*.h`), never `wwivbasic/internal/`. If a binding needs something the API doesn't
  have, add it to the API (with a test in `embedding_test.cpp`).
- **Choosing the engine.** `RunBasicScript` (`bbs/basic/basic.cpp`; called from
  `bbs/application.cpp`, `bbs/execexternal.cpp` and `bbs/menus/menucommands.cpp`) picks the
  engine at runtime: a `wwiv.json` setting, overridden by `bbs --basic_engine=legacy|next`.
  The default stays legacy.
- **Bindings match my_basic.** The existing bindings in `bbs/basic/wwiv*.cpp` are the spec:
  same module and function names, arguments, return values and side effects. `wwiv.data` must
  read and write the same files in the same format. `wwiv.io.file` and `wwiv.os` are only
  registered when `config.script_package_file_enabled()` / `script_package_os_enabled()` are
  set, and nothing runs unless `scripting_enabled()`, as in `Basic` today.
  `basicrun/wwiv_stub.cpp` already implements these modules against the public API with fake
  backends, so it's a useful starting point. Any intentional difference goes in `PORTING.md`.
- **Tests.** Port the cases in `bbs/basic/basic_test.cpp` so they run on both engines, and
  check `install/scripts/hello.bas` and `oneliners.bas` give the same output on both.

Ask the user rather than choosing:

- the name and shape of the `wwiv.json` engine setting;
- what `wwiv.io.file.open(..., "W")` should do (see "Open decisions" in `docs/PLAN.md`);
- anything that changes script-visible behaviour from my_basic.

Things that have bitten us:

- **Windows CI is 32-bit (x86).** WWIV's PR build uses `windows-x86-debug`; wwivbasic was only
  tested on x64 before the move. Watch for `size_t`/`int64_t` assumptions and type clashes
  (for example the `ssize_t` workaround in `internal/antlr.h`).
- **Line endings.** Some WWIV files use CRLF, including the top-level `CMakeLists.txt` and
  `core/command_line.h`. Keep each file's existing line endings; a whole-file line-ending change
  makes the diff unreadable.
- **Header order.** Include ANTLR only through `internal/antlr.h` (fmt first because of `EOF`,
  and the `ssize_t` rename on MSVC).

## Code style

- C++23 (GCC 14 or newer, or a current MSVC), like the rest of WWIV. Our code builds with
  `-Wall -Wextra` (`/W4` on MSVC, via `set_max_warnings`); keep it warning-free. Generated ANTLR
  code is excluded. Format with WWIV's `.clang-format` (LLVM base, 2-space indent, 100 columns, `int* p` pointer alignment).
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

- Do work on a feature branch and open a pull request to `wwivbbs/wwiv` `main`. Never push to
  `main`. The user reviews and merges pull requests themselves, as squash merges; don't merge
  them yourself.
- Before handing a pull request over, build with `-DWWIV_WWIVBASIC_NEXT=ON`, run `ctest`, and
  check that CI passes on both jobs (`linux-debug` and `windows-x86-debug`).
- Commit messages are short and imperative ("Add support for TRUE, FALSE").
