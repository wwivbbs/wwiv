/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2021, WWIV Software Services             */
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
#include "sdk/net/binkp.h"

#include "core/file.h"
#include "core/strings.h"
#include "core/textfile.h"
#include <map>
#include <string>

using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;

namespace wwiv::sdk {

// [[ VisibleForTesting ]]
std::optional<std::tuple<std::string, binkp_session_config_t>>
ParseBinkConfigLine(const std::string& line) {
  // A line will be of the format @node host:port
  if (line.empty() || line[0] != '@') {
    // skip empty lines and those not starting with @.
    return std::nullopt;
  }
  
  std::stringstream stream(line);
  std::string node_str;
  stream >> node_str;
  auto node = node_str.substr(1);
  std::string host_port_str;
  stream >> host_port_str;

  auto host = host_port_str;
  uint16_t port = 24554;  // default port
  if (host_port_str.find(':') != std::string::npos) {
    auto host_port = SplitString(host_port_str, ":");
    host = host_port[0];
    port = to_number<uint16_t>(host_port[1]);
  }
  
  binkp_session_config_t config;
  config.host = host;
  config.port = port;

  return {{node, config}};
}

static bool ParseAddressesFile(std::map<std::string, binkp_session_config_t>* node_config_map, const std::filesystem::path network_dir) {
  TextFile node_config_file(FilePath(network_dir, "binkp.net"), "rt");
  if (node_config_file.IsOpen()) {
    // Only load the configuration file if it exists.
    std::string line;
    while (node_config_file.ReadLine(&line)) {
      StringTrim(&line);
      auto r = ParseBinkConfigLine(line);
      if (r) {
        auto [node_number, node_config] = r.value();
        // Parsed a line correctly.
        node_config_map->emplace(node_number, node_config);
      }
    }
  }
  return true;
}

Binkp::Binkp(const std::filesystem::path& network_dir) {
  // network names will always be compared lower case.

  ParseAddressesFile(&node_config_, network_dir);
}

Binkp::~Binkp() = default;

const binkp_session_config_t* Binkp::binkp_session_config_for(const std::string& node) const {
  const auto iter = node_config_.find(node);
  if (iter != end(node_config_)) {
    return &iter->second;
  }
  return nullptr;
}

const binkp_session_config_t* Binkp::binkp_session_config_for(uint16_t node) const {
  return binkp_session_config_for(std::to_string(node));
}

} // namespace wwiv

