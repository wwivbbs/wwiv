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
#include "sdk/networks.h"

#include <exception>
#include <stdexcept>
#include <string>
#include <vector>

#include <cereal/cereal.hpp>
#include <cereal/access.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/archives/json.hpp>

#include "core/datafile.h"
#include "core/file.h"
#include "core/jsonfile.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/vardec.h"

using cereal::make_nvp;
using namespace wwiv::core;
using namespace wwiv::strings;

// We want to override how we store network_type_t to store it as a string, not int.
// This has to be in the global namespace. 
CEREAL_SPECIALIZE_FOR_ALL_ARCHIVES(network_type_t, cereal::specialization::non_member_load_save_minimal);
CEREAL_SPECIALIZE_FOR_ALL_ARCHIVES(fido_packet_t, cereal::specialization::non_member_load_save_minimal);

namespace cereal {

#define SERIALIZE(n, field) { try { ar(cereal::make_nvp(#field, n.field)); } catch(const cereal::Exception&) { ar.setNextName(nullptr); } }

template <typename T> inline
std::string to_enum_string(const T& t, const std::vector<std::string>& names) {
  try {
    return names.at(static_cast<int>(t));
  } catch (std::out_of_range&) {
    return names.at(0);
  }
}

template <typename T> inline
T from_enum_string(const std::string& v, const std::vector<std::string>& names) {
  try {
    for (size_t i = 0; i < names.size(); i++) {
      if (v == names.at(i)) {
        return static_cast<T>(i);
      }
    }
  } catch (std::out_of_range&) {
    // NOP
  }
  return static_cast<T>(0);
}

template <class Archive> inline 
std::string save_minimal(Archive const &, const network_type_t& t) {
  return to_enum_string<network_type_t>(t, {"wwivnet", "fido", "internet"});
}
template <class Archive> inline
void load_minimal(Archive const &, network_type_t& t, const std::string& v) {
  t = from_enum_string<network_type_t>(v, {"wwivnet", "fido", "internet"});
}

template <class Archive> inline
std::string save_minimal(Archive const &, const fido_packet_t& t) {
  return to_enum_string<fido_packet_t>(t, {"none", "type2+"});
}
template <class Archive> inline
void load_minimal(Archive const &, fido_packet_t& t, const std::string& v) {
  t = from_enum_string<fido_packet_t>(v,{"none", "type2+"});
}

template <class Archive> inline
std::string save_minimal(Archive const &, const fido_transport_t& t) {
  return to_enum_string<fido_transport_t>(t, {"directory", "binkp"});
}
template <class Archive> inline
void load_minimal(Archive const &, fido_transport_t& t, const std::string& v) {
  t = from_enum_string<fido_transport_t>(v, {"directory", "binkp"});
}

template <class Archive> inline
std::string save_minimal(Archive const &, const fido_mailer_t& t) {
  return to_enum_string<fido_mailer_t>(t, {"flo", "attach"});
}
template <class Archive> inline
void load_minimal(Archive const &, fido_mailer_t& t, const std::string& v) {
  t = from_enum_string<fido_mailer_t>(v, {"flo", "attach"});
}

template <class Archive>
void serialize(Archive & ar, fido_packet_config_t& n) {
  SERIALIZE(n, packet_type);
  SERIALIZE(n, compression_type);
  SERIALIZE(n, packet_password);
  SERIALIZE(n, areafix_password);
  SERIALIZE(n, max_archive_size);
  SERIALIZE(n, max_packet_size);
}

template <class Archive>
void serialize(Archive & ar, fido_network_config_t& n) {
  SERIALIZE(n, fake_outbound_node);
  SERIALIZE(n, mailer_type);
  SERIALIZE(n, transport);
  SERIALIZE(n, inbound_dir);
  SERIALIZE(n, secure_inbound_dir);
  SERIALIZE(n, outbound_dir);
  SERIALIZE(n, packet_config);
}

// This has to be in the cereal or default to match net_networks_rec which
// is in the default namespace.
template <class Archive>
void serialize(Archive & ar, net_networks_rec& n) {
  SERIALIZE(n, type);
  try {
    std::string name(n.name);
    ar(make_nvp("name", name));
    to_char_array(n.name, name);
  } catch (const cereal::Exception&) {
    ar.setNextName(nullptr);
  }
  SERIALIZE(n, dir);
  SERIALIZE(n, sysnum);

  // Seralize the Fido config
  if (n.type == network_type_t::ftn) {
    SERIALIZE(n, fido);
  }
}

}  // namespace cereal

