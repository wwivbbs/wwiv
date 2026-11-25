/**************************************************************************/
/*                                                                        */
/*                                WWIV Version 5                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
/*                                                                        */
/*    Licensed  under the  Apache License, Version  2.0 (the "License");  */
/*    you may not use this  file  except in compliance with the License.  */
/*    You may obtain a copy of the License at                             */
/*                                                                        */
/*                http://www.apache.org/licenses/LICENSE-2.0              */
/*                                                                        */
/*    Unless  required  by  applicable  law  or agreed to  in  writing,   */
/*    software  distributed  under  the  License  is  distributed on an   */
/*    "AS IS"  BASIS, WITHOUT  WARRANTIES  OR  CONDITIONS OF ANY  KIND,   */
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/*                                                                        */
/**************************************************************************/
#include "bbs/exec.h"
#include <sys/wait.h>

#define TTYDEFCHARS
#ifndef _POSIX_VDISABLE
#  define _POSIX_VDISABLE 0
#endif
#include <termios.h>
#include <sys/ttydefaults.h>
#undef TTYDEFCHARS

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <util.h>
#elif defined(__GLIBC__)
#include <pty.h>
#else /* bsd without glibc */
#include <libutil.h>
#endif

#include "bbs/application.h"
#include "bbs/bbs.h"
#include "bbs/exec_socket.h"
#include "bbs/stuffin.h"
#include "common/context.h"
#include "common/output.h"
#include "common/remote_io.h"
#include "core/log.h"
#include "core/os.h"
#include "sdk/vardec.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <optional>
#include <string_view>
#include <tuple>

static const char SHELL[] = "/bin/bash";

using namespace wwiv::bbs;


static int ReadWriteNonBinary(int sock, int pty_fd, fd_set& rfd) {
  if (FD_ISSET(sock, &rfd)) {
    char input{};
    if (read(sock, &input, 1) == 0) { 
      return -1;
    }
    if (static_cast<uint8_t>(input) == 0xff) {
      // IAC, skip over them so we ignore them for now
      // This was causing the do suppress GA (255, 253, 3)
      // to get interpreted as a SIGINT by dosemu on startup.
      LOG(INFO) << "IAC";
      if (read(sock, &input, 1) == 0) { 
        return -1;
      }
      if (read(sock, &input, 1) == 0) { 
        return -1;
      }
      return 0;
    }
    if (input == 3) {
      VLOG(1) << "control-c from user, skipping.";
      return 0;
    }
    VLOG(4) << "Read from Socket: input: " << input << " [" << static_cast<unsigned int>(input)
	    << "]";
    write(pty_fd, &input, 1);
    VLOG(4) << "read from socket, write to term: '" << input << "'";
  }
  if (FD_ISSET(pty_fd, &rfd)) {
    char input{};
    const auto n = read(pty_fd, &input, 1);
    if (n <= 0) {
      // EOF or error (including EAGAIN once the slave has closed). Do not
      // write a leftover NUL to the socket.
      return 0;
    }
    if (input == '\n') {
      VLOG(1) << "Performed LF -> CRLF translation.";
      write(sock, "\r\n", 2);
    } else {
      VLOG(4) << "Read From Terminal: '" << input << "'; [" << static_cast<unsigned>(input) << "]";
      write(sock, &input, 1);
    }
  }
  return 0;
}

static constexpr int READ_SIZE = 1024;
static int ReadWriteBinary(int sock, int pty_fd, fd_set& rfd) {
  VLOG(1) << "ReadWriteBinary Loop: << sock: " << sock
	    << "; pty_fd: " << pty_fd;
  if (FD_ISSET(sock, &rfd)) {
    char input[READ_SIZE + 10];
    const int num_read = read(sock, &input, READ_SIZE);
    if (num_read > 0) {
      const auto w = write(pty_fd, &input, num_read);
      VLOG(1) << "wrote[pty_fd]: " << num_read << ";w:" << w;
    } else {
      VLOG(1) << "num_read[sock] <= 0; " << num_read;
    }
  }
  if (FD_ISSET(pty_fd, &rfd)) {
    char input[READ_SIZE + 10];
    const int num_read = read(pty_fd, &input, READ_SIZE);
    if (num_read > 0) {
      const auto w = write(sock, &input, num_read);
      VLOG(1) << "wrote[sock]: " << num_read << ";w:" << w;
    } else {
      VLOG(1) << "num_read[pty_fd] <= 0; " << num_read;
    }
  }
  return 0;
}

