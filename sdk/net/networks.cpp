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
#include "sdk/net/networks.h"

#include "core/datafile.h"
#include "core/file.h"
#include "core/jsonfile.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include <stdexcept>
#include <string>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::stl;
using namespace wwiv::strings;

// ReSharper disable once CppUnusedIncludeDirective
#include "sdk/net/networks_cereal.h"

namespace wwiv::sdk {

const int Networks::npos; // reserve space.

Networks::Networks(const Config& config)
    : root_directory_(config.root_directory()), datadir_(config.datadir()) {
  if (!config.IsInitialized()) {
    return;
  }

  if (!File::Exists(FilePath(datadir_, NETWORKS_JSON)) &&
      !File::Exists(FilePath(datadir_, NETWORKS_DAT))) {
    // Nothing to do.
    initialized_ = true;
    return;
  }

  initialized_ = Load();
  if (!initialized_) {
    LOG(ERROR) << "Failed to read " << NETWORKS_JSON << " or " << NETWORKS_DAT;
  }
  initialized_ = true;
}

const Network& Networks::at(const std::string& name) const {
  for (const auto& n : networks_) {
    if (iequals(name, n.name)) {
      return n;
    }
  }
  const auto msg = fmt::format("Unable to find network named: {}", name);
  LOG(ERROR) << msg;
  throw std::out_of_range(msg);
}

static std::unique_ptr<Network> create_255_network() {
  auto net = std::make_unique<Network>();
  net->sysnum = static_cast<uint16_t>(-1);
  net->type = network_type_t::wwivnet;
  net->name = "<deleted network #255>";
  net->dir = "DELETED";
  return net;
}

static auto network_255 = create_255_network();

const Network& Networks::at(size_type num) const {
  if (networks_.empty()) {
    return *network_255;
  }
  if (num == 255) {
    // A network num 255 (-1 wrapped at uint8_t boundary) means an
    // invalid network in WWIV.
    return *network_255;
  }
  if (num >= ssize(networks_)) {
    DLOG(FATAL) << "Out of bounds at Networks::at: " << num << ">= size: " << ssize(networks_);
    LOG(ERROR) << "Out of bounds at Networks::at: " << num << ">= size: " << ssize(networks_);
    // A network num 255 (-1 wrapped at uint8_t boundary) means an
    // invalid network in WWIV.
    return *network_255;
  }
  return stl::at(networks_, num);
}

Network& Networks::at(size_type num) {
  if (networks_.empty()) {
    return *network_255;
  }
  if (num == 255) {
    // A network num 255 (-1 wrapped at uint8_t boundary) means an
    // invalid network in WWIV.
    return *network_255;
  }
  if (num >= ssize(networks_)) {
    DLOG(FATAL) << "Out of bounds at Networks::at: " << num << ">= size: " << ssize(networks_);
    LOG(ERROR) << "Out of bounds at Networks::at: " << num << ">= size: " << ssize(networks_);
    // A network num 255 (-1 wrapped at uint8_t boundary) means an
    // invalid network in WWIV.
    return *network_255;
  }
  return stl::at(networks_, num);
}

Network& Networks::at(const std::string& name) {
  for (auto& n : networks_) {
    if (iequals(name, n.name)) {
      return n;
    }
  }
  const auto msg = fmt::format("Unable to find network named: {}", name);
  LOG(ERROR) << msg;
  throw std::out_of_range(msg);
}

std::optional<const Network> Networks::by_uuid(const wwiv::core::uuid_t& uuid) {
  if (uuid.empty()) {
    return std::nullopt;
  }
  for (auto& n : networks_) {
    if (n.uuid == uuid) {
      return {n};
    }
  }

  return std::nullopt;
}

std::optional<const Network> Networks::by_uuid(const std::string& uuid_text) {
  auto o = uuid_t::from_string(uuid_text);
  if (!o) {
    return std::nullopt;
  }
  return by_uuid(o.value());
}

Networks::~Networks() = default;

auto Networks::network_number(const std::string& network_name) const -> size_type {
  auto i = 0;
  for (const auto& n : networks_) {
    if (iequals(network_name, n.name)) {
      return i;
    }
    ++i;
  }
  return npos;
}

bool Networks::contains(const std::string& network_name) const {
  for (const auto& n : networks_) {
    if (iequals(network_name, n.name)) {
      return true;
    }
  }
  return false;
}

std::size_t Networks::size() const noexcept { return networks_.size(); }

bool Networks::empty() const noexcept { return networks_.empty(); }

bool Networks::insert(int n, Network r) {
  if (!insert_at(networks_, n, r)) {
    return false;
  }
  SetNetworkNumbers();
  return true;
}

bool Networks::erase(int n) {
  if (!erase_at(networks_, n)) {
    return false;
  }
  SetNetworkNumbers();
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
  JsonFile json(FilePath(datadir_, NETWORKS_JSON), "networks", networks_);
  if (!json.Load()) {
    return false;
  }
  EnsureNetworksHaveUUID();
  EnsureNetDirAbsolute();
  SetNetworkNumbers();
  return true;
}

bool Networks::LoadFromDat() {
  DataFile<net_networks_rec_disk> file(FilePath(datadir_, NETWORKS_DAT),
                                       File::modeBinary | File::modeReadOnly, File::shareDenyNone);
  if (!file) {
    return false;
  }

  std::vector<net_networks_rec_disk> networks_disk;

  if (!file.ReadVector(networks_disk)) {
    return false;
  }

  std::random_device rd;
  uuid_generator uuid_gen(rd);
  for (const auto& n : networks_disk) {
    Network r = {};
    r.type = static_cast<network_type_t>(n.type);
    r.name = n.name;
    r.dir = n.dir;
    r.sysnum = n.sysnum;
    r.uuid = uuid_gen.generate();
    networks_.emplace_back(r);
  }
  SetNetworkNumbers();
  return true;
}

bool Networks::Save() {
  const auto dat = SaveToDat();
  const auto json = SaveToJSON();

  return dat && json;
}

void Networks::EnsureNetworksHaveUUID() {
  std::random_device rd;
  uuid_generator uuid_gen(rd);
  auto added{false};
  for (auto& n : networks_) {
    if (n.uuid.empty()) {
      n.uuid = uuid_gen.generate();
      added = true;
    }
  }
  if (added) {
    SaveToJSON();
  }
}

void Networks::EnsureNetDirAbsolute() {
  if (root_directory_.empty()) {
    // Nothing to do if we don't know the root
    return;
  }
  for (auto& n : networks_) {
    if (n.dir.is_relative()) {
      // make absolute
      n.dir = FilePath(root_directory_, n.dir);
    }
  }
}

// Set network_number on each network.
void Networks::SetNetworkNumbers() {
  auto nn = 0;
  for (auto& n : networks_) {
    n.network_number_ = nn++;
  }
}

bool Networks::SaveToJSON() {
  JsonFile json(FilePath(datadir_, NETWORKS_JSON), "networks", networks_);
  return json.Save();
}

bool Networks::SaveToDat() {
  std::vector<net_networks_rec_disk> disk;

  for (const auto& from : networks_) {
    net_networks_rec_disk to{};
    to.type = static_cast<uint8_t>(from.type);
    to_char_array(to.name, from.name);
    to_char_array(to.dir, from.dir.string());
    to.sysnum = from.sysnum;
    disk.emplace_back(to);
  }

  DataFile<net_networks_rec_disk> file(FilePath(datadir_, NETWORKS_DAT),
                                       File::modeBinary | File::modeReadWrite |
                                           File::modeCreateFile | File::modeTruncate,
                                       File::shareDenyReadWrite);
  if (!file) {
    return false;
  }
  return file.WriteVector(disk);
}


} // namespace wwiv::sdk
