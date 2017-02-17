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

#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <thread>

#include <signal.h>
#include <string>
#include <sys/types.h>
#include <vector>

#ifdef _WIN32

#include <process.h>
#include <WS2tcpip.h>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif  // max

#else  // _WIN32

#include <spawn.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/wait.h>
#endif  // __linux__


#include <cereal/cereal.hpp>
#include <cereal/access.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/archives/json.hpp>

#include "core/command_line.h"
#include "core/file.h"
#include "core/http_server.h"
#include "core/inifile.h"
#include "core/log.h"
#include "core/net.h"
#include "core/os.h"
#include "core/socket_connection.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/version.h"
#include "core/wwivport.h"
#include "sdk/config.h"
#include "sdk/datetime.h"
#include "sdk/vardec.h"
#include "sdk/filenames.h"
#include "wwivd/wwivd.h"

using std::cerr;
using std::clog;
using std::cout;
using std::endl;
using std::map;
using std::string;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;
using namespace wwiv::os;

pid_t bbs_pid = 0;

#ifdef DELETE
#undef DELETE
#endif  // DELETE

static std::string to_string(ConnectionType t) {
  switch (t) {
  case ConnectionType::BINKP: return "BinkP";
  case ConnectionType::HTTP: return "HTTP";
  case ConnectionType::SSH: return "SSH";
  case ConnectionType::TELNET: return "Telnet";
  case ConnectionType::UNKNOWN: return "*UNKNOWN*";
  }
  return "*UNKNOWN*";
}

struct NodeStatus {
public:
  ConnectionType type = ConnectionType::UNKNOWN;
  int node = 0;
  std::string description;
  bool connected = false;
};

class NodeManager {
public:
  NodeManager(int start, int end) : start_(start), end_(end) {
    for (int i = start; i <= end; i++) {
      clear_node(i, ConnectionType::UNKNOWN);
    }
    clear_node(0, ConnectionType::BINKP);
    clear_node(0, ConnectionType::HTTP);
  }
  virtual ~NodeManager() {}

  std::string status_string(const NodeStatus& n) {
    std::string s = n.description;
    if (n.connected) {
      s += " [";
      s += to_string(n.type);
      s += "]";
    }
    return s;
  }

  std::vector<std::string> status_lines() {
    std::lock_guard<std::mutex> lock(mu_);
    std::vector<std::string> v;
    for (const auto& n : nodes_) {
      v.push_back(StrCat("Node #", n.first, " ", status_string(n.second)));
    }
    v.push_back(StrCat("BinkP ", binkp_.description));
    v.push_back(StrCat("HTTP ", http_.description));

    return v;
  }

  NodeStatus& status_for_unlocked(int node, ConnectionType type) {
    if (node == 0) {
      if (type == ConnectionType::HTTP) {
        return http_;
      }
      else if (type == ConnectionType::BINKP) {
        return binkp_;
      }
    }
    return nodes_[node];
  }

  NodeStatus status_for_copy(int node, ConnectionType type) {
    std::lock_guard<std::mutex> lock(mu_);
    NodeStatus n = status_for_unlocked(node, type);
    return n;
  }

  void set_node(int node, ConnectionType type, const std::string& description) {
    std::lock_guard<std::mutex> lock(mu_);
    auto& n = status_for_unlocked(node, type);
    n.node = node;
    n.type = type;
    n.description = description;
    n.connected = true;
  }

  void clear_node(int node, ConnectionType type) {
    std::lock_guard<std::mutex> lock(mu_);
    auto& n = status_for_unlocked(node, type);
    n.node = node;
    n.type = type;
    n.connected = false;
    n.description = "Waiting for Call";
  }

  int nodes_used() const {
    std::lock_guard<std::mutex> lock(mu_);
    int count = 0;
    for (const auto& e : nodes_) {
      if (e.second.connected) {
        count++;
      }
    }
    return count;
  }

  int total_nodes() const { return end_ - start_ + 1; }

  int AquireNode(ConnectionType type) {
    std::lock_guard<std::mutex> lock(mu_);
    for (auto& e : nodes_) {
      if (!e.second.connected) {
        e.second.connected = true;
        e.second.type = type;
        e.second.description = "Connecting...";
        return e.second.node;
      }
    }
    // None
    return 0;
  }

