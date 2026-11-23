# Porting scripts from my_basic to wwivbasic

Most WWIVbasic scripts written for my_basic run unchanged. This guide lists
every place where wwivbasic intentionally differs, with the old form, the new
form, and why.

> Draft: entries are added as each difference lands.

## What stays the same

- `'` comments, `IF/ELSEIF/ELSE/ENDIF`, `FOR/NEXT`, `WHILE/WEND`, `DEF/ENDDEF`
- `PRINT a, b` and `PRINT a; b` (see "PRINT output" for how the output is laid out)
- `import "@wwiv.io"`, and calling `outstr()` etc. without the module prefix
- `import "file.bas"`, relative to the importing script. In the BBS, as before, only
  files in the scripts directory can be imported: `"../x.bas"` or an absolute path
  elsewhere is an error.
- Lists: `list()`, `push`, `get`, `remove`, `len` and the other my_basic list functions,
  `FOR x IN l`, and `iterator`/`move_next`/`get`. Indices start at 0, and printing a list
  prints `LIST`.
- Keywords and names are case-insensitive
- The `wwiv.*` module functions keep their names and behaviour

## Removed features (proposed)

GOTO/GOSUB and labels, classes, lambdas and coroutines are not supported. Use
`DEF ... ENDDEF` functions instead.

Not supported: the `^` operator, numbers written with an exponent (`1e3`), and
the functions `SQR`, `SIN`, `COS`, `TAN`, `ASIN`, `ACOS`, `ATAN`, `EXP`, `LOG`,
`RND` and `SRND`. WWIV scripts need real numbers only for things like file
sizes, times and ratios.

Dicts (`dict()`) are not supported yet. WWIV's `wwiv.data.save` and `load` only
support lists.

## PRINT output

my_basic (as WWIV configured it) put every PRINT item on its own line, and `;`
printed an extra newline. wwivbasic follows the usual BASIC rules:

- Items separated by `,` or `;` are printed on one line with nothing between
  them.
- A newline follows, unless the statement ends with `,` or `;`.

```basic
print "len: ", len(l)   ' my_basic: "len: " and "4" on separate lines
                        ' wwivbasic: "len: 4"
print "Name? ";         ' wwivbasic: no newline, the cursor stays on the line
```

Why: output that is laid out the way the script reads.

## TRUE and FALSE

my_basic's `TRUE` and `FALSE` were the integers 1 and 0. In wwivbasic they are
booleans:

- `PRINT TRUE` prints `TRUE` (my_basic: `1`), and `"x" + TRUE` is `xTRUE`.
- Comparing with numbers works as before: `1 = TRUE` and `0 = FALSE` are
  true, so `IF wwiv.eval(...) = TRUE` still works.
- In arithmetic, the left operand decides the result's type: `1 + TRUE` is
  `2`, but `TRUE + 1` is `TRUE`. Don't do arithmetic on `TRUE` and `FALSE`.
- `wwiv.data` saves them as 1 and 0, as my_basic did.

## Errors

Calling a function or module that doesn't exist, passing the wrong number of
arguments, or dividing by zero now stops the script with a message such as:

```
oneliners.bas:42: error: Unknown function 'outsrt'
```

my_basic reported some of these and silently ignored others. In the BBS the
error is shown in color 6 as that one line, with the script's name relative to
the scripts directory, instead of my_basic's "Error: Line, Col, Code, Message"
block. Variables that were never assigned still read as empty, as before, but
a dotted name such as `player.name` whose first part is neither a module nor a
variable is an error (`Unknown variable or module 'player'`).

## wwiv.* functions

The `wwiv`, `wwiv.io` and `wwiv.time` functions keep their names, arguments,
return values and side effects (`wwiv.module_name()` still prints `wwiv`).
Small differences in how arguments are checked:

- A number passed where text is expected is converted to text, as `PRINT`
  would show it (`wwiv.io.outstr(42)` writes `42`). my_basic stopped the script
  with an error.
