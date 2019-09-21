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

#include "core/file.h"
#include "core/log.h"
#include "core/os.h"
#include "core/socket_connection.h"
#include "core/stl.h"
#include "core/strings.h"
#include "sdk/config.h"
#include <atomic>
#include <iostream>
#include <signal.h>
#include <string>

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
  }
}

void BeforeStartServer() {
  // Not the best place, but this works.
  wwiv::core::InitializeSockets();
  signal(SIGTERM, signal_handler);
  signal(SIGINT, signal_handler);
  std::cerr << "set signal handlers" << std::endl;
}

void SwitchToNonRootUser(const std::string& wwiv_user) {
}

bool ExecCommandAndWait(const std::string& cmd, const std::string& pid, int node_number, SOCKET sock) {

  LOG(INFO) << pid << "Invoking Command Line (Win32):" << cmd;

  STARTUPINFO si = { sizeof(STARTUPINFO) };
  PROCESS_INFORMATION pi = {};
  char cmdstr[4000];
  to_char_array(cmdstr, cmd);

  const DWORD creation_flags = CREATE_NEW_CONSOLE;

  auto ok = CreateProcess(
    nullptr, 
    cmdstr,
    nullptr,
    nullptr,
    TRUE,
    creation_flags,
    nullptr,
    nullptr,
    &si,
    &pi);

  if (!ok) {
    // CreateProcess failed.
    LOG(ERROR) << "Error invoking CreateProcess.";
    return false;
  }

  if (sock != SOCKET_ERROR) {
    // We're done with this socket and the child has another reference count
    // to it.
    closesocket(sock);
  }

  // Wait until child process exits.
  auto dwExitCode = WaitForSingleObject(pi.hProcess, INFINITE);
  GetExitCodeProcess(pi.hProcess, &dwExitCode);

  // Close process and thread handles. 
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  if (node_number > 0) {
    LOG(INFO) << "Node #" << node_number << " exited with error code: " << dwExitCode;
  } else {
    LOG(INFO) << "Command: '" << cmd << "' exited with error code: " << dwExitCode;
  }
  return true;
}

