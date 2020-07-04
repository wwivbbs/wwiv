/**************************************************************************/
/*                                                                        */
/*                          WWIV BBS Software                             */
/*             Copyright (C)2017-2020, WWIV Software Services             */
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
#include "wwivd/wwivd_non_http.h"

#include "core/file.h"
#include "core/log.h"
#include "core/net.h"
#include "core/os.h"
#include "core/scope_exit.h"
#include "core/semaphore_file.h"
#include "core/socket_connection.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "sdk/ansi/makeansi.h"
#include "sdk/config.h"
#include "wwivd/connection_data.h"
#include "wwivd/node_manager.h"
#include "wwivd/wwivd.h"
#include <cctype>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace wwiv::wwivd {

using std::map;
using std::string;
using namespace std::chrono;
using namespace std::chrono_literals;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;
using namespace wwiv::os;

string to_string(const wwivd_matrix_entry_t& e) {
  std::ostringstream ss;
  ss << "[" << e.key << "] " << e.name << " (" << e.description << ")";
  return ss.str();
}

string to_string(const std::vector<wwivd_matrix_entry_t>& elements) {
  std::ostringstream ss;
  for (const auto& e : elements) {
    ss << "{" << to_string(e) << "}" << std::endl;
  }
  return ss.str();
}

std::string CreateCommandLine(const std::string& tmpl, std::map<char, std::string> params) {
  string out;

  for (auto it = tmpl.begin(); it != tmpl.end(); it++) {
    if (*it == '@') {
      ++it;
      if (it == tmpl.end()) {
        out.push_back('@');
        break;
      }
      try {
        out.append(params.at(*it));
      } catch (const std::out_of_range&) {
        out.push_back(*it);
      }
    } else {
      out.push_back(*it);
    }
  }

  return out;
}

std::filesystem::path node_file(const Config& config, ConnectionType ct, int node_number) {
  if (ct == ConnectionType::BINKP) {
    return FilePath(config.datadir(), "binkpinuse");
  }
  return FilePath(config.datadir(), StrCat("nodeinuse.", node_number));
}

static bool launch_cmd(const std::string& raw_cmd, std::shared_ptr<NodeManager> nodes,
                       int node_number, int sock, ConnectionType connection_type,
                       const string& remote_peer) {
  const auto pid = fmt::format("[{}] ", get_pid());
  nodes->set_node(node_number, connection_type, StrCat("Connected: ", remote_peer));

  const map<char, string> params = {{'N', std::to_string(node_number)}, {'H', std::to_string(sock)}};

  // Reset the socket back to blocking mode
  VLOG(2) << "Setting blocking mode.";
  if (!SetBlockingMode(sock)) {
    LOG(ERROR) << "Failed to reset the socket to blocking mode.";
  }

  const auto cmd = CreateCommandLine(raw_cmd, params);
  const auto result = ExecCommandAndWait(cmd, pid, node_number, sock);

  nodes->ReleaseNode(node_number);

  return result;
}

static bool launch_node(const Config& config, const std::string& raw_cmd,
                        std::shared_ptr<NodeManager> nodes, int node_number, int sock,
                        ConnectionType connection_type, const string& remote_peer) {
  ScopeExit at_exit([=] {
    closesocket(sock);
    VLOG(2) << "closed socket: " << sock;
  });

  const auto pid = fmt::format("[{}] ", get_pid());
  VLOG(1) << pid << "launching node(" << node_number << ")";
  const auto sem_text = fmt::format("Created by pid: {}\nremote peer: {}", pid, remote_peer);
  const auto sem_path = node_file(config, connection_type, node_number);

  try {
    auto semaphore_file = SemaphoreFile::try_acquire(sem_path, sem_text, std::chrono::seconds(60));
    return launch_cmd(raw_cmd, nodes, node_number, sock, connection_type, remote_peer);
  } catch (const semaphore_not_acquired& e) {
    LOG(ERROR) << pid << "Unable to create semaphore file: " << sem_path << "; errno: " << errno
               << "; what: " << e.what();
    return false;
  }
}

