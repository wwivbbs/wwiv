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

#include "core/file.h"
#include "core/log.h"
#include "core/os.h"
#include "core/scope_exit.h"
#include "core/socket_connection.h"
#include "core/strings.h"
#include "sdk/wwivd_config.h"
#include "wwivd/node_manager.h"
#include <atomic>
#include <iostream>
#include <csignal>
#include <string>
#include <vector>

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

/*
  #define CTRL_C_EVENT        0
  #define CTRL_BREAK_EVENT    1
  #define CTRL_CLOSE_EVENT    2
  #define CTRL_LOGOFF_EVENT   5
  #define CTRL_SHUTDOWN_EVENT 6
 */
static BOOL WINAPI WWIVDConsoleHandlerRoutine(_In_ DWORD dwCtrlType) {
  static std::string consoleHandlerNames[]{
    "CTRL_C_EVENT", "CTRL_BREAK_EVENT", "CTRL_CLOSE_EVENT", "???", "???", "CTRL_LOGOFF_EVENT", "CTRL_SHUTDOWN_EVENT"
  };
  std::cerr << "WWIVDConsoleHandlerRoutine: " << consoleHandlerNames[dwCtrlType] << std::endl;
  if (need_to_exit.exchange(true)) {
    // Already asked to exit once and still running: let the default
    // handler end the process.
    return FALSE;
  }
  return TRUE;
}

void BeforeStartServer() {
  // Not the best place, but this works.
  InitializeSockets();
  signal(SIGTERM, signal_handler);
  signal(SIGINT, signal_handler);

  // Set the console handler.
  if (!SetConsoleCtrlHandler(WWIVDConsoleHandlerRoutine, TRUE)) {
    LOG(WARNING) << "SetConsoleCtrlHandler failed";
  }

  std::cerr << "set signal handlers" << std::endl;
}

void SwitchToNonRootUser(const std::string&) {
}

bool ExecCommandAndWait(const wwivd_config_t& wc, wwiv::wwivd::NodeManager& node_manager,
                        const std::string& cmd, const std::string& pid, int node_number,
                        SocketHandle sock) {

  LOG(INFO) << pid << "Invoking Command Line (Win32):" << cmd;

  STARTUPINFOEX si{};
  si.StartupInfo.cb = sizeof(STARTUPINFO);

  if (wc.launch_minimized) {
    // dwFlags says to look at wShowWindow, and wShowWindow
    // says to minimize
    si.StartupInfo.dwFlags = STARTF_USESHOWWINDOW;
    si.StartupInfo.wShowWindow = SW_MINIMIZE;
  }

  // With bInheritHandles=TRUE alone, the child would inherit every
  // inheritable handle in wwivd: every other caller's socket and the
  // listening sockets. Then a caller's connection can't close while any
  // other node is running. Restrict inheritance to this caller's socket.
  std::vector<char> attr_buf;
  LPPROC_THREAD_ATTRIBUTE_LIST attrs{nullptr};
  HANDLE inherit_list[1]{};
  auto delete_attrs = finally([&attrs] {
    if (attrs) {
      DeleteProcThreadAttributeList(attrs);
    }
  });
  if (sock) {
    SIZE_T size{0};
    InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
    attr_buf.resize(size);
    auto* list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attr_buf.data());
    if (InitializeProcThreadAttributeList(list, 1, 0, &size)) {
      attrs = list;
      inherit_list[0] = reinterpret_cast<HANDLE>(sock.get());
      if (UpdateProcThreadAttribute(attrs, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit_list,
                                    sizeof(inherit_list), nullptr, nullptr)) {
        si.lpAttributeList = attrs;
      } else {
        LOG(WARNING) << "UpdateProcThreadAttribute failed: " << GetLastError()
                     << "; child will inherit all inheritable handles.";
      }
    } else {
      LOG(WARNING) << "InitializeProcThreadAttributeList failed: " << GetLastError();
    }
  }

  PROCESS_INFORMATION pi{};
  std::string cmdstr{cmd};

  DWORD creation_flags = CREATE_NEW_CONSOLE;
  if (si.lpAttributeList) {
    si.StartupInfo.cb = sizeof(STARTUPINFOEX);
    creation_flags |= EXTENDED_STARTUPINFO_PRESENT;
  }
  // Only inherit handles when there is a socket to hand over.
  const BOOL inherit_handles = sock ? TRUE : FALSE;

  const auto current_dir = File::current_directory().string();
  const auto ok = CreateProcess(
    nullptr, 
    cmdstr.data(),
    nullptr,
    nullptr,
    inherit_handles,
    creation_flags,
    nullptr,
    current_dir.c_str(),
    &si.StartupInfo,
    &pi);
  // Read this before closesocket can overwrite it.
  const auto create_error = ok ? 0 : GetLastError();

  // We're done with this socket; the child has its own reference to it.
  sock.reset();

  if (!ok) {
    // CreateProcess failed.
    LOG(ERROR) << "Error invoking CreateProcess: " << create_error;
    return false;
  }

  node_manager.set_pid(node_number, pi.dwProcessId);

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

