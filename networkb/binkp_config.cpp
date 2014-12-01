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

using std::clog;
using std::endl;
using std::map;
using std::string;
using std::stringstream;
using std::unique_ptr;
using std::vector;
using wwiv::core::IniFile;
using wwiv::strings::SplitString;
using wwiv::strings::StringToUnsignedShort;;
using wwiv::strings::StringPrintf;

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

BinkConfig::BinkConfig(const string& ini_filename, const string& node_config_filename) {

  ini_file_.reset(new IniFile(ini_filename, "NETWORK"));
  if (!ini_file_->IsOpen()) {
    throw config_error(StringPrintf("Unable to open ini file: '%s'", ini_filename.c_str()));
  }

  TextFile node_config_file(node_config_filename, "rt");
  if (!node_config_file.IsOpen()) {
    throw config_error(StringPrintf("Unable to open node config file: '%s'", node_config_filename.c_str()));
  }

  node_ = ini_file_->GetNumericValue("NODE");
  if (node_ == 0) {
    throw config_error(StringPrintf("NODE not specified in INI file: '%s'", ini_filename.c_str()));
  }
  system_name_.assign(ini_file_->GetValue("SYSTEM_NAME", ""));
  if (system_name_.empty()) {
    system_name_ = "Unnamed WWIV BBS";
  }

  const string current_directory = File::current_directory();
  network_dir_ = ini_file_->GetValue("NETWORK_DIR", current_directory.c_str());
  network_name_ = ini_file_->GetValue("NETWORK_NAME", "wwivnet");

  // A line will be of the format @node host:port [password].
  string line;
  while (node_config_file.ReadLine(&line)) {
    uint16_t node_number;
    BinkNodeConfig node_config;
    if (ParseBinkConfigLine(line, &node_number, &node_config)) {
      // Parsed a line correctly.
      node_config_.insert(std::make_pair(node_number, node_config));
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

