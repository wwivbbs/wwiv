/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*               Copyright (C)2016 WWIV Software Services                 */
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
#include "networkb/fido_util.h"

#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "core/command_line.h"
#include "core/file.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
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
namespace net {
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

std::string flo_name(const wwiv::sdk::fido::FidoAddress& source, const wwiv::sdk::fido::FidoAddress& dest, fido_bundle_status_t status) {
  string extension = "flo";
  if (status != fido_bundle_status_t::unknown) {
    extension[0] = static_cast<char>(status);
  }
  return bundle_name(source, dest, extension);
}

std::string bundle_name(const wwiv::sdk::fido::FidoAddress& source, const wwiv::sdk::fido::FidoAddress& dest, int dow, int bundle_number) {
  return bundle_name(source, dest, dow_extension(dow, bundle_number));
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
    // tm_year is since 1900.
    year -= 1900;
    t->tm_year;

    string hms;
    stream >> hms;
    vector<string> parts = SplitString(hms, ":");
    t->tm_hour = StringToInt(parts.at(0)) - 1;
    t->tm_min = StringToInt(parts.at(1));
    t->tm_sec = StringToInt(parts.at(2));

    return mktime(t);
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
  return temp;
}

FidoAddress get_address_from_origin(const std::string& text) {
  vector<string> lines = split_message(text);
  for (const auto& line : lines) {
    if (starts_with(line, " * Origin:")) {
      size_t start = line.find_last_of('(');
      size_t end = line.find_last_of(')');
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
  }
  return FidoAddress(0, 0, 0, 0, "");
}
}  // namespace fido
}  // namespace net
}  // namespace wwiv

