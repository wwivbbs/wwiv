/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*            Copyright (C)2016-2017, WWIV Software Services              */
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
#include "core/findfiles.h"

using std::string;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::stl;

namespace wwiv {
namespace sdk {
namespace fido {

static NodelistKeyword to_keyword(const std::string& k) {
  if (k.empty()) {
    return NodelistKeyword::node;
  }

  if (k == "Down") return NodelistKeyword::down;
  if (k == "Host") return NodelistKeyword::host;
  if (k == "Hub") return NodelistKeyword::hub;
  if (k == "Pvt") return NodelistKeyword::pvt;
  if (k == "Region") return NodelistKeyword::region;
  if (k == "Zone") return NodelistKeyword::zone;

  return NodelistKeyword::node;
}

static inline bool bool_flag(const std::string& value, const std::string& flag_name, bool& f) {
  if (value == flag_name) {
    f = true;
    return true;
  }
  return false;
}

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
//      StringTrim(&fs);
    }
    return true;
  }
  return false;
}

static string ToSpaces(const std::string& orig) {
  string s(orig);
  std::replace(std::begin(s), std::end(s), '_', ' ');
  return s;
}

//static 
bool NodelistEntry::ParseDataLine(const std::string& data_line, NodelistEntry& e) {
  if (data_line.front() == ';') {
    return false;
  }

  vector<string> parts = SplitString(data_line, ",");
  if (parts.size() < 6) {
    return false;
  }

  auto it = parts.cbegin();
  if (data_line.front() == ',') {
    // We have no 1st field, default the keyword and skip the iterator.
    e.keyword_ = NodelistKeyword::node;
  } else {
    e.keyword_ = to_keyword(*it++);
  }
  e.number_ = StringToUnsignedShort(*it++);
  e.name_ = ToSpaces(*it++);
  e.location_ = ToSpaces(*it++);
  e.sysop_name_ = ToSpaces(*it++);
  e.phone_number_ = *it++;
  if (it != parts.end()) {
    e.baud_rate_ = StringToUnsignedInt(*it++);
  }

  while (it != parts.end()) {
    const auto& f = *it++;
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

Nodelist::Nodelist(const std::string& path) 
  : entries_(), initialized_(Load(path)) {}

Nodelist::Nodelist(const std::vector<std::string>& lines) 
  : entries_(), initialized_(Load(lines)) {}

Nodelist::~Nodelist() {}

bool Nodelist::HandleLine(const string& line, uint16_t& zone, uint16_t& region, uint16_t& net, uint16_t& hub) {
  if (line.empty()) return true;
  if (line.front() == ';') {
    // TODO(rushfan): Do we care to do anything with this?
    return true;
  }
  NodelistEntry e{};
  if (!NodelistEntry::ParseDataLine(line, e)) {
    return false;
  }
  switch (e.keyword_) {
  case NodelistKeyword::down:
    // let's skip these for now
  break;
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
    e.address_ = address;
    if (zone != 0 && net != 0) {
      // skip malformed entries.
      entries_.emplace(address, e);
    }
  } break;
  case NodelistKeyword::pvt:
    // skip
  break;
  case NodelistKeyword::region:
  {
    region = e.number_;
    // also use region for the net since we're addressable
    // as zone:region/node.
    net = e.number_;
    hub = 0;
  } break;
  case NodelistKeyword::zone:
  {
    zone = e.number_;
    region = hub = net = 0;
  } break;
  }

  return true;
}

bool Nodelist::Load(const std::string& path) {
  TextFile f(path, "rt");
  if (!f) {
    return false;
  }
  string line;
  uint16_t zone = 0, region = 0, net = 0, hub = 0;
  while (f.ReadLine(&line)) {
    StringTrim(&line);
    HandleLine(line, zone, region, net, hub);
  }
  return true;
}

bool Nodelist::Load(const std::vector<std::string>& lines) {
  if (lines.empty()) return false;
  uint16_t zone = 0, region = 0, net = 0, hub = 0;
  for (const auto& raw_line : lines) {
    string line(raw_line);
    StringTrim(&line);
    HandleLine(line, zone, region, net, hub);
  }
  return true;
}

const std::vector<NodelistEntry> Nodelist::entries(uint16_t zone, uint16_t net) const {
  std::vector<NodelistEntry> entries;
  for (const auto& e : entries_) {
    if (e.first.zone() == zone && e.first.net() == net) {
      entries.push_back(e.second);
    }
  }
  return entries;
}

const std::vector<NodelistEntry> Nodelist::entries(uint16_t zone) const {
  std::vector<NodelistEntry> entries;
  for (const auto& e : entries_) {
    if (e.first.zone() == zone) {
      entries.push_back(e.second);
    }
  }
  return entries;
}

const std::vector<uint16_t> Nodelist::zones() const {
  std::set<uint16_t> s;
  for (const auto& e : entries_) {
    s.emplace(e.first.zone());
  }
  std::vector<uint16_t> zones;
  for (const auto& n : s) {
    zones.emplace_back(n);
  }
  return zones;
}

const std::vector<uint16_t> Nodelist::nets(uint16_t zone) const {
  std::set<uint16_t> s;
  for (const auto& e : entries_) {
    if (e.first.zone() == zone) {
      s.emplace(e.first.net());
    }
  }
  std::vector<uint16_t> nets;
  for (const auto& n : s) {
    nets.emplace_back(n);
  }
  return nets;
}

const std::vector<uint16_t> Nodelist::nodes(uint16_t zone, uint16_t net) const {
  std::vector<uint16_t> nodes;
  for (const auto& e : entries_) {
    if (e.first.zone() == zone && e.first.net() == net) {
      nodes.emplace_back(e.first.node());
    }
  }
  return nodes;
}

const NodelistEntry* Nodelist::entry(uint16_t zone, uint16_t net, uint16_t node) {
  FidoAddress a(zone, net, node, 0, "");
  if (!wwiv::stl::contains(entries_, a)) {
    return nullptr;
  }
  return &entries_.at(a);
}

static int year_of(time_t c) {
  struct tm* t = localtime(&c);
  return t->tm_year;
}

int extension_number(const std::string& fn) {
  if (!contains(fn, '.')) {
    return 0;
  }

  string num = fn.substr(fn.find_last_of('.') + 1);
  return StringToInt(num);
}

static std::string latest_extension(const std::map<int, int>& ey) {
  int highest_year = 0;
  for (const auto& y : ey) {
    if (y.second > highest_year) {
      highest_year = y.second;
    }
  }

  for (auto r = ey.rbegin(); r != ey.rend(); r++) {
    if ((*r).second == highest_year) {
      return StringPrintf("%03d", (*r).first);
    }
  }
  return "000";
}

// static
std::string Nodelist::FindLatestNodelist(const std::string& dir, const std::string& base) {
  const string filespec = StrCat(FilePath(dir, base), ".*");
  std::map<int, int> extension_year;
  FindFiles fnd(filespec, FindFilesType::files);
  for (const auto& ff : fnd) {
    File f(dir, ff.name);
    extension_year.emplace(extension_number(f.GetName()), year_of(f.creation_time()));
  }

  string ext = latest_extension(extension_year);
  return StrCat(base, ".", ext);
}


}  // namespace fido
}  // namespace sdk
}  // namespace wwiv