static ConnectionType connection_type_for(const wwivd_config_t& c, int port) {
  if (port == c.telnet_port) {
    return ConnectionType::TELNET;
  }
  if (port == c.binkp_port) {
    return ConnectionType::BINKP;
  }
  if (port == c.ssh_port) {
    return ConnectionType::SSH;
  }
  if (port == c.http_port) {
    return ConnectionType::HTTP;
  }
  // TODO(rushfan) ???
  return ConnectionType::TELNET;
}

static bool check_ansi(SocketConnection& conn) {
  const auto d = 3s;
  conn.send("Checking for ANSI Graphics... ", d);
  conn.send("\x1b[6n", d);
  auto res = conn.receive_upto(10, d);
  if (!res.empty() && res.front() == 27) {
    conn.send_line("ANSI detected.", d);
    return true;
  }
  conn.send_line("No ANSI detected.", d);
  return false;
}

std::string Color(int c, bool ansi) {
  static auto wwivd_curatr = 7;
  if (!ansi) {
    return "";
  }
  auto s = wwiv::sdk::ansi::makeansi(c, wwivd_curatr);
  wwivd_curatr = 0;
  return s;
}

ConnectionHandler::ConnectionHandler(ConnectionData d, accepted_socket_t a)
    : data(std::move(d)), r(a) {}

wwivd_matrix_entry_t ConnectionHandler::DoMatrixLogon(const wwivd_config_t& c) {

  SocketConnection conn(r.client_socket, SocketConnection::ExitMode::LEAVE_SOCKET_OPEN);
  if (c.bbses.empty()) {
    // This should be checked before calling this method.
    throw std::runtime_error("c.bbses.empty()");
  }
  if (c.bbses.size() == 1) {
    // Only have 1 entry, that's the one to display.
    return c.bbses.front();
  }

  const auto ansi = check_ansi(conn);
  const auto d = 1s;
  for (auto tries = 0; tries < 3; tries++) {
    conn.send_line(StrCat(Color(10, ansi), "Matrix Logon Menu"), d);
    conn.send_line("\r\n", d);
    for (const auto& b : c.bbses) {
      // Skip ones that require ANSI.
      if (b.require_ansi && !ansi) {
        continue;
      }
      std::ostringstream ss;
      ss << Color(14, ansi) << b.key << Color(3, ansi) << ") " << Color(10, ansi) << b.name;
      if (!b.description.empty()) {
        ss << Color(11, ansi) << "  (" << b.description << ")";
      }
      conn.send_line(ss.str(), d);
    }
    std::ostringstream ss;
    ss << Color(14, ansi) << '!' << Color(3, ansi) << ") " << Color(11, ansi) << " Logoff/Quit.";
    conn.send_line(ss.str(), d);

    conn.send_line("\r\n", d);
    conn.send(StrCat(Color(3, ansi), "Enter Selection: "), d);
    auto key_str = conn.receive_upto(1, std::chrono::seconds(15));
    // dump left overs
    conn.receive_upto(1024, std::chrono::milliseconds(1));
    if (key_str.empty()) {
      continue;
    }
    const auto key = key_str.front();

    for (const auto& b : c.bbses) {
      if (std::toupper(b.key) == std::toupper(key)) {
        conn.send_line("\r\n", d);
        return b;
      }
    }
    // Hangup
    if (key == '!') {
      conn.close();
      return {};
    }
  }

  conn.close();
  return {};
}

