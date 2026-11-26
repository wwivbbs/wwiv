# WWIV bindings for scripts

The modules WWIV gives scripts on the new engine, beyond the `wwiv`, `wwiv.io`,
`wwiv.io.file`, `wwiv.data`, `wwiv.os` and `wwiv.time` modules that the legacy
engine also has (see [PORTING.md](../PORTING.md) for how those differ).

The bindings live in WWIV, in `bbs/basic/next/`.

## wwiv.msg

The message areas (subs) the caller can read: reading them, and, when the
sysop allows it, posting and deleting posts (see "Posting and deleting"
below). It shows a script only what the caller could read in the BBS's own
reader:

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
| `COUNT` | How many messages the area has, including ones the caller can't read: the number the BBS's own sub list shows. `LEN(HEADERS())` is how many the caller can read. |
| `HEADERS()` | A LIST of the headers of the messages the caller can read, oldest first |
| `MESSAGE(n)` | Message `n`, from 1 to `COUNT`; an error if it doesn't exist or the caller can't read it |
| `FIND(h)` | The number the message `h` (a header or message from this area) has now, or 0 if it has been deleted or the caller can't read it |

Each call reads the message base again, so `COUNT` and `HEADERS()` see new
posts.

Message numbers are positions in the area, as in the BBS: when a post is
deleted, the ones after it move down by one. A script that keeps a list of
headers while the caller reads should find each message again with `FIND`
before reading it, and can check that the message's `ID` is the header's:

```basic
n = a.find(h)
If n = 0 Then
  print "That message has been deleted."
Else
  m = a.message(n)
  ' m.id = h.id unless it was deleted between FIND and MESSAGE.
EndIf
```

Opening an area whose message text file (`msgs/NAME.dat`) doesn't exist yet
creates it empty, as the BBS does.

### Posting and deleting

| Member | |
| --- | --- |
| `CAN_POST` | `TRUE` if the caller may post in the area now |
| `ANONYMOUS` | How the caller's posts here may be anonymous: `"no"`, `"allowed"` (`POST`'s `anonymous` chooses) or `"forced"` (always anonymous) |
| `POST(title, text[, to[, anonymous]])` | Posts `text` (a string, or a LIST of lines) titled `title`, to `to` (`""` for everyone), and returns the new message's number. `title` and `to` are at most 71 characters, with no control characters (CR, LF, ^D...). |
| `WRITE_POST([title[, to]])` | Lets the caller write a post in their own message editor (the full-screen editor or the line editor), as the BBS's post command does, and returns its number, or 0 if they abort. Without `title` the editor asks for one; where the caller may post anonymously, it asks about that too. |
| `REPLY(h)` | Lets the caller write a reply to the post `h` (a header or message from this area) in their editor: to its author, with "Re: " and its title offered as the title. Returns the reply's number, or 0 if they abort. |
| `DELETE_MESSAGE(h)` | Deletes the post `h` (a header or message from this area); `FALSE` if it was already gone |

Scripts can only post and delete when the sysop turns on "Enable MSG Writing"
in wwivconfig's scripting settings (the `script_flag_enable_msg_write` bit,
`0x0040`, in `config.json`'s `script_flags`). It's off by default: a script
can post without the caller seeing what it posts. Until it's on, `CAN_POST`
is `FALSE` and `POST`, `WRITE_POST`, `REPLY` and `DELETE_MESSAGE` are
errors.

With writing on, the BBS's own rules apply, as when the caller posts from the
BBS:

- `POST`, `WRITE_POST` and `REPLY` are errors, saying why, if the caller is
  restricted from posting, has posted as many messages as their SL allows
  today, doesn't pass the area's post ACS, or is restricted from networked
  areas and the area is networked.
- The post is signed as the caller's posts in the area are (their name and
  number, or their real name in areas that use real names), counts toward
  their posts and today's limit, and goes in the sysop log. A caller whose
  posts need validating gets an unvalidated post. The area's oldest posts go
  if it's full.
- `anonymous` (`TRUE` or `FALSE`) is allowed where `ANONYMOUS` is
  `"allowed"`; asking for an anonymous post anywhere else is an error. In
  "Dear Abby" areas `TRUE` posts as "Abby". Where it's `"forced"` every post
  is anonymous.
