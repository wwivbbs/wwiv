/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*            Copyright (C)2016-2021, WWIV Software Services              */
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
#include "sdk/fido/fido_address.h"

#include "core/cereal_utils.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include <cctype>
#include <set>
#include <string>

using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::stl;

namespace wwiv::sdk::fido {

template <typename T, typename C, typename I> static T next_int(C& c, I& it, std::set<char> stop) {
  std::string s;
  while (it != std::end(c) && !contains(stop, *it)) {
    if (!std::isdigit(*it)) {
      throw bad_fidonet_address(StrCat("Missing Unexpected nondigit. address: ", c));
    }
    s.push_back(*it++);
  }
  if (it != std::end(c) && contains(stop, *it)) {
    // skip over last
    ++it;
  }
  return static_cast<T>(to_number<int>(s));
}

FidoAddress::FidoAddress(const std::string& address) {
  const auto has_zone = contains(address, ':');
  const auto has_net = contains(address, '/');
  const auto has_point = contains(address, '.');
  const auto has_domain = contains(address, '@');

  if (!has_net) {
    throw bad_fidonet_address(StrCat("Missing Net or Node. Unable to parse address: ", address));
  }
  auto it = std::begin(address);
  if (has_zone) {
    zone_ = next_int<int16_t>(address, it, {':'});
  } else {
    // Per FSL-1002: "If 'ZZ:' is missing then assume 1 as the zone."
    zone_ = 1;
  }
  net_ = next_int<int16_t>(address, it, {'/'});
  node_ = next_int<int16_t>(address, it, {'@', '.'});
  if (has_point) {
    point_ = next_int<int16_t>(address, it, {'@'});
  }
  if (has_domain) {
    domain_ = std::string(it, std::end(address));
  }
}

std::string FidoAddress::as_string() const {
  return as_string(false, true);
}

std::string FidoAddress::as_string(bool include_domain) const {
  return as_string(include_domain, true);
}

std::string FidoAddress::as_string(bool include_domain, bool include_point) const {
  // start off with the minimal things we know.
  auto s = StrCat(zone_, ":", net_, "/", node_);

  if (include_point && point_ > 0) {
    s += StrCat(".", point_);
  }

  if (include_domain && !domain_.empty()) {
    s += StrCat("@", domain_);
  }

  return s;
}

bool FidoAddress::operator<(const FidoAddress& r) const {
  if (zone_ < r.zone_)
    return true;
  if (zone_ > r.zone_)
    return false;

  if (net_ < r.net_)
    return true;
  if (net_ > r.net_)
    return false;

  if (node_ < r.node_)
    return true;
  if (node_ > r.node_)
    return false;

  if (point_ < r.point_)
    return true;
  if (point_ > r.point_)
    return false;
  if (domain_ < r.domain_)
    return true;

  return false;
}

bool FidoAddress::operator==(const FidoAddress& o) const {
  if (zone_ != o.zone_)
    return false;
  if (net_ != o.net_)
    return false;
  if (node_ != o.node_)
    return false;
  if (point_ != o.point_)
    return false;
  if (domain_ != o.domain_)
    return false;
  return true;
}

bool FidoAddress::approx_equals(const FidoAddress& o) const {
  if (zone_ != o.zone_)
    return false;
  if (net_ != o.net_)
    return false;
  if (node_ != o.node_)
    return false;
  if (point_ != o.point_)
    return false;
  if (!domain_.empty() && !o.domain_.empty() && domain_ != o.domain_)
    return false;
  return true;
}

FidoAddress FidoAddress::without_domain() const {
  return FidoAddress(zone_, net_, node_, point_, "");
}

FidoAddress FidoAddress::with_domain(const std::string& domain) const {
  return FidoAddress(zone_, net_, node_, point_, domain);
}

FidoAddress FidoAddress::with_default_domain(const std::string& default_domain) const {
  const auto d = domain_.empty() ? default_domain : domain_;
  return FidoAddress(zone_, net_, node_, point_, d);
}

std::optional<FidoAddress> try_parse_fidoaddr(const std::string& addr, fidoaddr_parse_t p) {
  if (addr.empty()) {
    return std::nullopt;
  }
  try {
    FidoAddress a(addr);
    return {a};
  } catch (const bad_fidonet_address&) {
    if (p == fidoaddr_parse_t::strict) {
      return std::nullopt;
    }
  }

  // Try more lax route.
  auto s = addr;
  auto it = std::begin(s);
  while (it != std::end(s) && !isdigit(*it)) {
    ++it;
  }
  // This removes any non digits from the front
  std::string a2(it, std::end(s));
  StringTrim(&a2);

  if (const auto idx = a2.find(' '); idx != std::string::npos) {
    a2 = a2.substr(0, idx);
    StringTrim(&a2);
  }
  if (a2.empty()) {
    return std::nullopt;
  }
  try {
    FidoAddress a(a2);
    return {a};
  } catch (const bad_fidonet_address&) {
  }
  return std::nullopt;
}

std::string domain_from_address(const std::string& addr) {
  if (auto o = try_parse_fidoaddr(addr)) {
    return o->domain();
  }
  return {};
}

} // namespace wwiv
