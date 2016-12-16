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
#include "networkb/packets.h"
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
  for (const auto& x : context.subs.subs()) {
    for (const auto& n : x.nets) {
      if (n.net_num == context.network_number) {
        if (IsEqualsIgnoreCase(netname.c_str(), n.stype.c_str())) {
          // Since the subtype matches, we need to find the subboard base filename.
          // and return that.
          basename.assign(context.subs.sub(current).filename);
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
bool handle_post(Context& context, Packet& p) {

  ScopeExit at_exit;
  
  string raw_text = p.text;
  auto iter = raw_text.begin();
  string subtype = get_message_field(raw_text, iter, {'\0', '\r', '\n'}, 80);
  string title = get_message_field(raw_text, iter, {'\0', '\r', '\n'}, 80);
  string sender_name = get_message_field(raw_text, iter, {'\0', '\r', '\n'}, 80);
  string date_string = get_message_field(raw_text, iter, {'\0', '\r', '\n'}, 80);
  string text = string(iter, raw_text.end());
  if (VLOG_IS_ON(1)) {
    at_exit.swap([] {
      LOG(INFO) << "==============================================================";
    });
    VLOG(1) << "==============================================================";
    VLOG(1) << "  Processing New Post on subtype: " << subtype;
    VLOG(1) << "  Title:   " << title;
    VLOG(1) << "  Sender:  " << sender_name;
    VLOG(1) << "  Date:    " << date_string;
  }

  string basename;
  if (!find_basename(context, subtype, basename)) {
    LOG(INFO) << "    ! ERROR: Unable to find message of subtype: " << subtype;
    LOG(INFO) << "      title: " << title << "; writing to dead.net.";
    return write_wwivnet_packet(DEAD_NET, context.net, p);
  }

  if (!context.api.Exist(basename)) {
    LOG(INFO) << "WARNING Message area: '" << basename << "' does not exist.";;
    LOG(INFO) << "WARNING Attempting to create it.";
    // Since the area does not exist, let's create it automatically
    // like WWIV always does.
    unique_ptr<MessageArea> creator(context.api.Create(basename, -1));
    if (!creator) {
      LOG(INFO) << "    ! ERROR: Failed to create message area: " << basename << "; writing to dead.net.";
      return write_wwivnet_packet(DEAD_NET, context.net, p);
    }
  }

  unique_ptr<MessageArea> area(context.api.Open(basename, -1));
  if (!area) {
    LOG(INFO) << "    ! ERROR Unable to open message area: " << basename << "; writing to dead.net.";
    return write_wwivnet_packet(DEAD_NET, context.net, p);
  }

  if (area->Exists(p.nh.daten, title, p.nh.fromsys, p.nh.fromuser)) {
    LOG(INFO) << "    - Discarding Duplicate Message on sub: " << subtype 
              << "; title: " << title << ".";
    // Returning true since we properly handled this by discarding it.
    return true;
  }

  unique_ptr<Message> msg(area->CreateMessage());
  msg->header()->set_from_system(p.nh.fromsys);
  msg->header()->set_from_usernum(p.nh.fromuser);
  msg->header()->set_title(title);
  msg->header()->set_from(sender_name);
  msg->header()->set_daten(p.nh.daten);
  msg->text()->set_text(text);

  if (!area->AddMessage(*msg)) {
    LOG(ERROR) << "     ! Failed to add message: " << title << "; writing to dead.net";
    return write_wwivnet_packet(DEAD_NET, context.net, p);
  }
  LOG(INFO) << "    + Posted  '" << title << "' on sub: '" << subtype << "'.";
  return true;
}

}
}
}