- In networked areas, the post goes out on all of the area's networks. In
  an area that needs network validation, it waits for the sysop to validate
  it, as the BBS's posts do: a co-sysop's new post goes out right away, but
  their replies still wait. (Some details of how the BBS sends posts aren't
  matched yet: [#1782](https://github.com/wwivbbs/wwiv/issues/1782).)
- `WRITE_POST` and `REPLY` open the editor the caller uses in the BBS. A
  script with a full-screen layout open should close it first, since the
  editor has the whole screen.
- A reply works like one from the BBS's reader: the caller can quote the
  post (`/Q`, or auto-quote if they have it on), and on FTN areas the reply
  carries the post's MSGID so other systems thread it under the post. If
  the post has been deleted, the caller can still reply, without quoting.
- `DELETE_MESSAGE` deletes the caller's own posts, or any post if the caller
  is a co-sysop of the area. Deleting their own post takes back its credit.

```basic
a = wwiv.msg.area("1")
If a.can_post Then
  n = a.post("Hello", list("First line", "Second line"))
  print "Posted message "; n
EndIf
```

### Headers (`message_header`) and messages (`message`)

| Member | |
| --- | --- |
| `NUMBER` | The message number in its area, from 1, when it was read |
| `ID` | The post's serial number (its qscan value), which doesn't change when other posts are deleted |
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

### For full-screen UIs

These are the pieces a full-screen, lightbar-style UI is built from. Their
names are WWIV's own where WWIV has one (4.24's `okansi`, `ansic`, `prt`,
`pla`, `onek` and `nsl`; `goxy` as `GOTOXY`), otherwise Synchronet's. Without
ANSI the screen functions do nothing, so check `OKANSI()` and use a plain UI
instead. Once the caller hangs up, the input functions stop waiting and the
script stops at its next statement.

| Function | |
| --- | --- |
| `OKANSI()` | `TRUE` if the caller has ANSI on |
| `GOTOXY(x, y)` | Moves the cursor to column `x`, row `y`, counting from 1 |
| `CLREOL()` | Clears from the cursor to the end of the line |
| `ANSIC(n)` | Selects color `n` (0 to 9) of the caller's colors, as `\|#n` does |
| `PRT(n, text...)` | Writes `text` in color `n`, then goes back to color 0 |
| `PLA(text)` | Writes a line the caller can abort (Space or ^C); returns `TRUE` if they did |
| `ONEK(keys)` | Waits for the caller to press one of `keys` (case doesn't matter) and returns it in upper case. It doesn't echo the key or start a new line. Once the caller hangs up it returns the first of `keys`, so put the one that quits first. |
| `READKEY()` | Waits for a key and returns it; for a special key, its name: `UP`, `DOWN`, `LEFT`, `RIGHT`, `HOME`, `END`, `PGUP`, `PGDN`, `INS`, `DEL`, `F1` to `F10`. A result longer than one character is a special key; Enter is `CHR(13)`. Digits come back as digits. Keys the BBS handles itself, such as Ctrl+T, are skipped. `GETKEY` is unchanged. |
| `SCREEN_COLUMNS()`, `SCREEN_ROWS()` | The size of the caller's screen |
| `STRIP_COLORS(s)` | `s` without its pipe and heart color codes |
| `LEN_COLORS(s)` | How wide `s` shows on the screen |
| `PAD_COLORS(s, n)` | `s` padded with spaces to show `n` wide (`n` at most 65535) |
| `PAUSING(on)` | Turns the "More?" prompt on (`TRUE`) or off (`FALSE`), and returns whether it was on. The user's own setting comes back when the script ends. The caller can still switch it with Ctrl+Y. |
| `NSL()` | Seconds left in this call |

```basic
import "@wwiv.io"

If Not okansi() Then
  pl("This needs ANSI.")
  End
EndIf
x = pausing(FALSE)
cls()
gotoxy(1, 1)
prt(5, pad_colors("|#1Messages", screen_columns()))
gotoxy(1, screen_rows())
outstr("|#9Press Q to quit: ")
k = ""
While k <> "Q" And k <> "q"
  k = readkey()
  gotoxy(1, 3)
  clreol()
  outstr("You pressed: ", k)
Wend
```

## wwiv.screen

Full-screen layouts and lightbar lists, built in C++ on what the BBS's own
full-screen message reader uses. Both need ANSI: without it they stop the
script with an error, so check `wwiv.io.okansi()` first and use a plain UI
otherwise.

### Functions

| Function | |
| --- | --- |
| `FULLSCREEN(header_lines)` | Clears the screen and opens a full-screen layout (a `screen`): `header_lines` rows at the top for the script, a bar, the content rows, a bar, and the command line on the bottom row. Pausing is off while it's open. One can be open at a time. |
| `LIGHTBAR(items, top, height)` | A scrolling list (a `lightbar`) of the strings in the list `items`, `height` rows from screen row `top` (1 is the top row), as wide as the screen less one column. Nothing is drawn until `DRAW` or `RUN`. |

### Screens (`screen`)

| Member | |
| --- | --- |
| `width`, `height` | The size of the screen |
| `content_top`, `content_height` | The first row between the bars, and how many there are |
| `top_bar()` | Draws the bar under the header lines |
| `bottom_bar(text)` | Draws the bar over the command line, with `text` (if any) near its right end |
| `command_line(text)` | Writes `text` on the bottom row, then puts the cursor at its start |
| `clear_command_line()` | Clears the bottom row |
| `clear_content()` | Clears the rows between the bars |
| `readkey()` | `wwiv.io.readkey()`, showing the "are you there?" warning on the command line when the caller is idle |
| `close()` | Clears the screen and turns pausing back on. After `close()` the screen's methods are errors. |

### Lightbars (`lightbar`)

| Member | |
| --- | --- |
| `count` | How many items there are |
| `selected` | The selected item, counting from 0 (-1 if there are none). Set it to select an item; `DRAW` or `RUN` shows it. |
| `top_item` | The item on the list's first row |
| `color`, `selected_color` | Color codes for the items and the selected one (`"\|#0"` and `"\|17\|15"`, white on blue, unless set) |
| `draw()` | Draws the whole list |
| `set_items(items)` | Replaces the items, keeping the selection if it's still in the list |
| `run()` | Draws the list, then lets the caller move through it with Up, Down, Page Up, Page Down, Home and End, and returns the first other key, as `wwiv.io.readkey()` returns it (Enter is `CHR(13)`). Returns `""` once the caller hangs up. While a `FULLSCREEN` is open, keys are read through it, so an idle caller gets its "are you there?" warning on the command line. |
| `run(fn)` | Like `run()`, but for each other key calls the script function named `fn` as `fn(key, selected)`, and keeps going while it returns `TRUE`. A name that isn't a function is an error when `run` starts. It draws the list again after each call, since the function may have drawn over it, and returns the key `fn` returned `FALSE` for. |

A screen also closes when the script no longer holds it: when the script
ends, or when the variable holding it goes away, such as a local variable in
the `DEF` that opened it once that `DEF` returns. Pausing comes back then too,
but the screen isn't cleared; only the command line is, and the cursor goes to
its start, so whatever comes next starts on the bottom row. Keep the screen in
a global variable, or in a `Main()` that runs the whole UI, while it's in use.

Moving within the list redraws only the rows that change. Items are written
with MCI codes shown as written (they're often what other users wrote); the
selected one is shown without its own colors, so the highlight shows. Items
too wide for the list are cut.

```basic
import "@wwiv.io"
import "@wwiv.screen"

def OnKey(key, selected)
  If key = CHR(13) Then
    s.command_line("|#9You chose |#2" + get(items, selected))
    return TRUE
  EndIf
  return key <> "Q" And key <> "q"
enddef

If Not okansi() Then
  pl("This needs ANSI.")
  End
EndIf
items = list("Apples", "Bananas", "Cherries", "Dates", "Elderberries")
s = fullscreen(1)
gotoxy(1, 1)
outstr("|#5Fruit")
s.top_bar()
s.bottom_bar("")
s.command_line("|#9Enter chooses, Q quits")
l = lightbar(items, s.content_top, s.content_height)
k = l.run("OnKey")
s.close()
```

## wwiv.user

The caller's own details, and the public details of other users. Like
`wwiv.msg`, it's read-only, on by default, and the sysop can turn it off with
"Enable Package USER" in wwivconfig's scripting settings (the
`script_flag_disable_user` bit, `0x0020`, in `config.json`'s
`script_flags`).

```basic
import "@wwiv.user"

u = current()
print "Hello, "; u.name; " (SL "; u.sl; ")"
If u.cosysop Then
  print "You can see the co-sysop options."
EndIf

n = find("sysop")
If n > 0 Then
  print "The sysop last called on "; wwiv.time.format(user(n).last_on, "%m/%d/%y")
EndIf
```

### Functions

| Function | Returns |
| --- | --- |
| `CURRENT()` | The caller (a `user`), read when it's called |
| `FIND(name)` | The number of the user named `name` (case doesn't matter), or 0 if there's none |
| `USER(n)` | The public details of user `n` (a `user_info`); an error if there's no such user, or it's deleted or inactive |
| `MODULE_NAME()` | `"wwiv.user"` |

### The caller (`user`)

The names match the BBS's `user.*` expressions (in menus and `|{...}` codes)
where there is one.

| Member | |
| --- | --- |
| `NUMBER` | The user number |
| `NAME` | The name as stored, such as `RUSHFAN` (as `user.name` and `\|@N`) |
| `REAL_NAME` | The real name, or `""` if none |
| `SL`, `DSL` | The security level and download security level, as stored (a temporary SL from the sysop doesn't change them) |
| `AR`, `DAR` | The ARs and DARs as letters, such as `"AC"` |
| `SYSOP` | `TRUE` if the SL is 255 |
| `COSYSOP` | `TRUE` for the sysop, or if the caller's current SL (the stored one, before the BBS sets it) has the co-sysop ability |
| `LOGONS` | How many times the user has logged on |
| `POSTS`, `UPLOADS`, `DOWNLOADS` | How many messages posted, and files uploaded and downloaded |
| `LAST_ON` | When the user last logged on, as a time_t; for the caller, the start of this call |

### Other users (`user_info`)

Only what any caller may see of another user, whoever is asking:

| Member | |
| --- | --- |
| `NUMBER` | The user number |
| `NAME` | The name as stored, such as `SYSOP` |
| `LAST_ON` | When the user last logged on, as a time_t |
