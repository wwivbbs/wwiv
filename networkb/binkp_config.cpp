#include "networkb/binkp_config.h"

#include <iostream>
#include <memory>
#include <map>
#include <sstream>
#include <string>

#include "core/strings.h"
#include "core/inifile.h"
#include "core/file.h"
#include "core/textfile.h"
#include "sdk/networks.h"

using std::clog;
using std::endl;
using std::map;
using std::string;
using std::stringstream;
using std::unique_ptr;
using std::vector;
using wwiv::core::IniFile;
using namespace wwiv::strings;
using namespace wwiv::sdk;

namespace wwiv {
namespace net {

// [[ VisibleForTesting ]]
bool ParseBinkConfigLine(const string& line, uint16_t* node, BinkNodeConfig* config) {
  if (line.empty() || line[0] != '@') {
    // skip empty lines and those not starting with @.
    return false;
  }
  
  stringstream stream(line);
  string node_str;
  stream >> node_str;
  *node = StringToUnsignedShort(node_str.substr(1));
  string host_port_str;
  stream >> host_port_str;
  
  string host = host_port_str;
  uint16_t port = 24554;  // default port
  if (host_port_str.find(':') != string::npos) {
    vector<string> host_port = SplitString(host_port_str, ":");
    host = host_port[0];
    port = StringToUnsignedShort(host_port[1]);
  }
  
  string password = "-";
  stream >> password;

  config->host = host;
  config->port = port;
  config->password = password;

  return true;
}

BinkConfig::BinkConfig(const std::string& network_name, const Config& config, const Networks& networks) : network_name_(network_name) {
  system_name_ = config.config()->systemname;
  if (system_name_.empty()) {
    system_name_ = "Unnamed WWIV BBS";
  }

  const net_networks_rec& net = networks[network_name];
  node_ = net.sysnum;
  network_dir_ = net.dir;
  if (node_ == 0) {
    throw config_error(StringPrintf("NODE not specified for network: '%s'", network_name.c_str()));
  }

  TextFile node_config_file(network_dir_, "addresses.binkp", "rt");
  if (!node_config_file.IsOpen()) {
    throw config_error(StrCat("Unable to open node config file: ", node_config_file.full_pathname()));
  }

  // A line will be of the format @node host:port [password].
  string line;
  while (node_config_file.ReadLine(&line)) {
    uint16_t node_number;
    BinkNodeConfig node_config;
    if (ParseBinkConfigLine(line, &node_number, &node_config)) {
      // Parsed a line correctly.
      node_config_.emplace(node_number, node_config);
    }
  }
}

// For testing
BinkConfig::BinkConfig(int node_number, const std::string& system_name, int node_to_call) 
  : node_(node_number), system_name_(system_name) {
  BinkNodeConfig node_config { "example.com", 24554, "-" };
  node_config_.insert(std::make_pair(node_number, node_config));
}



BinkConfig::~BinkConfig() {}

const BinkNodeConfig* BinkConfig::node_config_for(int node) const {
  auto iter = node_config_.find(node);
  if (iter != end(node_config_)) {
    return &iter->second;
  }
  return nullptr;
}


}  // namespace net
}  // namespace wwiv

