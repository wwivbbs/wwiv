/**************************************************************************/
/*                                                                        */
/*                                WWIV Version 5                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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

#include <unistd.h>

#if defined(__APPLE__)
#include <util.h>
#elif defined(__GLIBC__)
#include <pty.h>
#else /* bsd without glibc */
#include <libutil.h>
#endif

#include "bbs/bbs.h"
#include "common/context.h"
#include "common/output.h"
#include "common/remote_io.h"
#include "core/log.h"
#include "core/os.h"
#include "sdk/vardec.h"

static const char SHELL[] = "/bin/bash";

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
    read(pty_fd, &input, 1);
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

static int UnixSpawn(const std::string& cmd, int flags, int sock) {
  if (cmd.empty()) {
    return 1;
  }
  VLOG(1) << "Exec: '" << cmd << "' errno: " << errno;

  int pid = -1;
  int pty_fd = -1;
  const bool binary = (flags & EFLAG_BINARY);
  if (binary) {
    LOG(INFO) << "Binary mode.";
  }
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
    return -1;
  }
  if (pid == 0) {
    // In the child
    const char* argv[4] = {SHELL, "-c", cmd.c_str(), 0};
    execv(SHELL, const_cast<char** const>(argv));
    exit(127);
  }

  // In the parent now.
  VLOG(1) << "In parent, pid " << pid << "; errno: " << errno;
  for (;;) {
    fd_set rfd;
    FD_ZERO(&rfd);

    FD_SET(pty_fd, &rfd);
    FD_SET(sock, &rfd);

    struct timeval tv = {1, 0};
    auto ret = select(std::max<int>(sock, pty_fd) + 1, &rfd, nullptr, nullptr, &tv);
    if (ret < 0) {
      LOG(INFO) << "select returned <0";
      break;
    }
    int status_code = 0;
    pid_t wp = waitpid(pid, &status_code, WNOHANG);
    if (wp == -1 || wp > 0) {
      // -1 means error and >0 is the pid
      VLOG(2) << "waitpid returned: " << wp << "; errno: " << errno;
      if (WIFEXITED(status_code)) {
        VLOG(1) << "child exited with code: " << WEXITSTATUS(status_code);
        break;
      }
      if (WIFSIGNALED(status_code)) {
        VLOG(1) << "child caught signal: " << WTERMSIG(status_code);
      } else {
        LOG(INFO) << "Raw status_code: " << status_code;
      }
      LOG(INFO) << "core dump? : " << WCOREDUMP(status_code);
      break;
    }

    if (pty_fd != -1) {
      // Only do this in STDIO mode.
      if (binary) {
        ReadWriteBinary(sock, pty_fd, rfd);
      } else {
        if (ReadWriteNonBinary(sock, pty_fd, rfd) == -1) {
          close(pty_fd);
          break;
        }
      }
    }
  }
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

int exec_cmdline(const std::string& cmdline, int flags) {
  if (flags & EFLAG_SYNC_FOSSIL) {
    LOG(ERROR) << "EFLAG_SYNC_FOSSIL is not supported on UNIX";
  }
  if (flags & EFLAG_NETFOSS) {
    LOG(ERROR) << "EFLAG_SYNC_FOSSIL is not supported on UNIX";
  }
  if (flags & EFLAG_COMIO) {
    LOG(ERROR) << "EFLAG_COMIO is not supported on UNIX";
  }

  if (a()->sess().ok_modem_stuff()) {
    VLOG(2) << "Temporarily pausing comm for spawn";
    bout.remoteIO()->close(true);
  }

  auto i = UnixSpawn(cmdline, flags, bout.remoteIO()->GetDoorHandle());

  // reengage comm stuff
  if (a()->sess().ok_modem_stuff()) {
    bout.remoteIO()->open();
  }
  return i;
}
