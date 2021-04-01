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
#include "sdk/fido/nodelist.h"

#include "core/datetime.h"
#include "core/file.h"
#include "core/findfiles.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/printf.h"
#include <set>
#include <string>
#include <utility>

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;
using namespace wwiv::stl;

namespace wwiv::sdk::fido {

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

static bool internet_flag(const std::string& value, const std::string& flag_name, bool& f, std::string& host, uint16_t& port) {
  if (!contains(value, ':')) return false;
  auto parts = SplitString(value, ":");
  if (parts.size() > 3) return false;

  if (parts.front() == flag_name) {
    f = true;
    if (parts.size() > 1) {
      port = to_number<uint16_t>(parts.back());
    }
    if (parts.size() == 3) {
      // flag:host:port
      host = at(parts, 1);
    }
    return true;
  }
  return false;
}

static bool bool_flag(const std::string& value, const std::string& flag_name, bool& f, std::string& fs) {
  if (!contains(value, ':')) return false;
  std::vector<std::string> parts = SplitString(value, ":");
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

static std::string ToSpaces(const std::string& orig) {
  std::string s(orig);
  std::replace(std::begin(s), std::end(s), '_', ' ');
  return s;
}

//static 
std::optional<NodelistEntry> NodelistEntry::ParseDataLine(const std::string& data_line) {
  if (data_line.front() == ';') {
    return std::nullopt;
  }

  auto parts = SplitString(data_line, ",");
  if (parts.size() < 6) {
    return std::nullopt;
  }

  NodelistEntry e{};
  auto it = parts.cbegin();
  if (data_line.front() == ',') {
    // We have no 1st field, default the keyword and skip the iterator.
    e.keyword_ = NodelistKeyword::node;
  } else {
    e.keyword_ = to_keyword(*it++);
  }
  e.number_ = to_number<uint16_t>(*it++);
  e.name_ = ToSpaces(*it++);
  e.location_ = ToSpaces(*it++);
  e.sysop_name_ = ToSpaces(*it++);
  e.phone_number_ = *it++;
  if (it != parts.end()) {
    e.baud_rate_ = to_number<unsigned int>(*it++);
  }

  while (it != parts.end()) {
    const auto& f = *it++;
    if (bool_flag(f, "CM", e.cm_)) continue;
    if (bool_flag(f, "ICM", e.icm_)) continue;
    if (bool_flag(f, "MO", e.mo_)) continue;
    if (bool_flag(f, "LO", e.lo_)) continue;
    if (bool_flag(f, "MN", e.mn_)) continue;

    if (bool ignore; bool_flag(f, "INA", ignore, e.hostname_)) continue;
    if (internet_flag(f, "IBN", e.binkp_, e.binkp_hostname_, e.binkp_port_)) continue;
    if (internet_flag(f, "ITN", e.telnet_, e.telnet_hostname_, e.telnet_port_)) continue;
    if (internet_flag(f, "IVN", e.vmodem_, e.vmodem_hostname_, e.vmodem_port_)) continue;

    // TODO(rushfan): Handle X? flags.
  }
  if (e.binkp_) {
    if (e.binkp_port_ == 0) e.binkp_port_ = 24554;
    if (e.binkp_hostname_.empty()) e.binkp_hostname_ = e.hostname();
  }
  if (e.telnet_) {
    if (e.telnet_port_ == 0) e.telnet_port_ = 24554;
    if (e.telnet_hostname_.empty()) e.telnet_hostname_ = e.hostname();
  }
  if (e.vmodem_) {
    if (e.vmodem_port_ == 0) e.vmodem_port_ = 24554;
    if (e.vmodem_hostname_.empty()) e.vmodem_hostname_ = e.hostname();
  }

  return {e};
}

Nodelist::Nodelist(const std::filesystem::path& path, std::string domain) 
  : domain_(std::move(domain)), initialized_(Load(path)) {}

Nodelist::Nodelist(const std::vector<std::string>& lines, std::string domain) 
  : domain_(std::move(domain)), initialized_(Load(lines)) {}

bool Nodelist::HandleLine(const std::string& line, uint16_t& zone, uint16_t& region, uint16_t& net, uint16_t& hub) {
  if (line.empty()) return true;
  if (line.front() == ';') {
    // TODO(rushfan): Do we care to do anything with this?
    return true;
  }
  if (auto e = NodelistEntry::ParseDataLine(line)) {
    switch (e->keyword()) {
    case NodelistKeyword::down:
      // let's skip these for now
    break;
    case NodelistKeyword::host:
    {
      net = e->number();
    } break;
    case NodelistKeyword::hub:
    {
      hub = e->number();
    } break;
    // Let's include pvt nodes since you can still route
    // to them.
    case NodelistKeyword::pvt:
    case NodelistKeyword::node:
    {
      FidoAddress address(zone, net, e->number(), 0, domain_);
      e->address(address);
      if (zone != 0 && net != 0) {
        // skip malformed entries.
        entries_.emplace(address, e.value());
      }
    } break;
    case NodelistKeyword::region:
    {
      region = e->number();
      // also use region for the net since we're addressable
      // as zone:region/node
      net = e->number();
      hub = 0;
    } break;
    case NodelistKeyword::zone:
    {
      zone = e->number();
      region = hub = net = 0;
    } break;
    }
    return true;
  }
  return false;
}

bool Nodelist::Load(const std::filesystem::path& path) {
  TextFile f(path, "rt");
  if (!f) {
    return false;
  }
  const auto lines = f.ReadFileIntoVector();
  return Load(lines);
}

bool Nodelist::Load(const std::vector<std::string>& lines) {
  if (lines.empty()) return false;
  // ReSharper disable CppTooWideScope
  uint16_t zone = 0, region = 0, net = 0, hub = 0;
  // ReSharper restore CppTooWideScope
  for (const auto& raw_line : lines) {
    auto line = StringTrim(raw_line);
    HandleLine(line, zone, region, net, hub);
  }
  return true;
}

const NodelistEntry& Nodelist::entry(const FidoAddress& a) const {
  if (stl::contains(entries_, a)) {
    return at(entries_, a);
  }
  if (a.has_domain()) {
    if (stl::contains(entries_, a.without_domain())) {
      return at(entries_, a.without_domain());
    }
  } else {
    if (stl::contains(entries_, a.with_domain(domain_))) {
      return at(entries_, a.with_domain(domain_));
    }
  }
  const auto s = fmt::format("Nodelist::entry: key missing: {} ", a.as_string(true, true));
  DLOG(FATAL) << s << ": at: \r\n" << os::stacktrace();
  throw std::out_of_range(s);
}

bool Nodelist::contains(const FidoAddress& a) const {
  if (stl::contains(entries_, a)) {
    return true;
  }
  if (a.has_domain()) {
    if (stl::contains(entries_, a.without_domain())) {
      return true;
    }
  } else {
    if (stl::contains(entries_, a.with_domain(domain_))) {
      return true;
    }
  }
  return false;
}

std::vector<NodelistEntry> Nodelist::entries(uint16_t zone, uint16_t net) const {
  std::vector<NodelistEntry> entries;
  for (const auto& e : entries_) {
    if (e.first.zone() == zone && e.first.net() == net) {
      entries.push_back(e.second);
    }
  }
  return entries;
}

std::vector<NodelistEntry> Nodelist::entries(uint16_t zone) const {
  std::vector<NodelistEntry> entries;
  for (const auto& e : entries_) {
    if (e.first.zone() == zone) {
      entries.push_back(e.second);
    }
  }
  return entries;
}

std::vector<uint16_t> Nodelist::zones() const {
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

std::vector<uint16_t> Nodelist::nets(uint16_t zone) const {
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

std::vector<uint16_t> Nodelist::nodes(uint16_t zone, uint16_t net) const {
  std::vector<uint16_t> nodes;
  for (const auto& e : entries_) {
    if (e.first.zone() == zone && e.first.net() == net) {
      nodes.emplace_back(e.first.node());
    }
  }
  return nodes;
}

const NodelistEntry* Nodelist::entry(uint16_t zone, uint16_t net, uint16_t node) {
  const FidoAddress a(zone, net, node, 0, "");
  if (!stl::contains(entries_, a)) {
    return nullptr;
  }
  return &entries_.at(a);
}

bool Nodelist::has_zone(int zone) const noexcept {
  return std::any_of(std::begin(entries_), std::end(entries_), 
    [zone](const auto& p) { return p.first.zone() == zone; });
}

static int year_of(time_t t) {
  const auto dt = DateTime::from_time_t(t);
  return dt.year();
}

int extension_number(const std::string& fn) {
  if (!contains(fn, '.')) {
    return 0;
  }

  const auto num = fn.substr(fn.find_last_of('.') + 1);
  return to_number<int>(num);
}

static std::string latest_extension(const std::map<int, int>& ey) {
  auto highest_year = 0;
  for (const auto& y : ey) {
    if (y.second > highest_year) {
      highest_year = y.second;
    }
  }

  for (auto r = ey.rbegin(); r != ey.rend(); ++r) {
    if ((*r).second == highest_year) {
      return fmt::sprintf("%03d", (*r).first);
    }
  }
  return "000";
}

// static
std::string Nodelist::FindLatestNodelist(const std::filesystem::path& dir, const std::string& base) {
  const auto filespec = FilePath(dir, StrCat(base, ".*"));
  std::map<int, int> extension_year;
  FindFiles fnd(filespec, FindFiles::FindFilesType::files);
  for (const auto& ff : fnd) {
    const auto fn = FilePath(dir, ff.name);
    extension_year.emplace(extension_number(fn.filename().string()),
                           year_of(File::last_write_time(fn)));
  }

  const auto ext = latest_extension(extension_year);
  return StrCat(base, ".", ext);
}


}  // namespace

