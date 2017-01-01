/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2016-2017, WWIV Software Services             */
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
#include "network2/subs.h"

// WWIV5 Network2
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <set>
#include <string>
#include <vector>

#include "core/command_line.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/log.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/os.h"
#include "core/textfile.h"
#include "core/wfndfile.h"
#include "networkb/binkp.h"
#include "networkb/binkp_config.h"
#include "networkb/connection.h"
#include "networkb/net_util.h"
#include "networkb/packets.h"
#include "networkb/ppp_config.h"
#include "network2/context.h"
#include "network2/email.h"

#include "sdk/bbslist.h"
#include "sdk/callout.h"
#include "sdk/connect.h"
#include "sdk/config.h"
#include "sdk/contact.h"
#include "sdk/datetime.h"
#include "sdk/filenames.h"
#include "sdk/networks.h"
#include "sdk/subscribers.h"
#include "sdk/subxtr.h"
#include "sdk/usermanager.h"
#include "sdk/msgapi/msgapi.h"
#include "sdk/msgapi/message_api_wwiv.h"

using std::cout;
using std::endl;
using std::make_unique;
using std::map;
using std::set;
using std::string;
using std::unique_ptr;
using std::vector;

using namespace wwiv::core;
using namespace wwiv::net;
using namespace wwiv::os;
using namespace wwiv::sdk;
using namespace wwiv::sdk::msgapi;
using namespace wwiv::stl;
using namespace wwiv::strings;

