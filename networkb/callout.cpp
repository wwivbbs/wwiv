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

using std::clog;
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

Callout::~Callout() {}

const net_call_out_rec* Callout::node_config_for(int node) const {
  auto iter = node_config_.find(node);
  if (iter != end(node_config_)) {
    return &iter->second;
  }
  return nullptr;
}


}  // namespace net
}  // namespace wwiv

