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
#include <string>
#include <thread>

#include <signal.h>
#include <string>
#include <sys/types.h>

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
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/wwivport.h"
#include "sdk/config.h"
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
    int node_number, int sock, ConnectionType connection_type,
    const string& remote_peer) {
  ScopeExit at_exit([=] {
    closesocket(sock);
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

  return true;
}

bool HandleAccept(
    const wwiv::sdk::Config& config, const wwivd_config_t& c,
    SOCKET sock, ConnectionType connection_type) {
    
  string remote_peer;
  if (GetRemotePeerAddress(sock, remote_peer)) {
    LOG(INFO) << "Connection from: " << remote_peer;
  }
  if (connection_type == ConnectionType::BINKP) {
    // BINKP Connection.
    if (!node_file(config, connection_type, 0)) {
      launch_node(config, c, 0, sock, connection_type, remote_peer);
      return true;
    }

  } else {
    // Telnet or SSH connection.  Find open node number and launch the child.
    for (int node = c.start_node; node <= c.end_node; node++) {
      if (!node_file(config, connection_type, node).Exists()) {
        launch_node(config, c, node, sock, connection_type, remote_peer);
        return true;
      }
    }
    LOG(INFO) << "Sending BUSY. No available node to handle connection.";
    send(sock, "BUSY\r\n", 6, 0);
    closesocket(sock);
    return true;
  }

  closesocket(sock);
  return false;
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
  VLOG(2) << "Using WWIV_USER(1): " << wwiv_user;
  if (wwiv_user.empty()) {
    wwiv_user = cmdline.arg("wwiv_user").as_string();
  }
  VLOG(2) << "Using WWIV_USER: " << wwiv_user;
  Config config(wwiv_dir);
  if (!config.IsInitialized()) {
    LOG(ERROR) << "Unable to load CONFIG.DAT";
    return 1;
  }

  wwivd_config_t c = LoadIniConfig(config);
  File::set_current_directory(c.bbsdir);

  BeforeStartServer();

  if (!DeleteAllSemaphores(config, c.start_node, c.end_node)) {
    LOG(ERROR) << "Unable to clear semaphores.";
  }

  fd_set fds{};
  SOCKET telnet_socket = INVALID_SOCKET;
  SOCKET ssh_socket = INVALID_SOCKET;
  SOCKET binkp_socket = INVALID_SOCKET;
  if (c.telnet_port > 0) {
    LOG(INFO) << "Listening to telnet on port: " << c.telnet_port;
    telnet_socket = CreateListenSocket(c.telnet_port);
  }
  if (c.ssh_port > 0) {
    LOG(INFO) << "Listening to SSH on port: " << c.ssh_port;
    ssh_socket = CreateListenSocket(c.ssh_port);
  }
  if (c.binkp_port > 0) {
    LOG(INFO) << "Listening to BINKP on port: " << c.binkp_port;
    binkp_socket = CreateListenSocket(c.binkp_port);
  }

  int max_fd = std::max<int>(std::max<int>(telnet_socket, ssh_socket), binkp_socket);
  socklen_t addr_size = sizeof(sockaddr_in);

  SwitchToNonRootUser(wwiv_user);

  while (true) {
    const int num_instances = (c.end_node - c.start_node + 1);
    const int used_nodes = load_used_nodedata(config, c.start_node, c.end_node);
    LOG(INFO) << "Nodes in use: (" << used_nodes << "/" << num_instances << ")";

    FD_ZERO(&fds);
    if (c.telnet_port > 0) {
      FD_SET(telnet_socket, &fds);
    }
    if (c.ssh_port > 0) {
      FD_SET(ssh_socket, &fds);
    }
    if (c.binkp_port > 0) {
      FD_SET(binkp_socket, &fds);
    }
    ConnectionType connection_type = ConnectionType::TELNET;
    VLOG(1) << "About to call select. (" << max_fd << ")";
    int status = select(max_fd + 1, &fds, nullptr, nullptr, nullptr);
    VLOG(1) << "After select.";
    if (status < 0) {
      LOG(ERROR) << "Error calling select; errno: " << errno;
      return 2;
    }

    struct sockaddr_in saddr = {};
    int client_sock = INVALID_SOCKET;
    if (c.telnet_port > 0 && FD_ISSET(telnet_socket, &fds)) {
      client_sock = accept(telnet_socket, (sockaddr*)&saddr, &addr_size);
      connection_type = ConnectionType::TELNET;
    } else if (c.ssh_port > 0 && FD_ISSET(ssh_socket, &fds)) {
      client_sock = accept(ssh_socket, (sockaddr*)&saddr, &addr_size);
      connection_type = ConnectionType::SSH;
    } else if (c.binkp_port > 0 && FD_ISSET(binkp_socket, &fds)) {
      client_sock = accept(binkp_socket, (sockaddr*)&saddr, &addr_size);
      connection_type = ConnectionType::BINKP;
    } else {
      LOG(ERROR) << "Unable to determine which socket triggered select!";
      continue;
    }

    if (client_sock == INVALID_SOCKET) {
      LOG(INFO)<< "Error accepting client socket. " << errno;
      continue;
    }

#if defined (WWIVD_USE_LINUX_FORK) && defined(__unix__)
    int childpid = fork();
    if (childpid == -1) {
      LOG(ERROR) << "Error spawning child process. " << errno;
      continue ;
    } else if (childpid == 0) {
      // The rest of this needs to now be in a new thread
      VLOG(2) << "In child process.";
      return HandleAccept(config, c, client_sock, connection_type) ? 0 : 1;
      // [ End of child process ]
    } else {
      // we're in the parent still.
      // N.B.: We won't close this when using threads since we're still in
      // the same process.
      closesocket(client_sock);
    }
#else
    // Changed the lambda to explicitly use the value for connection_type
    // rather than by reference because linux is using ConnectionType::TELNET
    // for ALL incoming connections when calling HandleAccept even though they
    // are correct at initial connection time. issue992
    auto f = [&config,&c,&client_sock,connection_type]{
      HandleAccept(config, c, client_sock, connection_type);
    };
    std::thread client(f);
    client.detach();
#endif
  }

  return 1;
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
    return 1;
  }
  if (cmdline.help_requested()) {
    cout << cmdline.GetHelp() << endl;
    return 0;
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
