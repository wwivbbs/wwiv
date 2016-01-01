/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*                Copyright (C)2015 WWIV Software Services                */
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
#include "wwivutil/messages/messages.h"

#include <cstdio>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "core/command_line.h"
#include "core/file.h"
#include "core/strings.h"
#include "sdk/config.h"
#include "sdk/net.h"
#include "sdk/networks.h"
#include "sdk/msgapi/msgapi.h"
#include "sdk/msgapi/message_api_wwiv.h"

using std::cerr;
using std::clog;
using std::cout;
using std::endl;
using std::setw;
using std::string;
using std::unique_ptr;
using std::vector;
using wwiv::core::BooleanCommandLineArgument;
using namespace wwiv::sdk;
using namespace wwiv::sdk::msgapi;

namespace wwiv {
namespace wwivutil {

bool MessagesCommand::AddSubCommands() {
  MessagesDumpHeaderCommand* dump = new MessagesDumpHeaderCommand();
  if (!add(dump)) { return false; }
  AddCommandsAndArgs(dump);
  return true;
}

MessagesDumpHeaderCommand::MessagesDumpHeaderCommand()
  : UtilCommand("dump", "Displays message header and text information.") {}

static void messages_usage() {
  cout << "Usage:   dump_headers <base sub filename>" << endl;
  cout << "Example: dump_headers general" << endl;
}

static string daten_to_humantime(uint32_t daten) {
  time_t t = static_cast<time_t>(daten);
  string human_date = string(asctime(localtime(&t)));
  wwiv::strings::StringTrimEnd(&human_date);

  return human_date;
}

bool MessagesDumpHeaderCommand::AddSubCommands() {
  add_argument({"start", "Starting message number.", "1"});
  add_argument({"end", "Last message number..", "-1"});
  add_argument(BooleanCommandLineArgument("all", "dumps everything, control lines too", false));

  return true;
}

int MessagesDumpHeaderCommand::ExecuteImpl(
  const string& basename, const string& subs_dir,
	const string& msgs_dir,
  const std::vector<net_networks_rec>& net_networks,
  int start, int end, bool all) {
  // TODO(rushfan): Create the right API type for the right message area.
  unique_ptr<MessageApi> api(new WWIVMessageApi(
      subs_dir, msgs_dir, net_networks));
  if (!api->Exist(basename)) {
    clog << "Message area: '" << basename << "' does not exist." << endl;
    return 1;
  }

  unique_ptr<MessageArea> area(api->Open(basename));
  if (!area) {
    clog << "Error opening message area: '" << basename << "'." << endl;
    return 1;
  }

  int num_messages = (end >= 0) ? end : area->number_of_messages();
  cout << "Message Area: '" << basename << "' has "
       << num_messages << " messages." << endl;
  for (int current = start; current <= num_messages; current++) {
    unique_ptr<MessageHeader> header(area->ReadMessageHeader(current));
    if (!header) {
      continue;
    }
    cout << "#" << setw(5) << std::left << current
         << " From: " << setw(20) << header->from()
         << "date: " << daten_to_humantime(header->daten()) << endl
         << "title: " << header->title();
    if (header->is_local()) {
      cout << "[LOCAL]";
    }
    if (header->is_deleted()) {
      cout << "[DELETED]";
    }
    if (header->is_locked()) {
      cout << "[LOCKED]";
    }
    if (header->is_private()) {
      cout << "[PRIVATE]";
    }
    cout << endl;
    if (all) {
      for (const auto& c : header->control_lines()) {
        cout << "c: " << c << endl;
      }
    }
    unique_ptr<MessageText> text(area->ReadMessageText(current));
    if (!text) {
      continue;
    }
    cout << "------------------------------------------------------------------------"
	       << endl;
    cout << text->text() << endl;
    cout << "------------------------------------------------------------------------"
	       << endl;
  }
  return 0;
}

int MessagesDumpHeaderCommand::Execute() {
  if (arg("help").as_bool()) {
    messages_usage();
    cout << GetHelp();
    return 0;
  } else if (remaining().empty()) {
    clog << "Missing sub basename." << endl;
    messages_usage();
    cout << GetHelp();
    return 2;
  }

  Networks networks(*config()->config());
  if (!networks.IsInitialized()) {
    clog << "Unable to load networks.";
    return 1;
  }

  const string basename(remaining().front());
  const int start = arg("start").as_int();
  int end = arg("end").as_int();
  const bool all = arg("all").as_bool();
  return ExecuteImpl(
    basename, config()->config()->datadir(), config()->config()->msgsdir(), 
    networks.networks(), start, end, all);
}

}  // namespace wwivutil
}  // namespace wwiv
