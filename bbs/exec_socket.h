/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                   Copyright (C)2021, WWIV Software Services            */
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

#include <filesystem>
#include <optional>
#include <string>

namespace wwiv::bbs {

enum class exec_socket_type_t { port, unix };

class ExecSocket final {
public:
  ExecSocket(const std::filesystem::path& dir, exec_socket_type_t type);
  std::optional<int> accept();

  [[nodiscard]] std::filesystem::path path() const;
  [[nodiscard]] int port() const noexcept { return port_; }
  [[nodiscard]] bool listening() const noexcept { return server_socket_ != -1; }

  /** returns the value to use for %Z, which is either the UNIX path or the socket hostname:port */
  [[nodiscard]] std::string z() const;

private:
  const std::filesystem::path dir_;
  exec_socket_type_t type_;
  std::filesystem::path unix_path_;
  int server_socket_{-1};
  int client_socket_{-1};
  int port_{0};
};

}

#endif
