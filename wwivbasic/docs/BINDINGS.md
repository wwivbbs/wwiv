# WWIV bindings for scripts

The modules WWIV gives scripts on the new engine, beyond the `wwiv`, `wwiv.io`,
`wwiv.io.file`, `wwiv.data`, `wwiv.os` and `wwiv.time` modules that the legacy
engine also has (see [PORTING.md](../PORTING.md) for how those differ).

The bindings live in WWIV, in `bbs/basic/next/`.

## wwiv.msg

Read-only access to the message areas (subs) the caller can read. It shows a
script only what the caller could read in the BBS's own reader:

- The areas are the caller's subs in the current conference, in the order the
  BBS lists them (`usub`): subs whose read ACS the caller doesn't pass aren't
  there.
- Unvalidated and deleted posts are left out unless the caller is a co-sysop
  of the sub (`lcs()`).
- For anonymous posts, `FROM` is `>UNKNOWN<` (or `Abby` / `Problemed Person`
  for "Dear Abby" posts) unless the caller may read anonymous posts (a
  co-sysop, or an SL with "read anonymous posts"). `ANONYMOUS` is `TRUE`
  either way. As in the BBS, the date of a "Dear Abby" post is hidden from
  everyone (`DATE` is 0).

The module is on by default; the sysop can turn it off with "Enable Package
MSG" in wwivconfig's scripting settings (the `script_flag_disable_msg` bit,
`0x0010`, in `config.json`'s `script_flags`).

```basic
import "@wwiv.msg"

for a in areas()
  print a.key; ") "; a.name; " ("; a.count; " messages)"
next

a = area("1")
for h in a.headers()
  print h.number; " "; h.title; " - "; h.from
next

m = a.message(1)
print m.title
for line in m.lines()
  print line
next
```

WWIV ships a complete message viewer written with it,
`install/scripts/msgview.bas` (in `scripts/` on a BBS): it lists the areas,
pages through an area's headers, and reads messages one after another.

### Functions

| Function | Returns |
| --- | --- |
| `AREAS()` | A LIST of the areas the caller can read, as the BBS lists them |
| `AREA(key)` | The area the caller chooses with `key` at the sub prompt (`"1"`, or a sub's own key); an error if there is none |
| `CURRENT_AREA()` | The area the caller is in; an error if there is none |
| `MODULE_NAME()` | `"wwiv.msg"` |

### Areas (`message_area`)

| Member | |
| --- | --- |
| `KEY` | The key that chooses the area, as in `AREA(key)` |
| `NAME` | The sub's name |
| `DESCRIPTION` | The sub's long description |
| `COUNT` | How many messages the area has, including ones the caller can't read |
| `HEADERS()` | A LIST of the headers of the messages the caller can read, oldest first |
| `MESSAGE(n)` | Message `n`, from 1 to `COUNT`; an error if it doesn't exist or the caller can't read it |

Each call reads the message base again, so `COUNT` and `HEADERS()` see new
posts.

### Headers (`message_header`) and messages (`message`)

| Member | |
| --- | --- |
| `NUMBER` | The message number in its area, from 1 |
| `TITLE` | The title (subject) |
| `FROM` | Who wrote it, such as `rushfan #1`, after the anonymous rules above |
| `TO` | Who it is to (`All` if no one) |
| `DATE` | When it was posted, as a time_t (`wwiv.time.format` formats it), or 0 if hidden |
| `ANONYMOUS` | `TRUE` if it was posted anonymously |

A `message` also has:

| Member | |
| --- | --- |
| `TEXT` | The text, each line followed by `\r\n`, without WWIV's control lines (`^D0...`) |
| `LINES()` | The text as a LIST of lines |

The text keeps its pipe and heart color codes. Print text other users wrote
(titles, names and message text) with `wwiv.io.outstr_nomci` or
`wwiv.io.pl_nomci`: they show the colors but, like the BBS's own reader, don't
run MCI codes (`|{...}` expressions and `|@` macros), so a post can't make the
reader's screen show the reader's own details.

## wwiv.io additions

The new engine adds these to `wwiv.io`; the other functions are as in the
legacy engine.

| Function | |
| --- | --- |
| `OUTSTR_NOMCI(text...)` | As `OUTSTR`, but with MCI codes shown as written instead of run. Colors still show. |
| `PL_NOMCI(text...)` | As `PL`, with MCI codes shown as written. |

`OUTSTR`, `PL` and `PRINT` still run MCI codes, as scripts usually want for
their own text.
