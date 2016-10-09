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

#include <signal.h>
#include <string>
#include <unistd.h>
#include <resolv.h>
#include <pwd.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
 
#include "core/command_line.h"
#include "core/file.h"
#include "core/inifile.h"
#include "core/log.h"
#include "core/os.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "sdk/config.h"
#include "sdk/vardec.h"
#include "sdk/filenames.h"

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

static pid_t bbs_pid = 0;


struct wwivd_config_t {
  int ssh_port = -1;
  int telnet_port = 2323;
  string ssh_cmd;
  string telnet_cmd;
  string bbsdir;
  bool syslog = true;

  int start_node;
  int end_node;
  int local_node;
};

enum class ConnectionType { SSH, TELNET };

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
      } catch (const std::out_of_range& e) {
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
    IniFile ini(wwivd_ini_fn.full_pathname(), "WWIVD");
    if (!ini.IsOpen()) {
      LOG(ERROR) << "Unable to open INI file: " << wwivd_ini_fn.full_pathname();
    }
    c.bbsdir = config.root_directory();
    c.ssh_cmd = ini.GetValue("ssh_command", "./bbs -XS -H@H -N@N");
    c.ssh_port = ini.GetNumericValue("ssh_port", -1);
    c.telnet_cmd = ini.GetValue("telnet_command", "./bbs -XT -H@H -N@N");
    c.telnet_port = ini.GetNumericValue("telnet_port", -1);
    c.syslog = ini.GetBooleanValue("use_syslog", true);
    c.local_node = ini.GetNumericValue("local_node", 1);
    c.start_node = ini.GetNumericValue("start_node", 2);
    c.end_node = ini.GetNumericValue("end_node", 4);
  }

  return c;
}

static const File node_file(const Config& config, int node_number) {
  return File(config.datadir(), StrCat("nodeinuse.", node_number));
}

static void huphandler(int mysignal) {
  cerr << endl;
  cerr << "Sending SIGHUP to BBS after receiving " << mysignal << "..." << endl;
  kill(bbs_pid, SIGHUP); // send SIGHUP to process group
}

static void sigint_handler(int mysignal) {
  cerr << endl;
  cerr << "Sending SIGINT to BBS after receiving " << mysignal << "..." << endl;
  kill(bbs_pid, SIGINT); // send SIGINT to process group
}

static bool DeleteAllSemaphores(const Config& config, int start_node, int end_node) {
  for(int i = start_node; i <= end_node; i++) {
    File semaphore_file(node_file(config, i));
    if (semaphore_file.Exists()) {
      semaphore_file.Delete();
    }
  }

  return true;
}

static int loadUsedNodeData(const Config& config, int start_node, int end_node) {
  int used_nodes = 0;
  for(int counter = start_node; counter <= end_node; counter++) {
    File semaphore_file(node_file(config, counter));
    if (semaphore_file.Exists()) {
      ++used_nodes;
    }
  }
  return used_nodes;
}

