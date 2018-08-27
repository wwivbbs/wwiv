/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2014-2017, WWIV Software Services             */
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
#include "core/stl.h"
#include "core/strings.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/vardec.h"

using cereal::make_nvp;
using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::strings;

#include "sdk/networks_cereal.h"

namespace wwiv {
namespace sdk {

const int Networks::npos;  // reserve space.

Networks::Networks(const Config& config) : datadir_(config.datadir()) {
  if (!config.IsInitialized()) {
    return;
  }

  {
    DataFile<net_networks_rec_disk> file_dat(FilePath(datadir_, NETWORKS_DAT),
                                             File::modeBinary | File::modeReadOnly,
                                             File::shareDenyNone);
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
    if (iequals(name.c_str(), n.name)) {
      return n;
    }
  }
  throw std::out_of_range(StrCat("Unable to find network named: ", name));
}

net_networks_rec& Networks::at(const std::string& name) {
  for (auto& n : networks_) {
    if (iequals(name.c_str(), n.name)) {
      return n;
    }
  }
  throw std::out_of_range(StrCat("Unable to find network named: ", name));
}

Networks::~Networks() {}

 auto Networks::network_number(const std::string& network_name) const -> size_type {
  Networks::size_type i = 0;
  for (const auto& n : networks_) {
    if (iequals(network_name.c_str(), n.name)) {
      return i;
    }
    ++i;
  }
  return npos;
}

bool Networks::contains(const std::string& network_name) const {
  for (const auto& n : networks_) {
    if (iequals(network_name.c_str(), n.name)) {
      return true;
    }
  }
  return false;
}

// TODO(rushfan): Since should we make this algo available 
// in wwiv::sdk since we do it on all containers often.
bool Networks::insert(std::size_t n, net_networks_rec r) {
  return insert_at(networks_, n, r);
}

bool Networks::erase(std::size_t n) {
  return erase_at(networks_, n);
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
  DataFile<net_networks_rec_disk> file(FilePath(datadir_, NETWORKS_DAT),
                                       File::modeBinary | File::modeReadOnly, File::shareDenyNone);
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

  DataFile<net_networks_rec_disk> file(FilePath(datadir_, NETWORKS_DAT),
                                       File::modeBinary | File::modeReadWrite |
                                       File::modeCreateFile | File::modeTruncate,
                                       File::shareDenyReadWrite);
  if (!file) {
    return false;
  }
  return file.WriteVector(disk);
}

}
}
