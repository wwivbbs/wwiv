/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*               Copyright (C)2016 WWIV Software Services                 */
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
#include "sdk/fido/nodelist.h"

#include <set>
#include <string>

#include "core/file.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"

using std::string;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::stl;

namespace wwiv {
namespace sdk {
namespace fido {

// Cribbed from networkb/net_util.h
// TODO: Move it somewhere here in SDK.
template <typename C, typename I>
static std::string get_field(const C& c, I& iter, std::set<char> stop, std::size_t max) {
  // No need to continue if we're already at the end.
  if (iter == c.end()) {
    return "";
  }

  const auto begin = iter;
  std::size_t count = 0;
  while (stop.find(*iter) == std::end(stop) && ++count < max && iter != c.end()) {
    iter++;
  }
  std::string result(begin, iter);

  // Stop over stop chars
  while (iter != c.end()
    && stop.find(*iter) != std::end(stop)) {
    iter++;
  }

  return result;
}

static NodelistKeyword to_keyword(const std::string& orig) {
  if (orig.empty()) {
    return NodelistKeyword::node;
  }
  string k(orig);
  StringLowerCase(&k);

  if (k == "down") return NodelistKeyword::down;
  if (k == "host") return NodelistKeyword::host;
  if (k == "hub") return NodelistKeyword::hub;
  if (k == "pvt") return NodelistKeyword::pvt;
  if (k == "region") return NodelistKeyword::region;
  if (k == "zone") return NodelistKeyword::zone;

  return NodelistKeyword::node;
}

static bool bool_flag(const std::string& value, const std::string& flag_name, bool& f) {
  if (value == flag_name) {
    f = true;
    return true;
  }
  return false;
}

struct internet_flag {
  string host;
  uint16_t port;
};

static bool internet_flag(const std::string& value, const std::string& flag_name, bool& f, string& host, uint16_t& port) {
  if (!contains(value, ':')) return false;
  vector<string> parts = SplitString(value, ":");
  if (parts.size() > 3) return false;

  if (parts.front() == flag_name) {
    f = true;
    if (parts.size() > 1) {
      port = StringToUnsignedShort(parts.back());
    }
    if (parts.size() == 3) {
      // flag:host:port
      host = parts.at(1);
    }
    return true;
  }
  return false;
}

static bool bool_flag(const std::string& value, const std::string& flag_name, bool& f, string& fs) {
  if (!contains(value, ':')) return false;
  vector<string> parts = SplitString(value, ":");
  if (parts.size() > 2) return false;

  if (parts.front() == flag_name) {
    f = true;
    if (parts.size() > 1) {
      fs = parts.back();
      StringTrim(&fs);
    }
    return true;
  }
  return false;
}

//static 
bool NodelistEntry::ParseDataLine(const std::string& data_line, NodelistEntry& e) {
  if (data_line.front() == ';') {
    return false;
  }

  vector<string> parts = SplitString(data_line, ",");
  for (auto& p : parts) {
    StringTrim(&p);
  }

  if (parts.size() < 8) {
    return false;
  }

  auto it = parts.begin();
  if (data_line.front() == ',') {
    // We have no 1st field, default the keyword and skip the iterator.
    e.keyword_ = NodelistKeyword::node;
  } else {
    e.keyword_ = to_keyword(*it++);
  }
  e.number_ = StringToUnsignedShort(*it++);
  e.name_ = *it++;
  e.location_ = *it++;
  e.sysop_name_ = *it++;
  e.phone_number_ = *it++;
  e.baud_rate_ = StringToUnsignedInt(*it++);

  while (it != parts.end()) {
    const auto f = *it++;
    if (bool_flag(f, "CM", e.cm_)) continue;
    if (bool_flag(f, "ICM", e.icm_)) continue;
    if (bool_flag(f, "MO", e.mo_)) continue;
    if (bool_flag(f, "LO", e.lo_)) continue;
    if (bool_flag(f, "MN", e.mn_)) continue;

    bool ignore;
    if (bool_flag(f, "INA", ignore, e.hostname_)) continue;
    if (internet_flag(f, "IBN", e.binkp_, e.binkp_hostname_, e.binkp_port_)) continue;
    if (internet_flag(f, "ITN", e.telnet_, e.telnet_hostname_, e.telnet_port_)) continue;
    if (internet_flag(f, "IVN", e.vmodem_, e.vmodem_hostname_, e.vmodem_port_)) continue;

    // TODO(rushfan): Handle X? flags.
  }
  if (e.binkp_) {
    if (e.binkp_port_ == 0) e.binkp_port_ = 24554;
    if (e.binkp_hostname_.empty()) e.binkp_hostname_ = e.hostname_;
  }
  if (e.telnet_) {
    if (e.telnet_port_ == 0) e.telnet_port_ = 24554;
    if (e.telnet_hostname_.empty()) e.telnet_hostname_ = e.hostname_;
  }
  if (e.vmodem_) {
    if (e.vmodem_port_ == 0) e.vmodem_port_ = 24554;
    if (e.vmodem_hostname_.empty()) e.vmodem_hostname_ = e.hostname_;
  }

  return true;
}

Nodelist::Nodelist(const std::string& path) : path_(path) {}

Nodelist::~Nodelist() {}

bool Nodelist::Load() {
  TextFile f(path_, "rt");
  if (!f) {
    return false;
  }
  string line;
  uint16_t zone = 0, region = 0, net = 0, hub = 0;
  while (f.ReadLine(&line)) {
    StringTrim(&line);
    if (line.empty()) continue;
    if (line.front() == ';') {
      // TODO(rushfan): Do we care to do anything with this?
      VLOG(3) << line;
      continue;
    }
    NodelistEntry e{};
    if (NodelistEntry::ParseDataLine(line, e)) {
      switch (e.keyword_) {
      case NodelistKeyword::down:
      {
        // let's skip these for now
      } break;
      case NodelistKeyword::host:
      {
        net = e.number_;
      } break;
      case NodelistKeyword::hub:
      {
        hub = e.number_;
      } break;
      case NodelistKeyword::node:
      {
        FidoAddress address(zone, net, e.number_, 0, "");
        VLOG(4) << address << "; " << line;
        entries_.emplace(address, e);
      } break;
      case NodelistKeyword::pvt:
      {
        // skip
      } break;
      case NodelistKeyword::region:
      {
        region = e.number_;
        hub = net = 0;
      } break;
      case NodelistKeyword::zone:
      {
        zone = e.number_;
        region = hub = net = 0;
      } break;
      default:
        break;
      }
    }
  }
  return true;
}

}  // namespace fido
}  // namespace sdk
}  // namespace wwiv

