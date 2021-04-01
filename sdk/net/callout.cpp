/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2021, WWIV Software Services             */
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
#include "sdk/net/callout.h"

#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "sdk/filenames.h"
#include "sdk/net/networks.h"
#include <algorithm>
#include <iomanip>
#include <filesystem>
#include <map>
#include <sstream>
#include <string>

using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;

namespace wwiv::sdk {

// [[ VisibleForTesting ]]
bool ParseCalloutNetLine(const std::string& ss, net_call_out_rec* con) {
  if (ss.empty() || ss[0] != '@') {
    // skip empty lines and those not starting with @.
    return false;
  }
  con->min_hr = -1;
  con->max_hr = -1;

  for (auto iter = ss.cbegin(); iter != ss.cend(); ++iter) {
    switch (*iter) {
    case '@': {
      con->sysnum = to_number<decltype(con->sysnum)>(std::string(++iter, ss.end()));
    } break;
    case '&':
      con->options |= unused_options_sendback;
      break;
    case '-':
      con->options |= unused_options_ATT_night;
      break;
    case '_':
      con->options |= unused_options_ppp;
      break;
    case '+':
      con->options |= options_no_call;
      break;
    case '~':
      con->options |= unused_options_receive_only;
      break;
    case '!': {
      con->options |= unused_options_once_per_day;
      ++iter;
      //auto unused_times_per_day = to_number<int>(string(++iter, ss.end()));
    } break;
    case '%': {
      con->macnum = to_number<decltype(con->macnum)>(std::string(++iter, ss.end()));
    } break;
    case '/': {
      con->call_every_x_minutes = to_number<decltype(con->call_every_x_minutes)>(std::string(++iter, ss.end()));
      if (con->call_every_x_minutes > 0) {
        // Let's set a minimum of 10 minutes in between calls.
        con->call_every_x_minutes =
            std::max<decltype(con->call_every_x_minutes)>(con->call_every_x_minutes, 10);
      }
    } break;
    case '(': {
      con->min_hr = to_number<decltype(con->min_hr)>(std::string(++iter, ss.end()));
    } break;
    case ')': {
      con->max_hr = to_number<decltype(con->max_hr)>(std::string(++iter, ss.end()));
    } break;
    case '|': {
      con->min_k = std::max<uint16_t>(1, to_number<uint16_t>(std::string(++iter, ss.end())));
    } break;
    case ';':
      con->options |= unused_options_compress;
      break;
    case '^':
      con->options |= unused_options_hslink;
      break;
    case '$':
      con->options |= unused_options_force_ac;
      break;
    case '=':
      con->options |= options_hide_pend;
      break;
    case '*':
      con->options |= unused_options_dial_ten;
      break;
    case '\"': {
      ++iter; // skip past first "
      std::string password;
      while (iter != ss.end() && *iter != '\"') {
        password.push_back(*iter++);
      }
      if (!password.empty() && password.back() == '\"') {
        // remove trailing "
        password.pop_back();
      }
      con->session_password = password;
    } break;
    default:
      break;
    }
  }
  return true;
}

std::string WriteCalloutNetLine(const net_call_out_rec& con) {
  std::ostringstream ss;
  if (con.options & unused_options_sendback) {
    ss << " &";
  }
  if (con.options & options_no_call) {
    ss << " +";
  }
  if (con.options & unused_options_receive_only) {
    ss << " ~";
  }
  if (con.options & unused_options_once_per_day) {
    ss << " !";
  }
  if (con.options & options_hide_pend) {
    ss << " =";
  }
  if (con.macnum > 0) {
    ss << " %" << static_cast<int>(con.macnum);
  }
  if (con.call_every_x_minutes > 0) {
    const int c = std::max(10, static_cast<int>(con.call_every_x_minutes));
    ss << " /" << c;
  }
  if (con.min_hr > 0) {
    ss << " (" << static_cast<int>(con.min_hr);
  }
  if (con.max_hr > 0) {
    ss << " )" << static_cast<int>(con.max_hr);
  }
  if (con.min_k > 0) {
    ss << " |" << static_cast<int>(con.min_k);
  }
  const auto options = ss.str();
  std::ostringstream s;
  s << "@" << std::left << std::setw(5) << con.sysnum;
  if (options.size() < 20) {
    s << std::left << std::setw(20);
  }
  s << options;
  s << " \"" << con.session_password << "\"";
  return s.str();
}

std::string CalloutOptionsToString(uint16_t options) {
  std::ostringstream ss;
  ss << "{" << options << "} ";
  if (options & options_no_call) {
    ss << "[nocall] ";
  }
  if (options & options_hide_pend) {
    ss << "[hide_pending] ";
  }
  return ss.str();
}

static bool ParseCalloutFile(std::map<uint16_t, net_call_out_rec>* node_config_map,
                             const std::filesystem::path& network_dir) {
  const auto path = FilePath(network_dir, CALLOUT_NET);
  if (!File::Exists(path)) {
    return false;
  }

  TextFile node_config_file(path, "rt");
  if (!node_config_file.IsOpen()) {
    return false;
  }
  // A line will be of the format @node host:port [password].
  std::string line;
  while (node_config_file.ReadLine(&line)) {
    StringTrim(&line);
    net_call_out_rec node_config{};
    if (ParseCalloutNetLine(line, &node_config)) {
      // Parsed a line correctly.
      node_config.ftn_address = StrCat("20000:20000/", node_config.sysnum);
      node_config_map->emplace(node_config.sysnum, node_config);
    }
  }
  return true;
}

Callout::Callout(const Network& net, int max_backups)
    : net_(net), max_backups_(max_backups) {
  ParseCalloutFile(&node_config_, net.dir);
}

Callout::Callout(std::initializer_list<net_call_out_rec> l) : net_(), max_backups_(0) {
  for (const auto& r : l) {
    node_config_.emplace(r.sysnum, r);
  }
}

Callout::~Callout() = default;

const net_call_out_rec* Callout::net_call_out_for(int node) const {
  VLOG(2) << "Callout::net_call_out_for(" << node << ")";
  const auto iter = node_config_.find(static_cast<uint16_t>(node));
  if (iter != end(node_config_)) {
    return &iter->second;
  }
  return nullptr;
}

const net_call_out_rec* Callout::net_call_out_for(const std::string& node) const {
  VLOG(2) << "Callout::net_call_out_for(" << node << ")";
  if (starts_with(node, "20000:20000/")) {
    auto s = node.substr(12);
    s = s.substr(0, s.find('/'));
    return net_call_out_for(to_number<int>(s));
  }
  return net_call_out_for(to_number<int>(node));
}

bool Callout::insert(uint16_t , const net_call_out_rec& orig) {
  net_call_out_rec c{orig};
  c.ftn_address = StrCat("20000:20000/", c.sysnum);
  node_config_.erase(c.sysnum);
  node_config_.emplace(c.sysnum, c);
  return true;
}

bool Callout::erase(uint16_t node) {
  node_config_.erase(node);
  return true;
}

bool Callout::Save() {
  backup_file(FilePath(net_.dir, CALLOUT_NET));
  TextFile node_config_file(FilePath(net_.dir, CALLOUT_NET), "wt");
  if (!node_config_file.IsOpen()) {
    return false;
  }
  for (const auto& kv : node_config_) {
    node_config_file.WriteLine(WriteCalloutNetLine(kv.second));
  }
  return true;
}

static std::string DumpCallout(const net_call_out_rec& n) {
  std::ostringstream ss;
  ss << "sysnum:        " << n.sysnum << std::endl;
  if (n.macnum) {
    ss << "macnum:        " << std::dec << n.macnum << std::endl;
  }
  if (n.options) {
    ss << "options:       " << CalloutOptionsToString(n.options) << std::endl;
  }
  if (n.min_hr > 0) {
    ss << "min_hr:        " << static_cast<int>(n.min_hr) << std::endl;
  }
  if (n.max_hr > 0) {
    ss << "max_hr:        " << static_cast<int>(n.max_hr) << std::endl;
  }
  ss << "password:       \"" << n.session_password << "\"" << std::endl;
  if (n.min_k) {
    ss << "min_k:         " << static_cast<int>(n.min_k) << std::endl;
  }
  ss << "callout.net line: " << std::endl;
  ss << WriteCalloutNetLine(n) << std::endl;
  return ss.str();
}

std::string Callout::ToString() const {
  std::ostringstream ss;
  for (const auto& kv : node_config_) {
    ss << DumpCallout(kv.second) << std::endl;
  }
  return ss.str();
}

} // namespace wwiv
