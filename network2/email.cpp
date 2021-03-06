/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2016-2021, WWIV Software Services             */
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
#include "network2/email.h"

// WWIV5 Network2
#include "core/log.h"
#include "core/os.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "network2/context.h"
#include "net_core/net_cmdline.h"
#include "sdk/filenames.h"
#include "sdk/usermanager.h"
#include "sdk/msgapi/email_wwiv.h"
#include "sdk/msgapi/message_api_wwiv.h"
#include "sdk/net/packets.h"
#include <iostream>
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

// Gets the user number or 0 if it is not found.
static int GetUserNumber(const std::string& name, UserManager& um) {
  const auto max = um.num_user_records();
  auto realname_pos = 0;
  for (auto i = 0; i <= max; i++) {
    auto u = um.readuser_nocache(i);
    if (!u) {
      continue;
    }
    if (iequals(name, u->name())) {
      return i;
    }
    if (const auto matches_realname = iequals(name, u->real_name());
        matches_realname && realname_pos == 0) {
      realname_pos = i;
    } else if (matches_realname && realname_pos != 0) {
      LOG(WARNING) << "Duplicate real names";
    }
  }
  // If we didn't find a handle, use the first known position
  // of the real name.  These are not guaranteed to be unique
  // like the handles are.
  return realname_pos;
}

bool handle_email_byname(Context& context, Packet& p) {
  auto iter = std::begin(p.text());
  const auto to_name = get_message_field(p.text(), iter, {'\0'}, 80);
  VLOG(1) << "Processing email by name to: '" << to_name << "'";

  // Rest of the message is the text.
  const auto text = string(iter, std::end(p.text()));

  const auto user_number = GetUserNumber(to_name, context.user_manager);
  if (user_number == 0) {
    // Not found.
    LOG(ERROR) << "    ! ERROR Received email to user: '" << to_name << "' who is not found on this system; writing to dead.net";
    // Write it to DEAD_NET
    return write_wwivnet_packet(DEAD_NET, context.net, p);
  }
  
  p.set_text(text);
  return handle_email(context, static_cast<uint16_t>(user_number), p);
}


bool handle_email(Context& context,
  uint16_t to_user, Packet& p) {
  LOG(INFO) << "==============================================================";
  ScopeExit at_exit([] {
    LOG(INFO) << "==============================================================";
  });
  LOG(INFO) << "Processing email to user #" << to_user;

  {
    User user;
    if (!context.user_manager.readuser(&user, to_user)) {
      // Unable to read user.
      LOG(INFO) << "ERROR: Unable to read user #" << to_user << ". Discarding message.";
      return false;
    }

    if (user.IsUserDeleted()) {
      LOG(INFO) << "User #" << to_user << " is deleted. Discarding message.";
      return false;
    }
  }

  EmailData d{};
  d.daten = p.nh.daten;
  d.from_network_number = context.network_number;
  d.from_system = p.nh.fromsys;
  d.from_user = p.nh.fromuser;
  // All local email should have the system number set to 0.
  d.system_number = 0;
  d.user_number = to_user;

  auto iter = std::begin(p.text());
  d.title = get_message_field(p.text(), iter, {'\0', '\r', '\n'}, 80);
  // Rest of the message is the text of the form:
  // SENDER_NAME<cr/lf>DATE_STRING<cr/lf>MESSAGE_TEXT.
  d.text = string(iter, std::end(p.text()));
  LOG(INFO) << "  Title: '" << d.title << "'";

  auto email = context.email_api().OpenEmail();
  if (!email) {
    LOG(ERROR) << "    ! ERROR creating email class; writing to dead.net";
    return write_wwivnet_packet(DEAD_NET, context.net, p);
  }
  auto added = email->AddMessage(d);
  if (!added) {
    LOG(ERROR) << "    ! ERROR adding email message; writing to dead.net";
    return write_wwivnet_packet(DEAD_NET, context.net, p);
  }
  User user;
  context.user_manager.readuser(&user, d.user_number);
  auto num_waiting = user.email_waiting();
  num_waiting++;
  user.email_waiting(num_waiting);
  context.user_manager.writeuser(&user, d.user_number);
  LOG(INFO) << "    + Received Email  '" << d.title << "'";
  return true;
}

}
