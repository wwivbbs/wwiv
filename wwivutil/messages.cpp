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
using wwiv::core::CommandLineCommand;
using namespace wwiv::sdk;
using namespace wwiv::sdk::msgapi;

namespace wwiv {
namespace wwivutil {

void messages_usage() {
  cout << "Usage:   dump_headers <base sub filename>" << endl;
  cout << "Example: dump_headers general" << endl;
}

string daten_to_humantime(uint32_t daten) {
  time_t t = static_cast<time_t>(daten);
  string human_date = string(asctime(localtime(&t)));
  wwiv::strings::StringTrimEnd(&human_date);

  return human_date;
}

static int dump(const string& basename, const string& subs_dir, const string& msgs_dir,
                const vector<net_networks_rec>& net_networks,
                int start, int end, bool all) {
  unique_ptr<WWIVMessageApi> api(new WWIVMessageApi(subs_dir, msgs_dir, net_networks));
  if (!api->Exist(basename)) {
    clog << "Message area: '" << basename << "' does not exist." << endl;
    return 1;
  }

  unique_ptr<WWIVMessageArea> area(api->Open(basename));
  if (!area) {
    clog << "Error opening message area: '" << basename << "'." << endl;
    return 1;
  }

  int num_messages = (end >= 0) ? end : area->number_of_messages();
  cout << "Message Area: '" << basename << "' has " << num_messages << " messages." << endl;
  for (int current = start; current <= num_messages; current++) {
    unique_ptr<MessageHeader> header(area->ReadMessageHeader(current));
    if (!header) {
      continue;
    }
    cout << "#" << setw(5) << std::left << current << " From: " << setw(20) << header->from()
         << "date: " << daten_to_humantime(header->daten()) << endl
         << "title: " << header->title() << endl;
    if (all) {
      for (const auto& c : header->control_lines()) {
        cout << "c: " << c << endl;
      }
      WWIVMessageHeader* wwiv_header = dynamic_cast<WWIVMessageHeader*>(header.get());
      if (wwiv_header) {
        cout << "ownersys: " << wwiv_header->data().ownersys << endl;
      }
    }
    unique_ptr<MessageText> text(area->ReadMessageText(current));
    if (!text) {
      continue;
    }
    cout << "------------------------------------------------------------------------" << endl;
    cout << text->text() << endl;
    cout << "------------------------------------------------------------------------" << endl;
  }
  return 0;
}

int dump_headers(const Config& config, const CommandLineCommand* command) {
  if (command->remaining().empty()) {
    cout << "Usage:   dump <subname>" << endl;
    cout << "Example: dump GENERAL" << endl;
    return 2;
  }

  Networks networks(config);
  if (!networks.IsInitialized()) {
    clog << "Unable to load networks.";
    return 1;
  }

  const string basename(command->remaining().front());
  const int start = command->arg("start").as_int();
  int end = command->arg("end").as_int();
  const bool all = command->arg("all").as_bool();
  return dump(basename, config.datadir(), config.msgsdir(), networks.networks(), start, end, all);
}

}  // namespace wwivutil
}  // namespace wwiv
