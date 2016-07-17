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
#include "network2/post.h"

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


static bool find_basename(Context& context, const string& netname, string& basename) {
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

// Alpha subtypes are seven characters -- the first must be a letter, but the rest can be any
// character allowed in a DOS filename.This main_type covers both subscriber - to - host and 
// host - to - subscriber messages. Minor type is always zero(since it's ignored), and the
// subtype appears as the first part of the message text, followed by a NUL.Thus, the message
// header info at the beginning of the message text is in the format 
// SUBTYPE<nul>TITLE<nul>SENDER_NAME<cr / lf>DATE_STRING<cr / lf>MESSAGE_TEXT.
bool handle_post(Context& context, const net_header_rec& nh,
  std::vector<uint16_t>& list, const string& raw_text) {

  ScopeExit at_exit([] {
    LOG << "==============================================================";
  });
  auto iter = raw_text.begin();
  string subtype = get_message_field(raw_text, iter, {'\0', '\r', '\n'}, 80);
  string title = get_message_field(raw_text, iter, {'\0', '\r', '\n'}, 80);
  string sender_name = get_message_field(raw_text, iter, {'\0', '\r', '\n'}, 80);
  string date_string = get_message_field(raw_text, iter, {'\0', '\r', '\n'}, 80);
  string text = string(iter, raw_text.end());
  LOG << "==============================================================";
  LOG << "  Processing New Post on subtype: " << subtype;
  LOG << "  title:   " << title;
  LOG << "  sender:  " << sender_name;
  LOG << "  date:    " << date_string;

  string basename;
  if (!find_basename(context, subtype, basename)) {
    LOG << "ERROR: Unable to find subtype of subtype: " << subtype;
    return false;
  }

  if (!context.api->Exist(basename)) {
    LOG << "WARNING Message area: '" << basename << "' does not exist.";;
    LOG << "WARNING Attempting to create it.";
    // Since the area does not exist, let's create it automatically
    // like WWIV alwyas does.
    unique_ptr<MessageArea> creator(context.api->Create(basename));
    if (!creator) {
      LOG << "ERROR: Failed to create message area: " << basename << ". Exiting.";
      return false;
    }
  }

  unique_ptr<MessageArea> area(context.api->Open(basename));
  if (!area) {
    LOG << "ERROR Unable to open message area: '" << basename << "'.";
    return false;
  }

  unique_ptr<Message> msg(area->CreateMessage());
  msg->header()->set_from_system(nh.fromsys);
  msg->header()->set_from_usernum(nh.fromuser);
  msg->header()->set_title(title);
  msg->header()->set_from(sender_name);
  msg->header()->set_daten(nh.daten);
  msg->text()->set_text(text);

  const int num_messages = area->number_of_messages();
  for (int current = 1; current <= num_messages; current++) {
    unique_ptr<MessageHeader> header(area->ReadMessageHeader(current));
    if (!header) {
      continue;
    }

    // Since we don't have a global message id, use the combination of
    // date + title + from system + from user.
    if (header->daten() == nh.daten
      && header->title() == title
      && header->from_system() == nh.fromsys
      && header->from_usernum() == nh.fromuser) {
      LOG << "  Discarding Duplicate Message.";
      return false;
    }
  }

  bool result = area->AddMessage(*msg);
  if (!result) {
    LOG << "  Failed to add message: " << title;
    return false;
  }
  LOG << "    + Posted  '" << title << "'";
  return true;
}

}
}
}
