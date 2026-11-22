# wwivbasic

A BASIC interpreter for [WWIV](https://github.com/wwiv/wwiv) scripts, written in C++23 on top of
an ANTLR 4 grammar. It is intended to replace the my_basic interpreter that WWIV uses today.
Most existing WWIVbasic scripts should run unchanged.

## Status

Work in progress. See [docs/PLAN.md](docs/PLAN.md) for the milestones.

- Done: build and test baseline (M0), parsing the existing WWIV scripts (M1), and lists,
  imports and stub `wwiv.*` modules so those scripts run (M2).
- Next: an embedding API with object values and member access (M3), then running inside
  WWIV behind a build option with the `wwiv.*` modules as bindings (M4), then SDK bindings such
  as messages and users (M5).

Where wwivbasic intentionally differs from my_basic, [PORTING.md](PORTING.md) describes the change.

## Example

```basic
' Comments start with '

def Greet(name)
  return "Hello, " + name
enddef

for i = 1 to 3
  print Greet("caller #"); i
next

count = 0
while count < 3
  count = count + 1
wend
print "count: ", count
```

## Building

You need:

- CMake 3.21 or newer, and Ninja
- A C++23 compiler: GCC 14 or newer, or a current Visual Studio
- Java, to run the ANTLR code generator (`antlr/antlr-4.13.2-complete.jar`)
- vcpkg (see below)

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug
```

On Windows, run these in an x64 Native Tools prompt and use the `windows-x64-debug` preset.
Build output goes to `_build/<preset>/`.

### vcpkg

The libraries this project needs (ANTLR runtime, fmt, gtest and so on) come from
[vcpkg](https://vcpkg.io). They're installed automatically on the first configure, at the
versions pinned by `builtin-baseline` in `vcpkg.json`.

CMake looks for vcpkg in this order:

1. `-DCMAKE_TOOLCHAIN_FILE=...` on the command line
2. The `VCPKG_ROOT` environment variable: the directory of a vcpkg checkout, the one that contains
   `scripts/buildsystems/vcpkg.cmake`
3. `../wwiv/vcpkg`: the vcpkg submodule of a WWIV checkout next to this one

If you have WWIV checked out next to wwivbasic, you don't need to set anything. Otherwise point
`VCPKG_ROOT` at any vcpkg checkout, for example `export VCPKG_ROOT=$HOME/src/wwiv/vcpkg` or
`set VCPKG_ROOT=W:\wwiv\vcpkg`. A fresh vcpkg clone needs `bootstrap-vcpkg.sh` (or `.bat`) run
once.

## Testing

```sh
ctest --preset linux-debug                  # all tests
ctest --preset linux-debug -R golden        # golden scripts only
```

There are two kinds of test:

- **Unit tests** (gtest) in `src/*_test.cpp`. `src/interpreter_test.cpp` runs small scripts and
  checks their output and errors.
- **Golden tests** in `tests/golden/`. Each `NAME.bas` is run and its output compared with
  `NAME.expected`. If the expected file starts with `#exit N`, the script must exit with code N.
  If there is a `NAME.input`, it is fed to the script as input (one line per `GETS`, `YN` and so
  on). `wwiv_hello.bas` and `wwiv_oneliners.bas` are unchanged copies of the scripts WWIV ships.
  To add one, drop in both files and re-run `cmake --preset ...`.

## Running scripts

```sh
_build/linux-debug/basicrun script.bas
_build/linux-debug/basicrun --check script.bas            # parse only
_build/linux-debug/basicrun --trace script.bas            # log execution to stderr
_build/linux-debug/basicrun --show_parsetree script.bas
```

Script output goes to stdout. Errors go to stderr as `file:line: error: message`, and the exit
code is 1 on error.

`basicrun` provides stand-in versions of the `wwiv`, `wwiv.io` and `wwiv.data` modules, so
scripts written for the BBS can run. Input (`GETS`, `GETKEY`, `YN`, `NY`) is read from stdin a
line at a time, screen actions such as `CLS` print a marker like `[cls]`, and `wwiv.data` keeps
saved lists in memory only:

```sh
printf 'Y\n3\nHello\nN\nN\n' | _build/linux-debug/basicrun tests/golden/wwiv_oneliners.bas
```

## Tools for working on the grammar

**See how a script parses.** `--show_parsetree` prints the parse tree without running the
script. It's the quickest way to check precedence or see which rule matched:

```sh
$ _build/linux-debug/basicrun --show_parsetree t.bas      # t.bas: a = 1 + 2 * 3
(main
    (statement
        (assignmentStatement
            (lvalue
                (variable a)) =
            (expr
                (expr 1) +
                (expr
                    (expr 2) *
                    (expr 3)))
            ...
```

**See what the script does.** `--trace` logs each function call, return value and assignment to
stderr:

```sh
$ _build/linux-debug/basicrun --trace t.bas
TRACE: ASSIGN a = '7'
```

**Use the ANTLR tool directly.** This uses the grammar files as they are, so you can try a change
without rebuilding. `-tokens` shows what the lexer produced, including the hidden whitespace
(channel 1) and comment (channel 2) tokens; that's useful when a keyword or identifier isn't
lexing the way you expect. `-tree` prints the parse tree.

```sh
java -cp antlr/antlr-4.13.2-complete.jar org.antlr.v4.gui.Interpreter \
    BasicLexer.g4 BasicParser.g4 main -tokens -tree t.bas
```

```
[@0,0:0='a',<ID>,1:0]
[@1,1:1=' ',<WS>,channel=1,1:1]
[@2,2:2='=',<'='>,1:2]
...
[@14,14:19='' note',<COMMENT>,channel=2,1:14]
[@15,20:20='\n',<NEWLINE>,1:20]
```

Use `-gui` instead to open a graphical tree viewer (`a.cmd t.bas` on Windows does this). The
viewer needs a desktop and a full Java runtime; the `-headless` packages can't show it.

## Layout

| Path | Contents |
| --- | --- |
| `BasicLexer.g4`, `BasicParser.g4` | The grammar |
| `src/` | The interpreter, the `basicrun` CLI and the unit tests |
| `src/stdlib/` | Built-in functions |
| `tests/golden/` | Golden test scripts |
| `core/` | A copy of WWIV's `core/` library |
| `docs/PLAN.md` | Plan and milestones |
| `PORTING.md` | Porting guide from my_basic |
| `AGENTS.md` | Notes for coding agents |
