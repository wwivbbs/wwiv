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

#include "core/datafile.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/vardec.h"

using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv {
namespace sdk {

const int Networks::npos;  // reserve space.

Networks::Networks(const Config& config) : datadir_(config.datadir()) {
  if (!config.IsInitialized()) {
    return;
  }

  {
    DataFile<net_networks_rec_disk> file(datadir_, NETWORKS_DAT, File::modeBinary | File::modeReadOnly, File::shareDenyNone);
    if (!file) {
      return;
    }
  }

  initialized_ = Load();
  if (!initialized_) {
    LOG(ERROR) << "Failed to read " << NETWORKS_DAT;
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
    r.type = n.type;
    strcpy(r.name, n.name);
    strcpy(r.dir, n.dir);
    r.sysnum = n.sysnum;
    networks_.emplace_back(r);
  }
  return true;
}

bool Networks::Save() {
  std::vector<net_networks_rec_disk> disk;

  for (const auto& from : networks_) {
    net_networks_rec_disk to{};
    to.type = from.type;
    strcpy(to.name, from.name);
    strcpy(to.dir, from.dir);
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