// Copy any remaining child output to the socket after the process exits.
// unzip -l and other short-lived programs often exit while most of their
// listing is still sitting in the pty buffer.
static void drain_pty_to_sock(int sock, int pty_fd, bool binary) {
  if (pty_fd < 0) {
    return;
  }
  const auto fl = fcntl(pty_fd, F_GETFL, 0);
  if (fl >= 0) {
    fcntl(pty_fd, F_SETFL, fl | O_NONBLOCK);
  }
  char buf[READ_SIZE];
  for (;;) {
    const auto n = read(pty_fd, buf, sizeof(buf));
    if (n <= 0) {
      break;
    }
    if (binary) {
      write(sock, buf, n);
      continue;
    }
    for (int i = 0; i < n; i++) {
      if (buf[i] == '\n') {
        write(sock, "\r\n", 2);
      } else {
        write(sock, &buf[i], 1);
      }
    }
  }
}

// Pumps stdio until the child exits or the socket closes.
// Returns the child's wait status if this function reaped it, otherwise
// nullopt so the caller can still waitpid.
static std::optional<int> pump_stdio(int sock, int pty_fd, pid_t pid, bool binary) {
  const auto max_fd = std::max<int>(sock, pty_fd) + 1;
  for (;;) {
    fd_set rfd;
    FD_ZERO(&rfd);
    if (pty_fd > -1) {
      FD_SET(pty_fd, &rfd);
    }
    FD_SET(sock, &rfd);

    struct timeval tv = {1, 0};
    auto ret = select(max_fd, &rfd, nullptr, nullptr, &tv);
    if (ret < 0) {
      LOG(INFO) << "select returned <0";
      break;
    }

    // Pump any available data before checking whether the child has
    // exited. The previous order reaped unzip (and similar) first and
    // returned with the listing still in the pty.
    if (pty_fd != -1) {
      // Only do this in STDIO mode.
      if (binary) {
        ReadWriteBinary(sock, pty_fd, rfd);
      } else {
        if (ReadWriteNonBinary(sock, pty_fd, rfd) == -1) {
          return std::nullopt;
        }
      }
    }

    // N.B. This has been copied to ExecSocket::process_active
    int status_code = 0;
    pid_t wp = waitpid(pid, &status_code, WNOHANG);
    if (wp == -1 || wp > 0) {
      // -1 means error and >0 is the pid
      VLOG(2) << "waitpid returned: " << wp << "; errno: " << errno;
      if (WIFEXITED(status_code)) {
        VLOG(1) << "child exited with code: " << WEXITSTATUS(status_code);
      } else if (WIFSIGNALED(status_code)) {
        VLOG(1) << "child caught signal: " << WTERMSIG(status_code);
        LOG(INFO) << "core dump? : " << WCOREDUMP(status_code);
      } else {
        LOG(INFO) << "Raw status_code: " << status_code;
        LOG(INFO) << "core dump? : " << WCOREDUMP(status_code);
      }
      drain_pty_to_sock(sock, pty_fd, binary);
      return status_code;
    }
  }
  return std::nullopt;
}

static int wait_for_exit(pid_t pid) {
  // Wait for child to exit.
  for (;;) {
    VLOG(1) << "about to call waitpid at the end";
    // In the parent, wait for the child to terminate.
    int status_code = 1;
    if (waitpid(pid, &status_code, 0) == -1) {
      if (errno != EINTR) {
        return -1;
      }
    } else {
      return status_code;
    }
  }
  // Should never happen.
  return -1;
}

// Reads and drops input waiting on fd without blocking. With the comm
// paused, the door (or pump_stdio) reads the caller's connection directly,
// so input the purge in ExecuteExternalProgram couldn't see (still in the
// kernel buffer, or read in as the comm thread stopped) would reach it.
static void drain_pending_input(int fd) {
  char buf[512];
  // Bounded, so a caller who never stops sending can't hold up the door.
  for (int i = 0; i < 64; i++) {
    pollfd p{fd, POLLIN, 0};
    if (poll(&p, 1, 0) <= 0 || !(p.revents & POLLIN)) {
      return;
    }
    if (read(fd, buf, sizeof(buf)) <= 0) {
      return;
    }
  }
}

