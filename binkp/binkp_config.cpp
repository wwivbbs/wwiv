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
#include "binkp/binkp_config.h"

#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "binkp/config_exceptions.h"
#include "sdk/net/networks.h"
#include "sdk/fido/fido_address.h"
#include "sdk/fido/fido_callout.h"
#include <memory>
#include <string>

using namespace wwiv::strings;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::sdk::fido;

namespace wwiv::net {

BinkConfig::BinkConfig(const std::string& callout_network_name, const Config& config,
                       const Networks& networks)
    : config_(config), callout_network_name_(callout_network_name), networks_(networks) {
  // network names will alwyas be compared lower case.
  StringLowerCase(&callout_network_name_);
  system_name_ = config.system_name();
  if (system_name_.empty()) {
    system_name_ = "Unnamed WWIV BBS";
  }
  sysop_name_ = config.sysop_name();
  if (sysop_name_.empty()) {
    sysop_name_ = "Unknown WWIV SysOp";
  }
  gfiles_directory_ = config.gfilesdir();

  if (networks.contains(callout_network_name)) {
    const Network& net = networks[callout_network_name];
    if (net.type == network_type_t::wwivnet) {
      callout_wwivnet_node_ = net.sysnum;
      if (callout_wwivnet_node_ == 0) {
        throw config_error(
            fmt::format("NODE not specified for network: '{}'", callout_network_name));
      }
      binkp_.reset(new Binkp(net.dir));
    } else if (net.type == network_type_t::ftn) {
      callout_fido_node_ = net.fido.fido_address;
      if (callout_fido_node_.empty()) {
        throw config_error(
            fmt::format("NODE not specified for network: '{}'", callout_network_name));
      }
    } else {
      throw config_error("BinkP is not supported for this network type.");
    }
    // TODO(rushfan): This needs to be a shim binkp that reads from the nodelist
    // or overrides.
    binkp_.reset(new Binkp(net.dir));
  }
}

const Network& BinkConfig::network(const std::string& network_name) const {
  return networks_[network_name];
}

const Network& BinkConfig::callout_network() const {
  return network(callout_network_name_);
}

void BinkConfig::session_identifier(std::string id) {
  session_identifier_ = std::move(id);
}

std::filesystem::path BinkConfig::network_dir(const std::string& network_name) const {
  return network(network_name).dir;
}

std::string BinkConfig::receive_dir(const std::string& network_name) const {
  auto dir = wwiv::core::FilePath(network_dir(network_name), session_identifier_).string();
  VLOG(1) << "       BinkConfig::receive_dir: " << dir;
  return dir;
}

static Network test_net(const std::string& network_dir) {
  Network net(network_type_t::wwivnet, "WWIVnet");
  net.sysnum = 1;
  net.dir = network_dir;
  return net;
}

// For testing
BinkConfig::BinkConfig(int node_number, const wwiv::sdk::Config& config,
                       const std::string& network_dir)
    : config_(config), callout_wwivnet_node_(node_number),
      networks_({test_net(network_dir)}) {
  binkp_.reset(new Binkp(network_dir));
  system_name_ = config.system_name();
  sysop_name_ = config.sysop_name();
  gfiles_directory_ = config.gfilesdir();
}

BinkConfig::~BinkConfig() = default;

const binkp_session_config_t* BinkConfig::binkp_session_config_for(const std::string& node) const {
  static binkp_session_config_t static_session{};

  if (callout_network().type == network_type_t::wwivnet) {
    if (!binkp_) {
      return nullptr;
    }
    return binkp_->binkp_session_config_for(node);
  } else if (callout_network().type == network_type_t::ftn) {
    try {
      FidoAddress address(node);
      FidoCallout fc(config_, callout_network());
      if (!fc.IsInitialized())
        return nullptr;
      auto fido_node = fc.fido_node_config_for(address);

      if (fido_node.binkp_config.host.empty()) {
        // We must have a host at least, otherwise we know this
        // is a completely empty record and we must return nullptr
        // since the node config is not found.
        return nullptr;
      }

      static_session = fido_node.binkp_config;
      if (static_session.port == 0) {
        // Set to default port.
        static_session.port = 24554;
      }

      if (static_session.port == 0 && static_session.host.empty()) {
        return nullptr;
      }

      return &static_session;
    } catch (const std::exception&) {
      return nullptr;
    }
  }
  return nullptr;
}

const binkp_session_config_t* BinkConfig::binkp_session_config_for(uint16_t node) const {
  return binkp_session_config_for(std::to_string(node));
}

} // namespace wwiv
