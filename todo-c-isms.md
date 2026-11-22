# TODO: C-isms in `core/` and `bbs/`

**Scope:** about 400 files and 59k lines, built as C++17. `bbs/prot/` (the zmodem/xmodem code) is left out because it is mostly old C and accounts for many of the `extern`, `NULL`, `exit()` and `malloc` hits.

Most of the codebase already uses modern C++ (`[[nodiscard]]`, `std::optional`, `noexcept`, fmt). The problems are concentrated in a few legacy files: `chat.cpp`, `listplus.cpp`, `netsup.cpp`, `multmail.cpp`, `xferovl.cpp`, `dropfile.cpp` and `syschat.cpp`.

Line numbers are as of commit 1be735c8d.

## P0: Memory-safety bugs (fix these first)

- [x] **`bbs/xferovl.cpp:434-456`:** `ext` is a fixed 4096-byte `BbsAllocA` buffer. Every extended-description line is `strcat`'d into it with no bounds check, so a long description file overflows the heap. Fix: use a `std::string ext` and `+=`.
- [x] **`bbs/dropfile.cpp:214,224`:**
  - `sprintf(pcb.openbps, "%-5.5s")` writes 6 bytes into `char[5]`.
  - `sprintf(pcb.firstname, "%-15.15s")` writes 16 bytes into `char[15]`.
  - Both only work because the next field happens to be overwritten afterwards.
  - `strtok` can return `nullptr` for an empty name, which is then passed to `sprintf`.
  - Fix: add a helper that fills a fixed-width field with no terminator (`std::string` → `memcpy` padded).
- [x] **`bbs/message_file.cpp:247-255`:** `strcpy(&b[j], line.c_str())` copies into a `calloc`'d block without checking `line.size()`. Fix: use a `std::vector<char>` or `std::array` with an explicit length check.
- [x] **`bbs/netsup.cpp:300-390`:** `strcpy`/`strcat`/`sprintf` into fixed `newname`/`qn`/`on`/`nm` buffers built from network message text, so remote input reaches the overflow. Fix: rewrite with `std::string` and `fmt::format`.
- [ ] **`bbs/message_file.cpp` `readfile`:** `out.size() - MSG_BLOCK_SIZE` is unsigned, so it wraps around for messages shorter than one block and the trailing `^Z` is never stripped from them. (`message_file_test.cpp` has a TODO and expects the current behavior.) Not an overflow, but a real bug.
- [ ] **`sdk/user.h` `User::set_name`:** `strcpy` into `name[31]` with no length check. Fix: `to_char_array_trim`, or reject names that are too long.

## P1: Manual memory management → RAII

- [ ] **Remove `BbsAllocA`** (`bbs/bbsutl.cpp:345`). It is a `calloc` wrapper whose callers all `free()` by hand. Convert each caller:
  - [ ] `voteedit.cpp:146` → `std::vector<char>`
  - [ ] `diredit.cpp:354,398` → `std::vector<uint32_t>`
  - [ ] `multmail.cpp:367` → `std::string`
  - [ ] `xferovl.cpp:434` (see P0)
- [ ] **`bbs/chat.cpp:61-65,649-670`:**
  - `static ch_action* actions[100]` holds `calloc`'d entries and uses `strcpy` into `char[80]` fields.
  - Off-by-one: `g_nNumActions` is incremented before storing, so slot 0 is never used and `free_actions` loops `<=`.
  - Fix: replace with `std::vector<ch_action>` and make the fields `std::string`.
