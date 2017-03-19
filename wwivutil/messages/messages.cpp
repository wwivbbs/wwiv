/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2017, WWIV Software Services             */
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
#include <map>
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

using std::clog;
using std::cout;
using std::endl;
using std::setw;
using std::string;
using std::make_unique;
using std::unique_ptr;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::msgapi;
using namespace wwiv::strings;

constexpr char CD = 4;

namespace wwiv {
namespace wwivutil {

// TODO(rushfan): This was copied from post.cpp. Let's share it.
static bool find_sub(wwiv::sdk::Subs& subs, const string& filename, subboard_t& sub) {
  for (const auto& x : subs.subs()) {
    if (iequals(filename, x.filename)) {
      sub = x;
      return true;
    }
  }
  return false;
}


OverflowStrategy overflow_strategy_from(const std::string& v) {
  string flag = ToStringLowerCase(v);
  if (flag == "one") {
    return OverflowStrategy::delete_one;
  }
  else if (flag == "all") {
    return OverflowStrategy::delete_all;
  }
  else if (flag == "none") {
    return OverflowStrategy::delete_none;
  }
  return OverflowStrategy::delete_none;
}

class DeleteMessageCommand: public UtilCommand {
public:
  DeleteMessageCommand() 
    : UtilCommand("delete", "Deletes message number specified by '--num'.") {}

  virtual ~DeleteMessageCommand() {}

  std::string GetUsage() const override final {
    std::ostringstream ss;
    ss << "Usage:   delete --num=NN <base sub filename>" << endl;
    ss << "Example: delete --num=10 general" << endl;
    return ss.str();
  }

  int Execute() override final {
    if (remaining().empty()) {
      clog << "Missing sub basename." << endl;
      cout << GetUsage() << GetHelp() << endl;
      return 2;
    }

    const string basename(remaining().front());
    std::map<int, std::unique_ptr<wwiv::sdk::msgapi::MessageApi>> apis;

    // TODO(rushfan): Create the right API type for the right message area.
    wwiv::sdk::msgapi::MessageApiOptions options;
    apis[2] = make_unique<WWIVMessageApi>(
        options,
        config()->bbsdir(),
        config()->config()->datadir(), 
        config()->config()->msgsdir(), 
        config()->networks().networks(),
      new NullLastReadImpl());

    subboard_t sub{};
    const auto& datadir = config()->config()->datadir();
    const auto& nets = config()->networks().networks();
    Subs subs(datadir, nets);
    if (!find_sub(subs, basename, sub)) {
      // set default.
      sub.storage_type = 2;
      sub.filename = basename;
    }

    if (!apis[sub.storage_type]->Exist(basename)) {
      clog << "Message area: '" << basename << "' does not exist." << endl;
      clog << "Attempting to create it." << endl;
      // Since the area does not exist, let's create it automatically
      // like WWIV always does.
      unique_ptr<MessageArea> creator(apis[sub.storage_type]->Create(basename, -1));
      return 1;
    }

    unique_ptr<MessageArea> area(apis[sub.storage_type]->Open(basename, -1));
    if (!area) {
      clog << "Unable to Open message area: '" << basename << "'." << endl;
      return 1;
    }

    int num_messages = area->number_of_messages();
    int message_number = arg("num").as_int();
    cout << "Message Sub: '" << basename << "' has "
         << num_messages << " messages." << endl;

    if (message_number < 0 || message_number > num_messages) {
      LOG(ERROR) << "Invalid message number #" << message_number;
      return 1;
    }
    bool success = area->DeleteMessage(message_number);
    if (!success) {
      LOG(ERROR) << "Unable to delete message #" << message_number << "; Try packing this sub.";
      return 1;
    }

    return 0;
  }

  bool AddSubCommands() override final {
    add_argument({"num", "Message Number to delete.", "-1"});
    return true;
  }
};

class PostMessageCommand: public UtilCommand {
public:
  PostMessageCommand()
    : UtilCommand("post", "Posts a new message.") {}

  bool AddSubCommands() override final {
    add_argument({"title", "message sub name to post on", ""});
    add_argument({"from", "message sub name to post on", ""});
    add_argument({"from_usernum", "message sub name to post on", ""});
    add_argument({"to", "message sub name to post on", ""});
    add_argument({"date", "message sub name to post on", ""});
    add_argument({"in_reply_to", "message sub name to post on", ""});
    add_argument({ "delete_overflow", "Strategy for deleting excess messages when adding new ones. (none|one|all)", "none" });

    return true;
  }

  std::string GetUsage() const override final {
    std::ostringstream ss;
    ss << "Usage:   post --title=\"Welcome\" --from_usernum=1 <base sub filename> <text filename>" << endl;
    ss << "Example: post general mymessage.txt" << endl;
    return ss.str();
  }

