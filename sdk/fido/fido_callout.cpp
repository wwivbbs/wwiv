/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2014-2021, WWIV Software Services             */
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
#include "sdk/fido/fido_callout.h"

#include <cereal/access.hpp>
// ReSharper disable once CppUnusedIncludeDirective
#include <cereal/cereal.hpp>
#include "core/file.h"
#include "core/jsonfile.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include <exception>
#include <string>

using namespace wwiv::core;
using namespace wwiv::sdk::net;
using namespace wwiv::stl;
using namespace wwiv::strings;

CEREAL_SPECIALIZE_FOR_ALL_ARCHIVES(wwiv::sdk::fido::FidoAddress,
                                   cereal::specialization::non_member_load_save_minimal);
#include "sdk/net/networks_cereal.h"

namespace cereal {

template <class Archive>
std::string save_minimal(Archive const&, const wwiv::sdk::fido::FidoAddress& a) {
  return a.as_string();
}

template <class Archive>
void load_minimal(Archive const&, wwiv::sdk::fido::FidoAddress& a, const std::string& s) {
  a = wwiv::sdk::fido::FidoAddress(s);
}

} // namespace cereal

namespace wwiv::sdk::fido {

FidoCallout::FidoCallout(const Config& config, const Network& net)
    : Callout(net, config.max_backups()), root_dir_(config.root_directory()), net_(net) {

  if (!config.IsInitialized()) {
    return;
  }

  initialized_ = Load();
  if (!initialized_) {
    LOG(ERROR) << "Failed to read " << FIDO_CALLOUT_JSON << " or " << FIDO_CALLOUT_JSON;
  }
  initialized_ = true;
}

FidoCallout::~FidoCallout() = default;

const net_call_out_rec* FidoCallout::net_call_out_for(int node) const {
  VLOG(2) << "       FidoCallout::net_call_out_for(" << node << ")";
  return net_call_out_for(fmt::format("20000:20000/{}@{}", node, net_.name));
}

const net_call_out_rec* FidoCallout::net_call_out_for(const FidoAddress& node) const {
  return net_call_out_for(node.as_string());
}

const net_call_out_rec* FidoCallout::net_call_out_for(const std::string& node) const {
  static net_call_out_rec nc{};
  VLOG(2) << "       FidoCallout::net_call_out_for(" << node << ")";

  try {
    const auto node_config = fido_node_config_for(FidoAddress(node));
    nc.session_password = node_config.binkp_config.password;
    return &nc;
  } catch (const std::exception&) {
  }
  return nullptr;
}

fido_node_config_t FidoCallout::fido_node_config_for(const FidoAddress& address) const {
  FidoAddress a = address;
  if (!contains(node_configs_, a)) {
    // Try 4D addressing if we don't have 5D.
    VLOG(2) << "FidoCallout::fido_node_config_for: Trying address without domain: " << address;
    a = FidoAddress(address.zone(), address.net(), address.node(), address.point(), "");
  }

  if (contains(node_configs_, a)) {
    return at(node_configs_, a);
  }
  return {};
}


static fido_packet_config_t default_packet_config() {
  fido_packet_config_t c{};
  c.compression_type = "ZIP";
  c.packet_type = fido_packet_t::type2_plus;
  c.max_archive_size = 1024*1024;
  c.max_packet_size = 1024*1024;

  return c;
}

static const fido_packet_config_t s_default_packet_config = default_packet_config();

fido_packet_config_t FidoCallout::packet_config_for(const FidoAddress& address) const {
  return packet_config_for(address, s_default_packet_config);
}

fido_packet_config_t
FidoCallout::packet_config_for(const FidoAddress& address,
                               const fido_packet_config_t& default_config) const {
  if (!contains(node_configs_, address)) {
    // Try 4D addressing if we don't have 5D.
    VLOG(2) << "FidoCallout::packet_config_for: Trying address without domain: " << address;
    const auto a = FidoAddress(address.zone(), address.net(), address.node(), address.point(), "");
    if (contains(node_configs_, a)) {
      return at(node_configs_, a).packet_config;
    }
    VLOG(2) << "FidoCallout::packet_config_for: Didn't have without zone, returning default";
    return default_config;
  }
  return at(node_configs_, address).packet_config;
}

fido_packet_config_t FidoCallout::merged_packet_config_for(const FidoAddress& address,
    const fido_packet_config_t& base_config) const {
  auto a = address;
  if (!contains(node_configs_, a)) {
    // Try 4D addressing if we don't have 5D.
    VLOG(2) << "FidoCallout::packet_config_for: Trying address without domain: " << address;
    a = FidoAddress(address.zone(), address.net(), address.node(), address.point(), "");
  }

  auto config = base_config;
  // base config
  if (contains(node_configs_, a)) {
    // handle overrides
    auto n = at(node_configs_, a).packet_config;
    if (base_config.packet_type == fido_packet_t::unset) {
      // Our base config is empty, just use the node one.
      return n;
    }
    if (!n.areafix_password.empty())
      config.areafix_password = n.areafix_password;
    if (!n.compression_type.empty())
      config.compression_type = n.compression_type;
    if (n.max_archive_size > 0)
      config.max_archive_size = n.max_archive_size;
    if (n.max_packet_size > 0)
      config.max_packet_size = n.max_packet_size;
    if (!n.packet_password.empty())
      config.packet_password = n.packet_password;
    if (n.packet_type != fido_packet_t::unset)
      config.packet_type = n.packet_type;
    if (n.netmail_status != fido_bundle_status_t::unknown) {
      config.netmail_status = n.netmail_status;
    }
  }
  return config;
}

bool FidoCallout::insert(const FidoAddress& a, const fido_node_config_t& c) {
  // emplace only inserts, doesn't update.
  node_configs_.erase(a);
  node_configs_.emplace(a, c);
  return true;
}

bool FidoCallout::erase(const FidoAddress& a) {
  node_configs_.erase(a);
  return true;
}

bool FidoCallout::Load() {
  node_configs_.clear();
  if (!File::Exists(FilePath(net_.dir, FIDO_CALLOUT_JSON))) {
    return true;
  }
  JsonFile json(FilePath(net_.dir, FIDO_CALLOUT_JSON), "callout", node_configs_);
  return json.Load();
}

bool FidoCallout::Save() {
  const auto dir = File::absolute(root_dir_, net_.dir);
  JsonFile json(FilePath(dir, FIDO_CALLOUT_JSON), "callout", node_configs_);
  return json.Save();
}

} // namespace wwiv
