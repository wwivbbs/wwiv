#include "networkb/callout.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <map>
#include <sstream>
#include <string>

#include "core/strings.h"
#include "core/inifile.h"
#include "core/file.h"
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
namespace net {

// [[ VisibleForTesting ]]
bool ParseCalloutNetLine(const string& ss, net_call_out_rec* con) {
  if (ss.empty() || ss[0] != '@') {
    // skip empty lines and those not starting with @.
    return false;
  }
  memset(con, 0, sizeof(net_call_out_rec));
  con->min_hr = -1;
  con->max_hr = -1;

  for (auto iter = ss.cbegin(); iter != ss.cend(); iter++) {
      switch (*iter) {
      case '@': {
        con->sysnum = StringToUnsignedShort(string(++iter, ss.end()));
      } break;
      case '&':
        con->options |= options_sendback;
        break;
      case '-':
        con->options |= options_ATT_night;
        break;
      case '_':
        con->options |= options_ppp;
        break;
      case '+':
        con->options |= options_no_call;
        break;
      case '~':
        con->options |= options_receive_only;
        break;
      case '!': {
        con->options |= options_once_per_day;
        con->times_per_day = std::max<uint8_t>(1, StringToUnsignedChar(string(++iter, ss.end())));
      } break;
      case '%': {
        con->macnum = StringToUnsignedChar(string(++iter, ss.end()));
      } break;
      case '/': {
        con->call_anyway = StringToUnsignedChar(string(++iter, ss.end()));
      } break;
      case '#': {
        con->call_x_days = StringToUnsignedChar(string(++iter, ss.end()));
      } break;
      case '(': {
        con->min_hr = StringToChar(string(++iter, ss.end()));
      } break;
      case ')': {
        con->max_hr = StringToChar(string(++iter, ss.end()));
      } break;
      case '|': {
        con->min_k = std::max<uint16_t>(1, StringToUnsignedShort(string(++iter, ss.end())));
      } break;
      case ';':
        con->options |= options_compress;
        break;
      case '^':
        con->options |= options_hslink;
        break;
      case '$':
        con->options |= options_force_ac;
        break;
      case '=':
        con->options |= options_hide_pend;
        break;
      case '*':
        con->options |= options_dial_ten;
        break;
      case '\"': {
        ++iter;  // skip past first "
        string password;
        while (iter != ss.end() && *iter != '\"') {
          password.push_back(*iter++);
        }
        if (password.back() == '\"') {
          // remove trailing "
          password.pop_back();
        }
        strncpy(con->password, password.c_str(), sizeof(con->password));
      }
      break;
      default:
        break;
      }
    }
    return true;
}

static bool ParseCalloutFile(std::map<uint16_t, net_call_out_rec>* node_config_map, const string network_dir) {
  TextFile node_config_file(network_dir, CALLOUT_NET, "rt");
  if (!node_config_file.IsOpen()) {
    return false;
  }
  // A line will be of the format @node host:port [password].
  string line;
  while (node_config_file.ReadLine(&line)) {
    net_call_out_rec node_config;
    if (ParseCalloutNetLine(line, &node_config)) {
      // Parsed a line correctly.
      node_config_map->emplace(node_config.sysnum, node_config);
    }
  }
  return true;
}

Callout::Callout(const string& network_dir) {
  ParseCalloutFile(&node_config_, network_dir);
}

Callout::Callout(std::initializer_list<net_call_out_rec> l) {
  for (const auto& r : l) {
    node_config_.emplace(r.sysnum, r);
  }
}

Callout::~Callout() {}

const net_call_out_rec* Callout::node_config_for(int node) const {
  auto iter = node_config_.find(node);
  if (iter != end(node_config_)) {
    return &iter->second;
  }
  return nullptr;
}

static std::string DumpCallout(const net_call_out_rec& n) {
  std::ostringstream ss;
  ss << "sysnum:        "  << n.sysnum << std::endl;
  if (n.macnum) {
    ss << "macnum:        " << std::dec << n.macnum << std::endl;
  }
  if (n.options) {
    ss << "options:       " << n.options << std::endl;
  }
  if (n.min_hr > 0) {
    ss << "min_hr:        " << static_cast<int>(n.min_hr) << std::endl;
  }
  if (n.max_hr > 0) {
    ss << "max_hr:        " <<  static_cast<int>(n.max_hr) << std::endl;
  }
  ss << "password:      \"" << n.password << "\"" << std::endl;
  if (n.times_per_day) {
    ss << "times_per_day: " << static_cast<int>(n.times_per_day) << std::endl;
  }
  if (n.call_x_days) {
    ss << "call_x_days:   " << static_cast<int>(n.call_x_days) << std::endl;
  }
  if (n.min_k) {
    ss << "min_k:         " << static_cast<int>(n.min_k) << std::endl;
  }
  if (n.opts && *n.opts) {
    ss << "opts:          " << n.opts << std::endl;
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

