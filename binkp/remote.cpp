/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2016-2021, WWIV Software Services             */
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
#include "binkp/remote.h"

#include "core/file.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "binkp/binkp_config.h"
#include "sdk/fido/fido_address.h"
#include <string>

using namespace wwiv::core;
using namespace wwiv::sdk::net;
using namespace wwiv::stl;
using namespace wwiv::strings;

namespace wwiv::net {

std::string ftn_address_from_address_list(const std::string& address_list, const std::string& domain) {
  VLOG(1) << "       ftn_address_from_address_list: '" << address_list << "'; domain: " << domain;
  auto v = SplitString(address_list, " ");
  std::string first;
  for (auto s : v) {
    StringTrim(&s);
    VLOG(1) << "       ftn_address_from_address_list(s): '" << s << "'";
    if (ends_with(s, StrCat("@", domain))) {
      if (first.empty()) {
        first = s;
      }
      try {
        // Let's ensure we have a well formed FidoAddress.
        const sdk::fido::FidoAddress a(s);
        // Check for zero zone, node or net.
        if (a.net() == -1 || a.node() == -1 || a.zone() == -1) {
          continue;
        }
      }
      catch (const sdk::fido::bad_fidonet_address& e) {
        LOG(WARNING) << "Caught bad_fidonet_address: " << e.what();
        // Just like above, we don't have a well formed FidoAddress
        // so we keep looping through the list.
        continue;
      }
      return s;
    }
  }
  return first;
}

std::set<sdk::fido::FidoAddress>
ftn_addresses_from_address_list(const std::string& address_list,
                              const std::set<sdk::fido::FidoAddress>& known_addresses) {
  std::set<sdk::fido::FidoAddress> valid_addresses;
  for (const auto& s : SplitString(address_list, " ")) {
    VLOG(2) << "       ftn_addresses_from_address_list: try_parse_fidoaddr: " << s;
    if (auto o = sdk::fido::try_parse_fidoaddr(s)) {
      for (const auto& a : known_addresses) {
        if (o->approx_equals(a)) {
          VLOG(2) << "       ftn_addresses_from_address_list: Adding address: "
                  << o.value().as_string(true, true);
          // Insert the one that has the domain on it.
          if (o->has_domain()) {
            valid_addresses.insert(o.value());
          } else {
            valid_addresses.insert(a);
          }
        }
      }
    }
  }
  return valid_addresses;
}

uint16_t wwivnet_node_number_from_ftn_address(const std::string& address) {
  std::string s = address;
  LOG(INFO) << "wwivnet_node_number_from_ftn_address: '" << s << "'";
  if (starts_with(s, "20000:20000/")) {
    s = s.substr(12);
    s = s.substr(0, s.find('/'));

    if (contains(s, '@')) {
      s = s.substr(0, s.find('@'));
    }
    return to_number<uint16_t>(s);
  }

  return WWIVNET_NO_NODE;
}

std::string fixup_address(const std::string& addr, const Network& net, const std::string& default_domain) {
  if (auto o = sdk::fido::try_parse_fidoaddr(addr)) {
    if (o->has_domain()) {
      return o->as_string(true, true);
    }
    if (net.type == network_type_t::ftn) {
      if (auto no = sdk::fido::try_parse_fidoaddr(net.fido.fido_address); no->has_domain()) {
        return StrCat(o->as_string(false, true), "@", no->domain());
      }
    }
    return StrCat(o->as_string(false, true), "@", default_domain);
  }
  // We don't have a parsable address here.
  return addr;
}

Remote::Remote(BinkConfig* config, bool remote_is_caller, const std::string& expected_remote_addr)
  : config_(config),
  default_domain_(config_->callout_network_name()),
  remote_is_caller_(remote_is_caller),
  expected_remote_node_(expected_remote_addr),
  domain_(default_domain_),
  network_name_(domain_) {

  // When sending, we should be talking to who we wanted to.
  if (!remote_is_caller) {
    const auto& n = config_->networks().at(config_->callout_network_name());
    const auto addr = fixup_address(expected_remote_addr, n, default_domain_);

    VLOG(3) << "*********** REMOTE IS NOT CALLER ****************: " << addr;
    if (auto o = sdk::fido::try_parse_fidoaddr(addr)) {
      ftn_addresses_.insert(o.value());
      if (o->has_domain()) {
        set_domain(o->domain());
      }
    }
    if (config->callout_network().type == network_type_t::wwivnet) {
      wwivnet_node_ = wwivnet_node_number_from_ftn_address(addr);
    }
  }
}

void Remote::set_address_list(const std::string& a) {
  address_list_ = ToStringLowerCase(a);
}

std::string Remote::network_name() const {
  if (!network_name_.empty()) {
    return network_name_;
  }
  VLOG(2) << "       Remote::network_name: returning default domain for network_name: " << default_domain_;
  return default_domain_;
}

std::string Remote::domain() const {
  return domain_;
}

void Remote::set_domain(const std::string& d) {
  VLOG(2) << "       Remote::set_domain: " << d;
  domain_ = ToStringLowerCase(d);
  network_name_ = domain_;

  for (const auto& n : config_->networks().networks()) {
    if (n.type == network_type_t::ftn) {
      if (const auto nd = sdk::fido::try_parse_fidoaddr(n.fido.fido_address)) {
        if (d == nd->domain()) {
          const auto lcnn = ToStringLowerCase(n.name);
          network_name_ = lcnn;
          VLOG(2) << "       Remote::set_domain: Setting Network Name: " << lcnn;
          return;
        }        
      }
    }
  }
}

const Network& Remote::network() const {
  return config_->network(network_name());
}

std::string Remote::ftn_address() const {
  if (ftn_addresses_.empty()) {
    return {};
  }
  return std::begin(ftn_addresses_)->as_string(true, true);
}

void Remote::set_ftn_addresses(const std::set<sdk::fido::FidoAddress>& a) {
  VLOG(1) << "       Remote::set_ftn_addresses; domain: " << domain_ << "; size: " << a.size();
  ftn_addresses_ = a;
  if (!a.empty()) {
    for (const auto& a1 : a) {
      VLOG(1) << "       Remote::set_ftn_addresses: " << a1;
      if (a1.has_domain()) {
        set_domain(a1.domain());
        VLOG(1) << "       Setting domain to: " << domain_;
        break;
      }
    }
  }
}

}
