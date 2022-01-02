/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2022, WWIV Software Services             */
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
#include "binkp/ppp_config.h"

#include "binkp/config_exceptions.h"
#include "core/file.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/format.h"
#include "sdk/filenames.h"
#include "sdk/net/networks.h"

#include <filesystem>
#include <map>
#include <sstream>
#include <string>
#include <utility>

using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::sdk;

namespace wwiv::net {

// [[ VisibleForTesting ]]
bool ParseAddressNetLine(const std::string& line, uint16_t* node, PPPNodeConfig* config) {
  if (line.empty() || line[0] != '@') {
    // skip empty lines and those not starting with @.
    return false;
  }
  
  std::stringstream stream(line);
  std::string node_str;
  stream >> node_str;
  *node = to_number<uint16_t>(node_str.substr(1));
  std::string email_address;
  stream >> config->email_address;
  
  return true;
}

static bool ParseAddressesFile(std::map<int, PPPNodeConfig>* node_config_map, const std::filesystem::path& network_dir) {
  TextFile node_config_file(FilePath(network_dir, ADDRESS_NET), "rt");
  if (!node_config_file.IsOpen()) {
    return false;
  }
  // A line will be of the format @node host:port [password].
  std::string line;
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
  system_name_ = config.system_name();
  if (system_name_.empty()) {
    system_name_ = "Unnamed WWIV BBS";
  }

  const auto& net = networks[callout_network_name];
  node_ = net.sysnum;
  if (node_ == 0) {
    throw config_error(fmt::format("NODE not specified for network: '{}'", callout_network_name));
  }

  ParseAddressesFile(&node_config_, net.dir);
}

PPPConfig::PPPConfig(int node_number, std::string system_name, const std::string& network_dir)
    : node_(node_number), system_name_(std::move(system_name)) {
  ParseAddressesFile(&node_config_, network_dir);
}

PPPConfig::~PPPConfig() = default;

const PPPNodeConfig* PPPConfig::ppp_node_config_for(int node) const {
  const auto iter = node_config_.find(node);
  if (iter != end(node_config_)) {
    return &iter->second;
  }
  return nullptr;
}


} // namespace wwiv