- [ ] **`bbs/syschat.cpp:64,601-642`:** `static char (*side0)[MAXLEN]` is set with `new[]`/`delete[]`, and there is a dead `if (!side0)` check, since `new` never returns null. Fix: use a local `std::vector<std::string>` passed to `two_way_chat`.
- [ ] **`bbs/message_file.cpp:43`:** `static gati_t* gat = new gati_t[2048]()` is never freed. Fix: use `static std::array<gati_t, 2048>`.
- [ ] **`bbs/subacc.cpp:266`:** `malloc(BUFSIZE)` → `std::vector<char>` or `std::unique_ptr<char[]>`.
- [ ] **`bbs/msgbase1.cpp:109`:** `unique_ptr<char[]>(new char[...])` → `std::make_unique<char[]>`, or better a `std::string`.
- [ ] **`.reset(new X)` → `std::make_unique`:**
  - `core/log.cpp:200-201`
  - `bbs/xinit.cpp:472,503`
  - `bbs/xferovl1.cpp:290`
  - `bbs/wqscn.cpp:34`
  - `bbs/application.cpp:972-994`: `reset_local_io` should take a `std::unique_ptr`.
- [ ] **`core/os_win.cpp:98`:** `calloc`/`free` for `SYMBOL_INFO` → `std::vector<std::byte>` or `unique_ptr` with a deleter.

## P2: C strings and C library calls

- [ ] **`strtok` (14 sites).** It is not reentrant and depends on hidden global state. Fix: use `wwiv::strings::SplitString`, or a `std::string_view` tokenizer.
  - [ ] `multmail.cpp:186-197` (worst: it keeps the tokenizer state in `static char* mml_s` across calls of `oneuser()`)
  - [ ] `bbsutl1.cpp:66,85`
  - [ ] `newuser.cpp:807`
  - [ ] `subreq.cpp:142`
  - [ ] `xferovl.cpp:454`
  - [ ] `dropfile.cpp:223`
- [ ] **Raw `strcpy`/`strcat`/`sprintf`:** concentrated in `chat.cpp`, `listplus.cpp`, `netsup.cpp`, `gfiles.cpp`, `dropfile.cpp`, `syschat.cpp`, `readmail.cpp` and `sysopf.cpp`. Many hits in `application.cpp` are already the safe `fmt::sprintf`.
- [ ] **`char*` in/out parameters:**
  - `chat.cpp` alone has 20, e.g. `main_loop(const char*, char*, char*, char*, …)`, `rip_words(…, char* wd, int size, …)`, `wusrinst(char*)`, `userinst(char*)`.
  - Also `lpfunc.cpp`, `newuser.cpp`, `basic/*.cpp`.
  - Fix: use `const std::string&` / `std::string_view` for input and return values for output.
- [ ] **Fixed `char[N]` locals (~100):** mainly `chat.cpp`, `dropfile.cpp`, `multmail.cpp`, `subreq.cpp`, and `lilo.cpp:84` (`static char g_szLastLoginDate[9]`).
- [ ] **`core/strings.h`** still exposes C-string APIs that keep these patterns alive. Deprecate them once their callers are converted:
  - `stripcolors(const char*) → char*`
  - `properize(char*)`
  - `StringTrim(char*)`
  - `strupr`, `strrev`, `strcasestr`
  - the `char*` overloads of `IsEquals`/`iequals`/`StringCompare`
- [ ] **`std::string*` out-params:** `StringTrim(std::string*)`, `StringReplace`, `inli(std::string*, std::string*, …)`, `make_abs_cmd(…, std::string* out)`, `modify_extended_description`. Fix: return by value or use references. For `ReadLine(std::string*)`, consider `std::optional<std::string>`.
- [ ] **`core/datetime.cpp:47,295,315,320`:**
  - `localtime`/`asctime` are not thread-safe. Fix: use `localtime_r`/`localtime_s` (or `<chrono>`).
  - At line 47, the code writes through the pointer returned by `localtime`, which points at shared static storage.
- [ ] **`atoi` (8 sites)** silently returns 0 on bad input (e.g. `datetime.cpp:49-51`). Fix: use the existing `to_number<T>`.

## P3: Global and static mutable state

