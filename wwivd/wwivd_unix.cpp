/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5                            */
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
#include "wwivd/wwivd.h"

#include <atomic>
#include <cstring>
#include <iostream>
#include <map>
#include <string>

#include <pwd.h>
#include <signal.h>
#include <spawn.h>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>

#include "core/file.h"
#include "core/inifile.h"
#include "core/log.h"
#include "core/net.h"
#include "core/os.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/wwivport.h"
#include "sdk/config.h"
#include "sdk/vardec.h"
#include "sdk/filenames.h"
#include "wwivd/node_manager.h"
#include "wwivd/sessions.h"
#include "wwivd/signals.h"

using std::string;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;
using namespace wwiv::os;

extern char **environ;

using namespace wwiv::wwivd;

void signal_handler(int mysignal) {
  // Only async-signal-safe calls in here: the main loop logs the request.
  switch (mysignal) {
  case SIGHUP:
    request_reload();
    break;
  case SIGINT:
  case SIGTERM:
    if (request_exit()) {
      // Asked twice: stop waiting for the callers to be hung up.
      signal(mysignal, SIG_DFL);
      raise(mysignal);
    }
    break;
  default:
    break;
  }
}

void BeforeStartServer() {
  struct sigaction sa {};
  sigfillset(&sa.sa_mask);
  sa.sa_handler = signal_handler;
  for (const auto sig : {SIGHUP, SIGINT, SIGTERM}) {
    if (sigaction(sig, &sa, nullptr) == -1) {
      LOG(ERROR) << "Unable to install signal handler for " << strsignal(sig);
    }
  }
  // SIGCHLD keeps its default, so an exited child stays a zombie until
  // ExecCommandAndWait reaps it: its pid (and process group) can't be
  // reused while shutdown might still signal it, and its exit code is
  // logged.
  // Let the socket library handle EPIPE
  signal(SIGPIPE, SIG_IGN);
}

void wwiv::wwivd::HangupChild(child_process_t child) {
  // ESRCH means the whole group is gone; don't fall back to the pid, which
  // could be reused by then.
  if (kill(-child, SIGHUP) == -1 && errno != ESRCH) {
    kill(child, SIGHUP);
  }
}

void wwiv::wwivd::KillChild(child_process_t child) {
  if (kill(-child, SIGKILL) == -1 && errno != ESRCH) {
    kill(child, SIGKILL);
  }
}

static uid_t GetWWIVUserId(const string& username) {
  passwd* pw = getpwnam(username.c_str());
  if (pw == nullptr) {
    // Unable to find user, let's return our current uid.
    LOG(ERROR) << "Unable to find uid for username: " << username;
    return getuid();
  }
  return pw->pw_uid;
}

static gid_t GetWWIVUserGroupId(const string& username) {
  passwd* pw = getpwnam(username.c_str());
  if (pw == nullptr) {
    // Unable to find user, let's return our current uid.
    LOG(ERROR) << "Unable to find uid for username: " << username;
    return getgid();
  }
  return pw->pw_gid;
}

void SwitchToNonRootUser(const std::string& wwiv_user) {
  auto current_uid = getuid();
  auto wwiv_uid = GetWWIVUserId(wwiv_user);
  if (wwiv_uid != current_uid) {
    // Try to set the group ID before user ID, since otherwise we don't
    // have permissions to set the groupid after becoming non-root.
    auto wwiv_gid = GetWWIVUserGroupId(wwiv_user);
    if (setgid(wwiv_gid) != 0) {
      LOG(ERROR) << "Unable to call setgid(" << wwiv_gid << "); errno: " << errno;
    }
    if (setuid(wwiv_uid) != 0) {
      LOG(ERROR) << "Unable to call setuid(" << wwiv_uid << "); errno: " << errno;
    }
  }
}

#if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 29))
#define WWIVD_HAVE_SPAWN_ADDCHDIR
#else
// Quotes s for sh, as one word.
static std::string shell_quote(const std::string& s) {
  std::string out{"'"};
  for (const auto c : s) {
    if (c == '\'') {
      out.append("'\\''");
    } else {
      out.push_back(c);
    }
  }
  out.push_back('\'');
  return out;
}
#endif

