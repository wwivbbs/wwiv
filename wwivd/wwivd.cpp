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

#include <cctype>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
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
#include "core/jsonfile.h"
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
  NodeManager(const std::string& name, ConnectionType type, int start, int end) 
    : name_(name), type_(type), start_(start), end_(end) {
    for (int i = start; i <= end; i++) {
      clear_node(i);
    }
  }
  virtual ~NodeManager() {}

  std::string status_string(const NodeStatus& n) const {
    std::string s = n.description;
    if (n.connected) {
      s += " [";
      s += to_string(n.type);
      s += "]";
    }
    return s;
  }

  std::vector<std::string> status_lines() const {
    std::lock_guard<std::mutex> lock(mu_);
    std::vector<std::string> v;
    for (const auto& n : nodes_) {
      std::ostringstream ss;
      ss << this->name_ << " ";
      if (n.first > 0) {
        ss << "Node #" << n.first << " ";
      }
      ss << status_string(n.second);
      v.push_back(ss.str());
    }

    return v;
  }

  NodeStatus& status_for_unlocked(int node) {
    return nodes_[node];
  }

  NodeStatus status_for_copy(int node) {
    std::lock_guard<std::mutex> lock(mu_);
    NodeStatus n = status_for_unlocked(node);
    return n;
  }

  void set_node(int node, ConnectionType type, const std::string& description) {
    std::lock_guard<std::mutex> lock(mu_);
    auto& n = status_for_unlocked(node);
    n.node = node;
    n.type = type;
    n.description = description;
    n.connected = true;
  }

  void clear_node(int node) {
    std::lock_guard<std::mutex> lock(mu_);
    auto& n = status_for_unlocked(node);
    n.node = node;
    n.type = type_;
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

  bool AcquireNode(int& node) {
    std::lock_guard<std::mutex> lock(mu_);
    for (auto& e : nodes_) {
      if (!e.second.connected) {
        e.second.connected = true;
        e.second.type = type_;
        e.second.description = "Connecting...";
        node = e.second.node;
        return true;
      }
    }
    // None
    node = -1;
    return false;
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
    n.type = type_;
    n.description = "Waiting For Call";
    return true;
  }
  int start_node() const { return start_; }
  int end_node() const { return end_; }

private:
  const std::string name_;
  const ConnectionType type_;
  int start_ = 0;
  int end_ = 0;
  std::map<int, NodeStatus> nodes_;

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
  StatusHandler(std::map<const std::string, std::shared_ptr<NodeManager>>* nodes) : nodes_(nodes) {}

  HttpResponse Handle(HttpMethod, const std::string&, std::vector<std::string> headers) override {
    // We only handle status
    HttpResponse response(200);
    response.headers.emplace("Content-Type: ", "text/json");

    status_reponse_t r{};
    for (const auto& n : *nodes_) {
      const auto v = n.second->status_lines();
      r.num_instances += n.second->total_nodes();
      r.used_instances += n.second->nodes_used();
      for (const auto& l : v) {
        r.lines.push_back(l);
      }
    }
    response.text = ToJson(r);
    return response;
  }

private:
  std::map<const std::string, std::shared_ptr<NodeManager>>* nodes_;
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

static bool launch_cmd(
  const std::string& raw_cmd, std::shared_ptr<NodeManager> nodes, int node_number, int sock,
  ConnectionType connection_type, const string remote_peer) {
  string pid = StringPrintf("[%d] ", get_pid());
  nodes->set_node(node_number, connection_type, StrCat("Connected: ", remote_peer));

  map<char, string> params = {
    { 'N', std::to_string(node_number) },
    { 'H', std::to_string(sock) }
  };

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

static bool launch_node(
    const Config& config, const std::string& raw_cmd,
    std::shared_ptr<NodeManager> nodes,
    int node_number, int sock, ConnectionType connection_type,
    const string remote_peer) {
  ScopeExit at_exit([=] { 
    closesocket(sock);
    VLOG(2) << "closed socket: " << sock;
  });

  string pid = StringPrintf("[%d] ", get_pid());
  VLOG(1) << pid << "launching node(" << node_number << ")";

  File semaphore_file(node_file(config, connection_type, node_number));
  if (!semaphore_file.Open(File::modeCreateFile|File::modeText|File::modeReadWrite|File::modeTruncate, File::shareDenyNone)) {
    LOG(ERROR) << pid << "Unable to create semaphore file: " << semaphore_file
               << "; errno: " << errno;
    return false;
  }
  semaphore_file.Write(StringPrintf("Created by pid: %s\nremote peer: %s",
      pid.c_str(), remote_peer.c_str()));
  semaphore_file.Close();
  launch_cmd(raw_cmd, nodes, node_number, sock, connection_type, remote_peer);

  VLOG(2) << "About to delete semaphore file: "<< semaphore_file.full_pathname();
  bool delete_ok = semaphore_file.Delete();
  if (!delete_ok) {
    LOG(ERROR) << pid << "Unable to delete semaphore file: "
        << semaphore_file.full_pathname()
        << "; errno: " << errno;
  } else {
    VLOG(2) << "Deleted semaphore file: " << semaphore_file.full_pathname();
  }
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
  ConnectionData(const wwiv::sdk::Config* g, const wwivd_config_t* t, 
    std::map<const std::string, std::shared_ptr<NodeManager>>* n,
    const accepted_socket_t a) : config(g), c(t), nodes(n), r(a) {}
  const wwiv::sdk::Config* config;
  const wwivd_config_t* c;
  std::map<const std::string, std::shared_ptr<NodeManager>>* nodes;
  const accepted_socket_t r;
};

static bool check_ansi(SocketConnection& conn) {
  auto d = std::chrono::seconds(3);
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


static void addto(std::string *ansi_str, int num) {
  if (ansi_str->empty()) {
    ansi_str->append("\x1b[");
  }
  else {
    ansi_str->append(";");
  }
  ansi_str->append(std::to_string(num));
}

/* Ripped from com.cpp -- maybe this should all move to core? */
static std::string makeansi(int attr, int current_attr) {
  static const std::vector<int> kAnsiColorMap = { '0', '4', '2', '6', '1', '5', '3', '7' };

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

static const wwivd_matrix_entry_t DoMatrixLogon(const Config& config, SocketConnection conn, const wwivd_config_t& c) {
  if (c.bbses.empty()) {
    // TODO(rushfan): Throw exception here?
    conn.close();
    return{};
  }
  if (c.bbses.size() == 1) {
    // Only have 1 entry, that's the one to display.
    return c.bbses.front();
  }

  bool ansi = check_ansi(conn);
  auto d = std::chrono::seconds(1);
  for (int tries = 0; tries < 3; tries++) {
    conn.send_line(StrCat(Color(10, ansi), "Matrix Logon Menu"), d);
    conn.send_line("\r\n", d);
    for (const auto& b : c.bbses) {
      // Skip ones that require ANSI.
      if (b.require_ansi && !ansi) { continue; }
      std::ostringstream ss;
      ss << Color(14, ansi) << b.key << Color(3, ansi) << ") " << Color(11, ansi) << b.description;
      conn.send_line(ss.str(), d);
    }
    std::ostringstream ss;
    ss << Color(14, ansi) << '!' << Color(3, ansi) << ") " << Color(11, ansi) << " Logoff/Quit.";
    conn.send_line(ss.str(), d);

    conn.send_line("\r\n", d);
    conn.send(StrCat(Color(3, ansi), "Enter Selection: "), d);
    string key_str = conn.receive_upto(1, std::chrono::seconds(15));
    // dump left overs
    conn.receive_upto(1024, std::chrono::milliseconds(1));
    if (key_str.empty()) { continue; }
    char key = key_str.front();

    for (const auto& b : c.bbses) {
      if (std::toupper(b.key) == std::toupper(key)) {
        conn.send_line("\r\n", d);
        return b;
      }
    }
    // Hangup
    if (key == '!') {
      conn.close();
      return{};
    }

  }

  conn.close();
  return{};

}

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
      auto& nodemgr = data.nodes->at("BINKP");
      int node = -1;
      if (nodemgr->AcquireNode(node)) {
        ScopeExit at_exit([=] {
          closesocket(sock);
          VLOG(2) << "closed socket: " << sock;
        });
        launch_cmd(data.c->binkp_cmd, nodemgr, 0, sock, connection_type, remote_peer);
      }
      return;
    }
    else if (connection_type == ConnectionType::HTTP) {
      // HTTP Request
      SocketConnection conn(data.r.client_socket);
      HttpServer h(conn);
      StatusHandler status(data.nodes);
      h.add(HttpMethod::GET, "/status", &status);
      h.Run();
      return;
    }

    // TODO(rushfan): Do matrix logon here.
    wwivd_matrix_entry_t bbs;
    if (connection_type == ConnectionType::TELNET) {
      bbs = DoMatrixLogon(
        *data.config, SocketConnection(data.r.client_socket, false), *data.c);
    }
    else if (connection_type == ConnectionType::SSH) {
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
      string raw_cmd = bbs.telnet_cmd;
      if (connection_type == ConnectionType::SSH) {
        raw_cmd = bbs.ssh_cmd;
      }
      launch_node(*data.config, raw_cmd, nodemgr, node, sock, connection_type, remote_peer);
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

  wwivd_config_t c{};
  c.Load(config);
  File::set_current_directory(config.root_directory());

  BeforeStartServer();

  std::map<const std::string, std::shared_ptr<NodeManager>> nodes;
  for (const auto& b : c.bbses) {
    nodes[b.name] = std::make_shared<NodeManager>(b.name, ConnectionType::TELNET, b.start_node, b.end_node);
  }
  // Add node manager for binkp.
  nodes["BINKP"] = std::make_shared<NodeManager>("BINKP", ConnectionType::BINKP, 0, 0);

  for (const auto& n : nodes) {
    if (!DeleteAllSemaphores(config, n.second->start_node(), n.second->end_node())) {
      LOG(ERROR) << "Unable to clear semaphores.";
    }
  }

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
