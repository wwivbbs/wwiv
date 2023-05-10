/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5                            */
/*             Copyright (C)2021-2022, WWIV Software Services             */
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

#define INCL_DOSPROCESS
#define INCL_DOSSESMGR
#include <os2.h>
#include <emx/umalloc.h> // for _lmalloc()

#include "core/file.h"
#include "core/log.h"
#include "core/os.h"
#include "core/socket_connection.h"
#include "core/strings.h"
#include "sdk/wwivd_config.h"
#include "wwivd/node_manager.h"

#include <atomic>
#include <iostream>
#include <csignal>
#include <string>
#include <process.h>
#include <libcx/spawn2.h>

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;
using namespace wwiv::os;

namespace wwiv::wwivd {

extern std::atomic<bool> need_to_exit;
} // namespace wwiv

using namespace wwiv::wwivd;

void signal_handler(int mysignal) {
  switch (mysignal) {
  // Graceful exit
  case SIGTERM: {
    std::cerr << "SIGTERM: " << mysignal << std::endl;
    // On Windows since we do this, we re-raise it.
    signal(mysignal, SIG_DFL);
    raise(mysignal);
  } break;
  // Interrupts
  case SIGINT: {
    std::cerr << "SIGINT: " << mysignal << std::endl;
    need_to_exit.store(true);
    // call default handler
  } break;
  default: {
    std::cerr << "Unknown signal: " << mysignal << std::endl;
  }
  }
}

void BeforeStartServer() {
  // Not the best place, but this works.
  InitializeSockets();
  signal(SIGTERM, signal_handler);
  signal(SIGINT, signal_handler);
  std::cerr << "set signal handlers" << std::endl;
}

void SwitchToNonRootUser(const std::string&) {
}


bool ExecCommandAndWait(const wwivd_config_t& wc, wwiv::wwivd::NodeManager& node_manager,
                        const std::string& cmd, const std::string& pid, int node_number,
                        SOCKET sock) {

  LOG(INFO) << pid << "Invoking Command Line (Win32):" << cmd;

  std::string exe = cmd;
  auto ss = wwiv::strings::SplitString(cmd, " ");
  char* argv = (char*) _lmalloc(1024);
  memset(argv, 0, 1024);
  const auto len = cmd.size();
  strcpy(argv, cmd.c_str());
  char* args = strchr(argv, ' ');
  if (args) { ++args; }

  std::ostringstream sso;
  for (int i=0; i<=cmd.size() + 1; i++) {
    if (argv[i] < 32) {
      sso << "[\\" << (int)argv[i] << "]";
    } else {
      sso << argv[i];
    }
  }
  VLOG(3) << "OS2 style args: " << sso.str();


  STARTDATA data;
  memset(&data, sizeof(data), 0);
  data.Length = sizeof(data);
  data.Related = SSF_RELATED_INDEPENDENT;
  data.FgBg = SSF_FGBG_FORE;
  data.TraceOpt = SSF_TRACEOPT_NONE;
  data.PgmTitle = (PSZ) "WWIV BBS";
  data.PgmName = (PSZ) "C:\\WWIV\\BBS.EXE"; // &argv[0];
  data.PgmInputs = (PSZ) args;
  data.TermQ = NULL;
  data.Environment = NULL;
  data.InheritOpt = SSF_INHERTOPT_PARENT;
  data.IconFile = NULL;
  data.PgmHandle = NULLHANDLE;
  data.InitXPos = data.InitYPos = data.InitXSize = data.InitYSize = 0;
  data.Reserved = 0;
  data.ObjectBuffer = NULL;
  data.ObjectBuffLen = 0;
  data.SessionType = SSF_TYPE_WINDOWABLEVIO;

  ULONG ulSid = 0, ulPid = 0;
  APIRET arc = DosStartSession(&data, &ulSid, &ulPid);
  LOG(INFO) << "DosStartSession ret: " << arc << "; child pid: " << ulPid;
  int child_pid = ulPid;

  RESULTCODES code;  // Result code for the child process
  PID pidOut = child_pid;
  node_manager.set_pid(node_number, child_pid);


  if(APIRET rc = DosWaitChild(DCWA_PROCESS, DCWW_WAIT, &code, &pidOut, child_pid); rc != NO_ERROR) {
    LOG(ERROR) << "DosWaitChild failed: Error: " << rc;
  }

  if (sock != INVALID_SOCKET) {
    // We're done with this socket.
    closesocket(sock);
  }

  if (node_number > 0) {
    LOG(INFO) << "Node #" << node_number << " exited with error code: " << code.codeResult;
  } else {
    LOG(INFO) << "Command: '" << cmd << "' exited with error code: " << code.codeResult;
  }
  return true;
}

