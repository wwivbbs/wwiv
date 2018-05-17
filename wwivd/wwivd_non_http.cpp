/**************************************************************************/
/*                                                                        */
/*                          WWIV BBS Software                             */
/*               Copyright (C)2017, WWIV Software Services                */
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

#include <string>
#include <vector>

#include <cereal/access.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include "core/file.h"
#include "core/http_server.h"
#include "core/inifile.h"
#include "core/jsonfile.h"
#include "core/log.h"
#include "core/net.h"
#include "core/os.h"
#include "core/scope_exit.h"
#include "core/semaphore_file.h"
#include "core/socket_connection.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/version.h"
#include "core/wwivport.h"
#include "sdk/config.h"
#include "sdk/datetime.h"
#include "wwivd/connection_data.h"
#include "wwivd/node_manager.h"
#include "wwivd/wwivd.h"

namespace wwiv {
namespace wwivd {

using std::map;
using std::string;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;
using namespace wwiv::os;

static string to_string(const NodeManager& nodes) {
  std::ostringstream ss;
  ss << "Nodes in use: (" << nodes.nodes_used() << "/" << nodes.total_nodes() << ")";
  return ss.str();
}

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

static string CreateCommandLine(const std::string& tmpl, std::map<char, std::string> params) {
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

const string node_file(const Config& config, ConnectionType ct, int node_number) {
  if (ct == ConnectionType::BINKP) {
    return FilePath(config.datadir(), "binkpinuse");
  }
  return FilePath(config.datadir(), StrCat("nodeinuse.", node_number));
}

static bool launch_cmd(const std::string& raw_cmd, std::shared_ptr<NodeManager> nodes,
                       int node_number, int sock, ConnectionType connection_type,
                       const string remote_peer) {
  auto pid = StringPrintf("[%d] ", get_pid());
  nodes->set_node(node_number, connection_type, StrCat("Connected: ", remote_peer));

  map<char, string> params = {{'N', std::to_string(node_number)}, {'H', std::to_string(sock)}};

  // Reset the socket back to blocking mode
  VLOG(2) << "Setting blocking mode.";
  if (!SetBlockingMode(sock)) {
    LOG(ERROR) << "Failed to reset the socket to blocking mode.";
  }

  const string cmd = CreateCommandLine(raw_cmd, params);
  bool result = ExecCommandAndWait(cmd, pid, node_number, sock);

  nodes->ReleaseNode(node_number);

  return result;
}

static bool launch_node(const Config& config, const std::string& raw_cmd,
                        std::shared_ptr<NodeManager> nodes, int node_number, int sock,
                        ConnectionType connection_type, const string remote_peer) {
  ScopeExit at_exit([=] {
    closesocket(sock);
    VLOG(2) << "closed socket: " << sock;
  });

  string pid = StringPrintf("[%d] ", get_pid());
  VLOG(1) << pid << "launching node(" << node_number << ")";
  const auto sem_text =
      StringPrintf("Created by pid: %s\nremote peer: %s", pid.c_str(), remote_peer.c_str());
  const auto sem_path = node_file(config, connection_type, node_number);

  try {
    SemaphoreFile semaphore_file =
        SemaphoreFile::try_acquire(sem_path, sem_text, std::chrono::seconds(60));
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
  } else if (port == c.binkp_port) {
    return ConnectionType::BINKP;
  } else if (port == c.ssh_port) {
    return ConnectionType::SSH;
  } else if (port == c.http_port) {
    return ConnectionType::HTTP;
  }
  // TODO(rushfan) ???
  return ConnectionType::TELNET;
}

static bool check_ansi(SocketConnection& conn) {
  using namespace std::chrono_literals;
  auto d = 3s;
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

static void addto(std::string* ansi_str, int num) {
  if (ansi_str->empty()) {
    ansi_str->append("\x1b[");
  } else {
    ansi_str->append(";");
  }
  ansi_str->append(std::to_string(num));
}

/* Ripped from com.cpp -- maybe this should all move to core? */
static std::string makeansi(int attr, int current_attr) {
  static const std::vector<int> kAnsiColorMap = {'0', '4', '2', '6', '1', '5', '3', '7'};

  int catr = current_attr;
  std::string out;
  //  if ((catr & 0x88) ^ (attr & 0x88)) {
  addto(&out, 0);
  addto(&out, 30 + kAnsiColorMap[attr & 0x07] - '0');
  addto(&out, 40 + kAnsiColorMap[(attr & 0x70) >> 4] - '0');
  catr = (attr & 0x77);
  //  }
  if ((catr & 0x07) != (attr & 0x07)) {
    addto(&out, 30 + kAnsiColorMap[attr & 0x07] - '0');
  }
  if ((catr & 0x70) != (attr & 0x70)) {
    addto(&out, 40 + kAnsiColorMap[(attr & 0x70) >> 4] - '0');
  }
  if ((catr & 0x08) != (attr & 0x08)) {
    addto(&out, 1);
  }
  if ((catr & 0x80) != (attr & 0x80)) {
    // Italics will be generated
    addto(&out, 3);
  }
  if (!out.empty()) {
    out += "m";
  }
  return out;
}

std::string Color(int c, bool ansi) {
  static int curatr = 7;
  if (!ansi) {
    return "";
  }
  auto s = makeansi(c, curatr);
  curatr = 0;
  return s;
}

static const wwivd_matrix_entry_t DoMatrixLogon(const Config& config,
                                                std::unique_ptr<SocketConnection> conn,
                                                const wwivd_config_t& c) {

  using namespace std::chrono_literals;
  if (c.bbses.empty()) {
    // This should be checked before calling this method.
    throw std::runtime_error("c.bbses.empty()");
  }
  if (c.bbses.size() == 1) {
    // Only have 1 entry, that's the one to display.
    return c.bbses.front();
  }

  auto ansi = check_ansi(*conn.get());
  auto d = 1s;
  for (auto tries = 0; tries < 3; tries++) {
    conn->send_line(StrCat(Color(10, ansi), "Matrix Logon Menu"), d);
    conn->send_line("\r\n", d);
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
      conn->send_line(ss.str(), d);
    }
    std::ostringstream ss;
    ss << Color(14, ansi) << '!' << Color(3, ansi) << ") " << Color(11, ansi) << " Logoff/Quit.";
    conn->send_line(ss.str(), d);

    conn->send_line("\r\n", d);
    conn->send(StrCat(Color(3, ansi), "Enter Selection: "), d);
    string key_str = conn->receive_upto(1, std::chrono::seconds(15));
    // dump left overs
    conn->receive_upto(1024, std::chrono::milliseconds(1));
    if (key_str.empty()) {
      continue;
    }
    auto key = key_str.front();

    for (const auto& b : c.bbses) {
      if (std::toupper(b.key) == std::toupper(key)) {
        conn->send_line("\r\n", d);
        return b;
      }
    }
    // Hangup
    if (key == '!') {
      conn->close();
      return {};
    }
  }

  conn->close();
  return {};
}

enum class BlockedConnectionAction { ALLOW, DENY };

struct BlockedConnectionResult { 
  BlockedConnectionAction action{BlockedConnectionAction::ALLOW}; 
  std::string remote_peer;
};
    // Can throw
static BlockedConnectionResult CheckForBlockedConnection(ConnectionData& data) {
  auto sock = data.r.client_socket;
  string remote_peer;
  if (!GetRemotePeerAddress(sock, remote_peer)) {
    // We fail open when we can't get the remote peer
    return {BlockedConnectionAction::ALLOW};
  }

  auto cc = get_dns_cc(remote_peer, "zz.countries.nerd.dk");
  LOG(INFO) << "Accepted HTTP connection on port: " << data.r.port << "; from: " << remote_peer
            << "; coutry code: " << cc;
  if (contains(data.c->blocking.block_cc_countries, cc)) {
    // We have a connection from a blocked country
    LOG(ERROR) << "Denying connection attempt from country " << cc << " for peer: " << remote_peer;
    return {BlockedConnectionAction::DENY, remote_peer};
  }
  return {BlockedConnectionAction::ALLOW, remote_peer};
}

void HandleBinkPConnection(ConnectionData data) {
  auto sock = data.r.client_socket;
  try {
    auto result = CheckForBlockedConnection(data);
    if (result.action == BlockedConnectionAction::DENY) {
      closesocket(sock);
      return;
    }

    auto& nodemgr = data.nodes->at("BINKP");
    int node = -1;
    if (nodemgr->AcquireNode(node)) {
      ScopeExit at_exit([=] {
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

void HandleConnection(ConnectionData data) {
  auto sock = data.r.client_socket;
  try {
    auto result = CheckForBlockedConnection(data);
    if (result.action == BlockedConnectionAction::DENY) {
      closesocket(sock);
      return;
    }

    if (data.c->bbses.empty()) {
      // If no bbses are defined, bail early and let someone know.
      SocketConnection conn(data.r.client_socket);
      LOG(ERROR) << "No BBSes defined in INIT for the Matrix.";
      conn.send_line("No BBSes defined in INIT for the Matrix.  Please tell the SysOp.",
                     std::chrono::seconds(1));
      return;
    }

    auto connection_type = connection_type_for(*data.c, data.r.port);

    wwivd_matrix_entry_t bbs;
    if (connection_type == ConnectionType::TELNET) {
      bbs = DoMatrixLogon(*data.config,
                          std::make_unique<SocketConnection>(data.r.client_socket, false), *data.c);
    } else if (connection_type == ConnectionType::SSH) {
      bbs = data.c->bbses.front();
    }

    VLOG(2) << "BBS is: " << bbs.name;

    if (!contains(*data.nodes, bbs.name)) {
      // HOW???
      SocketConnection conn(data.r.client_socket);
      conn.send_line(StrCat("Can't find config for bbs: ", bbs.name), std::chrono::seconds(1));
      return;
    }
    auto& nodemgr = data.nodes->at(bbs.name);

    // Telnet or SSH connection.  Find open node number and launch the child.
    int node = -1;
    if (nodemgr->AcquireNode(node)) {
      const auto& cmd = (connection_type == ConnectionType::SSH) ? bbs.ssh_cmd : bbs.telnet_cmd;
      launch_node(*data.config, cmd, nodemgr, node, sock, connection_type, result.remote_peer);
      VLOG(1) << "Exiting HandleConnection (launch_node)";
    } else {
      using namespace std::chrono_literals;
      LOG(INFO) << "Sending BUSY. No available node to handle connection.";
      SocketConnection conn(data.r.client_socket);
      conn.send_line("BUSY\r\n", 10s);
      VLOG(1) << "Exiting HandleConnection (busy)";
    }
  } catch (const std::exception& e) {
    LOG(ERROR) << "Handled Uncaught Exception: " << e.what();
  }
}

} // namespace wwivd
} // namespace wwiv