- [ ] **Statics that act like globals.** Each should become a member of a small class or of the `Session`/`Context`:
  - `chat.cpp:61-65`
  - `syschat.cpp:63-66`
  - `multmail.cpp:185-186`
  - `subacc.cpp:50-51` (`static unique_ptr<File>` plus `static char subdat_fn[MAX_PATH]`)
  - `wqscn.cpp:30`
  - `instmsg.cpp:47-51`
  - `listplus.cpp:73-76`
  - `qwk/qwk_mail_packet.cpp:69`
  - `wfc.cpp:79`
  - `conf.cpp:49`
  - `core/wfndfile_unix.cpp:36` (`static std::string s_path`, so `FindFiles` is not reentrant)
- [ ] **`extern` globals in headers:**
  - `bbs/crc.h:27` (`extern uint16_t crc`)
  - `bbs/listplus.h:25,79` (`foundany`, `lp_config`)
  - `bbs/lpfunc.cpp`, `utility.cpp`, `srsend.cpp`, `xferovl1.cpp`
- [ ] **Architectural (long term, not a quick fix):** about 3,800 `a()->` and 4,500 `bout.`/`bin.` uses form a service locator that makes most of `bbs/` hard to unit test. Do this gradually: pass `Context&`/`Output&` into new and refactored functions instead of reaching for `a()`.

## P4: Smaller idiom and cleanup items

- [ ] **Functions returning `int` 0/1 used as bool:** `check_ansi`, `rip_words`, `f_action`, `wusrinst`, `userinst`, `check_lines_needed`, `prep_search_rec`, `search_criteria`, and many in `listplus.cpp`. Return `bool`, or an `enum class` when there are more than two outcomes. Also `int ok1 = 0` flags (`xferovl.cpp:452`) → `bool`.
- [ ] **`goto`:** `lpfunc.cpp` (3), `listplus.cpp`, `dropfile.cpp`. Usually replaceable with an early return or a helper function.
- [ ] **`typedef` → `using`:** `core/findfiles.h:48-50`, `core/graphs.h:41`, `core/net.h:105`.
- [ ] **Object-like `#define` constants** in `core/file.h`, `core/net.h` and `bbs/qwk/qwk.h` → `constexpr`.
- [ ] **`using namespace std;` in 22 `.cpp` files** (none in headers): `core/os*.cpp`, `core/file.cpp`, `core/datetime.cpp`, `bbs/application.cpp`, `wfc.cpp`, `netsup.cpp`, and others. Drop it or replace with targeted `using std::x;`.
- [ ] **`core/strings.h:51-68`:** `StrCat` builds a `std::ostringstream` per argument, recursively, so it is O(n²). Fix: use a fold expression over a single stream, or `fmt::format`.
- [ ] **`catch (...)` that silently returns `{}`:** `core/strings.h:56,66`, `core/strings.cpp:387`, `core/textfile.cpp:196`, `core/inifile.cpp:185`. At minimum log the error; better, don't mark the function `noexcept` if it can fail.
- [ ] **`std::endl` (46)** → `'\n'` unless a flush is actually wanted.
- [ ] **`listplus.cpp:73-74`:** `static char _on_[] = "ON!"` should be `constexpr std::string_view`. Identifiers starting with `_` at namespace scope are reserved.
- [ ] **Header guards (optional):** 158 `#ifndef INCLUDED_…` guards and no `#pragma once`. This is only a consistency choice; skip it unless the churn is wanted.

## Suggested order

1. Fix the four P0 overflows; each is a small, self-contained PR.
2. Remove `BbsAllocA` and its five callers.
3. Rewrite `chat.cpp`; it alone contains most of P1, P2 and P3.
4. Replace `strtok` everywhere, starting with `multmail.cpp`.
5. Fix `localtime` and `asctime` in `datetime.cpp`.
6. Convert the C-string APIs in `core/strings.h` once their callers are gone.

The four P0 items are confirmed from reading the code but not reproduced at runtime.
