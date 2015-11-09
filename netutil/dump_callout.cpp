#include <cstdio>
#include <fcntl.h>
#include <iostream>
#ifdef _WIN32
#include <io.h>
#else  // _WIN32
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif 
#include <string>
#include <map>
#include <vector>
#include "core/file.h"
#include "core/strings.h"
#include "networkb/callout.h"
#include "networkb/connection.h"
#include "sdk/config.h"
#include "sdk/net.h"
#include "sdk/networks.h"

using std::cerr;
using std::clog;
using std::cout;
using std::endl;
using std::map;
using std::string;
using wwiv::net::Callout;
using wwiv::sdk::Config;
using wwiv::sdk::Networks;
using namespace wwiv::strings;

void dump_callout_usage() {
  cout << "Usage:   dump_callout" << endl;
  cout << "Example: dump_callout" << endl;
}

int dump_callout(int argc, char** argv) {
  string bbsdir = File::current_directory();
//  if (contains(args, "bbsdir")) {
//    bbsdir = args["bbsdir"];
//  }
  Config config(bbsdir);
  if (!config.IsInitialized()) {
    clog << "Unable to load config.dat.";
    return 1;
  }
  Networks networks(config);
  if (!networks.IsInitialized()) {
    clog << "Unable to load networks.";
    return 1;
  }

  map<const string, Callout> callouts;
  for (const auto net : networks.networks()) {
    string lower_case_network_name(net.name);
    StringLowerCase(&lower_case_network_name);
    callouts.emplace(lower_case_network_name, Callout(net.dir));
  }

  for (const auto& c : callouts) {
    std::cout << "CALLOUT.NET information: : " << c.first << std::endl;
    std::cout << "===========================================================" << std::endl;
    std::cout << c.second.ToString() << std::endl;
  }

  return 0;
}