- Too many arguments (such as `wwiv.io.nl(1, 2)`), a missing required one
  (`wwiv.command()`, `wwiv.time.format(t)`) or one of the wrong type
  (`wwiv.io.outstr(list())`) stop the script with an error that names the
  function, such as `OUTSTR: expected a STRING, got a LIST`.
- `wwiv.io.gets()` with no length, or a length that isn't positive, reads
  nothing and returns `""`; my_basic returned no value at all.
- `wwiv.io.gets(n)` reads at most 254 characters; my_basic overflowed a
  buffer for a longer `n` when the caller didn't use ANSI. (With ANSI, both
  stop at 78, the width of the line editor.)
- Once the caller has hung up, `wwiv.io.getkey()` and `wwiv.io.gets()` return
  `""` and the script stops at the next statement.

`wwiv.data` reads and writes the same files as before, in the same format, so
data saved by a script on one engine loads on the other. `LOAD` still appends
to the list. Two things to know:

- `TRUE` and `FALSE` are saved as the integers 1 and 0, as in my_basic (where
  they were integers).
- Real numbers are saved with 32-bit precision, as before, so a REAL that is
  saved and loaded again may differ in its last digits from the 64-bit value
  the script had. Saving a REAL that a 32-bit float can't hold (larger than
  about 3.4e38, infinite, or not a number) stops the script with an error;
  my_basic's REALs could never hold such values.
- The scope must be a string, as before (`wwiv.data.save(1, l)` is an error).

`wwiv.io.file` (when the sysop enables it):

- New: `WRITE(file, text...)` writes each argument as is, and
  `WRITELINE(file, text...)` writes each followed by a newline, to a file
  opened with mode `"W"`. Before, `"W"` could only empty a file. Each call is
  written to disk at once; a failed write is an error. Reading a file opened
  for writing, or writing one opened for reading, is an error.
- New: `CLOSE(file)` closes a file, so the script can open it again, for
  example to read back what it wrote. Using a closed file is an error.
- Opening a file the script already has open, when either handle writes, is
  an error (`CLOSE` it first). my_basic waited forever on Unix.
- `READLINES` reads from the start of the file each time; my_basic read
  nothing after an earlier `READLINES` or `READINTOSTRING`.
- `OPEN_OPTIONS("MENUS")` now opens files in the current menu set's `gfiles`
  directory (where the BBS keeps menu text), and only for reading (`"W"` is
  an error). my_basic accepted `MENUS` but opened the file in the BBS's
  gfiles anyway.
- Names Windows treats as devices (`CON`, `NUL`, `COM1`, ...) or that contain
  `:` can't be opened.
- `OPEN_OPTIONS` and `OPEN` return objects (printed as `file_options` and
  `file`); passing something else where one is expected is an error that
  names the function. The same goes for `wwiv.os.exec_options` and `EXEC`.
- As before, only the file name part of the name is used, a file that can't be
  read reads as empty, and files not closed stay open until the script ends.

## Real numbers

Real numbers work as in my_basic: `7 / 2` is `3.5`, `6 / 2` is the integer
`3`, a result that is a whole number becomes an integer (`1.5 + 1.5` is `3`),
`MOD` works on integers, reals print like `%g` (`3.14159`, `0.5`), and `FOR`
loops can count in reals (`FOR x = 0 TO 1 STEP 0.25`). Text is read as a
number in decimal only: `VAL("2.5")` is `2.5`, but `"0x10"`, `"inf"` and
`"nan"` read as 0.

The difference is precision: my_basic (as WWIV builds it) used 32-bit floats,
which keep about 7 significant digits, and wwivbasic uses 64-bit doubles. A
calculation can come out slightly differently in its last digits, but
printed results (6 significant digits) are almost always the same.

One printing difference: a whole number too large for an integer, such as a
timestamp or a file size in bytes, prints in full up to 10 digits
(`3000000000`), where my_basic printed `3e+09`.
