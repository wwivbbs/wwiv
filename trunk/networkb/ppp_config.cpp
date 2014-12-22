#include "networkb/ppp_config.h"

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
bool ParseAddressNetLine(const string& line, uint16_t* node, PPPNodeConfig* config) {
  if (line.empty() || line[0] != '@') {
    // skip empty lines and those not starting with @.
    return false;
  }
  
  stringstream stream(line);
  string node_str;
  stream >> node_str;
  *node = StringToUnsignedShort(node_str.substr(1));
  string email_address;
  stream >> config->email_address;
  
  return true;
}

PPPConfig::PPPConfig(const std::string& network_name, const Config& config, const Networks& networks) : network_name_(network_name) {
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

  TextFile node_config_file(network_dir_, "ADDRESS.NET", "rt");
  if (!node_config_file.IsOpen()) {
    throw config_error(StrCat("Unable to open node config file: ", node_config_file.full_pathname()));
  }

  // A line will be of the format @node host:port [password].
  string line;
  while (node_config_file.ReadLine(&line)) {
    uint16_t node_number;
    PPPNodeConfig node_config;
    if (ParseAddressNetLine(line, &node_number, &node_config)) {
      // Parsed a line correctly.
      node_config_.emplace(node_number, node_config);
    }
  }
}

PPPConfig::~PPPConfig() {}

const PPPNodeConfig* PPPConfig::node_config_for(int node) const {
  auto iter = node_config_.find(node);
  if (iter != end(node_config_)) {
    return &iter->second;
  }
  return nullptr;
}


}  // namespace net
}  // namespace wwiv

