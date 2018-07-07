/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2017, WWIV Software Services             */
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
#include "sdk/callout.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <map>
#include <sstream>
#include <string>

#include "core/strings.h"
#include "core/inifile.h"
#include "core/file.h"
#include "core/log.h"
#include "core/textfile.h"
#include "sdk/filenames.h"
#include "sdk/networks.h"

using std::endl;
using std::map;
using std::string;
using std::stringstream;
using std::unique_ptr;
using std::vector;
using wwiv::core::IniFile;
using namespace wwiv::strings;
using namespace wwiv::sdk;

namespace wwiv {
namespace sdk {

// [[ VisibleForTesting ]]
bool ParseCalloutNetLine(const string& ss, net_call_out_rec* con) {
  if (ss.empty() || ss[0] != '@') {
    // skip empty lines and those not starting with @.
    return false;
  }
  con->min_hr = -1;
  con->max_hr = -1;

  for (auto iter = ss.cbegin(); iter != ss.cend(); iter++) {
      switch (*iter) {
      case '@': {
        con->sysnum = to_number<uint16_t>(string(++iter, ss.end()));
      } break;
      case '&':
        con->options |= options_sendback;
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
        con->options |= options_receive_only;
        break;
      case '!': {
        con->options |= options_once_per_day;
        con->times_per_day = std::max<uint8_t>(1, to_number<uint8_t>(string(++iter, ss.end())));
      } break;
      case '%': {
        con->macnum = to_number<uint8_t>(string(++iter, ss.end()));
      } break;
      case '/': {
        con->call_anyway = to_number<uint8_t>(string(++iter, ss.end()));
        if (con->call_anyway > 0) {
          // Let's set a minimum of 10 minutes in between calls.
          con->call_anyway = std::max<uint8_t>(con->call_anyway, 10);
        }
      } break;
      case '(': {
        con->min_hr = to_number<int8_t>(string(++iter, ss.end()));
      } break;
      case ')': {
        con->max_hr = to_number<int8_t>(string(++iter, ss.end()));
      } break;
      case '|': {
        con->min_k = std::max<uint16_t>(1, to_number<uint16_t>(string(++iter, ss.end())));
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
        ++iter;  // skip past first "
        string password;
        while (iter != ss.end() && *iter != '\"') {
          password.push_back(*iter++);
        }
        if (!password.empty() && password.back() == '\"') {
          // remove trailing "
          password.pop_back();
        }
        con->session_password = password;
      }
      break;
      default:
        break;
      }
    }
    return true;
}

std::string CalloutOptionsToString(uint16_t options) { 
  std::ostringstream ss;
  ss << "{" << options << "} ";
  if (options & options_sendback) {
    ss << "[sendback] ";
  }
  if (options & options_no_call) {
    ss << "[nocall] ";
  }
  if (options & options_receive_only) {
    ss << "[receive_only] ";
  }
  if (options & options_once_per_day) {
    ss << "[x_per_day] ";
  }
  if (options & options_hide_pend) {
    ss << "[hide_pending] ";
  }
  return ss.str();
}



static bool ParseCalloutFile(std::map<uint16_t, net_call_out_rec>* node_config_map, const string network_dir) {
  TextFile node_config_file(network_dir, CALLOUT_NET, "rt");
  if (!node_config_file.IsOpen()) {
    return false;
  }
  // A line will be of the format @node host:port [password].
  string line;
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

Callout::Callout(const net_networks_rec& net) : net_(net) {
  ParseCalloutFile(&node_config_, net.dir);
}

Callout::Callout(std::initializer_list<net_call_out_rec> l) : net_() {
  for (const auto& r : l) {
    node_config_.emplace(r.sysnum, r);
  }
}

Callout::~Callout() {}

const net_call_out_rec* Callout::net_call_out_for(int node) const {
  VLOG(2) << "Callout::net_call_out_for(" << node << ")";
  auto iter = node_config_.find(node);
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

static std::string DumpCallout(const net_call_out_rec& n) {
  std::ostringstream ss;
    ss << "sysnum:        "  << n.sysnum << std::endl;
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
    ss << "max_hr:        " <<  static_cast<int>(n.max_hr) << std::endl;
  }
  ss << "password:       \"" << n.session_password << "\"" << std::endl;
  if (n.times_per_day) {
    ss << "times_per_day:   " << static_cast<int>(n.times_per_day) << std::endl;
  }
  if (n.min_k) {
    ss << "min_k:         " << static_cast<int>(n.min_k) << std::endl;
  }
  return ss.str();
}

std::string Callout::ToString() const {
  std::ostringstream ss;
  for (const auto& kv : node_config_) {
    ss << DumpCallout(kv.second) << std::endl;
  }
  return ss.str();
}

}  // namespace net
}  // namespace wwiv

