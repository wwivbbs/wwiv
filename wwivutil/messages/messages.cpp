/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2016 WWIV Software Services              */
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
#include <ctime>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "core/command_line.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "sdk/config.h"
#include "sdk/datetime.h"
#include "sdk/net.h"
#include "sdk/names.h"
#include "sdk/networks.h"
#include "sdk/msgapi/msgapi.h"
#include "sdk/msgapi/message_api_wwiv.h"

using std::cerr;
using std::clog;
using std::cout;
using std::endl;
using std::setw;
using std::string;
using std::make_unique;
using std::unique_ptr;
using std::vector;
using wwiv::core::BooleanCommandLineArgument;
using namespace wwiv::sdk;
using namespace wwiv::sdk::msgapi;

constexpr char CD = 4;

namespace wwiv {
namespace wwivutil {

class DeleteMessageCommand: public UtilCommand {
public:
  DeleteMessageCommand() 
    : UtilCommand("delete", "Deletes message number specified by '--num'.") {}

  virtual ~DeleteMessageCommand() {}

  virtual std::string GetUsage() const override final {
    std::ostringstream ss;
    ss << "Usage:   delete --num=NN <base sub filename>" << endl;
    ss << "Example: delete --num=10 general" << endl;
    return ss.str();
  }

  virtual int Execute() override final {
    if (remaining().empty()) {
      clog << "Missing sub basename." << endl;
      cout << GetUsage() << GetHelp() << endl;
      return 2;
    }

    const string basename(remaining().front());
    // TODO(rushfan): Create the right API type for the right message area.
    unique_ptr<MessageApi> api = make_unique<WWIVMessageApi>(
        config()->bbsdir(),
        config()->config()->datadir(), 
        config()->config()->msgsdir(), 
        config()->networks().networks());
    if (!api->Exist(basename)) {
      clog << "Message area: '" << basename << "' does not exist." << endl;
      clog << "Attempting to create it." << endl;
      // Since the area does not exist, let's create it automatically
      // like WWIV alwyas does.
      unique_ptr<MessageArea> creator(api->Create(basename));
      return 1;
    }

    unique_ptr<MessageArea> area(api->Open(basename));
    if (!area) {
      clog << "Unable to Open message area: '" << basename << "'." << endl;
      return 1;
    }

    int num_messages = area->number_of_messages();
    int message_number = arg("num").as_int();
    cout << "Message Area: '" << basename << "' has "
         << num_messages << " messages." << endl;

    if (message_number < 0 || message_number > num_messages) {
      clog << "Invalid message number: " << message_number << endl;
      return 1;
    }
    bool success = area->DeleteMessage(message_number);
    if (!success) {
      clog << "Error deleting message number: " << message_number << endl;
      return 1;
    }

    return 0;
  }

  virtual bool AddSubCommands() override final {
    add_argument({"num", "Message Number to delete.", "-1"});
    return true;
  }
};

static string Join(const vector<string> lines) {
  string out;
  for (const auto& line : lines) {
    out += line;
    out += "\r\n";
  }
  return out;
}

class PostMessageCommand: public UtilCommand {
public:
  PostMessageCommand()
    : UtilCommand("post", "Posts a new message.") {}

  virtual bool AddSubCommands() override final {
    add_argument({"title", "message sub name to post on", ""});
    add_argument({"from", "message sub name to post on", ""});
    add_argument({"from_usernum", "message sub name to post on", ""});
    add_argument({"to", "message sub name to post on", ""});
    add_argument({"date", "message sub name to post on", ""});
    add_argument({"in_reply_to", "message sub name to post on", ""});
    return true;
  }

  virtual std::string GetUsage() const override final {
    std::ostringstream ss;
    ss << "Usage:   post --title=\"Welcome\" --from_usernum=1 <base sub filename> <text filename>" << endl;
    ss << "Example: post --num=10 general mymessage.txt" << endl;
    return ss.str();
  }

