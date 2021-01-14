/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2016-2020, WWIV Software Services             */
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

#include "core/datetime.h"
#include "core/file.h"
#include "core/log.h"
#include "core/os.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/printf.h"
#include "network2/context.h"
#include "sdk/config.h"
#include "sdk/ssm.h"
#include "sdk/net/packets.h"
#include "sdk/net/subscribers.h"
#include "sdk/subxtr.h"
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

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
using namespace wwiv::sdk::net;
using namespace wwiv::stl;
using namespace wwiv::strings;

namespace wwiv::net::network2 {

struct sub_info_t {
  std::string stype;
  std::string flags;
  std::string description;
  uint16_t category = 0;
};

static string to_string(sub_info_t& s, uint16_t system_number) {
  return fmt::sprintf("%-7s %5u %-5s %s~%u", s.stype, system_number, s.flags, s.description, s.category);
}

static std::vector<string> create_sub_info(Context& context) {
  auto current = 0;
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
  auto subtype = StringTrim(text);
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
  nh.daten = daten_t_now();
  nh.fromsys = orig.tosys;
  nh.fromuser = orig.touser;
  nh.main_type = main_type;
  nh.tosys = orig.fromsys;
  nh.touser = orig.fromuser;
  string title; // empty

  auto text = subtype;
  text.push_back(0); // null after subtype.
  text.push_back(code);

  // Add "Sub type 'subtype' ('subname') + #0
  text.append(StrCat("Sub Type ", subtype));
  text.push_back(0); // null after title.

  // Add from Name + \r\n + date + \r\n + \r\n
  text.append(StrCat(context.config.sysop_name(), " #1\r\n"));
  text.append(StrCat(daten_to_wwivnet_time(daten_t_now()), "\r\n\r\n"));

  // Add the text that probably came from a SA or SR  + subtype + .net file.
  text.append(response_file_text);

  nh.length = size_int(text); // should be subtype.size() + 2
  const auto pendfile = create_pend(context.net.dir, false, '2');
  const Packet packet(nh, {}, text);
  return write_wwivnet_packet(pendfile, context.net, packet);
}

static bool IsHostedHere(Context& context, const std::string& subtype) {
  for (const auto& x : context.subs.subs()) {
    for (const auto& n : x.nets) {
      if (iequals(subtype, n.stype) 
          && n.host == 0
          && n.net_num == context.network_number) {
        return true;
      }
    }
  }
  return false;
}

bool handle_sub_add_req(Context& context, Packet& p) {
  const auto subtype = SubTypeFromText(p.text());
  const auto resp = [&](uint8_t code) -> bool {
    const string base = (code == sub_adddrop_ok) ? "sa" : "sr";
    const auto response_file = StrCat(base, subtype, ".net");
    string text;
    LOG(INFO) << "Candidate sa file: " << FilePath(context.net.dir, response_file).string();
    if (File::Exists(FilePath(context.net.dir, response_file))) {
      TextFile tf(FilePath(context.net.dir, response_file), "r");
      LOG(INFO) << "Sending SA file: " << tf;
      text = tf.ReadFileIntoString();
    }
    return send_sub_add_drop_resp(context, p.nh, main_type_sub_add_resp, code, subtype, text);
  };
  if (subtype.empty()) {
    return resp(sub_adddrop_error);
  }
  if (!IsHostedHere(context, subtype)) {
    const auto msg = fmt::format("Can't add system @{} to subtype: {}, it's not hosted here",p.nh.fromsys, subtype);
    context.ssm.send_local(1, msg);
    LOG(ERROR) << msg;
    return resp(sub_adddrop_not_host);
  }
  const auto filename = StrCat("n", subtype, ".net");
  std::set<uint16_t> subscribers;
  if (!ReadSubcriberFile(FilePath(context.net.dir, filename), subscribers)) {
    const std::string msg = "Unable to read subscribers file.";
    LOG(WARNING) << msg;
    return resp(sub_adddrop_error);
  }
  const auto result = subscribers.insert(p.nh.fromsys);
  if (result.second == false) {
    context.ssm.send_local(1, fmt::format("Can't add system @{} to subtype: {}, it's already there.",p.nh.fromsys, subtype));
    return resp(sub_adddrop_already_there);
  }
  if (!WriteSubcriberFile(FilePath(context.net.dir, filename), subscribers)) {
    LOG(INFO) << "Unable to write subscribers file.";
    context.ssm.send_local(1, fmt::format("Can't add system @{} to subtype: {}, failed to write subscriber file.",p.nh.fromsys, subtype));
    return resp(sub_adddrop_error);
  }

  // success!
  const auto msg = fmt::format("Added System @{} to subtype: {}",p.nh.fromsys, subtype);
  LOG(INFO) << msg;
  context.ssm.send_local(1, msg);
  return resp(sub_adddrop_ok);
}

bool handle_sub_drop_req(Context& context, Packet& p) {
  const auto subtype = SubTypeFromText(p.text());
  const auto resp = [&](uint8_t code) -> bool { 
    return send_sub_add_drop_resp(context, p.nh, main_type_sub_drop_resp, code, subtype, ""); 
  };
  if (subtype.empty()) {
    return resp(sub_adddrop_error);
  }
  if (!IsHostedHere(context, subtype)) {
    return resp(sub_adddrop_not_host);
  }
  const auto filename = StrCat("n", subtype, ".net");
  std::set<uint16_t> subscribers;
  if (!ReadSubcriberFile(FilePath(context.net.dir, filename), subscribers)) {
    LOG(INFO) << "Unable to read subscribers file.";
    return resp(sub_adddrop_error);
  }
  const auto num_removed = subscribers.erase(p.nh.fromsys);
  if (num_removed == 0) {
    return resp(sub_adddrop_not_there);
  }
  if (!WriteSubcriberFile(FilePath(context.net.dir, filename), subscribers)) {
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
  default:
    return fmt::format("Unknown response code {}", code);
  }

}

bool handle_sub_add_drop_resp(Context& context, Packet& p, const std::string& add_or_drop) {
  // We want to stop at the 1st \0
  auto subname = p.text();
  StringTrimEnd(&subname);

  auto b = std::begin(p.text());
  while (b != std::end(p.text()) && *b != '\0') {
    ++b;
  }
  if (b == std::end(p.text())) {
    LOG(INFO) << "Unable to determine code from add_drop response.";
    return false;
  } // NULL
  ++b;
  if (b == std::end(p.text())) {
    LOG(INFO) << "Unable to determine code from add_drop response.";
    return false;
  }

  LOG(INFO) << "Processed " << add_or_drop << " response from system @" << p.nh.fromsys << " to subtype: " << subname;

  auto code = *b++;
  auto code_string = SubAddDropResponseMessage(static_cast<uint8_t>(code));

  auto orig_title = get_message_field(p.text(), b, {'\0', '\r', '\n'}, 80);
  auto sender_date = get_message_field(p.text(), b, {'\0', '\r', '\n'}, 80);
  auto orig_date = get_message_field(p.text(), b, {'\0', '\r', '\n'}, 80);

  auto message_text = string(b, std::end(p.text()));
  net_header_rec nh = {};

  auto title = StrCat("WWIV AreaFix (", context.net.name, ") Response for subtype '", subname, "'");
  auto byname = StrCat("WWIV AreaFix (", context.net.name, ") @", p.nh.fromsys);
  auto body =
      StrCat("SubType '", subname, "', (", add_or_drop, ") Response: '", code_string, "'\r\n");
  body.append(message_text);

  nh.touser = 1;
  nh.fromuser = std::numeric_limits<uint16_t>::max();
  nh.main_type = main_type_email;
  nh.daten = daten_t_now();

  const auto filename = create_pend(context.net.dir, true, '2');
  return send_network_email(filename, context.net, nh, {}, body, byname, title);
}

bool handle_sub_list_info_response(Context& context, Packet& p) {
  TextFile subs_inf(FilePath(context.net.dir, "subs.inf"), "at");
  LOG(INFO) << "Received subs line for subs.inf:";
  LOG(INFO) << p.text();
  return subs_inf.Write(p.text()) > 0;
}

bool handle_sub_list_info_request(Context& context, Packet& p) {

  net_header_rec nh{};
  nh.fromsys = p.nh.tosys;
  nh.fromuser = p.nh.touser;
  nh.tosys = p.nh.fromsys;
  nh.touser = p.nh.fromuser;
  nh.main_type = main_type_sub_list_info;
  nh.minor_type = 1;
  nh.daten = daten_t_now();

  const auto lines = create_sub_info(context);
  const auto text = JoinStrings(lines, "\r\n");
  nh.length = size_int(text);

  LOG(INFO) << "Sending subs line for subs.inf:";
  LOG(INFO) << text;

  const auto pendfile = create_pend(context.net.dir, false, '2');
  const Packet np(nh, {}, text);
  return write_wwivnet_packet(pendfile, context.net, np);
}


}