// Can throw
ConnectionHandler::BlockedConnectionResult ConnectionHandler::CheckForBlockedConnection() {
  const auto sock = r.client_socket;
  string remote_peer;
  const auto& b = data.c->blocking;

  // We fail open when we can't get the remote peer
  if (!GetRemotePeerAddress(sock, remote_peer)) {
    LOG(ERROR) << "Allowing connections we can't determine the remote peer.";
    return BlockedConnectionResult(BlockedConnectionAction::ALLOW, remote_peer);
  }

  // Check for always allowed addresses
  if (b.use_goodip_txt && data.good_ips_) {
    if (data.good_ips_->IsAlwaysAllowed(remote_peer)) {
      LOG(INFO) << "Allowing connection for goodip.txt always-allowed peer: " << remote_peer;
      return BlockedConnectionResult(BlockedConnectionAction::ALLOW, remote_peer);
    }
  }

  // Check for always blocked addresses
  if (b.use_badip_txt && data.bad_ips_) {
    if (data.bad_ips_->IsBlocked(remote_peer)) {
      // We have a connection from a blocked country
      LOG(INFO) << "Denying connection attempt from badip.txt blocked peer: " << remote_peer;
      return BlockedConnectionResult(BlockedConnectionAction::DENY, remote_peer);
    }
  }

  // Check for country blocking if we have a DNS cc server defined.
  if (b.use_dns_cc && !b.dns_cc_server.empty()) {
    const auto cc = get_dns_cc(remote_peer, b.dns_cc_server);
    LOG(INFO) << "Accepted connection on port: " << r.port << "; from: " << remote_peer
              << "; country code: " << cc;
    if (contains(data.c->blocking.block_cc_countries, cc)) {
      // We have a connection from a blocked country
      LOG(INFO) << "Denying connection attempt from country " << cc << " for peer: " << remote_peer;
      return BlockedConnectionResult(BlockedConnectionAction::DENY, remote_peer);
    }
  }

  // Not blocked address nor blocked country. See if it's a new
  // connection, and if so, should we block it now.
  if (b.auto_blocklist && data.auto_blocker_) {
    if (!data.auto_blocker_->Connection(remote_peer)) {
      // We have a newly blocked address.
      LOG(INFO) << "Denying connection attempt from AutoBlocker: " << remote_peer;
      return BlockedConnectionResult(BlockedConnectionAction::DENY, remote_peer);
    }
  }

  // Nothing left to check, let the connection through.
  LOG(INFO) << "Allowing connection for peer: " << remote_peer;
  return BlockedConnectionResult(BlockedConnectionAction::ALLOW, remote_peer);
}

void ConnectionHandler::HandleBinkPConnection() {
  auto sock = r.client_socket;
  try {
    auto result = CheckForBlockedConnection();
    if (result.action == BlockedConnectionAction::DENY) {
      LOG(INFO) << "Sending BUSY. blocked.";
      SocketConnection conn(r.client_socket);
      conn.send_line("BUSY\r\n", 10s);
      closesocket(sock);
      return;
    }
    if (!data.concurrent_connections_->aquire(result.remote_peer)) {
      LOG(INFO) << "Binkp Connection blocked by concurent connection limit.";
      SocketConnection conn(r.client_socket);
      conn.send_line("BUSY\r\n", 10s);
      closesocket(sock);
      return;
    }
    ScopeExit at_exit([&] { data.concurrent_connections_->release(result.remote_peer); });

    auto& nodemgr = data.nodes->at("BINKP");
    int node = -1;
    if (nodemgr->AcquireNode(node)) {
      ScopeExit at_exit2([=] {
        closesocket(sock);
        VLOG(2) << "closed socket: " << sock;
      });
      launch_cmd(data.c->binkp_cmd, nodemgr, 0, sock, ConnectionType::BINKP, result.remote_peer);
    }

  } catch (const std::exception& e) {
    LOG(ERROR) << "HandleBinkPConnection: Handled Uncaught Exception: " << e.what();
  }
  VLOG(1) << "Exiting HandleBinkPConnection (exception)";
}

