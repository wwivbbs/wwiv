#include "networkb/binkp_config.h"

#include <iostream>
#include <memory>
#include <map>
#include <sstream>
#include <string>

#include "core/strings.h"
#include "core/wfile.h"
#include "core/wtextfile.h"

using std::clog;
using std::endl;
using std::map;
using std::string;
using std::stringstream;
using std::unique_ptr;
using std::vector;
using wwiv::strings::SplitString;
using wwiv::strings::StringToUnsignedShort;;
using wwiv::strings::StringPrintf;

namespace wwiv {
namespace net {

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

BinkConfig::BinkConfig(const std::string& config_file) : config_file_(config_file) {
  WTextFile config(config_file, "rt");
  if (!config.IsOpen()) {
    throw config_error(StringPrintf("Unable to open config file: '%s'",
				    config_file.c_str()));
  }
  // A line will be of the format @node host:port [password].
  string line;
  while (config.ReadLine(&line)) {
    uint16_t node;
    BinkNodeConfig config;
    if (ParseBinkConfigLine(line, &node, &config)) {
      // Parsed a line correctly.
      node_config_.insert(std::make_pair(node, config));
    }
  }
}

BinkConfig::~BinkConfig() {}

const BinkNodeConfig* BinkConfig::node_config_for(int node) {
  auto iter = node_config_.find(node);
  if (iter != end(node_config_)) {
    return &iter->second;
  }
  return nullptr;
}


}  // namespace net
}  // namespace wwiv

