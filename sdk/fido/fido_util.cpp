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
#include "sdk/fido/fido_util.h"

#include "core/datetime.h"
#include "core/file.h"
#include "core/findfiles.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/printf.h"
#include "sdk/fido/fido_address.h"
#include "sdk/fido/fido_directories.h"
#include "sdk/fido/fido_packets.h"
#include "sdk/fido/flo_file.h"
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::sdk::net;
using namespace wwiv::stl;
using namespace wwiv::strings;

namespace wwiv::sdk::fido {

constexpr char CZ = 26;

// We use DDHHMMSS like SBBSECHO v2 does.
std::string packet_name(DateTime& dt) {
  const auto buf = dt.to_string("%d%H%M%S");
  return StrCat(buf, ".pkt");
}

std::string bundle_name(const wwiv::sdk::fido::FidoAddress& source,
                        const wwiv::sdk::fido::FidoAddress& dest, const std::string& extension) {
  const int16_t net = source.net() - dest.net();
  const uint16_t node = source.node() - dest.node();

  return fmt::sprintf("%04.4x%04.4x.%s", net, node, extension);
}

std::string net_node_name(const FidoAddress& dest, const std::string& extension) {
  return fmt::sprintf("%04.4x%04.4x.%s", dest.net(), dest.node(), extension);
}

std::string flo_name(const FidoAddress& dest, fido_bundle_status_t status) {
  std::string extension = "flo";
  if (status != fido_bundle_status_t::unknown) {
    extension[0] = static_cast<char>(status);
  }
  return net_node_name(dest, extension);
}

std::string bundle_name(const FidoAddress& source, const FidoAddress& dest, int dow,
                        int bundle_number) {
  return bundle_name(source, dest, dow_extension(dow, bundle_number));
}

std::vector<std::string> dow_prefixes() {
  return std::vector<std::string> {"su", "mo", "tu", "we", "th", "fr", "sa", "su"};
}

std::string dow_extension(int dow_num, int bundle_number) {
  // TODO(rushfan): Should we assert of bundle_number > 25 (0-9=10, + a-z=26 = 0-35)

  const auto dow = dow_prefixes();
  auto ext = at(dow, dow_num);
  auto c = static_cast<char>('0' + bundle_number);
  if (bundle_number > 9) {
    c = static_cast<char>('a' + bundle_number - 10);
  }
  ext.push_back(c);
  return ext;
}

bool is_bundle_file(const std::string& name) {
  const auto dot = name.find_last_of('.');
  if (dot == std::string::npos) {
    return false;
  }
  auto ext = name.substr(dot + 1);
  if (ext.length() != 3) {
    return false;
  }
  ext.pop_back();
  StringLowerCase(&ext);
  const auto dow = dow_prefixes();
  return contains(dow, ext);
}

bool is_packet_file(const std::string& name) {
  const auto dot = name.find_last_of('.');
  if (dot == std::string::npos) {
    return false;
  }
  auto ext = name.substr(dot + 1);
  if (ext.length() != 3) {
    return false;
  }
  StringLowerCase(&ext);
  return ext == "pkt";
}

static std::string control_file_extension(fido_bundle_status_t status) {
  std::string s = "flo";
  s[0] = static_cast<char>(status);
  return s;
}

std::string control_file_name(const wwiv::sdk::fido::FidoAddress& dest,
                              fido_bundle_status_t status) {
  const auto net = dest.net();
  const auto node = dest.node();

  const auto ext = control_file_extension(status);
  return fmt::sprintf("%04.4x%04.4x.%s", net, node, ext);
}

// 10 Nov 16  21:15:45
std::string daten_to_fido(time_t t) {
  const auto dt = DateTime::from_time_t(t);
  return dt.to_string("%d %b %y  %H:%M:%S");
}

// Format: 10 Nov 16  21:15:45
daten_t fido_to_daten(std::string d) {
  try {
    std::vector<std::string> months = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                             "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    std::stringstream stream(d);
    auto now = time_t_now();
    auto* t = localtime(&now);
    stream >> t->tm_mday;
    std::string mon_str;
    stream >> mon_str;
    if (!contains(months, mon_str)) {
      // Unparsable date. return now.
      return daten_t_now();
    }
    t->tm_mon = static_cast<int>(
        std::distance(months.begin(), std::find(months.begin(), months.end(), mon_str)));
    int year;
    stream >> year;
    year %= 100;
    // This will work until 2099.
    t->tm_year = 100 + year;

    std::string hms;
    stream >> hms;
    auto parts = SplitString(hms, ":");
    t->tm_hour = to_number<int>(at(parts, 0)) - 1;
    t->tm_min = to_number<int>(at(parts, 1));
    t->tm_sec = to_number<int>(at(parts, 2));

    auto result = mktime(t);
    if (result < 0) {
      // Error.. return now so we don't blow stuff up.
      result = time_t_now();
    }
    return DateTime::from_time_t(result).to_daten_t();  
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
  return StrCat(to_zone_net_node(a), ".", a.point());
}

std::vector<std::string> split_message(const std::string& s) {
  std::string temp(s);
  temp.erase(std::remove(temp.begin(), temp.end(), 10), temp.end());
  temp.erase(std::remove(temp.begin(), temp.end(), '\x8d'), temp.end());
  return SplitString(temp, "\r");
}

/**
 * \brief Type of control line. Control-A Kludge or non-control-a like AREA, or none.
 */
enum class FtnControlLineType { control_a, plain_control_line, none };

static FtnControlLineType determine_kludge_line_type(const std::string& line) {
  if (line.empty()) {
    return FtnControlLineType::none;
  }
  if (line.front() == 0x01) {
    return line.size() > 1 ? FtnControlLineType::control_a : FtnControlLineType::none;
  }
  if (starts_with(line, "AREA:") || starts_with(line, "SEEN-BY: ")) {
    return FtnControlLineType::plain_control_line;
  }
  return FtnControlLineType::none;
}

std::string FidoToWWIVText(const std::string& ft, bool convert_control_codes) {
  // Some editors just use \n for line endings, we need \r\n for
  // FidoToWWIVText to work.  Replace the sole \n with \r\n
  const auto lf_only = ft.find('\r') == std::string::npos;

  // Split text into lines and process one at a time
  // this is easier to handle control lines, etc.
  std::string wt;
  for (const auto& sc : ft) {
    if (const auto c = static_cast<unsigned char>(sc); c == 0x8d) {
      // FIDOnet style Soft CR. Convert to CR
      wt.push_back(13);
    } else if (c == 10) {
      // NOP. We're skipping LF's so we can split on CR unless we only
      // have LF.
      if (lf_only) {
        wt.push_back(13);
      }
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

std::string WWIVToFidoText(const std::string& wt, const wwiv_to_fido_options& opts) {
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
      const auto code = at(line, 1);
      if (code < '0' || code > '9') {
        // Bogus control-D line, let's skip.
        VLOG(1) << "Invalid control-D line: '" << line << "'";
        continue;
      }
      // Strip WWIV control off.
      line = line.substr(2);
      const int8_t code_num = code - '0';
      if (code == '0') {
        if (starts_with(line, "MSGID:") || starts_with(line, "REPLY:") ||
            starts_with(line, "PID:")) {
          // Handle ^A Control Lines
          out << "\001" << line << "\r";
        } else if (starts_with(line, "AREA:") || starts_with(line, "SEEN-BY: ")) {
          // Handle kludge lines that do not start with ^A
          out << line << "\r";
        }
        // Skip all ^D0 lines other than ones we know.
        continue;
      }
      if (code_num > opts.max_optional_val_to_include) {
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
    const auto line_size = size_int(line);
    for (auto i = 0; i < line_size; i++) {
      const auto c = line[i];
      if (c == 0x03) {
        if (++i >= line_size) {
          // We're at the end.
          continue;
        }
        if (opts.wwiv_heart_color_codes && opts.allow_any_pipe_codes) {
          const auto color = at(opts.colors,line[i] - '0');
          out << fmt::format("|{:02}", color);
        }
        continue;
      }
      if (c == '|' && opts.allow_any_pipe_codes && opts.wwiv_pipe_color_codes && (line_size - i) > 2 && line[i+1] == '#') {
        const auto color = at(opts.colors, line[i + 2] - '0');
        out << fmt::format("|{:02}", color);
        if (i < line_size) {
          ++i;
        }
        if (i < line_size) {
          ++i;
        }
        continue;
      }
      if (c == '|' && !opts.allow_any_pipe_codes) {
        ++i;
        if (i < line_size) {
          ++i;
        }
        continue;
      }
      out << c;
    }
    out << "\r";
  }
  return out.str();
}

FidoAddress get_address_from_single_line(const std::string& line) {
  const auto start = line.find_last_of('(');
  const auto end = line.find_last_of(')');
  if (start == std::string::npos || end == std::string::npos) {
    return EMPTY_FIDO_ADDRESS;
  }

  const auto astr = line.substr(start + 1, end - start - 1);
  try {
    return FidoAddress(astr);
  } catch (std::exception&) {
    return EMPTY_FIDO_ADDRESS;
  }
}

FidoAddress get_address_from_origin(const std::string& text) {
  auto lines = split_message(text);
  for (const auto& line : lines) {
    if (starts_with(line, " * Origin:")) {
      return get_address_from_single_line(line);
    }
  }
  return EMPTY_FIDO_ADDRESS;
}

FidoAddress get_address_from_packet(const FidoPackedMessage& msg, const packet_header_2p_t& header) {
  if (auto a = get_address_from_origin(msg.vh.text); a.zone() > 0) {
    return a;
  }
  auto zone = header.orig_zone;
  if (zone <= 0) {
    zone = header.qm_orig_zone;
  }
  const auto net = msg.nh.orig_net;
  const auto node = msg.nh.orig_node;
  const auto point = header.orig_point;
  return FidoAddress(zone, net, node, point, "");
}

FidoAddress get_address_from_stored_message(const FidoStoredMessage& msg) {
  if (auto a = get_address_from_origin(msg.text); a.zone() > 0) {
    return a;
  }
  const auto zone = msg.nh.orig_zone;
  const auto net = msg.nh.orig_net;
  const auto node = msg.nh.orig_node;
  const auto point = msg.nh.orig_point;
  return FidoAddress(zone, net, node, point, "");
}

// Returns a vector of valid routes.
// A valid route is Zone:*, Zone:Net/*
static std::vector<std::string> parse_routes(const std::string& routes) {
  std::vector<std::string> result;
  auto raw = SplitString(routes, " ");
  for (const auto& rr : raw) {
    result.push_back(StringTrim(rr));
  }
  return result;
}

enum class RouteMatch { yes, no, exclude };

// route can be a mask of: {Zone:*, Zone:Net/*, or Zone:Net/node}
// also a route can be negated with ! in front of it.
static RouteMatch matches_route(const wwiv::sdk::fido::FidoAddress& a, const std::string& route) {
  auto r = StringTrim(route);

  auto positive = RouteMatch::yes;

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
      return route_address == a ? positive : RouteMatch::no;
    } catch (const std::exception&) {
      // Not a valid address.
      VLOG(2) << "Malformed route (not a complete address): " << route;
      return RouteMatch::no;
    }
  }
  // Remove the trailing *
  r.pop_back();

  if (contains(r, ':') && ends_with(r, "/")) {
    // We have a ZONE:NET/*
    r.pop_back();
    auto parts = SplitString(r, ":");
    if (parts.size() != 2) {
      VLOG(2) << "Malformed route: " << route;
      return RouteMatch::no;
    }
    const auto zone = to_number<uint16_t>(at(parts, 0));
    const auto net = to_number<uint16_t>(at(parts, 1));
    if (a.zone() == zone && a.net() == net) {
      return positive;
    }
  } else if (ends_with(r, ":")) {
    // We have a ZONE:*
    r.pop_back();
    const auto zone = to_number<uint16_t>(r);
    if (a.zone() == zone) {
      return positive;
    }
  }

  return RouteMatch::no;
}

bool RoutesThroughAddress(const FidoAddress& a, const std::string& routes) {
  if (routes.empty()) {
    return false;
  }
  const auto rs = parse_routes(routes);
  auto ok = false;
  for (const auto& rr : rs) {
    auto m = matches_route(a, rr);
    if (m != RouteMatch::no) {
      // Ignore any no matches, return otherwise;
      if (m != RouteMatch::no) {
        ok = (m == RouteMatch::yes);
      }
    }
  }
  return ok;
}

FidoAddress FindRouteToAddress(const FidoAddress& a,
                               const std::map<FidoAddress, fido_node_config_t>& node_configs_map) {

  for (const auto& nc : node_configs_map) {
    if (nc.first == a) {
      return a;
    }
    if (RoutesThroughAddress(a, nc.second.routes)) {
      return nc.first;
    }
  }
  return EMPTY_FIDO_ADDRESS;
}

FidoAddress FindRouteToAddress(const FidoAddress& a, const FidoCallout& callout) {
  return FindRouteToAddress(a, callout.node_configs_map());
}

bool exists_bundle(const wwiv::sdk::Config& config, const Network& net) {
  const FtnDirectories dirs(config.root_directory(), net);
  return exists_bundle(dirs.inbound_dir());
}

bool exists_bundle(const std::filesystem::path& dir) {
  const std::vector<std::string> extensions{"su?", "mo?", "tu?", "we?", "th?", "fr?", "sa?", "pkt"};
  for (const auto& e : extensions) {
    {
      FindFiles ff(FilePath(dir, StrCat("*.", e)), FindFiles::FindFilesType::files);
      for (const auto& f : ff) {
        if (f.size > 0)
          return true;
      }
    }
    {
      FindFiles ff(FilePath(dir, StrCat("*.", ToStringUpperCase(e))), FindFiles::FindFilesType::files);
      for (const auto& f : ff) {
        if (f.size > 0)
          return true;
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
std::string tz_offset_from_utc(const core::DateTime& dt) {
  auto z = dt.to_string("%z");
  if (z.empty()) {
    // who knows!
    return "0000";
  }
  if (z.front() == '+') {
    return z.substr(1);
  }
  return z;
}

// From file_manager.cpp
std::vector<fido_bundle_status_t> statuses{
    fido_bundle_status_t::crash, fido_bundle_status_t::normal, fido_bundle_status_t::direct,
    fido_bundle_status_t::immediate};

int ftn_bytes_waiting(const Network& net, const FidoAddress& dest) {
  const auto outbound_dir = FilePath(net.dir, net.fido.outbound_dir);
  auto size = 0;
  for (const auto& st : statuses) {
    const auto name = flo_name(dest, st);
    if (!File::Exists(FilePath(outbound_dir, name))) {
      continue;
    }
    LOG(INFO) << "Found file file: " << outbound_dir << "; name: " << name;
    const auto flo_path = FilePath(outbound_dir, name);
    FloFile flo(net, flo_path);
    if (!flo.Load()) {
      continue;
    }
    for (const auto& [flow_entry_name, _] : flo.flo_entries()) {
      File f(flow_entry_name);
      size += f.length();
    }
  }
  VLOG(2) << "ftn_bytes_waiting: size: " << size;
  return size;
}

} // namespace wwiv