bool ExecCommandAndWait(const wwivd_config_t& /*wc*/, wwiv::wwivd::NodeManager& node_manager,
                        const std::string& cmd, const std::string& pid, int node_number,
                        SocketHandle sock, const std::filesystem::path& working_dir,
                        Sessions* sessions) {
  // Its own process group, so a hangup reaches the BBS even when sh
  // doesn't exec it, and whatever it starts.
  posix_spawnattr_t attr;
  posix_spawnattr_init(&attr);
  auto destroy_attr = finally([&attr] { posix_spawnattr_destroy(&attr); });
  posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP);
  posix_spawnattr_setpgroup(&attr, 0);

  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  auto destroy_actions = finally([&actions] { posix_spawn_file_actions_destroy(&actions); });

  std::string sh{"sh"};
  std::string dc{"-c"};
  std::string cmdstr{cmd};
  if (!working_dir.empty()) {
    // Only the child changes directory: wwivd's own directory is shared by
    // every thread.
#ifdef WWIVD_HAVE_SPAWN_ADDCHDIR
    posix_spawn_file_actions_addchdir_np(&actions, working_dir.c_str());
#else
    cmdstr = StrCat("cd ", shell_quote(working_dir.string()), " && ", cmd);
#endif
  }
  char* argv[] = {sh.data(), dc.data(), cmdstr.data(), nullptr};
  if (sock) {
    // Client sockets are close-on-exec so that a child never inherits
    // another caller's connection. Pass just this one through.
#if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 29))
    // dup2 onto the same fd clears FD_CLOEXEC in the child only
    // (POSIX.1-2024; glibc >= 2.29).
    posix_spawn_file_actions_adddup2(&actions, sock.get(), sock.get());
#else
    // Older C libraries: clear it here. The socket is still exposed to a
    // child spawned by another thread in this short window.
    if (const auto flags = fcntl(sock.get(), F_GETFD); flags != -1) {
      fcntl(sock.get(), F_SETFD, flags & ~FD_CLOEXEC);
    }
#endif
  }

  VLOG(2) << pid << "Invoking Command Line (posix_spawn):" << cmd;
  pid_t child_pid = 0;
  const auto ret = posix_spawn(&child_pid, "/bin/sh", &actions, &attr, argv, environ);
  // The child has its own copy now (or failed to start); close ours so the
  // connection drops as soon as the child closes it.
  sock.reset();
  VLOG(2) << "after posix_spawn; ret: " << ret;
  if (ret != 0) {
    // fork failed, or the child couldn't change to working_dir.
    LOG(ERROR) << pid << "Error starting '" << cmd << "' in '" << working_dir.string()
               << "': " << strerror(ret);
    return false;
  }
  node_manager.set_pid(node_number, child_pid);
  if (sessions) {
    sessions->add_child(child_pid);
  }
  VLOG(2) << pid << "before waitpid";
  // Wait without reaping, so the pid can't be reused until the child is no
  // longer registered for shutdown to signal.
  siginfo_t info{};
  while (waitid(P_PID, static_cast<id_t>(child_pid), &info, WEXITED | WNOWAIT) == -1 &&
         errno == EINTR) {
  }
  int status = 0;
  const auto reap = [&] {
    while (waitpid(child_pid, &status, 0) == -1) {
      if (errno != EINTR) {
        break;
      }
    }
  };
  if (sessions && sessions->stopping()) {
    // The hangup reaches the shell too, which can exit at once while the
    // BBS it started is still logging the caller off. Wait for the rest of
    // the group; shutdown kills it after the grace period. Once the shell
    // is reaped, the group id still can't be reused while the group has
    // members.
    reap();
    while (kill(-child_pid, 0) == 0 || errno == EPERM) {
      sleep_for(std::chrono::milliseconds(50));
    }
    sessions->remove_child(child_pid);
  } else {
    if (sessions) {
      sessions->remove_child(child_pid);
    }
    reap();
  }
  VLOG(2) << pid << "after waitpid";

  std::ostringstream errs;
  errs << pid;
  if (node_number > 0) {
    errs << "Node #" << node_number;
  } else {
    errs << "cmd: " << cmd;
  }
  const auto err = errs.str();
  if (WIFEXITED(status)) {
    // Process exited.
    LOG(INFO) << err << " exited with error code: " << WEXITSTATUS(status);
  }
  else if (WIFSIGNALED(status)) {
    LOG(INFO) << err << " killed by signal: " << WTERMSIG(status);
  }
  else if (WIFSTOPPED(status)) {
    LOG(INFO) << err << " stopped by signal: " << WSTOPSIG(status);
  }

  return true;
}