  bool ReleaseNode(int node) {
    std::lock_guard<std::mutex> lock(mu_);
    if (!contains(nodes_, node)) {
      return false;
    }
    auto& n = nodes_.at(node);
    if (!n.connected) {
      return false;
    }
    n.connected = false;
    n.type = ConnectionType::UNKNOWN;
    n.description = "Waiting For Call";
    return true;
  }

private:
  int start_ = 0;
  int end_ = 0;
  std::map<int, NodeStatus> nodes_;
  NodeStatus binkp_;
  NodeStatus http_;

  mutable std::mutex mu_;
};

static std::string to_string(const NodeManager& nodes) {
  std::ostringstream ss;
  ss << "Nodes in use: (" << nodes.nodes_used() << "/" << nodes.total_nodes() << ")";
  return ss.str();
}

struct status_reponse_t {
  int num_instances;
  int used_instances;
  std::vector<std::string> lines;

  template <class Archive>
  void serialize(Archive & ar) {
    ar(cereal::make_nvp("num_instances", num_instances), 
       cereal::make_nvp("used_instances", used_instances), 
       cereal::make_nvp("lines", lines));
  }
};

std::string ToJson(status_reponse_t r) {
  std::ostringstream ss;
  try {
    cereal::JSONOutputArchive save(ss);
    save(cereal::make_nvp("status", r));
  }
  catch (const cereal::RapidJSONException& e) {
    LOG(ERROR) << e.what();
  }
  return ss.str();
}

class StatusHandler : public HttpHandler {
public:
  StatusHandler(NodeManager* nodes, int num, int used) : nodes_(nodes), num_(num), used_(used) {}

  HttpResponse Handle(HttpMethod, const std::string&, std::vector<std::string> headers) override {
    // We only handle status
    HttpResponse response(200);
    response.headers.emplace("Content-Type: ", "text/json");

    status_reponse_t r{};
    r.num_instances = num_;
    r.used_instances = used_;
    const auto v = nodes_->status_lines();
    for (const auto& l : v) {
      r.lines.push_back(l);
    }
    response.text = ToJson(r);
    return response;
  }

private:
  NodeManager* nodes_;
  int num_ = 0;
  int used_ = 0;
};


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

static wwivd_config_t LoadIniConfig(const Config& config) {
  File wwiv_ini_fn(config.root_directory(), "wwiv.ini");
  File wwivd_ini_fn(config.root_directory(), "wwivd.ini");

  wwivd_config_t c{};

  {
    IniFile ini(wwivd_ini_fn.full_pathname(), {"WWIVD"});
    if (!ini.IsOpen()) {
      LOG(ERROR) << "Unable to open INI file: " << wwivd_ini_fn.full_pathname();
    }
    c.bbsdir = config.root_directory();

    c.telnet_port = ini.value<int>("telnet_port", -1);
    c.telnet_cmd = ini.value<string>("telnet_command", "./bbs -XT -H@H -N@N");

    c.ssh_port = ini.value<int>("ssh_port", -1);
    c.ssh_cmd = ini.value<string>("ssh_command", "./bbs -XS -H@H -N@N");

    c.binkp_port = ini.value<int>("binkp_port", -1);
    c.binkp_cmd = ini.value<string>("binkp_command", "./networkb --receive --handle=@H");

    c.http_port = ini.value<int>("http_port", -1);
    c.http_address = ini.value<string>("http_address", "127.0.0.1");

    c.local_node = ini.value<int>("local_node", 1);
    c.start_node = ini.value<int>("start_node", 2);
    c.end_node = ini.value<int>("end_node", 4);
  }

  return c;
}

static const File node_file(const Config& config, ConnectionType ct, int node_number) {
  if (ct == ConnectionType::BINKP) {
    return File(config.datadir(), "binkpinuse");
  }
  return File(config.datadir(), StrCat("nodeinuse.", node_number));
}

static bool DeleteAllSemaphores(const Config& config, int start_node, int end_node) {
  // Delete telnet/SSH node semaphore files.
  for (int i = start_node; i <= end_node; i++) {
    File semaphore_file(node_file(config, ConnectionType::TELNET, i));
    if (semaphore_file.Exists()) {
      semaphore_file.Delete();
    }
  }

  // Delete any BINKP semaphores.
  File binkp_file(node_file(config, ConnectionType::BINKP, 0));
  if (binkp_file.Exists()) {
    binkp_file.Delete();
  }

  return true;
}

