/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
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
#ifndef INCLUDED_BBS_EXEC_SOCKET_H
#define INCLUDED_BBS_EXEC_SOCKET_H

#include <atomic>
#include <filesystem>
#include <optional>
#include <string>
#include "common/remote_io.h"
#include "core/net.h"

#if defined(__unix__)
#include <sys/types.h>
#endif

namespace wwiv::bbs {

enum class exec_socket_type_t { port, unix_domain };

enum class pump_socket_result_t { process_exit, socket_error };

#ifdef _WIN32
typedef void* EXEC_SOCKET_HANDLE;
#else 
typedef pid_t EXEC_SOCKET_HANDLE;
#endif

class ExecSocket final {
public:
  ExecSocket(const std::filesystem::path& dir)
      : ExecSocket(dir, 0, exec_socket_type_t::unix_domain) {}
  ExecSocket(uint16_t port) : ExecSocket("", port, exec_socket_type_t::port) {}
  ~ExecSocket();
  std::optional<int> accept();

  [[nodiscard]] std::filesystem::path path() const;
  [[nodiscard]] int port() const noexcept { return port_; }
  [[nodiscard]] bool listening() const noexcept { return server_socket_ != -1; }

  /** returns the value to use for %Z, which is either the UNIX path or the socket hostname:port */
  [[nodiscard]] std::string z() const;

  /** Handles pumping data between socket and remote IO */
  pump_socket_result_t pump_socket(EXEC_SOCKET_HANDLE hProcess, SOCKET sock, wwiv::common::RemoteIO& io);

  bool stop_pump();
  static bool process_still_active(EXEC_SOCKET_HANDLE h);

private:
  ExecSocket(const std::filesystem::path& dir, uint16_t port, exec_socket_type_t type);
  const std::filesystem::path dir_;
  exec_socket_type_t type_;
  std::filesystem::path unix_path_;
  SOCKET server_socket_{INVALID_SOCKET};
  SOCKET client_socket_{INVALID_SOCKET};
  int port_{0};
  std::atomic<bool> stop_;
};

}

#endif
