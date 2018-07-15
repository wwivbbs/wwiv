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
#include "network2/email.h"

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
#include "networkb/binkp.h"
#include "networkb/binkp_config.h"
#include "core/connection.h"
#include "networkb/net_util.h"
#include "sdk/net/packets.h"
#include "networkb/ppp_config.h"
#include "network2/context.h"

#include "sdk/bbslist.h"
#include "sdk/callout.h"
#include "sdk/connect.h"
#include "sdk/config.h"
#include "sdk/contact.h"
#include "core/datetime.h"
#include "sdk/filenames.h"
#include "sdk/networks.h"
#include "sdk/subxtr.h"
#include "sdk/usermanager.h"
#include "sdk/msgapi/email_wwiv.h"
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
using namespace wwiv::sdk::net;
using namespace wwiv::stl;
using namespace wwiv::strings;

namespace wwiv {
namespace net {
namespace network2 {

// Gets the user number or 0 if it is not found.
static int GetUserNumber(const std::string name, UserManager& um) {
  auto max = um.num_user_records();
  for (int i = 0; i <= max; i++) {
    User u;
    if (!um.readuser_nocache(&u, i)) {
      continue;
    }
    if (IsEqualsIgnoreCase(name.c_str(), u.GetName())) {
      return i;
    }
  }
  return 0;
}

bool handle_email_byname(Context& context, Packet& p) {
  VLOG(1) << "Processing email by name.";

  auto iter = std::begin(p.text());
  const string to_name = get_message_field(p.text(), iter, {'\0'}, 80);
  // Rest of the message is the text.
  const string text = string(iter, std::end(p.text()));

  auto user_number = GetUserNumber(to_name, context.user_manager);
  if (user_number == 0) {
    // Not found.
    LOG(ERROR) << "Received email to user: '" << to_name << "' who is not found on this system.";
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

  EmailData d = {};
  d.daten = p.nh.daten;
  d.from_network_number = context.network_number;
  d.from_system = p.nh.fromsys;
  d.from_user = p.nh.fromuser;
  // All local email should have the system number set to 0.
  d.system_number = 0;
  d.user_number = to_user;

  auto iter = std::begin(p.text());
  d.title = get_message_field(p.text(), iter, {'\0', '\r', '\n'}, 80);
  // Rest of the message is the text.
  d.text = string(iter, std::end(p.text()));
  LOG(INFO) << "  Title: '" << d.title << "'";

  std::unique_ptr<WWIVEmail> email(context.email_api().OpenEmail());
  if (!email) {
    LOG(ERROR) << "    ! ERROR creating email class; writing to dead.net";
    return write_wwivnet_packet(DEAD_NET, context.net, p);
  }
  bool added = email->AddMessage(d);
  if (!added) {
    LOG(ERROR) << "    ! ERROR adding email message; writing to dead.net";
    return write_wwivnet_packet(DEAD_NET, context.net, p);
  }
  User user;
  context.user_manager.readuser(&user, d.user_number);
  int num_waiting = user.GetNumMailWaiting();
  num_waiting++;
  user.SetNumMailWaiting(num_waiting);
  context.user_manager.writeuser(&user, d.user_number);
  LOG(INFO) << "    + Received Email  '" << d.title << "'";
  return true;
}

}
}
}
