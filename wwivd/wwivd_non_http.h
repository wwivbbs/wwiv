/**************************************************************************/
/*                                                                        */
/*                          WWIV BBS Software                             */
/*             Copyright (C)2017-2022, WWIV Software Services             */
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
/**************************************************************************/
#ifndef INCLUDED_WWIVD_WWIVD_NON_HTTP_H
#define INCLUDED_WWIVD_WWIVD_NON_HTTP_H

#include "core/socket_connection.h"
#include "sdk/wwivd_config.h"
#include "wwivd/connection_data.h"
#include "wwivd/node_manager.h"
#include "wwivd/sessions.h"
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace wwiv::wwivd {

std::string to_string(const wwiv::sdk::wwivd_matrix_entry_t& e);
std::string to_string(const std::vector<wwiv::sdk::wwivd_matrix_entry_t>& elements);
std::filesystem::path node_file(const wwiv::sdk::Config& config, ConnectionType ct,
                                int node_number);
std::string CreateCommandLine(const std::string& tmpl, std::map<char, std::string> params);

class ConnectionHandler {
public:
  enum class BlockedConnectionAction { ALLOW, DENY };
  enum class MailerModeResult { ALLOW, DENY };

  struct BlockedConnectionResult {
    BlockedConnectionResult(const BlockedConnectionAction& a, std::string r)
        : action(a), remote_peer(std::move(r)) {}
    BlockedConnectionAction action{BlockedConnectionAction::ALLOW};
    std::string remote_peer;
  };

  ConnectionHandler() = delete;
  /** Takes ownership of the accepted client socket. */
  ConnectionHandler(ConnectionData d, wwiv::core::accepted_socket_t a);

  void HandleConnection();
  void HandleBinkPConnection();

private:
  MailerModeResult DoMailerMode();
  BlockedConnectionResult CheckForBlockedConnection();
  /** Returns the BBS the caller picked, or nullopt if they quit or timed out. */
  std::optional<wwiv::sdk::wwivd_matrix_entry_t> DoMatrixLogon(const wwiv::sdk::wwivd_config_t& c);
  ConnectionData data;
  /**
   * The caller's socket. This is the only owner: it is either moved into
   * the child launch (which closes it once the child has its own copy), or
   * closed when the handler is destroyed. While wwivd reads and writes it,
   * shutdown can hang it up.
   */
  WatchedSocket sock_;
  int port_{0};
};

} // namespace

#endif