namespace wwiv {
namespace sdk {

const int Networks::npos;  // reserve space.

Networks::Networks(const Config& config) : datadir_(config.datadir()) {
  if (!config.IsInitialized()) {
    return;
  }

  {
    DataFile<net_networks_rec_disk> file_dat(datadir_, NETWORKS_DAT, File::modeBinary | File::modeReadOnly, File::shareDenyNone);
    if (!File::Exists(datadir_, NETWORKS_JSON) && !File::Exists(datadir_, NETWORKS_DAT)) {
      return;
    }
  }

  initialized_ = Load();
  if (!initialized_) {
    LOG(ERROR) << "Failed to read " << NETWORKS_JSON << " or " << NETWORKS_DAT;
  }
  initialized_ = true;
}

const net_networks_rec& Networks::at(const std::string& name) const {
  for (auto& n : networks_) {
    if (IsEqualsIgnoreCase(name.c_str(), n.name)) {
      return n;
    }
  }
  throw std::out_of_range(StrCat("Unable to find network named: ", name));
}

net_networks_rec& Networks::at(const std::string& name) {
  for (auto& n : networks_) {
    if (IsEqualsIgnoreCase(name.c_str(), n.name)) {
      return n;
    }
  }
  throw std::out_of_range(StrCat("Unable to find network named: ", name));
}

Networks::~Networks() {}

 auto Networks::network_number(const std::string& network_name) const -> size_type {
  Networks::size_type i = 0;
  for (const auto& n : networks_) {
    if (IsEqualsIgnoreCase(network_name.c_str(), n.name)) {
      return i;
    }
    ++i;
  }
  return npos;
}

bool Networks::contains(const std::string& network_name) const {
  for (const auto& n : networks_) {
    if (IsEqualsIgnoreCase(network_name.c_str(), n.name)) {
      return true;
    }
  }
  return false;
}

// TODO(rushfan): Since should we make this algo available 
// in wwiv::sdk since we do it on all containers often.
bool Networks::insert(std::size_t n, net_networks_rec r) {
  // TODO(rushfan): Add size checking
  auto it = networks_.begin();
  std::advance(it, n);
  networks_.insert(it, r);
  return true;
}

bool Networks::erase(std::size_t n) {
  // TODO(rushfan): Add size checking
  auto it = networks_.begin();
  std::advance(it, n);
  networks_.erase(it);
  return true;
}

bool Networks::Load() {
  if (LoadFromJSON()) {
    return true;
  }
  return LoadFromDat();
}

bool Networks::LoadFromJSON() {
  networks_.clear();
  JsonFile<decltype(networks_)> json(datadir_, NETWORKS_JSON, "networks", networks_);
  return json.Load();
}

bool Networks::LoadFromDat() {
  DataFile<net_networks_rec_disk> file(datadir_, NETWORKS_DAT, File::modeBinary | File::modeReadOnly, File::shareDenyNone);
  if (!file) {
    return false;
  }

  std::vector<net_networks_rec_disk> networks_disk;

  if (!file.ReadVector(networks_disk)) {
    return false;
  }
  for (const auto& n : networks_disk) {
    net_networks_rec r = {};
    r.type = static_cast<network_type_t>(n.type);
    strcpy(r.name, n.name);
    r.dir = n.dir;
    r.sysnum = n.sysnum;
    networks_.emplace_back(r);
  }
  return true;
}

bool Networks::Save() {
  bool dat = SaveToDat();
  bool json = SaveToJSON();

  return dat && json;
}

bool Networks::SaveToJSON() {
  JsonFile<decltype(networks_)> json(datadir_, NETWORKS_JSON, "networks", networks_);
  return json.Save();
}

bool Networks::SaveToDat() {
  std::vector<net_networks_rec_disk> disk;

  for (const auto& from : networks_) {
    net_networks_rec_disk to{};
    to.type = static_cast<uint8_t>(from.type);
    strcpy(to.name, from.name);
    to_char_array(to.dir, from.dir);
    to.sysnum = from.sysnum;
    disk.emplace_back(to);
  }

  DataFile<net_networks_rec_disk> file(datadir_, NETWORKS_DAT,
    File::modeBinary | File::modeReadWrite | File::modeCreateFile | File::modeTruncate, File::shareDenyReadWrite);
  if (!file) {
    return false;
  }
  return file.WriteVector(disk);
}

}
}
