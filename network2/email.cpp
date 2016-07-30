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
using namespace wwiv::stl;
using namespace wwiv::strings;

namespace wwiv {
namespace net {
namespace network2 {

// Gets the user number or 0 if it is not found.
static int GetUserNumber(const std::string name, UserManager& um) {
  auto max = um.GetNumberOfUserRecords();
  for (int i = 0; i <= max; i++) {
    User u;
    if (!um.ReadUserNoCache(&u, i)) {
      continue;
    }
    if (IsEqualsIgnoreCase(name.c_str(), u.GetName())) {
      return i;
    }
  }
  return 0;
}

bool handle_email_byname(Context& context,
  const net_header_rec& nh,
  std::vector<uint16_t>& list, const std::string& orig_text) {
  VLOG(1) << "Processing email by name.";

  auto iter = orig_text.begin();
  const string to_name = get_message_field(orig_text, iter, {'\0'}, 80);
  // Rest of the message is the text.
  const string text = string(iter, orig_text.end());

  auto user_number = GetUserNumber(to_name, context.user_manager);
  if (user_number == 0) {
    // Not found.
    LOG(ERROR) << "Received email to user: '" << to_name << "' who is not found on this system.";
    // Write it to DEAD_NET
    return write_packet(DEAD_NET, context.net, nh, list, orig_text);
  }
  
  return handle_email(context, user_number, nh, list, text);
}


bool handle_email(Context& context,
  uint16_t to_user, const net_header_rec& nh, 
  std::vector<uint16_t>& list, const string& text) {
  LOG(INFO) << "==============================================================";
  ScopeExit at_exit([] {
    LOG(INFO) << "==============================================================";
  });
  LOG(INFO) << "Processing email to user #" << to_user;

  {
    User user;
    if (!context.user_manager.ReadUser(&user, to_user)) {
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
  d.daten = nh.daten;
  d.from_network_number = context.network_number;
  d.from_system = nh.fromsys;
  d.from_user = nh.fromuser;
  // All local email should have the system number set to 0.
  d.system_number = 0;
  d.user_number = to_user;

  auto iter = text.begin();
  d.title = get_message_field(text, iter, {'\0', '\r', '\n'}, 80);
  // Rest of the message is the text.
  d.text = string(iter, text.end());
  LOG(INFO) << "  Title: '" << d.title << "'";

  std::unique_ptr<WWIVEmail> email(context.api.OpenEmail());
  bool added = email->AddMessage(d);
  if (!added) {
    LOG(ERROR) << "    ! ERROR adding email message; writing to dead.net";
    return write_packet(DEAD_NET, context.net, nh, list, text);
  }
  User user;
  context.user_manager.ReadUser(&user, d.user_number);
  int num_waiting = user.GetNumMailWaiting();
  num_waiting++;
  user.SetNumMailWaiting(num_waiting);
  context.user_manager.WriteUser(&user, d.user_number);
  LOG(INFO) << "    + Received Email  '" << d.title << "'";
  return true;
}

}
}
}
