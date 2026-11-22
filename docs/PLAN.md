# Plan: replacing my_basic in WWIV

The working copy of this plan (with discussion) lives in the shared doc
"WWIVbasic: Replacing my_basic — Plan". This file tracks the same milestones
next to the code; tick items off here as they land.

## Goal

Replace the my_basic interpreter behind `RunBasicScript` in WWIV with
wwivbasic. Existing scripts should mostly run unchanged; intentional breaks
are documented in [PORTING.md](../PORTING.md).

Done when:

- `install/scripts/hello.bas` and `oneliners.bas` run, unchanged or with only
  edits that PORTING.md documents.
- The `wwiv`, `wwiv.io`, `wwiv.io.file`, `wwiv.data`, `wwiv.os` and
  `wwiv.time` modules exist with the same names and behaviour.
- WWIV runs scripts through wwivbasic and `deps/my_basic` is removed.

Out of scope for the first release: GOTO/GOSUB, classes, lambdas, coroutines.

## Compatibility policy

Keep compatible: `'` comments, `WHILE/WEND`, `NOT`, `PRINT a, b` / `PRINT a; b`
without parentheses, `import "@wwiv.io"`, a trailing `;` on a statement,
module/function names and behaviour, case-insensitivity.

Intentional breaks (proposed): `iterator`/`move_next`/`get` → `FOR EACH`;
no GOTO/GOSUB, classes, lambdas or coroutines; integers only for now.

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

- [ ] List type in `Value` (reference semantics)
- [ ] `list()`, `len`, `push`, `remove`, `get`, indexing
- [ ] `FOR EACH x IN l` (plus iterator shim if kept)
- [x] Runtime errors for unknown functions/modules and bad arity
- [ ] `import "file.bas"` loads the file
- [ ] stdlib gaps the scripts need

### M3: WWIV modules

Done when every function registered in `bbs/basic/*.cpp` exists here.

- [ ] Host interface so the interpreter doesn't depend on bbs; fake for tests
- [ ] `wwiv`, `wwiv.io`, `wwiv.io.file`, `wwiv.data` (same on-disk format),
      `wwiv.os`, `wwiv.time`

### M4: Integrate into WWIV

Done when WWIV uses wwivbasic by default and `deps/my_basic` is deleted.

- [ ] Use WWIV's `core/` instead of the copy here
- [ ] Add to the WWIV CMake build
- [ ] `RunBasicScript` switch between my_basic and wwivbasic
- [ ] Debugger with the same `/debug/v1` HTTP API (or defer)
- [ ] Publish the porting guide; switch default; remove my_basic

## Open decisions

- Unqualified names after import — implemented as recommended (script and root
  functions win over imported ones); revisit if needed
- Keep the iterator shim for one release? — M2
- Floating-point numbers now or later? — M2
- Do any scripts use `dict()`? — M2
- Repo layout: move into WWIV tree or submodule? — M4
- Debugger required before removing my_basic? — M4
- ANTLR runtime + Java build step in WWIV, or check in generated parser? — M4
