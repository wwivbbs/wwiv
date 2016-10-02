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
#include <signal.h>
#include <string>
#include <unistd.h>
#include <resolv.h>
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
using std::string;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;
using namespace wwiv::os;

#define BBS_BINARY "bbs"

static pid_t bbs_pid = 0;

static const File node_file(const Config& config, int node_number) {
  return File(config.datadir(), StrCat("nodeinuse.", node_number));
}

static void huphandler(int mysignal) {
  // TODO(rushfan): Should this be clog? Where does stderr go?
  cout << endl;
  cout << "Sending SIGHUP to BBS after receiving " << mysignal << "..." << endl;
  kill(bbs_pid, SIGHUP); // send SIGHUP to process group
}

static int loadUsedNodeData(const Config& config, int num_instances) {
  int used_nodes = 0;
  for(int counter = 1; counter <= num_instances; counter++) {
    File semaphore_file(node_file(config, counter));
    if (semaphore_file.Exists()) {
      ++used_nodes;
    }
  }
  return used_nodes;
}

static bool launchNode(const Config& config, int node_number, int sock) {
  File semaphore_file(node_file(config, node_number));
  if (!semaphore_file.Open(File::modeCreateFile|File::modeText|File::modeReadWrite|File::modeTruncate, File::shareDenyNone)) {
    // TODO(rushfan): What to do?
    clog << "Unable to create semaphore file: " << semaphore_file << "; errno: "
         << errno << endl;
  }
  semaphore_file.Close();
  const string cmd = StringPrintf("./%s -N%u -XT -H%d",
				  BBS_BINARY, node_number, sock);

  LOG(INFO) << "Invoking WWIV with command line:" << cmd;
  pid_t child_pid = fork();
  if (child_pid == -1) {
    // fork failed.
    LOG(ERROR) << "Error forking WWIV.";
    return false;
  } else if (child_pid == 0) {
    // child process.
    struct sigaction sa;
    execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
    // Should not happen unless we can't exec /bin/sh
    _exit(127);
  }
  bbs_pid = child_pid;
  int status = 0;
  while (waitpid(child_pid, &status, 0) == -1) {
    if (errno != EINTR) {
      break;
    }
  }

  bool delete_ok = semaphore_file.Delete();
  if (!delete_ok) {
    LOG(ERROR) << "Unable to delete semaphore file: "<< semaphore_file << "; errno: "
         << errno;
  }
  LOG(INFO) << "Node #" << node_number << "exited with error code ?";
  return true;
}

void setup_signal_handlers() {
  struct sigaction sa;
  sa.sa_handler = huphandler;
  sa.sa_flags = SA_RESETHAND;
  sigfillset(&sa.sa_mask);
  if (sigaction(SIGHUP, &sa, nullptr) == -1) {
    clog << "Unable to install signal handler for SIGHUP";
  }
}

/**
 *  This program is the manager of the nodes for the WWIV BBS software
 *  on UNIX platforms.
 */
int main(int argc, char *argv[])
{
  Logger::Init(argc, argv);
  ScopeExit at_exit(Logger::ExitLogger);
  CommandLine cmdline(argc, argv, "net");
  cmdline.AddStandardArgs();

  LOG(INFO) << "wwivd - WWIV UNIX Daemon.";

  const string wwiv_dir = environment_variable("WWIV_DIR");
  string config_dir = !wwiv_dir.empty() ? wwiv_dir : File::current_directory();
  Config config(config_dir);
  if (!config.IsInitialized()) {
    LOG(ERROR) << "Unable to load CONFIG.DAT";
    return 1;
  }
  File::set_current_directory(config_dir);
  IniFile ini("wwiv.ini", "WWIV");
  const int num_instances = ini.GetNumericValue("NUM_INSTANCES", 4);
  setup_signal_handlers();

  int used_nodes = loadUsedNodeData(config, num_instances);
  LOG(INFO) << "Found " << used_nodes << "/" << num_instances
       << " Nodes in use.";

  int port = 2323;
  struct sockaddr_in my_addr;
  struct sockaddr_in saddr;
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == -1) {
    LOG(ERROR) << "Unable to create socket";
    return 1;
  }
  int optval = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
    LOG(ERROR) << "Unable to create socket";
    return 1;
  }
  if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) == -1) {
    LOG(ERROR) << "Unable to create socket";
    return 1;
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
    return 2;
  }
  if (listen(sock, 10) == -1) {
    LOG(ERROR) << "Error listening " << errno;
    return 3;
  }
 
  socklen_t addr_size = sizeof(sockaddr_in);
  while (true) {
    int client_sock = accept(sock, (sockaddr*)&saddr, &addr_size);
    clog << "Connection from: " << inet_ntoa(saddr.sin_addr) << endl;
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
      for (int node = 1; node <= num_instances; node++) {
        if (!node_file(config, node).Exists()) {
          launchNode(config, node, client_sock);
          return 0;
        }
      }
    } else {
      // we're in the parent still.
      close(client_sock);
    }
  }

  return 1;
}