  virtual int Execute() {
    if (remaining().size() < 2) {
      clog << "Missing sub basename." << endl;
      cout << GetUsage() << GetHelp();
      return 2;
    }

    const string basename(remaining().front());
    const auto& datadir = config()->config()->datadir();
    const auto& nets = config()->networks().networks();
    Subs subs(datadir, nets);
    if (!subs.Load()) {
      LOG(ERROR) << "Unable to open subs. ";
      return 1;
    }
    if (!subs.exists(basename)) {
      LOG(ERROR) << "No sub exists with filename: " << basename;
      return 1;
    }

    // TODO(rushfan): Load sub data here;
    // TODO(rushfan): Create the right API type for the right message area.
    wwiv::sdk::msgapi::MessageApiOptions options;
    options.overflow_strategy = overflow_strategy_from(sarg("delete_overflow"));

    subboard_t sub{};
    if (!find_sub(subs, basename, sub)) {
      // set default.
      sub.storage_type = 2;
      sub.filename = basename;
    }
    std::map<int, std::unique_ptr<wwiv::sdk::msgapi::MessageApi>> apis;

    apis[2] = make_unique<WWIVMessageApi>(
      options, *config()->config(), config()->networks().networks(), new NullLastReadImpl());
    if (!apis[sub.storage_type]->Exist(basename)) {
      clog << "Message area: '" << basename << "' does not exist." << endl;
      clog << "Attempting to create it." << endl;
      // Since the area does not exist, let's create it automatically
      // like WWIV alwyas does.
      unique_ptr<MessageArea> creator(apis[sub.storage_type]->Create(basename, -1));
      if (!creator) {
        clog << "Failed to create message area: " << basename << ". Exiting." << endl;
        return 1;
      }
    }

    unique_ptr<MessageArea> area(apis[sub.storage_type]->Open(basename, -1));
    if (!area) {
      clog << "Error opening message area: '" << basename << "'." << endl;
      return 1;
    }
    area->set_storage_type(sub.storage_type);
    area->set_max_messages(sub.maxmsgs);

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
    vector<string> lines = wwiv::strings::SplitString(raw_text, "\n", false);

    unique_ptr<Message> msg(area->CreateMessage());
    msg->header()->set_from_system(0);
    msg->header()->set_from_usernum(static_cast<uint16_t>(from_usernum));
    msg->header()->set_title(title);
    msg->header()->set_from(from);
    msg->header()->set_to(to);
    msg->header()->set_daten(static_cast<uint32_t>(daten));
    msg->header()->set_in_reply_to(in_reply_to);
    msg->text()->set_text(JoinStrings(lines, "\r\n"));

    return area->AddMessage(*msg) ? 0 : 1;
  }
};

class PackMessageCommand: public UtilCommand {
public:
  PackMessageCommand()
    : UtilCommand("pack", "Packs a WWIV type-2 message area.") {}

  bool AddSubCommands() override final {
    add_argument(BooleanCommandLineArgument{"backup", "make a backup of the subs", true});
    add_argument({ "delete_overflow", "Strategy for deleting excess messages when adding new ones. (none|one|all)", "none" });
    return true;
  }

  std::string GetUsage() const override final {
    std::ostringstream ss;
    ss << "Usage:   pack <base sub filename>" << endl;
    ss << "Example: post general" << endl;
    return ss.str();
  }

  bool backup(Config& config, const string& name) {
    string sub_filename = StrCat(config.datadir(), name, ".sub");
    string dat_filename = StrCat(config.msgsdir(), name, ".dat");

    time_t now = time(nullptr);
    struct tm* local = localtime(&now);

    string backup_extension = put_time(local, ".backup.%Y%m%d%H%M%S");
    File::Copy(sub_filename, StrCat(sub_filename, backup_extension));
    File::Copy(dat_filename, StrCat(dat_filename, backup_extension));
    return true;
  }

