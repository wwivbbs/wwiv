/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*                Copyright (C)2016, WWIV Software Services               */
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

static bool find_subscriber_file_name(Context& context, const string& netname, string& basename) {
  int current = 0;
  for (const auto& x : context.xsubs) {
    for (const auto& n : x.nets) {
      if (n.net_num == context.network_number) {
        if (IsEqualsIgnoreCase(netname.c_str(), n.stype)) {
          // Since the subtype matches, we need to find the subboard base filename.
          // and return that.
          basename.assign(context.subs.at(current).filename);
          return true;
        }
      }
    }
    ++current;
  }
  return false;
}


bool ReadSubcriberFile(const std::string& dir, const std::string& filename, std::set<uint16_t>& subscribers) {
  subscribers.clear();

  TextFile file(dir, filename, "rt");
  if (!file.IsOpen()) {
    return false;
  }

  string line;
  while (file.ReadLine(&line)) {
    StringTrim(&line);
    uint16_t s = StringToUnsignedShort(line);
    if (s > 0) {
      subscribers.insert(s);
    }
  }
  return true;
}

bool WriteSubcriberFile(const std::string& dir, const std::string& filename, const std::set<uint16_t>& subscribers) {
  TextFile file(dir, filename, "wt");
  if (!file.IsOpen()) {
    return false;
  }

  for (const auto s : subscribers) {
    file.WriteLine(std::to_string(s));
  }
  return true;
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
  const std::string& subtype) {
  const string pendfile = create_pend(context.net->dir, false, 2);
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
  nh.length = text.size();  // should be subtype.size() + 2
  return write_packet(pendfile, *context.net, nh, {}, text);
}

static bool IsHostedHere(Context& context, const std::string& subtype) {
  for (const auto x : context.xsubs) {
    for (const auto n : x.nets) {
      if (IsEqualsIgnoreCase(subtype.c_str(), n.stype) && n.host == 0 && n.net_num == context.network_number) {
        return true;
      }
    }
  }
  return false;
}

bool handle_sub_add_req(Context& context, const net_header_rec& nh, const std::string& text) {
  const string subtype = SubTypeFromText(text);
  auto resp = [&subtype, &context, &nh](int code) -> bool { return send_sub_add_drop_resp(context, nh, main_type_sub_add_resp, code, subtype); };
  if (subtype.empty()) {
    return resp(sub_adddrop_error);
  }
  if (!IsHostedHere(context, subtype)) {
    // TODO send response of not hosted here.
    return resp(sub_adddrop_not_host);
  }
  string filename = StrCat("n", subtype, ".net");
  std::set<uint16_t> subscribers;
  if (!ReadSubcriberFile(context.net->dir, filename, subscribers)) {
    LOG << "Unable to read subscribers file.";
    return resp(sub_adddrop_error);
  }
  // TODO: check to see if already subscribed.
  auto result = subscribers.insert(nh.fromsys);
  if (result.second == false) {
    return resp(sub_adddrop_already_there);
  }
  if (!WriteSubcriberFile(context.net->dir, filename, subscribers)) {
    LOG << "Unable to write subscribers file.";
    return resp(sub_adddrop_error);
  }

  // success!
  LOG << "Added system @" << nh.fromsys << " to subtype: " << subtype;
  return resp(sub_adddrop_ok);
}

bool handle_sub_drop_req(Context& context, const net_header_rec& nh, const std::string& text) {
  const string subtype = SubTypeFromText(text);
  auto resp = [&subtype, &context, &nh](int code) -> bool { return send_sub_add_drop_resp(context, nh, main_type_sub_drop_resp, code, subtype); };
  if (subtype.empty()) {
    return resp(sub_adddrop_error);
  }
  if (!IsHostedHere(context, subtype)) {
    return resp(sub_adddrop_not_host);
  }
  string filename = StrCat("n", subtype, ".net");
  std::set<uint16_t> subscribers;
  if (!ReadSubcriberFile(context.net->dir, filename, subscribers)) {
    LOG << "Unable to read subscribers file.";
    return resp(sub_adddrop_error);
  }
  // TODO: check to see if already subscribed.
  set<uint16_t>::size_type num_removed = subscribers.erase(nh.fromsys);
  if (num_removed == 0) {
    return resp(sub_adddrop_not_there);
  }
  if (!WriteSubcriberFile(context.net->dir, filename, subscribers)) {
    LOG << "Unable to write subscribers file.";
    return resp(sub_adddrop_error);
  }

  // success!
  LOG << "Added system @" << nh.fromsys << " to subtype: " << subtype;
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

bool handle_sub_add_drop_resp(Context& context, const net_header_rec& nhorig, const std::string& add_or_drop, const std::string& text) {
  string subname = text;
  char code = subname.back();
  subname.pop_back();
  StringTrimEnd(&subname);

  string code_string = SubAddDropResponseMessage(static_cast<uint8_t>(code));

  net_header_rec nh = {};

  string now_human = wwiv::sdk::daten_to_date(nhorig.daten);
  string title = StringPrintf("WWIV AreaFix (%s) Response for subtype '%s'", context.net->name, now_human.c_str());
  string byname = StringPrintf("WWIV AreaFix (%s) @%u", context.net->name, nhorig.fromsys);
  string body = StringPrintf("SubType '%s', (%s) Response: '%s'", subname.c_str(), add_or_drop.c_str(), code_string.c_str());

  nh.touser = 1;
  nh.fromuser = std::numeric_limits<uint16_t>::max();
  nh.main_type = main_type_email;
  nh.daten = wwiv::sdk::time_t_to_daten(time(nullptr));

  string filename = create_pend(context.net->dir, true, context.network_number);
  return send_network(filename, *context.net, nh, {}, body, byname, title);
}

}
}
}
