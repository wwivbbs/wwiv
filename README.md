# wwivbasic

A BASIC interpreter for [WWIV](https://github.com/wwiv/wwiv) scripts, written in C++23 on top of
an ANTLR 4 grammar. It is intended to replace the my_basic interpreter that WWIV uses today.
Most existing WWIVbasic scripts should run unchanged.

## Status

Work in progress. See [docs/PLAN.md](docs/PLAN.md) for the milestones.

- Done: build and test baseline (M0), and parsing the existing WWIV scripts (M1).
- Next: lists and `FOR EACH` (M2), then the `wwiv.*` modules (M3) and integration into WWIV (M4).

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
