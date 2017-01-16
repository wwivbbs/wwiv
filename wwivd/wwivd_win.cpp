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

#include <iostream>
#include <map>
#include <string>

#include <signal.h>
#include <string>
#include <sys/types.h>

#include "core/file.h"
#include "core/inifile.h"
#include "core/log.h"
#include "core/os.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/wwivport.h"
#include "sdk/config.h"

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;
using namespace wwiv::os;

bool InitializeSockets() {
  WSADATA wsadata;
  int result = WSAStartup(MAKEWORD(2, 2), &wsadata);
  if (result != 0) {
    LOG(ERROR) << "WSAStartup failed with error: " << result;
    return false;
  }
  return true;
}

void BeforeStartServer() {
  // Not the best place, but this works.
  static bool initialized = InitializeSockets();
}

void SwitchToNonRootUser(const std::string& wwiv_user) {
}

bool ExecCommandAndWait(const std::string& cmd, const std::string& pid, int node_number) {

  LOG(INFO) << pid << "Invoking Command Line (Win32):" << cmd;

  STARTUPINFO si = { sizeof(STARTUPINFO) };
  PROCESS_INFORMATION pi = {};
  char cmdstr[4000];
  to_char_array(cmdstr, cmd);

  DWORD creation_flags = CREATE_NEW_CONSOLE;

  BOOL ok = CreateProcess(
    NULL, 
    cmdstr,
    NULL,
    NULL,
    TRUE,
    creation_flags,
    NULL,
    NULL,
    &si,
    &pi);

  if (!ok) {
    // CreateProcess failed.
    LOG(ERROR) << "Error invoking CreateProcess.";
    return false;
  }

  // Wait until child process exits.
  DWORD dwExitCode = WaitForSingleObject(pi.hProcess, INFINITE);
  GetExitCodeProcess(pi.hProcess, &dwExitCode);
  LOG(INFO) << "Node #" << node_number << " exited with error code: " << dwExitCode;

  // Close process and thread handles. 
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  return true;
}

