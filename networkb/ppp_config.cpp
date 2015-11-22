/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.0x                             */
/*               Copyright (C)2015, WWIV Software Services                */
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
#include "sdk/filenames.h"
#include "sdk/networks.h"

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

static bool ParseAddressesFile(std::map<uint16_t, PPPNodeConfig>* node_config_map, const string network_dir) {
  TextFile node_config_file(network_dir, ADDRESS_NET, "rt");
  if (!node_config_file.IsOpen()) {
    return false;
  }
  // A line will be of the format @node host:port [password].
  string line;
  while (node_config_file.ReadLine(&line)) {
    uint16_t node_number;
    PPPNodeConfig node_config;
    if (ParseAddressNetLine(line, &node_number, &node_config)) {
      // Parsed a line correctly.
      node_config_map->emplace(node_number, node_config);
    }
  }
  return true;
}

PPPConfig::PPPConfig(const std::string& callout_network_name, const Config& config, const Networks& networks)
    : callout_network_name_(callout_network_name) {
  system_name_ = config.config()->systemname;
  if (system_name_.empty()) {
    system_name_ = "Unnamed WWIV BBS";
  }

  const net_networks_rec& net = networks[callout_network_name];
  node_ = net.sysnum;
  if (node_ == 0) {
    throw config_error(StringPrintf("NODE not specified for network: '%s'", callout_network_name.c_str()));
  }

  ParseAddressesFile(&node_config_, net.dir);
}

PPPConfig::PPPConfig(int node_number, const string& system_name, const string& network_dir) 
    : node_(node_number), system_name_(system_name) {
  ParseAddressesFile(&node_config_, network_dir);
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