  virtual int Execute() {
    if (remaining().size() < 1) {
      clog << "Missing sub basename." << endl;
      cout << GetUsage() << GetHelp();
      return 2;
    }

    const auto& datadir = config()->config()->datadir();
    const auto& nets = config()->networks().networks();
    Subs subs(datadir, nets);
    if (!subs.Load()) {
      LOG(ERROR) << "Unable to open subs. ";
      return 1;
    }

    const string basename(remaining().front());
    if (!subs.exists(basename)) {
      LOG(ERROR) << "No sub exists with filename: " << basename;
      return 1;
    }

    wwiv::sdk::msgapi::MessageApiOptions options;
    options.overflow_strategy = overflow_strategy_from(sarg("delete_overflow"));
    subboard_t sub{};
    if (!find_sub(subs, basename, sub)) {
      LOG(ERROR) << "Unable to find sub.";
      // set default.
    }

    if (sub.storage_type != 2) {
      LOG(ERROR) << "Can only pack type 2";
    }

    auto api = make_unique<WWIVMessageApi>(
      options, *config()->config(), config()->networks().networks(), new NullLastReadImpl());
    if (!api->Exist(basename)) {
      clog << "Message area: '" << basename << "' does not exist." << endl;
      clog << "Attempting to create it." << endl;
      // Since the area does not exist, let's create it automatically
      // like WWIV alwyas does.
      unique_ptr<MessageArea> creator(api->Create(basename, -1));
      if (!creator) {
        clog << "Failed to create message area: " << basename << ". Exiting." << endl;
        return 1;
      }
    }

    // Ensure we can open it.
    {
      unique_ptr<WWIVMessageArea> area(api->Open(basename, -1));
      if (!area) {
        clog << "Error opening message area: '" << basename << "'." << endl;
        return 1;
      }
      area->set_storage_type(sub.storage_type);
      area->set_max_messages(sub.maxmsgs);
    }

    if (barg("backup")) {
      backup(*config()->config(), basename);
    }

    string newbasename = StrCat(basename, ".new");
    {
      unique_ptr<WWIVMessageArea> area(api->Open(basename, -1));
      unique_ptr<WWIVMessageArea> newarea(api->Create(newbasename, -1));
      if (!newarea) {
        clog << "Unable to create new area: " << newbasename;
        return 1;
      }
      int total = area->number_of_messages();
      for (int i = 1; i <= total; i++) {
        unique_ptr<WWIVMessage> message(area->ReadMessage(i));
        if (!message) { 
          LOG(ERROR) << "Unable to load message #" << i;
          continue; 
        }
        if (!newarea->AddMessage(*message.get())) {
          LOG(ERROR) << "Error adding message: " << message->header()->title();
        } else {
          cout << "[" << i << "]";
        }
      }
    }

    // Copy "new" versions back to sub and dat
    const string orig_sub_fn = StrCat(config()->config()->datadir(), basename, ".sub");
    File::Remove(orig_sub_fn);
    if (!File::Rename(StrCat(config()->config()->datadir(), newbasename, ".sub"), orig_sub_fn)) {
      clog << "Unable to move sub";
    }
    const string orig_dat_fn = StrCat(config()->config()->msgsdir(), basename, ".dat");
    File::Remove(orig_dat_fn);
    if (!File::Rename(StrCat(config()->config()->msgsdir(), newbasename, ".dat"), orig_dat_fn)) {
      clog << "Unable to move dat";
    }

    return 0;
  }
};

bool MessagesCommand::AddSubCommands() {
  if (!add(make_unique<MessagesDumpHeaderCommand>())) { return false; }
  if (!add(make_unique<DeleteMessageCommand>())) { return false; }
  if (!add(make_unique<PostMessageCommand>())) { return false; }
  if (!add(make_unique<PackMessageCommand>())) { return false; }
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
  const string& basename,
  int start, int end, bool all) {

  const auto& datadir = config()->config()->datadir();
  const auto& nets = config()->networks().networks();

  Subs subs(datadir, nets);
  if (!subs.Load()) {
    LOG(ERROR) << "Unable to open subs. ";
    return 1;
  }
  if (!subs.exists(basename)) {
    LOG(ERROR) << "No sub exists with filename: " << basename;
    return 1;
  }

  const wwiv::sdk::msgapi::MessageApiOptions options;
  auto x = new NullLastReadImpl();
  subboard_t sub{};
  if (!find_sub(subs, basename, sub)) {
    // set default.
    sub.storage_type = 2;
    sub.filename = basename;
  }
  std::map<int, std::unique_ptr<wwiv::sdk::msgapi::MessageApi>> apis;

  apis[2] = std::make_unique<WWIVMessageApi>(options, *config()->config(), config()->networks().networks(), x);
  if (!apis[sub.storage_type]->Exist(basename)) {
    clog << "Message area: '" << basename << "' does not exist." << endl;
    return 1;
  }

  unique_ptr<MessageArea> area(apis[sub.storage_type]->Open(basename, -1));
  if (!area) {
    clog << "Error opening message area: '" << basename << "'." << endl;
    return 1;
  }
  area->set_storage_type(sub.storage_type);
  area->set_max_messages(sub.maxmsgs);

  int num_messages = (end >= 0) ? end : area->number_of_messages();
  cout << "Message Sub: '" << basename << "' has "
       << num_messages << " messages." << endl;
  for (int current = start; current <= num_messages; current++) {
    unique_ptr<MessageHeader> header(area->ReadMessageHeader(current));
    if (!header) {
      continue;
    }
    cout << "#" << setw(5) << std::left << current
         << " From: " << setw(20) << header->from()
         << "date: " << daten_to_wwivnet_time(header->daten()) << endl
         << "title: " << header->title();
    if (header->local()) {
      cout << "[LOCAL]";
    }
    if (header->deleted()) {
      cout << "[DELETED]";
    }
    if (header->locked()) {
      cout << "[LOCKED]";
    }
    if (header->private_msg()) {
      cout << "[PRIVATE]";
    }
    cout << endl;
    if (all) {
      cout << "qscan: " <<  header.get()->last_read() << endl;
    }
    if (header->deleted()) {
      // Don't try to read the text of deleted messages.
      continue;
    }
    unique_ptr<MessageText> text(area->ReadMessageText(current));
    if (!text) {
      continue;
    }
    cout << string(72, '-') << endl;
    std::vector<string> lines = wwiv::strings::SplitString(text->text(), "\n", false);
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
  return ExecuteImpl(basename, start, end, all);
}

}  // namespace wwivutil
}  // namespace wwiv