static bool launch_node(
    const Config& config, const wwivd_config_t& c,
    NodeManager* nodes,
    int node_number, int sock, ConnectionType connection_type,
    const string remote_peer) {
  ScopeExit at_exit([=] { 
    closesocket(sock);
    VLOG(2) << "closed socket: " << sock;
  });

  string pid = StringPrintf("[%d] ", get_pid());
  VLOG(1) << pid << "launch_node(" << node_number << ")";

  File semaphore_file(node_file(config, connection_type, node_number));
  if (!semaphore_file.Open(File::modeCreateFile|File::modeText|File::modeReadWrite|File::modeTruncate, File::shareDenyNone)) {
    LOG(ERROR) << pid << "Unable to create semaphore file: " << semaphore_file
               << "; errno: " << errno;
    return false;
  }
  semaphore_file.Write(StringPrintf("Created by pid: %s\nremote peer: %s",
      pid.c_str(), remote_peer.c_str()));
  semaphore_file.Close();
  nodes->set_node(node_number, connection_type, StrCat("Connected: ", remote_peer));

  map<char, string> params = {
      {'N', std::to_string(node_number) },
      {'H', std::to_string(sock) }
  };

  string raw_cmd;
  if (connection_type == ConnectionType::TELNET) {
    raw_cmd = c.telnet_cmd;
  } else if (connection_type == ConnectionType::SSH) {
    raw_cmd = c.ssh_cmd;
  } else if (connection_type == ConnectionType::BINKP) {
    raw_cmd = c.binkp_cmd;
  }

  const string cmd = CreateCommandLine(raw_cmd, params);
  ExecCommandAndWait(cmd, pid, node_number, sock);

  VLOG(2) << "About to delete semaphore file: "<< semaphore_file.full_pathname();
  bool delete_ok = semaphore_file.Delete();
  if (!delete_ok) {
    LOG(ERROR) << pid << "Unable to delete semaphore file: "
        << semaphore_file.full_pathname()
        << "; errno: " << errno;
  } else {
    VLOG(2) << "Deleted semaphore file: " << semaphore_file.full_pathname();
  }
  nodes->clear_node(node_number, connection_type);
  nodes->ReleaseNode(node_number);
  VLOG(2) << "After NodeManager::ReleaseNode(" << node_number << ")";
  return true;
}

static ConnectionType connection_type_for(const wwivd_config_t& c, int port) {
  if (port == c.telnet_port) {
    return ConnectionType::TELNET;
  }
  else if (port == c.binkp_port) {
    return ConnectionType::BINKP;
  }
  else if (port == c.ssh_port) {
    return ConnectionType::SSH; 
  }
  else if (port == c.http_port) {
    return ConnectionType::HTTP;
  }
  // TODO(rushfan) ???
  return ConnectionType::TELNET;
}

struct ConnectionData {
  ConnectionData(const wwiv::sdk::Config* g, const wwivd_config_t* t, NodeManager* n,
    const accepted_socket_t a) : config(g), c(t), nodes(n), r(a) {}
  const wwiv::sdk::Config* config;
  const wwivd_config_t* c;
  NodeManager* nodes;
  const accepted_socket_t r;
};