  virtual int Execute() {
    if (remaining().size() < 2) {
      clog << "Missing sub basename." << endl;
      cout << GetUsage() << GetHelp();
      return 2;
    }

    const string basename(remaining().front());
    // TODO(rushfan): Create the right API type for the right message area.
    unique_ptr<WWIVMessageApi> api = make_unique<WWIVMessageApi>(config()->bbsdir(),
      config()->config()->datadir(), config()->config()->msgsdir(), config()->networks().networks());
    if (!api->Exist(basename)) {
      clog << "Message area: '" << basename << "' does not exist." << endl;
      clog << "Attempting to create it." << endl;
      // Since the area does not exist, let's create it automatically
      // like WWIV alwyas does.
      unique_ptr<MessageArea> creator(api->Create(basename));
      if (!creator) {
        clog << "Failed to create message area: " << basename << ". Exiting." << endl;
        return 1;
      }
    }

    unique_ptr<MessageArea> area(api->Open(basename));
    if (!area) {
      clog << "Error opening message area: '" << basename << "'." << endl;
      return 1;
    }

    const string filename = remaining().at(1);
    string from = arg("from").as_string();
    string title = arg("title").as_string();
    string to = arg("to").as_string();
    time_t daten = time(nullptr);
    string date_str = arg("date").as_string();
 #ifndef __unix__
    // This doesn't work on GCC until GCC 5 even though it's C++11.
    if (!date_str.empty()) {
      std::istringstream ss(date_str);
      std::tm dt = {};
      ss >> std::get_time(&dt, "Www Mmm dd hh:mm:ss yyyy");
      if (ss) {
        daten = mktime(&dt);
      }
    }
#endif  // __unix__
    string in_reply_to = arg("in_reply_to").as_string();
    int from_usernum = arg("from_usernum").as_int();
    if (from_usernum >= 1 && from.empty()) {
      Names names(*config()->config());
      from = names.UserName(from_usernum);
    }

    TextFile text_file(filename, "r");
    string raw_text = text_file.ReadFileIntoString();
    vector<string> lines = wwiv::strings::SplitString(raw_text, "\n");

    unique_ptr<Message> msg(area->CreateMessage());
    msg->header()->set_from_system(0);
    msg->header()->set_from_usernum(static_cast<uint16_t>(from_usernum));
    msg->header()->set_title(title);
    msg->header()->set_from(from);
    msg->header()->set_to(to);
    msg->header()->set_daten(static_cast<uint32_t>(daten));
    msg->header()->set_in_reply_to(in_reply_to);
    msg->text()->set_text(Join(lines));

    return area->AddMessage(*msg) ? 0 : 1;
  }
};

bool MessagesCommand::AddSubCommands() {
  if (!add(make_unique<MessagesDumpHeaderCommand>())) { return false; }
  if (!add(make_unique<DeleteMessageCommand>())) { return false; }
  if (!add(make_unique<PostMessageCommand>())) { return false; }
  return true;
}

MessagesDumpHeaderCommand::MessagesDumpHeaderCommand()
  : UtilCommand("dump", "Displays message header and text information.") {}

std::string MessagesDumpHeaderCommand::GetUsage() const {
  std::ostringstream ss;
  ss << "Usage:   dump_headers <base sub filename>" << endl;
  ss << "Example: dump_headers general" << endl;
  return ss.str();
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
  unique_ptr<MessageApi> api(new WWIVMessageApi(config()->bbsdir(),
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
    unique_ptr<MessageText> text(area->ReadMessageText(current));
    if (!text) {
      continue;
    }
    cout << "------------------------------------------------------------------------"
	       << endl;
    std::vector<string> lines = wwiv::strings::SplitString(text->text(), "\n");
    for (const auto& line : lines) {
      if (line.empty()) {
        continue;
      }
      if (line.front() != CD || all) {
        cout << line << endl;
      }
    }
    cout << "------------------------------------------------------------------------"
	       << endl;
  }
  return 0;
}

int MessagesDumpHeaderCommand::Execute() {
  if (remaining().empty()) {
    clog << "Missing sub basename." << endl;
    cout << GetUsage() << GetHelp();
    return 2;
  }

  const string basename(remaining().front());
  const int start = arg("start").as_int();
  int end = arg("end").as_int();
  const bool all = arg("all").as_bool();
  return ExecuteImpl(
    basename, config()->config()->datadir(), config()->config()->msgsdir(), 
    config()->networks().networks(), start, end, all);
}

}  // namespace wwivutil
}  // namespace wwiv
