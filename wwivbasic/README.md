# wwivbasic

A BASIC interpreter for WWIV scripts, written in C++23 on top of
an ANTLR 4 grammar. It is intended to replace the my_basic interpreter that WWIV uses today.
Most existing WWIVbasic scripts should run unchanged.

## Status

Built with WWIV by default (`WWIV_WWIVBASIC_NEXT`, off on OS/2), but WWIV still runs scripts
on my_basic unless the sysop turns on "Use New Engine" in wwivconfig's scripting settings (or
`bbs --basic_engine=next`). See [docs/PLAN.md](docs/PLAN.md) for the milestones.

- Done: build and test baseline (M0), parsing the existing WWIV scripts (M1), lists, imports
  and stub `wwiv.*` modules so those scripts run (M2), and the embedding API with object values
  and member access (M3).
  Running inside the BBS with the `wwiv.*` modules as bindings (M4).
- Next: SDK bindings such as messages and users (M5).

wwivbasic was developed in its own repository (github.com/wwiv/wwivbasic, now archived) and
moved here at the start of M4.

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

wwivbasic builds as part of WWIV when `WWIV_WWIVBASIC_NEXT` is on (the default except on
OS/2; pass `-DWWIV_WWIVBASIC_NEXT=OFF` to skip it). That also adds the
`wwivbasic` feature in WWIV's `vcpkg.json`, which installs the ANTLR runtime. From the top of
the WWIV tree:

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug
```

Java 11 or newer is needed only if you change the grammar (see below).

### The generated parser

The C++ lexer and parser are generated from `BasicLexer.g4` and `BasicParser.g4` by ANTLR
(`antlr/antlr-4.13.2-complete.jar`) and checked in under `generated/`, so a build doesn't need
Java. If Java is installed, editing a grammar file regenerates them on the next build; commit
the regenerated files with the grammar change. Without Java, the checked-in files are used, and
configuring fails if the grammar no longer matches them (`generated/grammar.sha256` records
which grammar they came from). CI builds with `-DWWIVBASIC_GENERATE_PARSER=OFF`, so it fails if the checked-in files are out of
date.

## Testing

```sh
ctest --preset linux-debug                          # all of WWIV's tests
ctest --preset linux-debug -R wwivbasic.golden      # wwivbasic's golden scripts
_build/linux-debug/wwivbasic/wwivbasic_tests        # wwivbasic's unit tests
```

There are two kinds of test:

- **Unit tests** (gtest) in `*_test.cpp`, next to the code. `interpreter_test.cpp` runs small
  scripts and checks their output and errors.
- **Golden tests** in `tests/golden/`. Each `NAME.bas` is run and its output compared with
  `NAME.expected`. If the expected file starts with `#exit N`, the script must exit with code N.
  If there is a `NAME.input`, it is fed to the script as input (one line per `GETS`, `YN` and so
  on). `wwiv_hello.bas` and `wwiv_oneliners.bas` are unchanged copies of the scripts WWIV ships.
  To add one, drop in both files and configure again.

## Running scripts

```sh
_build/linux-debug/wwivbasic/basicrun script.bas
_build/linux-debug/wwivbasic/basicrun --check script.bas            # parse only
_build/linux-debug/wwivbasic/basicrun --trace script.bas            # log execution to stderr
_build/linux-debug/wwivbasic/basicrun --show_parsetree script.bas
```

Script output goes to stdout. Errors go to stderr as `file:line: error: message`, and the exit
code is 1 on error.

`basicrun` provides stand-in versions of the `wwiv`, `wwiv.io` and `wwiv.data` modules, so
scripts written for the BBS can run. Input (`GETS`, `GETKEY`, `YN`, `NY`) is read from stdin a
line at a time, screen actions such as `CLS` print a marker like `[cls]`, and `wwiv.data` keeps
saved lists in memory only:

```sh
printf 'Y\n3\nHello\nN\nN\n' | _build/linux-debug/wwivbasic/basicrun wwivbasic/tests/golden/wwiv_oneliners.bas
```

## Tools for working on the grammar

**See how a script parses.** `--show_parsetree` prints the parse tree without running the
script. It's the quickest way to check precedence or see which rule matched:

```sh
$ _build/linux-debug/wwivbasic/basicrun --show_parsetree t.bas      # t.bas: a = 1 + 2 * 3
(main
    (statement
        (assignmentStatement
            (lvalue
                (qualifiedName a)) =
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
$ _build/linux-debug/wwivbasic/basicrun --trace t.bas
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

## Embedding

The BBS (and `basicrun`) use wwivbasic through the public headers at the top of
`wwivbasic/` (`interpreter.h`, `module.h`, `object.h`, `value.h`, `console.h`,
`stdio_console.h`) and the `wwivbasic_interpreter` library. Nothing outside
`wwivbasic/` should include `wwivbasic/internal/` or the ANTLR headers. A host
provides a `Console` (where `PRINT` writes and input comes from) and adds
modules of native functions:

```cpp
#include "wwivbasic/interpreter.h"
#include "wwivbasic/stdio_console.h"

wwivbasic::StdioConsole console;   // or your own Console implementation
wwivbasic::Interpreter interp(console);

auto& io = interp.add_module("my.io");
io.native_function("HELLO", [](std::string name) { return "Hello, " + name; });

auto result = interp.run_file("script.bas");
for (const auto& error : result.errors) {
  // "script.bas:3: error: ..."
}
```

Scripts call `my.io.hello("x")`, or `hello("x")` after `import "@my.io"`. Native
functions can take and return `int`, `double`, `bool`, `std::string` or
`wwivbasic::Value` (and can return a `std::vector`, which becomes a list);
arguments are converted and their count is checked.

### Objects

A module can describe a C++ class so scripts can use its objects' properties
and methods (`wwivbasic/object.h`):

```cpp
struct Header { int number; std::string subject; std::string summary(int width) const; };

auto& header = io.class_<Header>("header");
header.property("number", &Header::number)                 // read-only
      .property("subject", &Header::subject, &Header::subject)  // read-write
      .method("summary", &Header::summary);
io.native_function("FIRST", [&]() { return header.wrap(Header{1, "Hello"}); });
```

```basic
h = my.io.first()
print h.number; ": "; h.subject; " "; h.summary(3)
h.subject = "Re: Hello"
```

Properties and methods can be data members, member functions or lambdas, and
member names are case-insensitive. `wrap()` turns a C++ object (or a
`std::shared_ptr` to one) into a script value, and `unwrap()` turns it back.
Objects are shared by reference, like lists, and print as their class name.

## Layout

| Path | Contents |
| --- | --- |
| `BasicLexer.g4`, `BasicParser.g4` | The grammar |
| `generated/` | The lexer and parser ANTLR generates from the grammar (don't edit) |
| `cmake/` | Build helpers: regenerating the parser |
| `*.h` | The public embedding API |
| `internal/` | The interpreter's internals |
| `stdlib/` | Built-in functions |
| `testing/` | A fake `Console` for tests |
| `basicrun/` | The `basicrun` CLI and its stand-in `wwiv.*` modules |
| `tests/golden/` | Golden test scripts |
| `docs/PLAN.md` | Plan and milestones |
| `PORTING.md` | Porting guide from my_basic |
| `AGENTS.md` | Notes for coding agents |
