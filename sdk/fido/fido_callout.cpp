/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2014-2016 WWIV Software Services              */
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

#include <exception>
#include <stdexcept>
#include <string>
#include <vector>

#include <cereal/cereal.hpp>
#include <cereal/access.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/archives/json.hpp>

#include "core/datafile.h"
#include "core/file.h"
#include "core/jsonfile.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/vardec.h"

using cereal::make_nvp;
using std::string;
using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::strings;


CEREAL_SPECIALIZE_FOR_ALL_ARCHIVES(wwiv::sdk::fido::FidoAddress, cereal::specialization::non_member_load_save_minimal);
#include "sdk/networks_cereal.h"

namespace cereal {
#define SERIALIZE(n, field) { try { ar(cereal::make_nvp(#field, n.field)); } catch(const cereal::Exception&) { ar.setNextName(nullptr); } }

template <class Archive> inline
std::string save_minimal(Archive const &, const wwiv::sdk::fido::FidoAddress& a) {
  return a.as_string();
}

template <class Archive> inline
void load_minimal(Archive const &, wwiv::sdk::fido::FidoAddress& a, const std::string& s) {
  a = wwiv::sdk::fido::FidoAddress(s);
}

}  // namespace cereal

namespace wwiv {
namespace sdk {
namespace fido {

FidoCallout::FidoCallout(const Config& config, const net_networks_rec& net)
    : Callout(net), root_dir_(config.root_directory()), net_(net) {

  if (!config.IsInitialized()) {
    return;
  }

  initialized_ = Load();
  if (!initialized_) {
    LOG(ERROR) << "Failed to read " << FIDO_CALLOUT_JSON << " or " << FIDO_CALLOUT_JSON;
  }
  initialized_ = true;
}

FidoCallout::~FidoCallout() {}

fido_packet_config_t FidoCallout::packet_override_for(const FidoAddress& a) const {
  if (!contains(node_configs_, a)) {
    return{};
  }
  return node_configs_.at(a).packet_config;
}

const net_call_out_rec* FidoCallout::net_call_out_for(int node) const {
  VLOG(2) << "FidoCallout::net_call_out_for(" << node << ")";
  return net_call_out_for(StringPrintf("20000:20000/%d@%s", node, net_.name));
}

const net_call_out_rec* FidoCallout::net_call_out_for(const std::string& node) const {
  static net_call_out_rec nc{};
  VLOG(2) << "FidoCallout::net_call_out_for(" << node << ")";

  try {
    auto node_config = fido_node_config_for(FidoAddress(node));
    memset(&nc, 0, sizeof(net_call_out_rec));
    to_char_array(nc.password, node_config.binkp_config.password);
    return &nc;
  } catch (const std::exception&) {
  }
  return nullptr;
}

fido_node_config_t FidoCallout::fido_node_config_for(const FidoAddress& address) const {
  FidoAddress a = address;
  if (!contains(node_configs_, a)) {
    // Try 4D addressing if we don't have 5D.
    VLOG(2) << "FidoCallout::fido_node_config_for: Trying address without zone";
    a = FidoAddress(address.zone(), address.net(), address.node(), address.point(), "");
  }

  if (contains(node_configs_, a)) {
    return node_configs_.at(a);
  }
  return{};
}

fido_packet_config_t FidoCallout::packet_config_for(const FidoAddress& address) const {
  FidoAddress a = address;
  if (!contains(node_configs_, a)) {
    // Try 4D addressing if we don't have 5D.
    VLOG(2) << "FidoCallout::packet_config_for: Trying address without zone";
    a = FidoAddress(address.zone(), address.net(), address.node(), address.point(), "");
  }

  // base config
  fido_packet_config_t config = net_.fido.packet_config;
  if (contains(node_configs_, a)) {
    // handle overrides
    const fido_packet_config_t n = node_configs_.at(a).packet_config;
    if (!n.areafix_password.empty()) config.areafix_password = n.areafix_password;
    if (!n.compression_type.empty()) config.compression_type = n.compression_type;
    if (n.max_archive_size > 0) config.max_archive_size = n.max_archive_size;
    if (n.max_packet_size > 0) config.max_packet_size = n.max_packet_size;
    if (!n.packet_password.empty()) config.packet_password = n.packet_password;
    if (n.packet_type != fido_packet_t::unset) config.packet_type = n.packet_type;
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
  const string dir = File::MakeAbsolutePath(root_dir_, net_.dir);
  if (!File::Exists(dir, FIDO_CALLOUT_JSON)) {
    return true;
  }
  JsonFile<decltype(node_configs_)> json(dir, FIDO_CALLOUT_JSON, "callout", node_configs_);
  return json.Load();
}

bool FidoCallout::Save() {
  const string dir = File::MakeAbsolutePath(root_dir_, net_.dir);
  JsonFile<decltype(node_configs_)> json(dir, FIDO_CALLOUT_JSON, "callout", node_configs_);
  return json.Save();
}

}
}
}