ConnectionHandler::MailerModeResult ConnectionHandler::DoMailerMode() {
  SocketConnection conn(r.client_socket, SocketConnection::ExitMode::RESET_TO_BLOCKING);

  const string text = "CONNECT 2400\r\nWWIV - Server\r\nPress <ESC> twice for the BBS...\r\n";
  LOG(INFO) << "In DoMailerMode.";
  conn.send_line(text, 10s);

  const auto end = system_clock::now() + 10s;
  auto num_escapes = 0;
  while (system_clock::now() < end && num_escapes < 2) {
    conn.send(".", 1s);
    auto received = conn.receive_upto(1, 1s);
    if (!received.empty() && received.front() == 27) {
      ++num_escapes;
    }
  }

  conn.send("\r\n\r\n", 1s);

  return num_escapes > 1 ? ConnectionHandler::MailerModeResult::ALLOW
                         : ConnectionHandler::MailerModeResult::DENY;
}

void ConnectionHandler::HandleConnection() {
  auto sock = r.client_socket;
  try {
    SocketConnection conn(r.client_socket);
    auto result = CheckForBlockedConnection();
    if (result.action == BlockedConnectionAction::DENY) {
      VLOG(2) << "HandleConnection: Blocked: " << result.remote_peer;
      conn.send_line("BUSY\r\n", 10s);
      closesocket(sock);
      return;
    }
    if (!data.concurrent_connections_->aquire(result.remote_peer)) {
      LOG(INFO) << " Blocked by concurrent limit: " << result.remote_peer;
      conn.send_line("BUSY\r\n", 10s);
      closesocket(sock);
      return;
    }
    ScopeExit at_exit([&] { data.concurrent_connections_->release(result.remote_peer); });
    const auto connection_type = connection_type_for(*data.c, r.port);

    if (data.c->blocking.mailer_mode && connection_type == ConnectionType::TELNET) {
      auto mailer_result = DoMailerMode();
      if (mailer_result == MailerModeResult::DENY) {
        LOG(INFO) << "DENY (from MailerMode, didn't press ESC twice)";
        closesocket(sock);
        return;
      }
      LOG(INFO) << "ACCEPT (From MailerMode)";
    }

    if (data.c->bbses.empty()) {
      // If no bbses are defined, bail early and let someone know.
      LOG(ERROR) << "No BBSes defined in wwivconfig for the Matrix.";
      conn.send_line("No BBSes defined in wwivconfig for the Matrix.  Please tell the SysOp.",
                     std::chrono::seconds(1));
      return;
    }

    wwivd_matrix_entry_t bbs;
    if (connection_type == ConnectionType::TELNET) {
      bbs = DoMatrixLogon(*data.c);
    } else if (connection_type == ConnectionType::SSH) {
      bbs = data.c->bbses.front();
    }

    VLOG(2) << "BBS is: " << bbs.name;

    if (!contains(*data.nodes, bbs.name)) {
      // HOW???
      conn.send_line(StrCat("Can't find config for bbs: ", bbs.name), std::chrono::seconds(1));
      return;
    }
    auto& nodemgr = data.nodes->at(bbs.name);

    // Telnet or SSH connection.  Find open node number and launch the child.
    auto node = -1;
    if (nodemgr->AcquireNode(node)) {
      const auto& cmd = (connection_type == ConnectionType::SSH) ? bbs.ssh_cmd : bbs.telnet_cmd;
      launch_node(*data.config, cmd, nodemgr, node, sock, connection_type, result.remote_peer);
      VLOG(1) << "Exiting HandleConnection (launch_node)";
    } else {
      using namespace std::chrono_literals;
      LOG(INFO) << "Sending BUSY. No available node to handle connection.";
      conn.send_line("BUSY\r\n", 10s);
      VLOG(1) << "Exiting HandleConnection (busy)";
    }
  } catch (const std::exception& e) {
    LOG(ERROR) << "Handled Uncaught Exception: " << e.what();
  }
}

void HandleConnection(std::unique_ptr<ConnectionHandler> h) { h->HandleConnection(); }

void HandleBinkPConnection(std::unique_ptr<ConnectionHandler> h) { h->HandleBinkPConnection(); }

} // namespace wwiv
