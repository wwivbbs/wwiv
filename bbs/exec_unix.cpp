/**************************************************************************/
/*                                                                        */
/*                                WWIV Version 5                          */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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
#define _POSIX_VDISABLE 0
#endif
#include <sys/ttydefaults.h>
#include <termios.h>
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
#include "bbs/remote_io.h"

#include "core/log.h"
#include "core/os.h"

static const char SHELL[] = "/bin/bash";

static int UnixSpawn(const std::string& cmd, int flags) {
  if (cmd.empty()) {
    return 1;
  }
  LOG(INFO) << "Exec: '" << cmd << "' errno: " << errno;
  const int sock = a()->remoteIO()->GetDoorHandle();
  int pid = -1;
  int master_fd = -1;
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

    pid = forkpty(&master_fd, nullptr, &tio, &ws);
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
  LOG(INFO) << "In parent, pid " << pid << "; errno: " << errno;
  for (;;) {
    fd_set rfd;
    FD_ZERO(&rfd);

    FD_SET(master_fd, &rfd);
    FD_SET(sock, &rfd);

    struct timeval tv = {1, 0};
    auto ret = select(std::max<int>(sock, master_fd) + 1, &rfd, nullptr, nullptr, &tv);
    if (ret < 0) {
      LOG(INFO) << "select returned <0";
      break;
    }
    int status_code = 0;
    pid_t wp = waitpid(pid, &status_code, WNOHANG);
    if (wp == -1 || wp > 0) {
      // -1 means error and >0 is the pid
      LOG(INFO) << "waitpid returned: " << wp << "; errno: " << errno;
      if (WIFEXITED(status_code)) {
        LOG(INFO) << "child exited with code: " << WEXITSTATUS(status_code);
        break;
      } else if (WIFSIGNALED(status_code)) {
        LOG(INFO) << "child caught signal: " << WTERMSIG(status_code);
      } else {
        LOG(INFO) << "Raw status_code: " << status_code;
      }
      LOG(INFO) << "core dump? : " << WCOREDUMP(status_code);
      break;
    }
    bool dump = false;
    if (FD_ISSET(sock, &rfd)) {
      char input{};
      read(sock, &input, 1);
      if (static_cast<uint8_t>(input) == 0xff) {
        // IAC, skip over them so we ignore them for now
        // This was causing the do suppress GA (255, 253, 3)
        // to get interpreted as a SIGINT by dosemu on startup.
        VLOG(1) << "IAC";
        read(sock, &input, 1);
        read(sock, &input, 1);
        continue;
      }
      if (input == 3) {
        LOG(INFO) << "control-c from user, skipping.";
        dump = true;
        continue;
      }
      VLOG(4) << "Read from Socket: input: " << input << " [" << static_cast<unsigned int>(input)
              << "]";
      write(master_fd, &input, 1);
      VLOG(3) << "read from socket, write to term: '" << input << "'";
    }
    if (FD_ISSET(master_fd, &rfd)) {
      char input{};
      read(master_fd, &input, 1);
      if (input == '\n') {
        write(sock, "\r\n", 2);
      } else {
        VLOG(3) << "Read From Terminal: Char: '" << input << "'; [" << static_cast<unsigned>(input)
                << "]";
        write(sock, &input, 1);
      }
    }
  }
  // Wait for child to exit.
  for (;;) {
    LOG(INFO) << "about to call waitpid at the end";
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

int exec_cmdline(const std::string cmdline, int flags) {
  if (flags & EFLAG_FOSSIL) {
    LOG(ERROR) << "EFLAG_FOSSIL is not supported on UNIX";
  }
  if (flags & EFLAG_COMIO) {
    LOG(ERROR) << "EFLAG_COMIO is not supported on UNIX";
  }

  if (a()->context().ok_modem_stuff()) {
    LOG(INFO) << "Temporarily pausing comm for spawn";
    a()->remoteIO()->close(true);
  }

  auto i = UnixSpawn(cmdline, flags);

  // reengage comm stuff
  if (a()->context().ok_modem_stuff()) {
    a()->remoteIO()->open();
  }
  return i;
}
