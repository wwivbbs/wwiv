# Debugging WWIVbasic scripts

WWIV can pause a script and let a debugger step through it, set breakpoints,
and look at its variables and call stack. The debugger is
[wwivdbg](https://github.com/wwiv/wwivdbg), a text-mode client; anything that
speaks the HTTP API below works too (even `curl`).

## Running a script under the debugger

```sh
bbs -d --run_basic=hello.bas
```

`-d` (`--debug`) makes *every* script wait for a debugger to attach before
it runs, so only use it locally as the sysop. The script waits up to two
minutes; press Ctrl-C to skip the wait and run it without the debugger. While
the script is paused, Ctrl-C ends it.

The debugger listens on `127.0.0.1` only, on port 9948 (`--debug_port=N` to
change it). Anyone who can reach the
port can read the script's variables and stop it. To debug a BBS on another
machine, forward the port (`ssh -L 9948:localhost:9948 bbs-host`).

Modules are the script's files, named relative to the scripts directory
(`hello.bas`, `lib/util.bas`).

## How stepping works

- When a debugger attaches, the script pauses before its next statement (its
  first, since it was waiting).
- **Trace in** runs one statement and pauses at the next, inside a function it
  calls.
- **Step over** pauses at the next statement that is no deeper than the
  current one, so calls (and an `IMPORT`ed file's top-level code) run
  without stopping, unless they hit a breakpoint. Stepping over a function's
  last statement pauses in its caller.
- **Run** continues until a breakpoint.
- A **line breakpoint** pauses when the script enters that line. A second
  statement on the same line (`IF x THEN y`) doesn't fire it again, but the
  line coming round again in a loop does.
- **Detach** removes the breakpoints and lets the script run to the end.

## HTTP API

All paths are under `/debug/v1/`. Commands other than `attach`, `state`,
`source` and `watch` need an attached debugger and answer `400 Not Attached`
otherwise.

| Request | Does | Answers |
| --- | --- | --- |
| `POST attach` | Attaches (one debugger at a time; `400` if one is attached) | `{"attached": true, "initial_module": "hello.bas"}` |
| `POST detach` | Detaches; the script runs on | `detached` |
| `POST breakpoint` with form fields `line`, `module` (default: the current module) and `typ` (only `line`) | Adds a line breakpoint (the same line twice gives the same breakpoint) | The breakpoint |
| `DELETE breakpoint/ID` | Removes a breakpoint (`404` if there's none) | |
| `POST tracein`, `POST stepover` | Steps, and waits (up to 5 seconds) for the script to pause again | The state |
| `POST run` | Runs to the next breakpoint | The state, as it was before running |
| `POST stop` | Ends the script at its next statement | The state, as it was before stopping |
| `GET state` | | The state |
| `GET source` with an optional `module` query parameter (default: the current module) | | The module's text (`404` if it hasn't been loaded) |
| `GET watch` | | The `vars` of the state |

`GET /debug/status` answers `ok`.

The state:

```json
{
  "version": "1",
  "state": "paused",
  "initial_module": "dbg.bas",
  "location": {"module": "lib.bas", "row": 3, "col": 3, "pos": 0},
  "stack": ["twice (lib.bas:3)", "dbg.bas:3"],
  "vars": [
    {"name": "n", "type": "INTEGER", "value": "1", "frame": 0},
    {"name": "x", "type": "INTEGER", "value": "1", "frame": 1}
  ],
  "breakpoints": [
    {"id": 1, "module": "lib.bas", "typ": "line", "line": 3, "function_name": "", "hit_count": 1}
  ]
}
```

- `state` is `waiting` (for a debugger to attach), `running`, `paused` or
  `finished`.
- `location` is the statement the script is about to run (`row` and `col`
  count from 1). `pos` is always 0.
- `stack` is innermost first: `function (module:line)` for a function call,
  `module:line` for a file's top-level code.
- `vars` are those visible in each frame of `stack` (`frame` indexes it): a
  function's parameters and locals, and with a file's top-level frame, the
  globals of its module. Variables of a module other than the script's own
  are named `module.name`. Lists show their items (`[1, "two", [3]]`).
- `stack` and `vars` are from the last time the script paused.

## Differences from the legacy (my_basic) debugger

- The state has `state`, and the version is `"1"`.
- Modules are file names relative to the scripts directory (`hello.bas`)
  instead of upper-case module names (`HELLO`).
- `stack` entries include the file and line.
- `tracein` and `stepover` wait for the step to finish before answering, so
  the state they return is where the script stopped.
- `source` takes an optional `module`.
- The server listens on `127.0.0.1` instead of every interface.

## For embedders

The interpreter's side of this is in `wwivbasic/interpreter.h`:
`Interpreter::set_statement_hook` (called before each statement, with the
file, line, column and nesting depth; the script pauses for as long as the
hook doesn't return), and `call_stack()`, `variables()` and `source()`, which
the hook can call. WWIV's debugger (`bbs/basic/next/debug_session.*` and
`debug_server.*`) is built on them.
