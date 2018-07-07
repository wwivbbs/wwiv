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
#include "sdk/fido/fido_util.h"

#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "core/command_line.h"
#include "core/file.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "core/findfiles.h"
#include "core/datetime.h"
#include "sdk/fido/fido_address.h"

using std::string;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::sdk::fido;
using namespace wwiv::stl;
using namespace wwiv::strings;

namespace wwiv {
namespace sdk {
namespace fido {

constexpr char CZ = 26;


// We use DDHHMMSS like SBBSECHO v2 does.
std::string packet_name(time_t now) {

  auto dt = DateTime::from_time_t(now);
  auto buf = dt.to_string("%d%H%M%S");
  return StrCat(buf, ".pkt");
}

std::string bundle_name(const wwiv::sdk::fido::FidoAddress& source, const wwiv::sdk::fido::FidoAddress& dest, const std::string& extension) {
  int16_t net = source.net() - dest.net();
  uint16_t node = source.node() - dest.node();

  return StringPrintf("%04.4x%04.4x.%s", net, node, extension.c_str());
}

std::string net_node_name(const wwiv::sdk::fido::FidoAddress& dest, const std::string& extension) {
  return StringPrintf("%04.4x%04.4x.%s", dest.net(), dest.node(), extension.c_str());
}

std::string flo_name(const wwiv::sdk::fido::FidoAddress& dest, fido_bundle_status_t status) {
  string extension = "flo";
  if (status != fido_bundle_status_t::unknown) {
    extension[0] = static_cast<char>(status);
  }
  return net_node_name(dest, extension);
}

std::string bundle_name(const wwiv::sdk::fido::FidoAddress& source, const wwiv::sdk::fido::FidoAddress& dest, int dow, int bundle_number) {
  return bundle_name(source, dest, dow_extension(dow, bundle_number));
}

std::vector<std::string> dow_prefixes() {
  static const std::vector<string> dow = { "su", "mo", "tu", "we", "th", "fr", "sa", "su" };
  return dow;
}

std::string dow_extension(int dow_num, int bundle_number) {
  // TODO(rushfan): Should we assert of bundle_number > 25 (0-9=10, + a-z=26 = 0-35)

  static const std::vector<string> dow = {"su", "mo", "tu", "we", "th", "fr", "sa", "su"};
  auto ext = dow.at(dow_num);
  auto c = static_cast<char>('0' + bundle_number);
  if (bundle_number > 9) {
    c = static_cast<char>('a' + bundle_number - 10);
  }
  ext.push_back(c);
  return ext;
}

bool is_bundle_file(const std::string& name) {
  static const std::vector<string> dow = { "su", "mo", "tu", "we", "th", "fr", "sa" };
  auto dot = name.find_last_of('.');
  if (dot == string::npos) { return false; }
  auto ext = name.substr(dot + 1);
  if (ext.length() != 3) { return false; }
  ext.pop_back();
  StringLowerCase(&ext);
  return contains(dow, ext);
}

bool is_packet_file(const std::string& name) {
  auto dot = name.find_last_of('.');
  if (dot == string::npos) {
    return false;
  }
  auto ext = name.substr(dot + 1);
  if (ext.length() != 3) {
    return false;
  }
  ext.pop_back();
  StringLowerCase(&ext);
  return ext == "pkt";
}

static string control_file_extension(fido_bundle_status_t status) {
  string s = "flo";
  s[0] = static_cast<char>(status);
  return s;
}

std::string control_file_name(const wwiv::sdk::fido::FidoAddress& dest, fido_bundle_status_t status) {
  auto net = dest.net();
  auto node = dest.node();

  const auto ext = control_file_extension(status);
  return StringPrintf("%04.4x%04.4x.%s", net, node, ext.c_str());
}

// 10 Nov 16  21:15:45
std::string daten_to_fido(time_t t) {
  auto dt = DateTime::from_time_t(t);
  return dt.to_string("%d %b %y  %H:%M:%S");
}

// Format: 10 Nov 16  21:15:45
daten_t fido_to_daten(std::string d) {
  try {
    vector<string> months = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    std::stringstream stream(d);
    auto now = time_t_now();
    struct tm* t = localtime(&now);
    stream >> t->tm_mday;
    string mon_str;
    stream >> mon_str;
    if (!contains(months, mon_str)) {
      // Unparsable date. return now.
      return daten_t_now();
    }
    t->tm_mon = std::distance(months.begin(), std::find(months.begin(), months.end(), mon_str));
    int year;
    stream >> year;
    year %= 100;
    // This will work until 2099.
    t->tm_year = 100 + year;

    string hms;
    stream >> hms;
    vector<string> parts = SplitString(hms, ":");
    t->tm_hour = to_number<int>(parts.at(0)) - 1;
    t->tm_min = to_number<int>(parts.at(1));
    t->tm_sec = to_number<int>(parts.at(2));

    auto result = mktime(t);
    if (result < 0) {
      // Error.. return now so we don't blow stuff up.
      result = time_t_now();
    }
    return time_t_to_daten(result);
  } catch (const std::exception& e) {
    LOG(ERROR) << "exception in fido_to_daten('" << d << "'): " << e.what();
    return daten_t_now();
  }
}

std::string to_net_node(const wwiv::sdk::fido::FidoAddress& a) {
  return StrCat(a.net(), "/", a.node());
}

std::string to_zone_net_node(const wwiv::sdk::fido::FidoAddress& a) {
  return StrCat(a.zone(), ":", to_net_node(a));
}

std::string to_zone_net_node_point(const wwiv::sdk::fido::FidoAddress& a) {
  return StrCat(to_zone_net_node(a),  ".", a.point());
}

std::vector<std::string> split_message(const std::string& s) {
  string temp(s);
  temp.erase(std::remove(temp.begin(), temp.end(), 10), temp.end());
  temp.erase(std::remove(temp.begin(), temp.end(), '\x8d'), temp.end());
  return SplitString(temp, "\r");
}

/**
 * \brief Type of control line. Control-A Kludge or non-control-a like AREA, or none.
 */
enum class FtnControlLineType {
  control_a,
  plain_control_line,
  none
};

static FtnControlLineType determine_kludge_line_type(const std::string& line) {
  if (line.empty()) {
    return FtnControlLineType::none;
  }
  if (line.front() == 0x01) {
    return line.size() > 1 ? FtnControlLineType::control_a : FtnControlLineType::none;
  }
  if (starts_with(line, "AREA:")
    || starts_with(line, "SEEN-BY: ")) {
    return FtnControlLineType::plain_control_line;
  }
  return FtnControlLineType::none;
}

string FidoToWWIVText(const string& ft, bool convert_control_codes) {
  // Split text into lines and process one at a time
  // this is easier to handle control lines, etc.
  string wt;
  for (const auto& sc : ft) {
    const auto c = static_cast<unsigned char>(sc);
    if (c == 0x8d) {
      // FIDOnet style Soft CR. Convert to CR
      wt.push_back(13);
    } else if (c == 10) {
      // NOP. We're skipping LF's so we can split on CR
    } else {
      wt.push_back(c);
    }
  }

  auto lines = SplitString(wt, "\r", false);
  wt.clear();

  if (!convert_control_codes) {
    for (const auto& line : lines) {
      wt.append(line).append("\r\n");
    }
    return wt;
  }

  for (auto line : lines) {
    if (line.empty()) {
      wt.append("\r\n");
      continue;
    }

    // According to FSC-0068. Kludge lines are not normally displayed
    // when reading messages.
    switch (determine_kludge_line_type(line)) {
    case FtnControlLineType::control_a: {
      line = line.substr(1);
      wt.push_back(4);
      wt.push_back('0');
    } break;
    case FtnControlLineType::plain_control_line: {
      wt.push_back(4);
      wt.push_back('0');
    } break;
    case FtnControlLineType::none:
    default:
    break;
    }
    wt.append(line).append("\r\n");
  }
  return wt;
}

string WWIVToFidoText(const string& wt) {
  return WWIVToFidoText(wt, 9);

}

string WWIVToFidoText(const string& wt, int8_t max_optional_val_to_include) {
  auto temp = wt;
  // Fido Text is CR, not CRLF, so remove the LFs
  temp.erase(std::remove(temp.begin(), temp.end(), 10), temp.end());
  // Also remove the soft CRs since WWIV has no concept.
  // TODO(rushfan): Is this really needed.
  temp.erase(std::remove(temp.begin(), temp.end(), '\x8d'), temp.end());
  // Remove the trailing control-Z or trailing null characters (if one exists).
  while (!temp.empty() && (temp.back() == CZ || temp.back() == 0)) {
    temp.pop_back();
  }

  // Split this into lines, then we'll handle converting of
  // WWIV style control codes to FTN style kludges as needed.
  const auto lines = SplitString(temp, "\r", false);
  std::ostringstream out;
  for (auto line : lines) {
    if (line.empty()) {
      // Handle the empty line case first. Everything else can assume non-empty now.
      out << "\r";
      continue;
    }
    if (line.front() == 0x04 && line.size() > 2) {
      // WWIV style control code.
      auto code = line.at(1);
      if (code < '0' || code > '9') {
        // Bogus control-D line, let's skip.
        VLOG(1) << "Invalid control-D line: '" << line << "'";
        continue;
      }
      // Strip WWIV control off.
      line = line.substr(2);
      int8_t code_num = code - '0';
      if (code == '0') {
        if (starts_with(line, "MSGID:") || starts_with(line, "REPLY:") || starts_with(line, "PID:")) {
          // Handle ^A Control Lines
          out << "\001" << line << "\r";
        }
        else if (starts_with(line, "AREA:") || starts_with(line, "SEEN-BY: ")) {
          // Handle kludge lines that do not start with ^A
          out << line << "\r";
        }
        // Skip all ^D0 lines other than ones we know.
        continue;
      } else if (code_num > max_optional_val_to_include) {
        // Skip values higher than we want.
        continue;
      }
    } else if (line.front() == 0x04) {
      // skip ^D line that's not well formed (i.e. more than 2 characters lone)
      // TODO(rushfan): Open question should we emit a blank line for a ^DN line that
      //                is exactly 2 chars long?
      continue;
    }
    if (line.back() == 0x01 /* CA */) {
      // A line ending in ^A means it soft-wrapped.
      line.pop_back();
    }
    if (line.front() == 0x02 /* CB */) {
      // Starting with CB is centered. Let's just strip it.
      line = line.substr(1);
    }

    // Strip out WWIV color codes.
    for (std::size_t i = 0; i < line.length(); i++) {
      if (line[i] == 0x03) {
        i++;
        continue;
      }
      out << line[i];
    }
    out << "\r";
  }
  return out.str();
}

FidoAddress get_address_from_single_line(const std::string& line) {
  auto start = line.find_last_of('(');
  auto end = line.find_last_of(')');
  if (start == string::npos || end == string::npos) {
    return FidoAddress(0, 0, 0, 0, "");
  }

  auto astr = line.substr(start + 1, end - start - 1);
  try {
    return FidoAddress(astr);
  } catch (std::exception&) {
    return FidoAddress(0, 0, 0, 0, "");
  }
}

FidoAddress get_address_from_origin(const std::string& text) {
  auto lines = split_message(text);
  for (const auto& line : lines) {
    if (starts_with(line, " * Origin:")) {
      return get_address_from_single_line(line);
    }
  }
  return FidoAddress(0, 0, 0, 0, "");
}

// Returns a vector of valid routes.
// A valid route is Zone:*, Zone:Net/*
static std::vector<std::string> parse_routes(const std::string& routes) {
  std::vector<std::string> result;
  auto raw = SplitString(routes, " ");
  for (const auto& rr : raw) {
    string r(rr);
    StringTrim(&r);
    result.push_back(r);
  }
  return result;
}


enum class RouteMatch { yes, no, exclude };

// route can be a mask of: {Zone:*, Zone:Net/*, or Zone:Net/node}
// also a route can be negated with ! in front of it.
static RouteMatch matches_route(const wwiv::sdk::fido::FidoAddress& a, const std::string& route) {
  string r(route);
  StringTrim(&r);
  
  RouteMatch positive = RouteMatch::yes;

  // Negated route.
  if (starts_with(r, "!")) {
    r = r.substr(1);
    positive = RouteMatch::exclude;
  }

  // Just a "*: for the route.
  if (r == "*") {
    return positive;
  }

  // No wild card, so see if it's a complete route.
  if (!ends_with(r, "*")) {
    try {
      // Let's see if it's an address.
      FidoAddress route_address(r);
      return (route_address == a) ? positive : RouteMatch::no;
    } catch (const std::exception&) {
      // Not a valid address.
      VLOG(2) << "Malformed route (not a complete address): " << route;
      return RouteMatch::no;
    }
  } else {
    // Remove the trailing *
    r.pop_back();
  }

  if (contains(r, ':') && ends_with(r, "/")) {
    // We have a ZONE:NET/*
    r.pop_back();
    auto parts = SplitString(r, ":");
    if (parts.size() != 2) {
      VLOG(2) << "Malformed route: " << route;
      return RouteMatch::no;
    }
    auto zone = to_number<uint16_t>(parts.at(0));
    auto net = to_number<uint16_t>(parts.at(1));
    if (a.zone() == zone && a.net() == net) {
      return positive;
    }
  } else if (ends_with(r, ":")) {
    // We have a ZONE:*
    r.pop_back();
    auto zone = to_number<uint16_t>(r);
    if (a.zone() == zone) {
      return positive;
    }
  }

  return RouteMatch::no;
}

bool RoutesThroughAddress(const wwiv::sdk::fido::FidoAddress& a, const std::string& routes) {
  if (routes.empty()) {
    return false;
  }
  const auto rs = parse_routes(routes);
  bool ok = false;
  for (const auto& rr : rs) {
    RouteMatch m = matches_route(a, rr);
    if (m != RouteMatch::no) {
       // Ignore any no matches, return otherwise;
      if (m != RouteMatch::no) {
        ok = (m == RouteMatch::yes);
      }
    }
  }
  return ok;
}

wwiv::sdk::fido::FidoAddress FindRouteToAddress(
  const wwiv::sdk::fido::FidoAddress& a,
  const std::map<wwiv::sdk::fido::FidoAddress, fido_node_config_t>& node_configs_map) {

  for (const auto& nc : node_configs_map) {
    if (nc.first == a) {
      return a;
    }
    if (RoutesThroughAddress(a, nc.second.routes)) {
      return nc.first;
    }
  }
  return FidoAddress(0, 0, 0, 0, "");
}

wwiv::sdk::fido::FidoAddress FindRouteToAddress(
  const wwiv::sdk::fido::FidoAddress& a, const wwiv::sdk::fido::FidoCallout& callout) {
  return FindRouteToAddress(a, callout.node_configs_map());
}

bool exists_bundle(const wwiv::sdk::Config& config, const net_networks_rec& net) {
  FtnDirectories dirs(config.root_directory(), net);
  return exists_bundle(dirs.inbound_dir());
}

bool exists_bundle(const std::string& dir) {
  const std::vector<string> extensions{"su?", "mo?", "tu?", "we?", "th?", "fr?", "sa?", "pkt"};
  for (const auto& e : extensions) {
    {
      FindFiles ff(FilePath(dir, StrCat("*.", e)), FindFilesType::files);
      for (const auto& f : ff) {
        if (f.size > 0) return true;
      }
    }
    {
      FindFiles ff(FilePath(dir, StrCat("*.", ToStringUpperCase(e))), FindFilesType::files);
      for (const auto& f : ff) {
        if (f.size > 0) return true;
      }
    }
  }
  return false;
}

/** 
 * Display ISO 8601 offset from UTC in timezone
 * (1 minute=1, 1 hour=100)
 * If timezone cannot be determined, no characters.
 */
std::string tz_offset_from_utc() {
  auto dt = DateTime::now();
  return dt.to_string("%z");
}

static std::vector<std::pair<std::string, flo_directive>> ParseFloFile(const std::string path) {
  TextFile file(path, "r");
  if (!file.IsOpen()) {
    return{};
  }

  std::vector<std::pair<std::string, flo_directive>> result;
  string line;
  while (file.ReadLine(&line)) {
    StringTrim(&line);
    if (line.empty()) { continue; }
    auto st = line.front();
    if (st == '^' || st == '#' || st == '~') {
      const string fn = line.substr(1);
      result.emplace_back(fn, static_cast<flo_directive>(st));
    }
  }
  return result;
}

FloFile::FloFile(const net_networks_rec& net, const std::string& dir, const std::string filename)
  : net_(net), dir_(dir), filename_(filename), dest_() {
  if (!contains(filename, '.')) {
    // This is a malformed flo file
    return;
  }

  string::size_type dot = filename.find_last_of('.');
  if (dot == string::npos) {
    // WTF
    return;
  }

  auto basename = ToStringLowerCase(filename.substr(0, dot));
  auto ext = ToStringLowerCase(filename.substr(dot + 1));

  if (ext.length() != 3) {
    // malformed flo file
    return;
  }

  if (basename.length() != 8) {
    // malformed flo file
    return;
  }

  if (!ends_with(ext, "lo")) {
    // malformed flo file
    return;
  }
  auto st = to_lower_case(ext.front());
  status_ = static_cast<fido_bundle_status_t>(st);

  auto netstr = basename.substr(0, 4);
  auto net_num = to_number<int16_t>(netstr, 16);
  auto nodestr = basename.substr(4);
  auto node_num = to_number<int16_t>(nodestr, 16);

  FidoAddress source(net_.fido.fido_address);
  dest_.reset(new FidoAddress(source.zone(), net_num, node_num, 0, source.domain()));

  Load();
}

FloFile::~FloFile() {
}

bool FloFile::insert(const std::string& file, flo_directive directive) { 
  entries_.push_back(std::make_pair(file, directive));
  return true;
}

bool FloFile::erase(const std::string& file) {
  for (auto it = entries_.begin(); it != entries_.end(); it++) {
    if ((*it).first == file) {
      entries_.erase(it);
      return true;
    }
  }
  return false;
}

bool FloFile::Load() {
  File f(dir_, filename_);
  exists_ = f.Exists();
  if (!exists_) {
    return false;
  }
  
  poll_ = f.length() == 0;
  entries_ = ParseFloFile(FilePath(dir_, filename_));
  return true;
}

bool FloFile::Save() {
  if (poll_ || !entries_.empty()) {
    File f(dir_, filename_);
    if (!f.Open(File::modeCreateFile | File::modeReadWrite | File::modeText | File::modeTruncate, File::shareDenyReadWrite)) {
      return false;
    }
    for (const auto& e : entries_) {
      auto dr = static_cast<char>(e.second);
      auto& name = e.first;
      f.Writeln(StrCat(dr, name));
    }
    return true;
  } else if (File::Exists(dir_, filename_)) {
    return File::Remove(dir_, filename_);
  }
  return true;
}


FidoAddress FloFile::destination_address() const {
  return *dest_.get();
}

FtnDirectories::FtnDirectories(const std::string& bbsdir, const net_networks_rec& net)
	: bbsdir_(bbsdir), 
    net_(net),
    net_dir_(File::absolute(bbsdir, net.dir)),
    inbound_dir_(File::absolute(net_dir_, net_.fido.inbound_dir)),
    temp_inbound_dir_(File::absolute(net_dir_, net_.fido.temp_inbound_dir)),
    temp_outbound_dir_(File::absolute(net_dir_, net_.fido.temp_outbound_dir)),
    outbound_dir_(File::absolute(net_dir_, net_.fido.outbound_dir)),
    netmail_dir_(File::absolute(net_dir_, net_.fido.netmail_dir)),
    bad_packets_dir_(File::absolute(net_dir_, net_.fido.bad_packets_dir)) {}

FtnDirectories::~FtnDirectories() {}

const std::string& FtnDirectories::net_dir() const { return net_dir_; }
const std::string& FtnDirectories::inbound_dir() const { return inbound_dir_; }
const std::string& FtnDirectories::temp_inbound_dir() const { return temp_inbound_dir_; }
const std::string& FtnDirectories::temp_outbound_dir() const { return temp_outbound_dir_; }
const std::string&FtnDirectories::outbound_dir() const { return outbound_dir_; }
const std::string&FtnDirectories::netmail_dir() const { return netmail_dir_; }
const std::string&FtnDirectories::bad_packets_dir() const { return bad_packets_dir_; }


}  // namespace fido
}  // namespace net
}  // namespace wwiv

