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

bool handle_email(Context& context,
  uint16_t to_user, const net_header_rec& nh, const string& text) {
  LOG << "==============================================================";
  ScopeExit at_exit([] {
    LOG << "==============================================================";
  });
  LOG << "Processing email to user #" << to_user;

  {
    User user;
    if (!context.user_manager->ReadUser(&user, to_user)) {
      // Unable to read user.
      LOG << "ERROR: Unable to read user #" << to_user << ". Discarding message.";
      return false;
    }

    if (user.IsUserDeleted()) {
      LOG << "User #" << to_user << " is deleted. Discarding message.";
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
  d.user_number = nh.touser;

  auto iter = text.begin();
  d.title = get_message_field(text, iter, {'\0', '\r', '\n'}, 80);
  // Rest of the message is the text.
  d.text = string(iter, text.end());
  LOG << "  Title: '" << d.title << "'";

  std::unique_ptr<WWIVEmail> email(context.api->OpenEmail());
  bool added = email->AddMessage(d);
  if (!added) {
    LOG << "    ! ERROR adding email message.";
    return false;
  }
  User user;
  context.user_manager->ReadUser(&user, d.user_number);
  int num_waiting = user.GetNumMailWaiting();
  num_waiting++;
  user.SetNumMailWaiting(num_waiting);
  context.user_manager->WriteUser(&user, d.user_number);
  LOG << "    + Received Email  '" << d.title << "'";
  return true;
}

}
}
}