static bool launchNode(
    const Config& config, const wwivd_config_t& c,
    int node_number, int sock, ConnectionType connection_type) {

  const string pid = StringPrintf("[%d] ", getpid());
  VLOG(1) << pid << "launchNode(" << node_number << ")";
  File semaphore_file(node_file(config, node_number));
  if (!semaphore_file.Open(File::modeCreateFile|File::modeText|File::modeReadWrite|File::modeTruncate, File::shareDenyNone)) {
    LOG(ERROR) << pid << "Unable to create semaphore file: " << semaphore_file
               << "; errno: " << errno;
    return false;
  }
  semaphore_file.Write(StringPrintf("Created by pid: %s\n", pid.c_str()));
  semaphore_file.Close();

  map<char, string> params = {
      {'N', std::to_string(node_number) },
      {'H', std::to_string(sock) }
  };

  string telnet_or_ssh_cmd = c.telnet_cmd;
  if (connection_type == ConnectionType::SSH) {
    telnet_or_ssh_cmd = c.ssh_cmd;
  }
  const string cmd = CreateCommandLine(telnet_or_ssh_cmd, params);

  LOG(INFO) << pid << "Invoking WWIV with command line:" << cmd;
  pid_t child_pid = fork();
  if (child_pid == -1) {
    // fork failed.
    LOG(ERROR) << pid << "Error forking WWIV.";
    return false;
  } else if (child_pid == 0) {
    // child process.
    struct sigaction sa;
    execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
    LOG(ERROR) << "Unable to exec: '" << cmd.c_str() << "' errno: " << errno;
    // Should not happen unless we can't exec /bin/sh
    _exit(127);
  }
  bbs_pid = child_pid;
  int status = 0;
  VLOG(2) << pid << "before waitpid";
  while (waitpid(child_pid, &status, 0) == -1) {
    if (errno != EINTR) {
      break;
    }
  }
  VLOG(2) << pid << "after waitpid";

  if (WIFEXITED(status)) {
    // Process exited.
    LOG(INFO) << pid << "Node #" << node_number << " exited with error code: " << WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    LOG(INFO) << pid << "Node #" << node_number << " killed by signal: " << WTERMSIG(status);
  } else if (WIFSTOPPED(status)) {
    LOG(INFO) << pid << "Node #" << node_number << " stopped by signal: " << WSTOPSIG(status);
  }

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

void setup_sighup_handlers() {
  struct sigaction sa;
  sa.sa_handler = huphandler;
  sa.sa_flags = SA_RESETHAND;
  sigfillset(&sa.sa_mask);
  if (sigaction(SIGHUP, &sa, nullptr) == -1) {
    LOG(ERROR) << "Unable to install signal handler for SIGHUP";
  }
}

void setup_sigint_handlers() {
  struct sigaction sa;
  sa.sa_handler = sigint_handler;
  sa.sa_flags = SA_RESETHAND;
  sigfillset(&sa.sa_mask);
  if (sigaction(SIGHUP, &sa, nullptr) == -1) {
    LOG(ERROR) << "Unable to install signal handler for SIGINT";
  }
}

int CreateListenSocket(int port) {
  struct sockaddr_in my_addr;
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == -1) {
    LOG(ERROR) << "Unable to create socket";
    return -1;
  }
  int optval = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
    LOG(ERROR) << "Unable to create socket";
    return -1;
  }
  if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) == -1) {
    LOG(ERROR) << "Unable to create socket";
    return -1;
  }
  // Try to set nodelay.
  setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));

  my_addr.sin_family = AF_INET ;
  my_addr.sin_port = htons(port);
  memset(&(my_addr.sin_zero), 0, 8);
  my_addr.sin_addr.s_addr = INADDR_ANY ;

  if (bind(sock, (sockaddr*)&my_addr, sizeof(my_addr)) == -1) {
    LOG(ERROR)  << "Error binding to socket, make sure nothing else is listening "
        << "on this port: " << errno;
    return -1;
  }
  if (listen(sock, 10) == -1) {
    LOG(ERROR) << "Error listening " << errno;
    return -1;
  }

  return sock;
}

uid_t GetWWIVUserId(const string& username) {
  passwd* pw = getpwnam(username.c_str());
  if (pw == nullptr) {
    // Unable to find user, let's return our current uid.
    LOG(ERROR) << "Unable to find uid for username: " << username;
    return getuid();
  }
  return pw->pw_uid;
}
/**
 *  This program is the manager of the nodes for the WWIV BBS software
 *  on UNIX platforms.
 */
