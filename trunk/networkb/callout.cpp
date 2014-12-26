#include "networkb/callout.h"

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

  int p=0;
  while (p < ss.size()) {
      switch (ss[p]) {
      case '@':
        ++p;
        con->macnum     = 0;
        con->options    = 0;
        con->call_anyway  = 0;
        con->password[0]  = 0;
        con->sysnum     = StringToUnsignedShort(&(ss[p]));
        con->min_hr     = -1;
        con->max_hr     = -1;
        con->times_per_day  = 0;
        con->min_k      = 0;
        con->call_x_days  = 0;
        break;
      case '&':
        con->options |= options_sendback;
        ++p;
        break;
      case '-':
        con->options |= options_ATT_night;
        ++p;
        break;
      case '_':
        con->options |= options_ppp;
        ++p;
        break;
      case '+':
        con->options |= options_no_call;
        ++p;
        break;
      case '~':
        con->options |= options_receive_only;
        ++p;
        break;
      case '!':
        con->options |= options_once_per_day;
        ++p;
        con->times_per_day = StringToUnsignedChar(&(ss[p]));
        if (!con->times_per_day) {
          con->times_per_day = 1;
        }
        break;
      case '%':
        ++p;
        con->macnum = StringToUnsignedChar(&(ss[p]));
        break;
      case '/':
        ++p;
        con->call_anyway = StringToUnsignedChar(&(ss[p]));
        ++p;
        break;
      case '#':
        ++p;
        con->call_x_days = StringToUnsignedChar(&(ss[p]));
        break;
      case '(':
        ++p;
        con->min_hr = StringToChar(&(ss[p]));
        break;
      case ')':
        ++p;
        con->max_hr = StringToChar(&(ss[p]));
        break;
      case '|':
        ++p;
        con->min_k = StringToUnsignedShort(&(ss[p]));
        if (!con->min_k) {
          con->min_k = 0;
        }
        break;
      case ';':
        ++p;
        con->options |= options_compress;
        break;
      case '^':
        ++p;
        con->options |= options_hslink;
        break;
      case '$':
        ++p;
        con->options |= options_force_ac;
        break;
      case '=':
        ++p;
        con->options |= options_hide_pend;
        break;
      case '*':
        ++p;
        con->options |= options_dial_ten;
        break;
      case '\"': {
        ++p;
        int i = 0;
        while ((i < 19) && (ss[p + static_cast<long>(i)] != '\"')) {
          ++i;
        }
        for (int i1 = 0; i1 < i; i1++) {
          con->password[i1] = ss[p+i1];
        }
        con->password[i] = 0;
        p += (i + 1);
      }
      break;
      default:
        ++p;
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

