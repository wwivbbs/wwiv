/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5                            */
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
#include "wwivd/wwivd.h"

#include <atomic>
#include <iostream>
#include <map>
#include <string>

#include <pwd.h>
#include <signal.h>
#include <spawn.h>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
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

using std::cerr;
using std::clog;
using std::cout;
using std::endl;
using std::map;
using std::string;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;
using namespace wwiv::os;

// From wwivd.cpp

pid_t bbs_pid = 0;
extern char **environ;

namespace wwiv {
namespace wwivd {

extern std::atomic<bool> need_to_exit;
extern std::atomic<bool> need_to_reload_config;

} // namespace wwivd
} // namespace wwiv
using namespace wwiv::wwivd;

void signal_handler(int mysignal) {
  cerr << "Received signal: " << mysignal << endl;
  switch (mysignal) {
#ifdef __unix__
  case SIGHUP: {
    cerr << "Received SIGHUP" << endl;
    need_to_reload_config.store(true);
    break;
  }
  case SIGINT: {
    cerr << endl;
    if (bbs_pid != 0) {
      cerr << "Sending SIGINT to BBS after receiving " << mysignal << "..." << endl;
      kill(bbs_pid, mysignal); // send SIGHUP to process group
    }
  } break;
#endif
  // Graceful exit
  case SIGTERM: {
    cerr << "Received SIGTERM" << endl;
    need_to_exit.store(true);
  } break;
#ifdef __unix__
  // Not graceful exit
  case SIGKILL: {
    cerr << "Received SIGKILL" << endl;
    need_to_exit.store(true);
  } break;
#endif  // __unix__
  }

}

void BeforeStartServer() {
  struct sigaction sa {};
  sigfillset(&sa.sa_mask);
  sa.sa_handler = signal_handler;
  if (sigaction(SIGHUP, &sa, nullptr) == -1) {
    LOG(ERROR) << "Unable to install signal handler for SIGINT";
  }
  if (sigaction(SIGINT, &sa, nullptr) == -1) {
    LOG(ERROR) << "Unable to install signal handler for SIGINT";
  }
  if (sigaction(SIGTERM, &sa, nullptr) == -1) {
    LOG(ERROR) << "Unable to install signal handler for SIGINT";
  }
  if (sigaction(SIGKILL, &sa, nullptr) == -1) {
    LOG(ERROR) << "Unable to install signal handler for SIGINT";
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

void SwitchToNonRootUser(const std::string& wwiv_user) {
  uid_t current_uid = getuid();
  uid_t wwiv_uid = GetWWIVUserId(wwiv_user);
  if (wwiv_uid != current_uid) {
    if (setuid(wwiv_uid) != 0) {
      LOG(ERROR) << "Unable to call setuid(" << wwiv_uid << "); errno: " << errno;
      // TODO(rushfan): Should we exit or continue here?
    }
  }
}

bool ExecCommandAndWait(const std::string& cmd, const std::string& pid, int node_number, SOCKET sock) {
  char sh[21];
  char dc[21];
  char cmdstr[4000];
  to_char_array(sh, "sh");
  to_char_array(dc, "-c");
  to_char_array(cmdstr, cmd);
  char* argv[] = { sh, dc, cmdstr, NULL };

  LOG(INFO) << pid << "Invoking Command Line (posix_spawn):" << cmd;
  pid_t child_pid = 0;
  int ret = posix_spawn(&child_pid, "/bin/sh", NULL, NULL, argv, environ);
  if (sock != SOCKET_ERROR) {
    // close the socket since we've forked.
    closesocket(sock);
  }
  VLOG(2) << "after posix_spawn; ret: " << ret;
  if (ret != 0) {
    // fork failed.
    LOG(ERROR) << pid << "Error forking WWIV.";
    return false;
  }
  bbs_pid = child_pid;
  int status = 0;
  VLOG(2) << pid << "before waitpid";
  while (waitpid(child_pid, &status, 0) == -1) {
    if (errno != EINTR) {
      break;
    }
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

