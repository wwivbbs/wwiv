/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2017, WWIV Software Services             */
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
/**************************************************************************/
#include "sdk/binkp.h"

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
namespace sdk {

// [[ VisibleForTesting ]]
bool ParseBinkConfigLine(const string& line, std::string& node, binkp_session_config_t& config) {

  // A line will be of the format @node host:port
  if (line.empty() || line[0] != '@') {
    // skip empty lines and those not starting with @.
    return false;
  }
  
  stringstream stream(line);
  string node_str;
  stream >> node_str;
  node = node_str.substr(1);
  string host_port_str;
  stream >> host_port_str;
  
  string host = host_port_str;
  uint16_t port = 24554;  // default port
  if (host_port_str.find(':') != string::npos) {
    vector<string> host_port = SplitString(host_port_str, ":");
    host = host_port[0];
    port = to_number<uint16_t>(host_port[1]);
  }
  
  config.host = host;
  config.port = port;

  return true;
}

static bool ParseAddressesFile(std::map<std::string, binkp_session_config_t>* node_config_map, const string network_dir) {
  TextFile node_config_file(network_dir, "binkp.net", "rt");
  if (node_config_file.IsOpen()) {
    // Only load the configuration file if it exists.
    string line;
    while (node_config_file.ReadLine(&line)) {
      std::string node_number;
      binkp_session_config_t node_config;
      StringTrim(&line);
      if (ParseBinkConfigLine(line, node_number, node_config)) {
        // Parsed a line correctly.
        node_config_map->emplace(node_number, node_config);
      }
    }
  }
  return true;
}

Binkp::Binkp(const std::string& network_dir) {
  // network names will alwyas be compared lower case.

  ParseAddressesFile(&node_config_, network_dir);
}

Binkp::~Binkp() {}

const binkp_session_config_t* Binkp::binkp_session_config_for(const std::string& node) const {
  auto iter = node_config_.find(node);
  if (iter != end(node_config_)) {
    return &iter->second;
  }
  return nullptr;
}

const binkp_session_config_t* Binkp::binkp_session_config_for(uint16_t node) const {
  return binkp_session_config_for(std::to_string(node));
}

}  // namespace net
}  // namespace wwiv

