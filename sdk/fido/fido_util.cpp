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
#include "sdk/datetime.h"
#include "sdk/filenames.h"
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

// We use DDHHMMSS like SBBSECHO does.
std::string packet_name(time_t now) {
  auto tm = localtime(&now);

  string fmt = "%d%H%M%S";

  char buf[1024];
  auto res = strftime(buf, sizeof(buf), fmt.c_str(), tm);
  if (res <= 0) {
    LOG(ERROR) << "Unable to create packet name from strftime";
    to_char_array(buf, "DDHHMMSS");
  }
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
  std::string ext = dow.at(dow_num);
  char c = static_cast<char>('0' + bundle_number);
  if (bundle_number > 9) {
    c = static_cast<char>('a' + bundle_number - 10);
  }
  ext.push_back(c);
  return ext;
}

bool is_bundle_file(const std::string& name) {
  static const std::vector<string> dow = { "su", "mo", "tu", "we", "th", "fr", "sa", "su" };
  string::size_type dot = name.find_last_of('.');
  if (dot == string::npos) { return false; }
  string ext = name.substr(dot + 1);
  if (ext.length() != 3) { return false; }
  ext.pop_back();
  StringLowerCase(&ext);
  return contains(dow, ext);
}

static string control_file_extension(fido_bundle_status_t status) {
  string s = "flo";
  s[0] = static_cast<char>(status);
  return s;
}

std::string control_file_name(const wwiv::sdk::fido::FidoAddress& dest, fido_bundle_status_t status) {
  int16_t net = dest.net();
  int16_t node = dest.node();

  const string ext = control_file_extension(status);
  return StringPrintf("%04.4x%04.4x.%s", net, node, ext.c_str());
}

// 10 Nov 16  21:15:45
std::string daten_to_fido(time_t t) {
  auto tm = localtime(&t);
  const string fmt = "%d %b %y  %H:%M:%S";

  char buf[1024];
  auto res = strftime(buf, sizeof(buf), fmt.c_str(), tm);
  if (res <= 0) {
    LOG(ERROR) << "Unable to create date format in daten_to_fido from strftime";
    return "";
  }
  return buf;
}

// Format: 10 Nov 16  21:15:45
time_t fido_to_daten(std::string d) {
  try {
    vector<string> months = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    std::stringstream stream(d);
    auto now = time(nullptr);
    struct tm* t = localtime(&now);
    stream >> t->tm_mday;
    string mon_str;
    stream >> mon_str;
    if (!contains(months, mon_str)) {
      // Unparsable date. return now.
      return time(nullptr);
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
    t->tm_hour = StringToInt(parts.at(0)) - 1;
    t->tm_min = StringToInt(parts.at(1));
    t->tm_sec = StringToInt(parts.at(2));

    auto result = mktime(t);
    if (result < 0) {
      // Error.. return now so we don't blow stuff up.
      result =  time(nullptr);
    }
    return result;
  } catch (const std::exception& e) {
    LOG(ERROR) << "exception in fido_to_daten('" << d << "'): " << e.what();
    return time(nullptr);
  }
}

std::string to_net_node(const wwiv::sdk::fido::FidoAddress& a) {
  return StrCat(a.net(), "/", a.node());
}

std::string to_zone_net_node(const wwiv::sdk::fido::FidoAddress& a) {
  return StrCat(a.zone(), ":", to_net_node(a));
}

std::vector<std::string> split_message(const std::string& s) {
  string temp(s);
  temp.erase(std::remove(temp.begin(), temp.end(), 10), temp.end());
  temp.erase(std::remove(temp.begin(), temp.end(), '\x8d'), temp.end());
  return SplitString(temp, "\r");
}

// TODO(rushfan): split text into lines and process one at a time
// this will be easier to handle control lines, etc.
std::string FidoToWWIVText(const std::string& ft, bool convert_control_codes) {
  std::string wt;
  bool newline = true;
  for (auto& sc : ft) {
    unsigned char c = static_cast<unsigned char>(sc);
    if (c == 13) {
      wt.push_back(13);
      wt.push_back(10);
      newline = true;
    } else if (c == 0x8d) {
      // FIDOnet style Soft CR
      wt.push_back(13);
      wt.push_back(10);
      newline = true;
    } else if (c == 10) {
      // NOP
    } else if (c == 1 && newline && convert_control_codes) {
      // Control-A on a newline.  Since FidoNet uses control-A as a control
      // code, WWIV uses control-D + '0', we'll change it to control-D + '0'
      wt.push_back(4);  // control-D
      wt.push_back('0');
      newline = false;
    } else {
      newline = false;
      wt.push_back(c);
    }
  }
  return wt;
}

std::string WWIVToFidoText(const std::string& wt) {
  string temp(wt);
  // Fido Text is CR, not CRLF, so remove the LFs
  temp.erase(std::remove(temp.begin(), temp.end(), 10), temp.end());
  // Also remove the soft CRs since WWIV has no concept.
  // TODO(rushfan): Is this really needed.
  temp.erase(std::remove(temp.begin(), temp.end(), '\x8d'), temp.end());
  // Remove the trailing control-Z (if one exists).
  if (!temp.empty() && temp.back() == '\x1a') {
    temp.pop_back();
  }

  // Split this into lines, then we'll handle converting of
  // WWIV style control codes to FTN style kludges as needed.
  const auto lines = SplitString(temp, "\r");
  std::ostringstream out;
  for (auto line : lines) {
    if (!line.empty() && line.front() == 0x04 && line.size() > 2) {
      // WWIV style control code.
      char code = line.at(1);
      if (code < '0' || code > '9') {
        // Bogus control-D line, let's skip.
        VLOG(1) << "Invalid control-D line: '" << line << "'";
        continue;
      }
      // Strip WWIV control off.
      line = line.substr(2);
      if (code == '0') {
        if (starts_with(line, "MSGID:") || starts_with(line, "REPLY:") || starts_with(line, "PID:")) {
          out << "\001" << line << "\r";
        }
        // Skip all ^D0 lines other than ones we know.
        continue;
      }
    }
    out << line << "\r";
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
  vector<string> lines = split_message(text);
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
  std::vector<std::string> raw = SplitString(routes, " ");
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
    vector<string> parts = SplitString(r, ":");
    if (parts.size() != 2) {
      VLOG(2) << "Malformed route: " << route;
      return RouteMatch::no;
    }
    auto zone = StringToUnsignedShort(parts.at(0));
    auto net = StringToUnsignedShort(parts.at(1));
    if (a.zone() == zone && a.net() == net) {
      return positive;
    }
  } else if (ends_with(r, ":")) {
    // We have a ZONE:*
    r.pop_back();
    auto zone = StringToUnsignedShort(r);
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
  const std::vector<std::string> rs = parse_routes(routes);
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
  auto net_dir = File::absolute(config.root_directory(), net.dir);
  auto inbounddir = File::absolute(net_dir, net.fido.inbound_dir);
  return exists_bundle(inbounddir);
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
    char st = line.front();
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

  string basename = ToStringLowerCase(filename.substr(0, dot));
  string ext = ToStringLowerCase(filename.substr(dot + 1));

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
  char st = tolower(ext.front());
  status_ = static_cast<fido_bundle_status_t>(st);

  auto netstr = basename.substr(0, 4);
  auto net_num = static_cast<int16_t>(std::stol(netstr.c_str(), nullptr, 16));
  auto nodestr = basename.substr(4);
  auto node_num = static_cast<int16_t>(std::stol(nodestr.c_str(), nullptr, 16));

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


}  // namespace fido
}  // namespace net
}  // namespace wwiv