namespace wwiv {
namespace net {
namespace network2 {

struct sub_info_t {
  std::string stype;
  std::string flags;
  std::string description;
  uint16_t category = 0;
};

static string to_string(sub_info_t& s, uint16_t system_number) {
  return StringPrintf("%-7s %5u %-5s %s~%u", s.stype.c_str(), system_number, s.flags.c_str(), s.description.c_str(), s.category);
}

static std::vector<string> create_sub_info(Context& context) {
  int current = 0;
  std::vector <std::string> result;
  for (const auto& x : context.subs.subs()) {
    for (const auto& n : x.nets) {
      if (n.net_num != context.network_number) {
        continue;
      }
      if (n.host != 0) {
        continue;
      }
      if (!(n.flags & XTRA_NET_AUTO_INFO)) {
        // Not allowed to subs.lst info for sub
        continue;
      }
      sub_info_t s;
      const auto& b = context.subs.sub(current);
      s.stype = n.stype;
      s.category = n.category;
      s.description = stripcolors(x.desc);
      if (s.description.empty()) {
        s.description = stripcolors(context.subs.sub(current).name);
      }
      if (s.description.size() > 60) {
        s.description.resize(60);
      }
      if (b.anony & anony_ansi_only) {
        s.flags += 'A';
      }
      if (x.nets.size() > 1) {
        s.flags += 'G';
      }
      if (b.anony & anony_val_net) {
        s.flags += 'N';
      }
      if (n.flags & XTRA_NET_AUTO_ADDDROP) {
        s.flags += 'R';
      }
      if (b.anony & anony_no_tag) {
        s.flags += 'T';
      }
      result.emplace_back(to_string(s, context.net.sysnum));
    }
    ++current;
  }
  return result;
}

static string SubTypeFromText(const std::string& text) {
  string subtype = text;
  StringTrim(&subtype);
  if (subtype.back() == '\0') subtype.pop_back();
  if (subtype.size() > 7) {
    return "";
  }
  return subtype;
}

static bool send_sub_add_drop_resp(Context& context, 
  net_header_rec orig,
  uint8_t main_type, uint8_t code,
  const std::string& subtype,
  const std::string& response_file_text) {
  net_header_rec nh = {};
  nh.daten = time_t_to_daten(time(nullptr));
  nh.fromsys = orig.tosys;
  nh.fromuser = orig.touser;
  nh.main_type = main_type;
  nh.tosys = orig.fromsys;
  nh.touser = orig.fromuser;
  string title; // empty

  string text = subtype;
  text.push_back(0); // null after subtype.
  text.push_back(code);

  // Add "Sub type 'subtype' ('subname') + #0
  text.append(StrCat("Sub Type ", subtype));
  text.push_back(0); // null after title.

  // Add from Name + \r\n + date + \r\n + \r\n
  text.append(StrCat(context.config.config()->sysopname, " #1\r\n"));
  text.append(StrCat(daten_to_wwivnet_time(time(nullptr)), "\r\n\r\n"));

  // TODO: Add SA or SR  + subtype + .net file text
  text.append(response_file_text);

  nh.length = text.size();  // should be subtype.size() + 2
  const string pendfile = create_pend(context.net.dir, false, '2');
  Packet packet(nh, {}, std::move(text));
  return write_wwivnet_packet(pendfile, context.net, packet);
}

static bool IsHostedHere(Context& context, const std::string& subtype) {
  for (const auto x : context.subs.subs()) {
    for (const auto n : x.nets) {
      if (IsEqualsIgnoreCase(subtype.c_str(), n.stype.c_str()) 
          && n.host == 0
          && n.net_num == context.network_number) {
        return true;
      }
    }
  }
  return false;
}

bool handle_sub_add_req(Context& context, Packet& p) {
  const string subtype = SubTypeFromText(p.text);
  auto resp = [&](int code) -> bool { 
    string base = (code == sub_adddrop_ok) ? "sa" : "sr";
    string response_file = StrCat(base, subtype, ".net");
    string text;
    LOG(INFO) << "Candidate sa file: " << FilePath(context.net.dir, response_file);
    if (File::Exists(context.net.dir, response_file)) {
      TextFile tf(context.net.dir, response_file, "r");
      LOG(INFO) << "Sending SA file: " << tf.full_pathname();
      text = tf.ReadFileIntoString();
    }
    return send_sub_add_drop_resp(context, p.nh, main_type_sub_add_resp, code, subtype, text);
  };
  if (subtype.empty()) {
    return resp(sub_adddrop_error);
  }
  if (!IsHostedHere(context, subtype)) {
    return resp(sub_adddrop_not_host);
  }
  string filename = StrCat("n", subtype, ".net");
  std::set<uint16_t> subscribers;
  if (!ReadSubcriberFile(context.net.dir, filename, subscribers)) {
    LOG(INFO) << "Unable to read subscribers file.";
    return resp(sub_adddrop_error);
  }
  auto result = subscribers.insert(p.nh.fromsys);
  if (result.second == false) {
    return resp(sub_adddrop_already_there);
  }
  if (!WriteSubcriberFile(context.net.dir, filename, subscribers)) {
    LOG(INFO) << "Unable to write subscribers file.";
    return resp(sub_adddrop_error);
  }

  // success!
  LOG(INFO) << "Added system @" << p.nh.fromsys << " to subtype: " << subtype;
  return resp(sub_adddrop_ok);
}

bool handle_sub_drop_req(Context& context, Packet& p) {
  const string subtype = SubTypeFromText(p.text);
  auto resp = [&](int code) -> bool { 
    return send_sub_add_drop_resp(context, p.nh, main_type_sub_drop_resp, code, subtype, ""); 
  };
  if (subtype.empty()) {
    return resp(sub_adddrop_error);
  }
  if (!IsHostedHere(context, subtype)) {
    return resp(sub_adddrop_not_host);
  }
  string filename = StrCat("n", subtype, ".net");
  std::set<uint16_t> subscribers;
  if (!ReadSubcriberFile(context.net.dir, filename, subscribers)) {
    LOG(INFO) << "Unable to read subscribers file.";
    return resp(sub_adddrop_error);
  }
  set<uint16_t>::size_type num_removed = subscribers.erase(p.nh.fromsys);
  if (num_removed == 0) {
    return resp(sub_adddrop_not_there);
  }
  if (!WriteSubcriberFile(context.net.dir, filename, subscribers)) {
    LOG(INFO) << "Unable to write subscribers file.";
    return resp(sub_adddrop_error);
  }

  // success!
  LOG(INFO) << "Dropped system @" << p.nh.fromsys << " to subtype: " << subtype;
  return resp(sub_adddrop_ok);
}


static string SubAddDropResponseMessage(uint8_t code) {
  switch (code) {
  case sub_adddrop_already_there: return "You are already there";
  case sub_adddrop_error: return "Error Adding or Droppign Sub";
  case sub_adddrop_not_allowed: return "Not allowed to add or drop this sub";
  case sub_adddrop_not_host: return "This system is not the host";
  case sub_adddrop_not_there: return "You were not subscribed to the sub";
  case sub_adddrop_ok: return "Add or Drop successful";
  default: return StringPrintf("Unknown response code %d", code);
  }

}

bool handle_sub_add_drop_resp(Context& context, Packet& p, const std::string& add_or_drop) {
  // We want to stop at the 1st \0
  string subname = p.text.c_str();
  StringTrimEnd(&subname);

  auto b = p.text.begin();
  while (b != p.text.end() && *b != '\0') { b++; }
  if (b == p.text.end()) {
    LOG(INFO) << "Unable to determine code from add_drop response.";
    return false;
  } // NULL
  b++;
  if (b == p.text.end()) {
    LOG(INFO) << "Unable to determine code from add_drop response.";
    return false;
  }

  LOG(INFO) << "Processed " << add_or_drop << " response from system @" << p.nh.fromsys << " to subtype: " << subname;

  char code = *b++;
  string code_string = SubAddDropResponseMessage(static_cast<uint8_t>(code));

  string orig_title = get_message_field(p.text, b, { '\0', '\r', '\n' }, 80);
  string sender_date = get_message_field(p.text, b, { '\0', '\r', '\n' }, 80);
  string orig_date = get_message_field(p.text, b, { '\0', '\r', '\n' }, 80);

  string message_text = string(b, p.text.end());
  net_header_rec nh = {};

  string title = StrCat("WWIV AreaFix (", context.net.name, ") Response for subtype '", subname, "'");
  string byname = StrCat("WWIV AreaFix (", context.net.name, ") @", p.nh.fromsys);
  string body = StrCat("SubType '", subname, "', (", add_or_drop, ") Response: '", code_string, "'\r\n");
  body.append(message_text);

  nh.touser = 1;
  nh.fromuser = std::numeric_limits<uint16_t>::max();
  nh.main_type = main_type_email;
  nh.daten = wwiv::sdk::time_t_to_daten(time(nullptr));

  const string filename = create_pend(context.net.dir, true, static_cast<char>('0' + context.network_number));
  return send_network_email(filename, context.net, nh, {}, body, byname, title);
}

bool handle_sub_list_info_response(Context& context, Packet& p) {
  TextFile subs_inf(context.net.dir, "subs.inf", "at");
  LOG(INFO) << "Received subs line for subs.inf:";
  LOG(INFO) << p.text;
  return subs_inf.Write(p.text) > 0;
}

bool handle_sub_list_info_request(Context& context, Packet& p) {

  net_header_rec nh{};
  nh.fromsys = p.nh.tosys;
  nh.fromuser = p.nh.touser;
  nh.tosys = p.nh.fromsys;
  nh.touser = p.nh.fromuser;
  nh.main_type = main_type_sub_list_info;
  nh.minor_type = 1;
  nh.daten = wwiv::sdk::time_t_to_daten(time(nullptr));

  auto lines = create_sub_info(context);
  string text = JoinStrings(lines, "\r\n");
  nh.length = text.size();

  LOG(INFO) << "Sending subs line for subs.inf:";
  LOG(INFO) << text;

  const string pendfile = create_pend(context.net.dir, false, '2');
  Packet np(nh, {}, text);
  return write_wwivnet_packet(pendfile, context.net, np);
}


}
}
}
