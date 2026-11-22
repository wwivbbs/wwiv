# TODO: Structural issues in `bbs/` (address before most of `todo-c-isms.md`)

These four structural issues should come before most of the C-ism list in `todo-c-isms.md`, because fixing them deletes or contains much of the code that list complains about.

The exception is the four P0 overflows in `todo-c-isms.md`: fix those now regardless, since they're small and `netsup.cpp` is reachable from remote network input.

Counts below are as of commit 1be735c8d.

## 1. `bbs/` duplicates the data layer that `sdk/` already provides

`sdk/` has proper, tested APIs for message storage and email, but much of `bbs/` still reads and writes the on-disk formats itself with `Read(&rec, sizeof(rec))`.

- [ ] **Message text storage:** `bbs/message_file.cpp` and `bbs/subacc.cpp` reimplement the storage that `sdk/msgapi/type2_text.cpp` provides. `message_file.cpp` keeps that state in a process-wide `static gat` array; the sdk version loads it per call.
- [ ] **Email:** these files work directly on `mailrec`, alongside `sdk/msgapi/email_wwiv.cpp`:
  - `readmail.cpp` (21 raw record reads/writes)
  - `email.cpp` (14)
  - `attach.cpp` (11)
  - `multmail.cpp` (8)
- [ ] **Votes:** `vote.cpp` and `voteedit.cpp` read `votingrec` directly. There is no sdk API for votes yet, so one needs to be added.

**Why first:** two implementations of the same file format can drift apart and corrupt data. Moving `bbs/` onto the sdk APIs deletes several P0/P1/P2 items in `todo-c-isms.md` instead of fixing them one by one:

- the `message_file.cpp` overflow
- the static `gat`
- the `subacc.cpp` buffers
- the `multmail.cpp` buffers

## 2. Thin test coverage in `bbs/`

- [ ] Add characterization tests around each module before refactoring it.

`bbs/` has 21 test files for about 95 source files, compared with 40 in `sdk/` and 26 in `core/`. The code most in need of change is the least tested: `readmail`, `email`, `msgbase1`, `chat`, `listplus` and `netsup`. `todo.md` already says "redo readmail(...) It's a friggin mess." Tests are what make #1 and the C-ism rewrites safe.

## 3. Global service locator: `a()`, `bout`, `bin`

- [ ] Adopt a rule: new and refactored functions take `Context&` (and an output/input interface) as parameters rather than calling `a()`.

104 of the 115 `.cpp` files in `bbs/` call `a()`. In total there are about 3,800 `a()->` and 4,500 `bout.`/`bin.` uses. That hidden coupling is the main reason `bbs/` is hard to test (#2).

The way out is already started: `common/context.h` defines `Context` and `SessionContext`, and `Application` exposes them. Don't do a big-bang rewrite; with the rule above, coverage improves file by file.

## 4. File layout left over from DOS overlays

- [ ] When a module is touched for #1 or #3, move it into a feature-named unit (as `menus/` and `qwk/` already are). Don't reorganize this on its own.
- [ ] Confine raw `Read(&x, sizeof x)` of the packed on-disk structs in `sdk/vardec.h` to sdk serialization code. Doing #1 achieves this for `bbs/`.

`bbs/` is mostly free functions (343 declared in its headers), grouped into files like `bbsovl1/2/3`, `bbsutl/1/2` and `xferovl/xferovl1`. Those names come from DOS overlay segments, not from features, so related logic is scattered and file names don't tell you where anything lives.

## Suggested overall order

1. [ ] Fix the four P0 overflows from `todo-c-isms.md` now.
2. [ ] Email first: the sdk API already exists and the bbs code is messy.
   - Add characterization tests.
   - Move the code onto `sdk/msgapi`.
   - Take `Context&` instead of calling `a()`.
3. [ ] Message text storage: `message_file.cpp`/`subacc.cpp` → `type2_text`.
4. [ ] Votes: add a small sdk API, then move `vote.cpp`/`voteedit.cpp` onto it.
5. [ ] Clean up the remaining C-isms in each file as you pass through it, rather than as a separate sweep. `chat.cpp` is the main file that probably needs a dedicated rewrite.

These findings are based on file counts, includes and grep, plus a check that the duplicated sdk code exists. The bbs and sdk versions of the format code have not been compared line by line.
