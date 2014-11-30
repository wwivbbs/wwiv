//
// NetworkB(inkp) prototype.
//
// example usage:
//
// G:\tmp>\Debug\networkb.exe --send --config=G:\tmp\n.ini --addresses=G:\tmp\addresses.binkp
//
// n.ini:
// [NETWORK]
// NODE=1
// SYSTEM_NAME=My Test System
//
// addresses.binkp:
// @2 localhost:24554 -
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

  std::string config_filename;
  std::string node_config_filename;

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
    node_config_filename = candidate_node_config.GetFullPathName();
    if (!candidate_node_config.Exists()) {
      clog << "Node Config file " << node_config_filename << " does not exist." << endl;
      return 1;
    }
  }

  try {
    BinkConfig config(config_filename, node_config_filename);
    if (contains(args, "receive")) {
      clog << "BinkP receive" << endl;
      unique_ptr<SocketConnection> c(Accept(24554));
      BinkP binkp(c.get(), &config, BinkSide::ANSWERING, 2);
      binkp.Run();
    } else if (contains(args, "send")) {
      // send
      unique_ptr<SocketConnection> c(Connect("localhost", 24554));
      BinkP binkp(c.get(), &config, BinkSide::ORIGINATING, 1);
      binkp.Run();
    } else {
      clog << "No command given to send or receive.  Either use '--send --node=#' or --receive";
    }
  } catch (const socket_error& e) {
    clog << e.what() << std::endl;
  } catch (const std::exception& e) {
    clog << e.what() << std::endl;
  }
  
}
