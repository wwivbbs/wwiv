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

#include "core/command_line.h"
#include "core/file.h"
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
    if (node == 0) {
      if (type == ConnectionType::HTTP) {
        return http_;
      }
      else if (type == ConnectionType::BINKP) {
        return binkp_;
      }
    }
    return nodes_.at(node);
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

private:
  int start_ = 0;
  int end_ = 0;
  std::map<int, NodeStatus> nodes_;
  NodeStatus binkp_;
  NodeStatus http_;

  mutable std::mutex mu_;
};

static std::vector<std::string> read_lines(SocketConnection& conn) {
  std::vector<std::string> lines;
  while (true) {
    auto s = conn.read_line(1024, std::chrono::milliseconds(10));
    if (s.empty()) break;
    LOG(INFO) << s;
    lines.push_back(s);
  }
  return lines;
}

// Subset from https://www.w3.org/Protocols/rfc2616/rfc2616-sec6.html#sec6.1.1
static std::map<int, std::string> CreateHttpStatusMap() {
  std::map<int, std::string> m = {
    { 200, "OK" },
    { 204, "No Content" },
    { 301, "Moved Permanently" },
    { 302, "Found" },
    { 304, "Not Modified" },
    { 307, "Temporary Redirect" },
    { 400, "Bad Request" },
    { 401, "Unauthorized" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
    { 405, "Method Not Allowed" },
    { 406, "Not Acceptable" },
    { 408, "Request Time-out" },
    { 412, "Precondition Failed" },
    { 500, "Internal Server Error" },
    { 501, "Not Implemented" },
    { 503, "Service Unavailable" }
  };
  return m;
}

enum class HttpMethod {
  OPTIONS,
  GET,
  HEAD,
  POST,
  PUT,
  DELETE,
  TRACE,
  CONNECT
};

class HttpResponse {
public:
  HttpResponse(int s) : status(s) {};
  HttpResponse(int s, const std::string& t) : status(s), text(t) {};
  HttpResponse(int s, std::map<std::string, std::string>& h, const std::string& t) : status(s), headers(h), text(t) {};

  int status;
  std::map<std::string, std::string> headers;
  std::string text;
};

class HttpHandler {
public:
  virtual HttpResponse Handle(HttpMethod method, const std::string& path, std::vector<std::string> headers) = 0;
};

class HttpServer {
public:
  HttpServer(SocketConnection& conn) : conn_(conn) {}
  virtual ~HttpServer() {}
  bool add(HttpMethod method, const std::string& root, HttpHandler* handler) {
    if (method != HttpMethod::GET) {
      return false;
    }
    get_.emplace(root, handler);
    return true;
  }

  void SendResponse(HttpResponse& r) {
    LOG(INFO) << "SendResponse()";
    static const auto statuses = CreateHttpStatusMap();
    const auto d = std::chrono::seconds(1);
    conn_.send_line(StrCat("HTTP/1.1 ", r.status, " ", statuses.at(r.status)), d);
    conn_.send_line(StrCat("Date: ", wwiv::sdk::daten_to_wwivnet_time(time(nullptr))), d);
    conn_.send_line(StrCat("Server: wwivd/", wwiv_version, beta_version), d);
    if (!r.text.empty()) {
      auto content_length = r.text.size();
      conn_.send_line(StrCat("Content-Length: ", content_length), d);
      conn_.send("\r\n", d);
      conn_.send(r.text, d);
    }
    else {
      conn_.send_line("Connection: close", d);
      conn_.send("\r\n", d);
    }
    LOG(INFO) << "SendResponse(done)";
  }

  bool Run() {
    LOG(INFO) << "HttpServer::Run()";
    const auto d = std::chrono::seconds(1);
    auto inital_requestline = conn_.read_line(1024, std::chrono::milliseconds(10));
    if (inital_requestline.empty()) {
      return false;
    }
    auto cmd_parts = SplitString(inital_requestline, " ");
    if (cmd_parts.size() < 3) {
      return false;
    }
    auto path = cmd_parts.at(1);
    auto headers = read_lines(conn_);
    auto cmd = cmd_parts.at(0);
    if (cmd == "GET") {
      // handle get.  Read headers
      for (const auto& e : get_) {
        if (starts_with(path, e.first)) {
          auto r = e.second->Handle(HttpMethod::GET, path, headers);
          SendResponse(r);
          return true;
        }
        HttpResponse r404(404);
        SendResponse(r404);
        return true;
      }
    }
    HttpResponse r405(405);
    SendResponse(r405);
    return false;
  }

private:
  SocketConnection conn_;
  std::map<std::string, HttpHandler*> get_;
};

class StatusHandler : public HttpHandler {
public:
  StatusHandler(NodeManager* nodes, int num, int used) : nodes_(nodes), num_(num), used_(used) {}

  HttpResponse Handle(HttpMethod method, const std::string& path, std::vector<std::string> headers) override {
    // We only handle status
    HttpResponse response(200);
    response.headers.emplace("Content-Type: ", "text/json");
    std::ostringstream ss;
    ss << "{\r\n";
    ss << "  \"num_instances\" : " << num_ << "," << "\r\n";
    ss << "  \"used_instances\": " << used_ << ", " << "\r\n";
    ss << "  \"status\": [" << "\r\n";
    const auto v = nodes_->status_lines();
    for (const auto& l : v) {
      ss << "    { \"" << l << "\" }, \r\n";
    }
    ss << "  ]" << "\r\n";
    ss << "}" << "\r\n";
    response.text = ss.str();
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

static int load_used_nodedata(const Config& config, int start_node, int end_node) {
  int used_nodes = 0;
  for(int counter = start_node; counter <= end_node; counter++) {
    File semaphore_file(node_file(config, ConnectionType::TELNET, counter));
    if (semaphore_file.Exists()) {
      ++used_nodes;
    }
  }
  return used_nodes;
}

static bool launch_node(
    const Config& config, const wwivd_config_t& c,
    NodeManager* nodes,
    int node_number, int sock, ConnectionType connection_type,
    const string& remote_peer) {
  
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
  ExecCommandAndWait(cmd, pid, node_number);

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

static void HandleAccept(
    const wwiv::sdk::Config& config, const wwivd_config_t& c, NodeManager* nodes,
    const accepted_socket_t r, const std::string& remote_peer) {
  auto sock = r.client_socket;
  ScopeExit at_exit([=] { closesocket(sock); });
  auto connection_type = connection_type_for(c, r.port);

  try {
    if (connection_type == ConnectionType::BINKP) {
      // BINKP Connection.
      if (!node_file(config, connection_type, 0)) {
        launch_node(config, c, nodes, 0, sock, connection_type, remote_peer);
      }
      return;
    }

    // Telnet or SSH connection.  Find open node number and launch the child.
    for (int node = c.start_node; node <= c.end_node; node++) {
      if (!node_file(config, connection_type, node).Exists()) {
        launch_node(config, c, nodes, node, sock, connection_type, remote_peer);
        return;
      }
    }
    LOG(INFO) << "Sending BUSY. No available node to handle connection.";
    send(sock, "BUSY\r\n", 6, 0);
  }
  catch (const std::exception& e) {
    LOG(ERROR) << "Handled Uncaught Exception: " << e.what();
  }
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

  SocketSet sockets;
  if (c.telnet_port > 0) {
    sockets.add(c.telnet_port, "TELNET");
  }
  if (c.ssh_port > 0) {
    sockets.add(c.ssh_port, "SSH");
  }
  if (c.binkp_port > 0) {
    sockets.add(c.binkp_port, "BINKP");
  }
  if (c.http_port > 0) {
    sockets.add(c.http_port, "HTTP");
    // TODO(rushfan):   
    // http_address;
  }

  SwitchToNonRootUser(wwiv_user);

  while (true) {
    const int num_instances = (c.end_node - c.start_node + 1);
    const int used_nodes = load_used_nodedata(config, c.start_node, c.end_node);
    LOG(INFO) << "Nodes in use: (" << used_nodes << "/" << num_instances << ")";
    auto r = sockets.Run();
    if (r.client_socket == INVALID_SOCKET) {
      LOG(INFO) << "Error accepting client socket. " << errno;
      return 2;
    }
    string remote_peer;
    if (GetRemotePeerAddress(r.client_socket, remote_peer)) {
      auto cc = get_dns_cc(remote_peer, "zz.countries.nerd.dk");
      LOG(INFO) << "Accepted connection on port: " << r.port << "; from: " << remote_peer
        << "; coutry code: " << cc;
    }
    if (r.port == c.http_port) {
      // HTTP Request
      SocketConnection conn(r.client_socket);
      HttpServer h(conn);
      StatusHandler status(&nodes, num_instances, used_nodes);
      h.add(HttpMethod::GET, "/status", &status);
      h.Run();
    }
    else {
      // BBS or BinkP Request
      std::thread client(HandleAccept, std::ref(config), std::ref(c), &nodes, r, std::ref(remote_peer));
      client.detach();
    }
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
