/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                Copyright (C)2016, WWIV Software Services               */
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
/*                                                                        */
/**************************************************************************/
#include "networkb/remote.h"

#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "core/file.h"
#include "core/md5.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"


using std::string;
using std::vector;

using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::strings;

namespace wwiv {
namespace net {

std::string ftn_address_from_address_list(const string& network_list, const string& network_name) {
  VLOG(1) << "       ftn_address_from_address_list: '" << network_list << "'; network_name: " << network_name;
  vector<string> v = SplitString(network_list, " ");
  for (auto s : v) {
    StringTrim(&s);
    VLOG(1) << "       ftn_address_from_address_list(s): '" << s << "'";
    if (ends_with(s, StrCat("@", network_name))) {
      return s;
    }
  }
  return "";
}

// Returns the single network name from the address list (only used when we
// are in answering mode, where a single address is presented) or the empty
// string if no address is present.
string network_name_from_single_address(const string& network_list) {
  vector<string> v = SplitString(network_list, " ");
  if (v.empty()) {
    return "";
  }
  string s = v.front();
  string::size_type index = s.find_last_of("@");
  if (index == string::npos) {
    return "";
  }
  return s.substr(index + 1);
}

int wwivnet_node_number_from_ftn_address(const string& address) {
  string s = address;
  if (starts_with(s, "20000:20000/")) {
    s = s.substr(12);
    s = s.substr(0, s.find('/'));

    if (contains(s, '@')) {
      s = s.substr(0, s.find('@'));
    }
    return StringToInt(s);
  }

  return -1;
}

Remote::Remote(BinkConfig* config, bool caller, const std::string& expected_remote_node)
  : config_(config), is_caller_(caller), expected_remote_node_(expected_remote_node),
  default_network_name_(config_->callout_network_name()) {
  network_name_ = default_network_name_;

  // When sending, we should be talking to who we wanted to.
  if (caller) {
    ftn_address_ = expected_remote_node;
    wwivnet_node_ = wwivnet_node_number_from_ftn_address(ftn_address_);
  }
}

void Remote::set_address_list(const std::string& a) {
  address_list_ = a;
  StringLowerCase(&address_list_);

  if (!is_caller_) {
    auto name = network_name_from_single_address(address_list_);
    if (!name.empty()) {
      network_name_ = name;
    }

    ftn_address_ = ftn_address_from_address_list(address_list_, network_name_);
    if (network().type == network_type_t::wwivnet) {
      wwivnet_node_ = wwivnet_node_number_from_ftn_address(ftn_address_);
    }
  }
}

const std::string Remote::network_name() const {
  if (!network_name_.empty()) {
    return network_name_;
  }
  return default_network_name_;
}

const net_networks_rec& Remote::network() const {
  return config_->network(network_name());
}



}
}
