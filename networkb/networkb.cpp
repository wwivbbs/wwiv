//
// NetworkB(inkp) prototype.
//
// example usage:
//
// Originating:
// G:\tmp>\Debug\networkb.exe --send --network=wwivnet --node=[node number to call]
//
// Answering Side:
// \Debug\networkb.exe --receive --network=wwivnet
//
// \tmp\n.ini:
// [NETWORK]
// NODE=2
// SYSTEM_NAME=My Test System
// NETWORK_NAME=wwivnet
// # Normally DIR would be C:\bbs\wwivnet or something like that.
// NETWORK_DIR=G:\tmp 
// 
// \tmp1\n.ini is the same with these differences
// NODE=1
// NETWORK_DIR=G:\tmp1 
//
// addresses.binkp:
// @1 localhost:24554 -
// @2 localhost:24554 -
//

#include "networkb/binkp.h"
#include "networkb/binkp_config.h"
#include "networkb/connection.h"
#include "networkb/socket_connection.h"
#include "networkb/socket_exceptions.h"
#include "networkb/wfile_transfer_file.h"

#include "sdk/config.h"
#include "sdk/networks.h"

#include <fcntl.h>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/stl.h"
#include "core/strings.h"
#include "core/file.h"

using std::clog;
using std::endl;
using std::map;
using std::string;
using std::unique_ptr;
using std::vector;

using namespace wwiv::net;
using wwiv::stl::contains;
using namespace wwiv::strings;
using namespace wwiv::sdk;

static map<string, string> ParseArgs(int argc, char** argv) {
  map<string, string> args;
  for (int i=0; i < argc; i++) {
    const string s(argv[i]);
    if (starts_with(s, "--")) {
      const vector<string> delims = SplitString(s, "=");
      const string value = (delims.size() > 1) ? delims[1] : "";
      args.emplace(delims[0].substr(2), value);
    }
  }
  return args;
}

int main(int argc, char** argv) {
  map<string, string> args = ParseArgs(argc, argv);

  for (auto arg : args) {
    clog << "k: " << arg.first << "; v: " << arg.second << endl;
  }

  string network_name;
  string node_config_filename;

  network_name = args.at("network");
  if (network_name.empty()) {
    clog << "--network=[network name] must be specified." << endl;
    return 1;
  }
  
  string bbsdir = File::current_directory();
  if (contains(args, "bbsdir")) {
    bbsdir = args["bbsdir"];
  }
  Config config(bbsdir);
  if (!config.IsInitialized()) {
    clog << "Unable to load config.dat." << endl;
    return 1;
  }
  Networks networks(config);
  if (!networks.IsInitialized()) {
    clog << "Unable to load networks." << endl;
    return 1;
  }

  int expected_remote_node = 0;
  if (contains(args, "node")) {
    expected_remote_node = std::stoi(args["node"]);
  }
  try {
    BinkConfig config(network_name, config, networks);
    unique_ptr<SocketConnection> c;
    BinkSide side = BinkSide::ORIGINATING;
    if (contains(args, "receive")) {
      clog << "BinkP receive" << endl;
      side = BinkSide::ANSWERING;
      c = Accept(24554);
    } else if (contains(args, "send")) {
      clog << "BinkP send to: " << expected_remote_node << endl;
      const BinkNodeConfig* node_config = config.node_config_for(expected_remote_node);
      if (node_config == nullptr) {
        clog << "Unable to find node condfig for node: " << expected_remote_node << endl;
        return 2;
      }
      c = Connect(node_config->host, node_config->port);
    } else {
      clog << "No command given to send or receive.  Either use '--send --node=#' or --receive";
      return 1;
    }
    BinkP::received_transfer_file_factory_t factory = [&](const string& filename) { 
      File* f = new File(config.network_dir(), filename);
      return new WFileTransferFile(filename, unique_ptr<File>(f)); 
    };
    BinkP binkp(c.get(), &config, side, expected_remote_node, factory);
    binkp.Run();
  } catch (const socket_error& e) {
    clog << e.what() << std::endl;
  } catch (const std::exception& e) {
    clog << e.what() << std::endl;
  }
  
}
