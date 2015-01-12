/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.0x                             */
/*             Copyright (C)2014-2015 WWIV Software Services              */
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
#include "core/strings.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/vardec.h"

using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv {
namespace sdk {

const int Networks::npos;  // reserve space.

Networks::Networks(const Config& config) {
  if (!config.IsInitialized()) {
    throw std::invalid_argument("config must be initialized");
  }

  DataFile<net_networks_rec> file(config.datadir(), NETWORKS_DAT, File::modeBinary|File::modeReadOnly, File::shareDenyNone);
  if (!file) {
    return;
  }
  
  const int num_records = file.number_of_records();
  networks_.resize(num_records);
  initialized_ = file.Read(&networks_[0], num_records);
  if (!initialized_) {
    std::clog << "failed to read the expected number of bytes: " << num_records * sizeof(net_networks_rec) << std::endl;
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


}
}