// Signals a socket door's process group, or just the door if the group
// couldn't be made. ESRCH means the whole group is gone, so there's nothing
// left to signal; falling back to the pid then could hit a reused one.
static void signal_door(pid_t pid, int sig) {
  if (kill(-pid, sig) == -1 && errno != ESRCH) {
    kill(pid, sig);
  }
}

// Stops a socket door that didn't exit within its grace period: SIGTERM to
// its process group (a launcher script and the emulator it started), then
// SIGKILL to the group if the door itself still hasn't exited. Once the
// door's own process is gone the rest of the group is left alone, since a
// launcher may leave a process behind on purpose to finish up (qemudoor.sh
// copies the door's files back).
static void terminate_door(pid_t pid, std::string_view reason) {
  static constexpr auto term_grace = std::chrono::seconds(3);
  LOG(INFO) << reason << "; sending SIGTERM to pid " << pid << " and its process group";
  signal_door(pid, SIGTERM);
  const auto deadline = std::chrono::steady_clock::now() + term_grace;
  for (;;) {
    // Not reaped yet (bbs resets SIGCHLD to SIG_DFL), so the group id can't
    // be reused before the SIGKILL. Checked once more after the last sleep,
    // so a door that exits right at the deadline doesn't get its leftovers
    // killed.
    if (!wwiv::bbs::ExecSocket::process_still_active(pid)) {
      return;
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      break;
    }
    wwiv::os::sleep_for(std::chrono::milliseconds(50));
  }
  LOG(INFO) << "Door ignored SIGTERM; sending SIGKILL to pid " << pid << " and its process group";
  signal_door(pid, SIGKILL);
}

/**
 * returns a tuple of (pid, pty).
 * if pid == -1, fork failed.
 * if pty == -1, there is no pty
 */
// own_group puts the child in its own process group. Only for socket doors
// that bbs pumps and may have to stop: anything else stays in bbs's group,
// so in a local session it keeps the terminal (a background group would be
// stopped by SIGTTIN/SIGTTOU and never return).
static std::tuple<pid_t, int> UnixSpawn(const wwiv::bbs::CommandLine& cmdline, int flags, int sock,
                                        bool own_group) {
  if (cmdline.empty()) {
    std::make_tuple(-1, -1);
  }
  auto cmd = cmdline.cmdline();
  LOG(INFO) << "Exec: '" << cmd << "'";

  int pid = -1;
  int pty_fd = -1;
  if (flags & EFLAG_STDIO) {
    LOG(INFO) << "Exec using STDIO: '" << cmd << "' errno: " << errno;
    struct winsize ws {};
    ws.ws_col = 80;
    ws.ws_row = 25;
    struct termios tio {};
    tio.c_iflag = TTYDEF_IFLAG;
    tio.c_oflag = TTYDEF_OFLAG;
    tio.c_lflag = TTYDEF_LFLAG;
    tio.c_cflag = TTYDEF_CFLAG;
    memcpy(&tio.c_cc, ttydefchars, sizeof(tio.c_cc));
    cfsetspeed(&tio, TTYDEF_SPEED);

    pid = forkpty(&pty_fd, nullptr, &tio, &ws);
  } else {
    pid = fork();
  }
  if (pid == -1) {
    auto e = errno;
    LOG(ERROR) << "Fork Failed: errno: '" << e << "'";
    // Fork failed.
    return std::make_tuple(-1, -1);
  }
  if (pid == 0) {
    // In the child
    if (own_group) {
      setpgid(0, 0);
    }
    const char* argv[4] = {SHELL, "-c", cmd.c_str(), 0};
    execv(SHELL, const_cast<char** const>(argv));
    exit(127);
  }

  // In the parent now. Also set the group here, so it's in place before we
  // could signal it, whichever runs first.
  if (own_group) {
    setpgid(pid, pid);
  }
  VLOG(1) << "In parent, pid " << pid << "; errno: " << errno;
  return std::make_tuple(pid, pty_fd);
}

