# Porting scripts from my_basic to wwivbasic

Most WWIVbasic scripts written for my_basic run unchanged. This guide lists
every place where wwivbasic intentionally differs, with the old form, the new
form, and why.

> Draft: entries are added as each difference lands.

## What stays the same

- `'` comments, `IF/ELSEIF/ELSE/ENDIF`, `FOR/NEXT`, `WHILE/WEND`, `DEF/ENDDEF`
- `PRINT a, b` and `PRINT a; b` (see "PRINT output" for how the output is laid out)
- `import "@wwiv.io"`, and calling `outstr()` etc. without the module prefix
- `import "file.bas"`, relative to the importing script
- Lists: `list()`, `push`, `get`, `remove`, `len` and the other my_basic list functions,
  `FOR x IN l`, and `iterator`/`move_next`/`get`. Indices start at 0, and printing a list
  prints `LIST`.
- Keywords and names are case-insensitive
- The `wwiv.*` module functions keep their names and behaviour

## Removed features (proposed)

GOTO/GOSUB and labels, classes, lambdas and coroutines are not supported. Use
`DEF ... ENDDEF` functions instead.

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

## Errors

Calling a function or module that doesn't exist, passing the wrong number of
arguments, or dividing by zero now stops the script with a message such as:

```
oneliners.bas:42: error: Unknown function 'outsrt'
```

my_basic reported some of these and silently ignored others. Variables that
were never assigned still read as empty, as before.
