//
// NetworkB(inkp) prototype.
//
// example usage:
//
// G:\tmp>\Debug\networkb.exe --send --config=G:\tmp\n.ini --node=1
//
// n.ini:
// [NETWORK]
// NODE=2
// SYSTEM_NAME=My Test System
// NETWORK_NAME=wwivnet
// # Normally DIR would be C:\bbs\wwivnet or something like that.
// DIR=G:\tmp 
//
// addresses.binkp:
// @1 localhost:24554 -
//

#include "networkb/binkp.h"
#include "networkb/binkp_config.h"
#include "networkb/connection.h"
#include "networkb/socket_connection.h"
#include "networkb/socket_exceptions.h"

#include <fcntl.h>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/stl.h"
#include "core/strings.h"
#include "core/wfile.h"

using std::clog;
using std::endl;
using std::map;
using std::string;
using std::unique_ptr;
using std::vector;

using namespace wwiv::net;
using wwiv::stl::contains;
using wwiv::strings::starts_with;
using wwiv::strings::SplitString;

static map<string, string> ParseArgs(int argc, char** argv) {
  map<string, string> args;
  for (int i=0; i < argc; i++) {
    const string s(argv[i]);
    if (starts_with(s, "--")) {
      vector<string> delims = SplitString(s, "=");
      string value = (delims.size() > 1) ? delims[1] : "";
      // lame old GCC doesn't have emplace.
      args.insert(std::make_pair(delims[0].substr(2), value));
    }
  }
  return args;
}

int main(int argc, char** argv) {
  map<string, string> args = ParseArgs(argc, argv);

  for (auto arg : args) {
    clog << "k: " << arg.first << "; v: " << arg.second << endl;
  }

  string config_filename;
  string node_config_filename;

  if (!contains(args, "config")) {
    clog << "--config=<full path to config file> must be specified" << endl;
    return 1;
  }

  config_filename = args["config"];
  WFile config_file(config_filename);
  if (!config_file.Exists()) {
    clog << "Config file " << config_filename << " does not exist." << endl;
    return 1;
  }

  if (contains(args, "addresses")) {
    node_config_filename = args["addresses"];
    if (!WFile::Exists(node_config_filename)) {
      clog << "Node Config file " << node_config_filename << " does not exist." << endl;
      return 1;
    }
  } else {
    WFile candidate_node_config(config_file.GetParent(), "addresses.binkp");
    node_config_filename = candidate_node_config.full_pathname();
    if (!candidate_node_config.Exists()) {
      clog << "Node Config file " << node_config_filename << " does not exist." << endl;
      return 1;
    }
  }

  int expected_remote_node = 0;
  if (contains(args, "node")) {
    expected_remote_node = std::stoi(args["node"]);
  }
  try {
    BinkConfig config(config_filename, node_config_filename);
    unique_ptr<SocketConnection> c;
    BinkSide side = BinkSide::ORIGINATING;
    if (contains(args, "receive")) {
      clog << "BinkP receive" << endl;
      side = BinkSide::ANSWERING;
      c = Accept(24554);
    } else if (contains(args, "send")) {
      clog << "BinkP send to: " << expected_remote_node << endl;
      side = BinkSide::ORIGINATING;
      const BinkNodeConfig* node_config = config.node_config_for(expected_remote_node);
      if (node_config == nullptr) {
        clog << "Unable to find node condfig for node: " << expected_remote_node << endl;
        return 2;
      }
      c = Connect(node_config->host, node_config->port);
    } else {
      clog << "No command given to send or receive.  Either use '--send --node=#' or --receive";
    }
    BinkP binkp(c.get(), &config, side, expected_remote_node);
    binkp.Run();
  } catch (const socket_error& e) {
    clog << e.what() << std::endl;
  } catch (const std::exception& e) {
    clog << e.what() << std::endl;
  }
  
}