int exec_cmdline(wwiv::bbs::CommandLine& cmdline, int flags) {
  bool need_reopen_io = false;
  std::unique_ptr<wwiv::bbs::ExecSocket> exec_socket;
  if (a()->sess().ok_modem_stuff()) {
    if (flags & EFLAG_SYNC_FOSSIL) {
      LOG(ERROR) << "EFLAG_SYNC_FOSSIL is not supported on UNIX";
    }
    if (flags & EFLAG_NETFOSS) {
      LOG(ERROR) << "EFLAG_SYNC_FOSSIL is not supported on UNIX";
    }
    if (flags & EFLAG_COMIO) {
      LOG(ERROR) << "EFLAG_COMIO is not supported on UNIX";
    }
    if (flags & EFLAG_STDIO) {
      VLOG(2) << "Temporarily pausing comm for spawn";
      need_reopen_io = true;
      bout.remoteIO()->close(true);
    } else if (flags & EFLAG_LISTEN_SOCK) {
      exec_socket = std::make_unique<wwiv::bbs::ExecSocket>(0);
      // Add %Z into the commandline.
      if (exec_socket->listening()) {
        cmdline.add('Z', exec_socket->z());
        VLOG(1) << "listening to socket: " << exec_socket->z();
      }

    } else if (flags & EFLAG_UNIX_SOCK) {
      exec_socket = std::make_unique<wwiv::bbs::ExecSocket>(a()->sess().dirs().scratch_directory());
      // Add %Z into the commandline.
      if (exec_socket->listening()) {
        cmdline.add('Z', exec_socket->z());
        VLOG(1) << "listening to socket: " << exec_socket->z();
      }

    } else {
      VLOG(2) << "Temporarily pausing comm for spawn";
      need_reopen_io = true;
      bout.remoteIO()->close(true);
    }
  }
  if (exec_socket) {
    const auto exit_grace = std::chrono::seconds(std::max(0, a()->door_exit_grace_seconds()));
    exec_socket->set_exit_grace(exit_grace);
    if (flags & EFLAG_WWIVFOSS) {
      // Works with either a TCP (LISTEN_SOCK) or UNIX socket.
      wwiv::bbs::wwivfoss_options_t opts{};
      opts.heartbeat = std::chrono::seconds(std::max(0, a()->wwivfoss_heartbeat_seconds()));
      opts.caller_hangup_grace = exit_grace;
      opts.socket_closed_grace = exit_grace;
      exec_socket->enable_wwivfoss(opts);
    }
  }

  const auto sock = bout.remoteIO()->GetDoorHandle();
  if (need_reopen_io) {
    // The comm thread is stopped; drop anything it left behind.
    bout.remoteIO()->purgeIn();
    drain_pending_input(static_cast<int>(sock));
  }
  // Socket doors get their own process group, so a door that won't exit can
  // be stopped with whatever it started (a launcher script and its
  // emulator). forkpty (STDIO) already makes a new session.
  const bool own_group = exec_socket != nullptr && !(flags & EFLAG_STDIO);
  auto [pid, pty] = UnixSpawn(cmdline, flags, sock, own_group);
  if (pid == -1) {
    return 1;
  }

  int result = 0;
  bool reaped = false;
  if (flags & EFLAG_STDIO) {
    if (const auto status = pump_stdio(sock, pty, pid, (flags & EFLAG_BINARY))) {
      result = status.value();
      reaped = true;
    }
  } else if (exec_socket) {
    if (const auto sock = exec_socket->accept()) {
      DCHECK(bout.remoteIO());
      if (exec_socket->pump_socket(pid, sock.value(), *bout.remoteIO()) !=
          pump_socket_result_t::process_exit) {
        terminate_door(pid, "Door did not exit within its grace period");
      }
    } else {
      terminate_door(pid, "Door never connected to its socket");
    }
  }

  if (pty >= 0) {
    close(pty);
  }

  // Wait for child to exit if the stdio pump did not already reap it.
  // Reaping twice returns ECHILD and -1, which list_arc_out treats as
  // failure and aborts the View after the first More? prompt.
  if (!reaped) {
    result = wait_for_exit(pid);
  }

  // reengage comm stuff
  if (need_reopen_io) {
    bout.remoteIO()->open();
  }
  return result;
}