static void HandleConnection(ConnectionData data) {
  /*const wwiv::sdk::Config& config, const wwivd_config_t& c, NodeManager* nodes,
    const accepted_socket_t r) {*/
  auto sock = data.r.client_socket;
  try {
    string remote_peer;
    if (GetRemotePeerAddress(sock, remote_peer)) {
      auto cc = get_dns_cc(remote_peer, "zz.countries.nerd.dk");
      LOG(INFO) << "Accepted connection on port: " << data.r.port << "; from: " << remote_peer
        << "; coutry code: " << cc;
    }

    auto connection_type = connection_type_for(*data.c, data.r.port);
    if (connection_type == ConnectionType::BINKP) {
      // BINKP Connection.
      if (!node_file(*data.config, connection_type, 0)) {
        launch_node(*data.config, *data.c, data.nodes, 0, sock, connection_type, remote_peer);
      }
      return;
    }
    else if (connection_type == ConnectionType::HTTP) {
      // HTTP Request
      SocketConnection conn(data.r.client_socket);
      HttpServer h(conn);
      StatusHandler status(data.nodes, data.nodes->total_nodes(), data.nodes->nodes_used());
      h.add(HttpMethod::GET, "/status", &status);
      h.Run();
      return;
    }

    // Telnet or SSH connection.  Find open node number and launch the child.
    int node = data.nodes->AquireNode(connection_type);
    if (node > 0) {
      launch_node(*data.config, *data.c, data.nodes, node, sock, connection_type, remote_peer);
      VLOG(1) << "Exiting HandleConnection (launch_node)";
      return;
    }
    LOG(INFO) << "Sending BUSY. No available node to handle connection.";
    send(sock, "BUSY\r\n", 6, 0);
    VLOG(1) << "Exiting HandleConnection (busy)";
  }
  catch (const std::exception& e) {
    LOG(ERROR) << "Handled Uncaught Exception: " << e.what();
  }
  VLOG(1) << "Exiting HandleConnection (exception)";
}

/**
 *  This program is the manager of the nodes for the WWIV BBS software
 *  on UNIX platforms.
 */
int Main(CommandLine& cmdline) {

  string wwiv_dir = environment_variable("WWIV_DIR");
  if (wwiv_dir.empty()) {
    wwiv_dir = cmdline.arg("bbsdir").as_string();
  }
  VLOG(2) << "Using WWIV_DIR: " << wwiv_dir;

  string wwiv_user = environment_variable("WWIV_USER");
  if (wwiv_user.empty()) {
    wwiv_user = cmdline.arg("wwiv_user").as_string();
    VLOG(2) << "Using WWIV_USER(cmdline): " << wwiv_user;
  }
  else {
    VLOG(2) << "Using WWIV_USER(env): " << wwiv_user;
  }

  const Config config(wwiv_dir);
  if (!config.IsInitialized()) {
    LOG(ERROR) << "Unable to load CONFIG.DAT";
    return EXIT_FAILURE;
  }

  const wwivd_config_t c = LoadIniConfig(config);
  File::set_current_directory(c.bbsdir);

  BeforeStartServer();

  if (!DeleteAllSemaphores(config, c.start_node, c.end_node)) {
    LOG(ERROR) << "Unable to clear semaphores.";
  }

  NodeManager nodes(c.start_node, c.end_node);
  auto fn = [&](accepted_socket_t r) {
    std::thread client(HandleConnection, ConnectionData(&config, &c, &nodes, r));
    client.detach();
  };

  SocketSet sockets;
  if (c.telnet_port > 0) {
    sockets.add(c.telnet_port, fn, "TELNET");
  }
  if (c.ssh_port > 0) {
    sockets.add(c.ssh_port, fn, "SSH");
  }
  if (c.binkp_port > 0) {
    sockets.add(c.binkp_port, fn, "BINKP");
  }
  if (c.http_port > 0) {
    sockets.add(c.http_port, fn, "HTTP");
    // TODO(rushfan):   
    // http_address;
  }

  SwitchToNonRootUser(wwiv_user);

  if (!sockets.Run()) {
    LOG(INFO) << "Error accepting client socket. " << errno;
    return 2;
  }

  return EXIT_FAILURE;
}

int main(int argc, char* argv[]) {
  Logger::Init(argc, argv);
  ScopeExit at_exit(Logger::ExitLogger);
  CommandLine cmdline(argc, argv, "net");
  cmdline.AddStandardArgs();
  cmdline.add_argument({"wwiv_user", "WWIV User to use.", "wwiv2"});
  cmdline.set_no_args_allowed(true);

  if (!cmdline.Parse()) {
    cout << cmdline.GetHelp() << endl;
    return EXIT_FAILURE;
  }
  if (cmdline.help_requested()) {
    cout << cmdline.GetHelp() << endl;
    return EXIT_SUCCESS;
  }

  LOG(INFO) << "wwivd - WWIV Daemon.";
#ifdef __unix__
  signal(SIGCHLD, SIG_IGN);
#endif  // __unix__

  try {
    return Main(cmdline);
  } catch (const std::exception& e) {
    LOG(ERROR) << "Caught top level exception: " << e.what();
    return EXIT_FAILURE;
  }
}
