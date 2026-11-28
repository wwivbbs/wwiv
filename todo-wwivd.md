# TODO: `wwivd/` robustness, code quality, and modern C++

From a review of `wwivd/` (plus the parts of `core/net`, `core/socket_connection`, and `core/pipe` it uses) and the open issues that touch it: #1564, #1640, #1656, #1577, #1594, #1421.

Items marked *(likely)* were found by reading the code and still need a repro.

## Phase 1: socket ownership and hung sessions (#1564)

Done and tested on Linux. The Windows parts (`wwivd_win.cpp`, `core/pipe_win32.cpp`, pipe mode) have not been compiled or run yet: CI will compile them, and they still need a test on a real Windows BBS.

- [x] **Single owner for each client socket.** The socket for a telnet session is closed in `ExecCommandAndWait`, in `launch_node`'s `finally`, and again by `SocketConnection conn(sock)` in `HandleConnection`. BinkP and the DENY paths close twice. Between closes, another thread's `accept()` can reuse the number, and the next close hangs up a different caller. Add a move-only `SocketHandle` in `core/net.h` and pass ownership explicitly.
- [x] **Children inherit every socket** *(likely root cause of #1564)*. Windows `CreateProcess(..., bInheritHandles=TRUE, ...)` passes every other caller's socket and the listening sockets to each child. On Unix, `accept()` doesn't set close-on-exec. Fixes:
  - Windows: `STARTUPINFOEX` + `PROC_THREAD_ATTRIBUTE_HANDLE_LIST` with only the caller's socket.
  - Unix: `accept4(SOCK_CLOEXEC)`, close-on-exec on listening sockets, and `posix_spawn_file_actions_adddup2(sock, sock)` to pass the one socket.
- [x] **Pipe mode never tells the BBS the caller hung up** *(likely)*.
  - When the caller hangs up, `socket_pipe_loop_one` returns without closing the data pipe.
  - The outer loop then calls `Create()` again and leaks the old handle.
  - `Pipe::peek` can't tell "no data" from "broken pipe", so neither side ever sees the other go away.
  - `tv_usec = 100; // 100ms` is 100 µs, so output to the caller goes through a 200 ms sleep path.
- [x] `DoMatrixLogon` calls `conn.close()` on Quit or timeout, then `HandleConnection` writes to the closed socket and closes it again.
- [x] `telnet_to` calls `in.close()`, which closes the socket, and then the caller closes it again.
- [x] BinkP: when `AcquireNode` fails, the socket is leaked and no `BUSY` is sent.
- [x] `SocketSet::RunOnce` passes `INVALID_SOCKET` to the handler when `accept()` fails (EMFILE under a flood).

## Phase 2: thread lifetime and shutdown

- [x] Client threads and the callout thread are detached and hold pointers or references to locals in `Main` (`config`, `c`, `nodes`, `data`, `clock`). SIGTERM leads to use after free. Move to `std::jthread` + `std::stop_token` with a session registry that shutdown can stop and join, and remove the `need_to_exit` and `need_to_reload_config` globals. (`wwivd/sessions.h`; the signal flags now live behind `wwivd/signals.h`.)
- [x] `File::set_current_directory` is called per connection (`launch_cmd`, `HandleConnection`) and races across threads. Pass the working directory to `posix_spawn` (`addchdir_np`) and `CreateProcess` instead. (OS/2's `DosStartSession` has no directory argument, so it changes directory under a lock.)
- [x] SIGHUP reloads only the callout thread's config copy. Either reload the connection config too or document the limitation. (Documented in `whatsnew.txt` and logged on HUP.)
- [x] Shutdown hangs up on callers who are online. Sockets wwivd still reads (matrix/mailer prompt, `@telnet:` proxy, pipe mode) are shut down. Each child gets SIGHUP to its process group, and the BBS (`bbs/hangup_signal.h`) shuts its socket down and logs the caller off as usual. Any child still running after 10 seconds gets SIGKILL. Warning callers first is #1790.
  - [ ] Windows, socket mode: there's no signal to ask the BBS to hang up, so the node is ended with `TerminateProcess` after the grace period, without a clean logoff. Pipe mode is clean, since wwivd owns the socket there.

## Phase 3: data races and process handling

- [x] `BadIp::Block` writes the `unordered_set` while other threads call `IsBlocked`, with no lock. Use a `std::shared_mutex`.
- [x] `BlockingHandler` iterates `AutoBlocker::auto_blocked()` without holding `mu_`. Return a snapshot copy under the lock. (`blocked()`, `escalate_block()` and `Save()` take the lock too.)
- [x] `Color()` keeps its state in a `static` that every connection thread shares. (It was also reset to 0 instead of the color just sent, so bold was never turned off.)
- [ ] Unix:
  - [x] `signal(SIGCHLD, SIG_IGN)` auto-reaps children, so `waitpid` returns `ECHILD` and the logged exit code is always 0.
  - [x] `bbs_pid` is a non-atomic global overwritten by every spawn, so SIGINT goes to the last process started. (Removed: SIGINT now shuts wwivd down like SIGTERM.)
  - [x] The signal handler uses `std::cerr`.
  - [x] `sigaction(SIGKILL)` always fails, and every failure message says "SIGINT".
- [ ] `SwitchToNonRootUser`: treat a failed `setuid`/`setgid` as fatal when running as root, and call `initgroups`.
- [ ] Beginday re-runs every 5 seconds forever if the command fails to update `status.dat`.
- [ ] Verify both `SetBlockingMode` helpers:
  - On Windows, `core/net.cpp` sets `FIONBIO=1`, which is *non-blocking*.
  - The anonymous-namespace copy in `socket_connection.cpp` always sets `O_NONBLOCK` on Unix.

## Phase 4: correctness bugs

- [ ] `AutoBlocker` keeps each address's recent connections in a `std::set<time_t>`, so any number of connections in the same second count as one, and a burst never triggers the auto-block. Keep a count (or a `std::multiset`).
- [ ] `nets.cpp`: `network_number` only increments for wwivnet and ftn networks, so `@T` is wrong when an internet or news network comes earlier in the list.
- [ ] `DoMatrixLogon` hides BBSes that need ANSI from non-ANSI callers but still accepts their key.
- [ ] `use_dns_rbl` / `dns_rbl_server` default to on but are never checked. Implement the check or remove the setting.
- [ ] `SysopHandler`'s "last user" is really the highest user number among instances.
- [ ] `consoleHandlerNames[dwCtrlType]` reads out of bounds for unexpected values (Windows).

## Phase 5: issues and features

- [ ] **#1640: node liveness.**
  - `DeleteAllSemaphores` removes `nodeinuse.N` at startup without checking. Check the pid recorded in the semaphore file instead: `kill(pid,0)` plus `/proc/<pid>/comm` on Unix, `OpenProcess` on Windows.
  - Add a watchdog that kills a child that is still running a grace period after its socket closed.
  - Keep the Windows process handle rather than the pid.
- [ ] **#1656: ANSI before the mailer prompt.** Run `check_ansi()` once before `DoMailerMode`. Show `gfiles/wwivd_mailer.{ans,msg}` if present, with an optional countdown at a sysop-chosen X/Y.
- [ ] **#1577: country blocking.**
  - Remove the hard-coded nerd.dk hack.
  - Add a `CountryLookup` interface with DNS and CSV implementations (ip2location, ipapi.is, MaxMind lite).
  - The CSV version uses a sorted range vector with `std::ranges::upper_bound` and is reloaded on HUP.
- [ ] **#1594 / #1421: proxy targets.**
  - Generalise `@telnet:` and `telnet_to` into proxy targets (`@telnet:`, `@rlogin:`).
  - Use `poll()` instead of `select()`, since fds ≥ `FD_SETSIZE` are undefined behaviour with `select()`.
- [ ] **IPv6:** the listener and `GetRemotePeerAddress` use `sockaddr_in` only.

## Phase 6: modern C++ cleanups

- [ ] `std::optional<int> AcquireNode(peer)` instead of an `int&` out-parameter. Add `[[nodiscard]]` and fix the `aquire` spelling.
- [ ] Use `std::expected` (C++23) for the block-check and launch results.
- [ ] Move `ExecCommandAndWait`, `signal_handler`, `BeforeStartServer`, and `SwitchToNonRootUser` into `wwiv::wwivd`.
- [ ] Initialise `NodeStatus::pid` and `connection_time`, and `status_line_t`'s fields.
- [ ] Use `system_clock::time_point` instead of `time_t` in `NodeStatus` and `auto_blocked_entry_t`.
- [ ] Wrap `std::map<const std::string, std::shared_ptr<NodeManager>>*` in a `NodeManagers` class.
- [ ] `std::erase_if` in the AutoBlocker cleanup loops, `std::ranges::count_if` in `nodes_used`, lambdas instead of `std::bind`, and `fmt::format` instead of `ostringstream`.
- [ ] `char cmdstr[4000]` plus `to_char_array` silently truncates. Use `std::string::data()`.
- [ ] `ParseIpFile`:
  - It builds a `std::regex` for every line.
  - `sscanf` and `mktime` sit inside a `catch (...)` that can never fire.
  - It duplicates `LoadLinesIntoSet`, which doesn't handle `#` comments or `1.2.3.4#note`.
- [ ] Move the `WWIV_USE_PIPES` and `_WIN32` blocks in `launch_node` behind a per-platform session bridge.

## Phase 7: tests and the HTTP endpoints

- [ ] Rename `wwivd_non_http_test.cpp` to `ips_test.cpp`. Add tests for `NodeManager`, `ConcurrentConnections`, `CreateCommandLine`, `ParseIpFile`, `ParseLastOnFile`, and the JSON handlers.
- [ ] Pull the block-check policy into a pure function, and drive `DoMatrixLogon` and `DoMailerMode` through the `Connection` interface plus a `Clock` so tests can fake both.
- [ ] HTTP endpoints:
  - They have no authentication and return caller IPs, blocklists, and handles.
  - Default `http_address` to `127.0.0.1`.
  - Log when `listen()` fails.
  - Stop logging every response body at VLOG(1).