int Main(CommandLine& cmdline) {

  string wwiv_dir = environment_variable("WWIV_DIR");
  if (wwiv_dir.empty()) {
    wwiv_dir = File::current_directory();
  }
  string wwiv_user = environment_variable("WWIV_USER");
  if (wwiv_user.empty()) {
    wwiv_user = "wwiv";
  }
  Config config(wwiv_dir);
  if (!config.IsInitialized()) {
    LOG(ERROR) << "Unable to load CONFIG.DAT";
    return 1;
  }

  wwivd_config_t c = LoadIniConfig(config);
  File::set_current_directory(c.bbsdir);

  setup_sighup_handlers();
  setup_sigint_handlers();

  if (!DeleteAllSemaphores(config, c.start_node, c.end_node)) {
    LOG(ERROR) << "Unable to clear semaphores.";
  }

  fd_set fds;
  int telnet_socket = -1;
  int ssh_socket = -1;
  if (c.telnet_port > 0) {
    LOG(INFO) << "Listening to telnet on port: " << c.telnet_port;
    telnet_socket = CreateListenSocket(c.telnet_port);
  }
  if (c.ssh_port > 0) {
    LOG(INFO) << "Listening to SSH on port: " << c.ssh_port;
    ssh_socket = CreateListenSocket(c.ssh_port);
  }

  int max_fd = std::max<int>(telnet_socket, ssh_socket);
  socklen_t addr_size = sizeof(sockaddr_in);

  uid_t current_uid = getuid();
  uid_t wwiv_uid = GetWWIVUserId(wwiv_user);
  if (wwiv_uid != current_uid) {
    if (setuid(wwiv_uid) != 0) {
      LOG(ERROR) << "Unable to call setuid(" << wwiv_uid << "); errno: " << errno;
      // TODO(rushfan): Should we exit or continue here?
    }
  }

  while (true) {
    const int num_instances = (c.end_node - c.start_node + 1);
    const int used_nodes = loadUsedNodeData(config, c.start_node, c.end_node);
    LOG(INFO) << "Nodes in use: (" << used_nodes << "/" << num_instances << ")";

    FD_ZERO(&fds);
    if (c.telnet_port > 0) {
      FD_SET(telnet_socket, &fds);
    }
    if (c.ssh_port > 0) {
      FD_SET(ssh_socket, &fds);
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
    int client_sock = -1;
    if (c.telnet_port > 0 && FD_ISSET(telnet_socket, &fds)) {
      client_sock = accept(telnet_socket, (sockaddr*)&saddr, &addr_size);
      connection_type = ConnectionType::TELNET;
    } else if (c.ssh_port > 0 && FD_ISSET(ssh_socket, &fds)) {
      client_sock = accept(ssh_socket, (sockaddr*)&saddr, &addr_size);
      connection_type = ConnectionType::SSH;
    } else {
      LOG(ERROR) << "Unable to determine which socket triggered select!";
      continue;
    }

    LOG(INFO) << "Connection from: " << inet_ntoa(saddr.sin_addr) << endl;
    if (client_sock == -1) {
      LOG(INFO)<< "Error accepting client socket. " << errno;
      continue;
    }

    int childpid = fork();
    if (childpid == -1) {
      LOG(ERROR) << "Error spawning child process. " << errno;
      continue;
    } else if (childpid == 0) {
      // We're in the child process now.
      // Find open node number and launch the child.
      for (int node = c.start_node; node <= c.end_node; node++) {
        if (!node_file(config, node).Exists()) {
          launchNode(config, c, node, client_sock, connection_type);
          return 0;
        }
      }
      LOG(INFO) << "Sending BUSY. No available node to handle connection.";
      send(client_sock, "BUSY\r\n", 6, 0);
      close(client_sock);
      return 0;
    } else {
      // we're in the parent still.
      close(client_sock);
    }
  }

  return 1;
}

int main(int argc, char* argv[]) {
  Logger::Init(argc, argv);
  ScopeExit at_exit(Logger::ExitLogger);
  CommandLine cmdline(argc, argv, "net");
  cmdline.AddStandardArgs();

  LOG(INFO) << "wwivd - WWIV UNIX Daemon.";

  signal(SIGCHLD, SIG_IGN);

  try {
    return Main(cmdline);
  } catch (const std::exception& e) {
    LOG(ERROR) << "Caught top level exception: " << e.what();
    return EXIT_FAILURE;
  }
}
