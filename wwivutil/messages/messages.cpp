/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2020, WWIV Software Services             */
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

#include "core/command_line.h"
#include "core/datetime.h"
#include "core/file.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "sdk/config.h"
#include "sdk/msgapi/message_api_wwiv.h"
#include "sdk/msgapi/msgapi.h"
#include "sdk/names.h"
#include "sdk/net/networks.h"
#include "wwivutil/util.h"
#include <ctime>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

using std::clog;
using std::cout;
using std::endl;
using std::make_unique;
using std::setw;
using std::string;
using std::unique_ptr;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::msgapi;
using namespace wwiv::strings;

constexpr char CD = 4;

namespace wwiv::wwivutil {

static subboard_t default_sub(const std::string& fn) {
  subboard_t sub{};
  sub.storage_type = 2;
  sub.filename = fn;
  return sub;
}

// N.B(rushfan): This is similar to the one in post.cpp.  But that one uses
// network number and network name as key and not filename for sub file.
static std::optional<subboard_t> find_sub(const wwiv::sdk::Subs& subs, const string& filename) {
  for (const auto& x : subs.subs()) {
    if (iequals(filename, x.filename)) {
      return {x};
    }
  }
  return std::nullopt;
}

bool BaseMessagesSubCommand::CreateMessageApiMap(const std::string& basename) {
  const auto& datadir = config()->config()->datadir();
  const auto& nets = config()->networks().networks();

  subs_ = std::make_unique<Subs>(datadir, nets);
  if (!subs_->Load()) {
    LOG(ERROR) << "Unable to open subs. ";
    return false;
  }
  if (!subs_->exists(basename)) {
    LOG(ERROR) << "No sub exists with filename: " << basename;
    return false;
  }

  const wwiv::sdk::msgapi::MessageApiOptions options;
  auto* x = new NullLastReadImpl();
  sub_ = find_sub(*subs_, basename).value_or(default_sub(basename));

  apis_[sub_.storage_type] = std::make_unique<WWIVMessageApi>(options, *config()->config(),
                                                            config()->networks().networks(), x);

  if (!wwiv::stl::contains(apis_, 2)) {
    // We always want to add type 2 
    apis_[2] = std::make_unique<WWIVMessageApi>(options, *config()->config(),
                                                config()->networks().networks(), x);
  }
  if (!apis_[sub_.storage_type]->Exist(sub_)) {
    clog << "Message area: '" << sub_.filename << "' does not exist." << endl;
    return false;
  }
  return true;
}

BaseMessagesSubCommand::~BaseMessagesSubCommand() = default;


class DeleteMessageCommand : public BaseMessagesSubCommand {
public:
  DeleteMessageCommand() : BaseMessagesSubCommand("delete", "Deletes message number specified by '--num'.") {}

  virtual ~DeleteMessageCommand() = default;

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
    if (!CreateMessageApiMap(basename)) {
      clog << "Error Creating message apis." << endl;
      return 1;
    }
    
    unique_ptr<MessageArea> area(api().CreateOrOpen(sub(), -1));
    if (!area) {
      clog << "Unable to Open message area: '" << sub().filename << "'." << endl;
      return 1;
    }

    const auto num_messages = area->number_of_messages();
    const auto message_number = arg("num").as_int();
    cout << "Message Sub: '" << basename << "' has " << num_messages << " messages." << endl;
    cout << string(72, '-') << endl;

    if (message_number < 0 || message_number > num_messages) {
      LOG(ERROR) << "Invalid message number #" << message_number;
      return 1;
    }
    
    if (const auto success = area->DeleteMessage(message_number); !success) {
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

class PostMessageCommand : public BaseMessagesSubCommand {
public:
  PostMessageCommand() : BaseMessagesSubCommand("post", "Posts a new message.") {}

  bool AddSubCommands() override final {
    add_argument({"title", "message sub name to post on", ""});
    add_argument({"from", "message sub name to post on", ""});
    add_argument({"from_usernum", "message sub name to post on", ""});
    add_argument({"to", "message sub name to post on", ""});
    add_argument({"date", "message sub name to post on", ""});
    add_argument({"in_reply_to", "message sub name to post on", ""});
    add_argument({"delete_overflow",
                  "Strategy for deleting excess messages when adding new ones. (none|one|all)",
                  "none"});

    return true;
  }

  std::string GetUsage() const override final {
    std::ostringstream ss;
    ss << "Usage:   post --title=\"Welcome\" --from_usernum=1 <base sub filename> <text filename>"
       << endl;
    ss << "Example: post general mymessage.txt" << endl;
    return ss.str();
  }

  int Execute() override {
    if (remaining().size() < 2) {
      clog << "Missing sub basename." << endl;
      cout << GetUsage() << GetHelp();
      return 2;
    }

    const string basename(remaining().front());
    if (!CreateMessageApiMap(basename)) {
      clog << "Error Creating message apis." << endl;
      return 1;
    }
    
    unique_ptr<MessageArea> area(api().CreateOrOpen(sub_, -1));
    if (!area) {
      clog << "Error opening message area: '" << basename << "'." << endl;
      return 1;
    }

    const auto filename = remaining().at(1);
    auto from = arg("from").as_string();
    auto title = arg("title").as_string();
    auto to = arg("to").as_string();
    auto daten = DateTime::now();
    auto date_str = arg("date").as_string();
    //#ifndef __unix__
    // This doesn't work on GCC until GCC 5 even though it's C++11.
    if (!date_str.empty()) {
      std::istringstream ss(date_str);
      std::tm dt = {};
      ss >> std::get_time(&dt, "Www Mmm dd hh:mm:ss yyyy");
      if (!ss.fail()) {
        daten = DateTime::from_time_t(mktime(&dt));
      }
    }
//#endif // __unix__
    auto in_reply_to = arg("in_reply_to").as_string();
    auto from_usernum = arg("from_usernum").as_int();
    if (from_usernum >= 1 && from.empty()) {
      Names names(*config()->config());
      from = names.UserName(from_usernum);
    }

    TextFile text_file(filename, "r");
    auto raw_text = text_file.ReadFileIntoString();
    auto lines = wwiv::strings::SplitString(raw_text, "\n", false);

    unique_ptr<Message> msg(area->CreateMessage());
    msg->header().set_from_system(0);
    msg->header().set_from_usernum(static_cast<uint16_t>(from_usernum));
    msg->header().set_title(title);
    msg->header().set_from(from);
    msg->header().set_to(to);
    msg->header().set_daten(daten.to_daten_t());
    msg->header().set_in_reply_to(in_reply_to);
    msg->text().set_text(JoinStrings(lines, "\r\n"));

    MessageAreaOptions area_options{};
    area_options.send_post_to_network = true;
    return area->AddMessage(*msg, area_options) ? 0 : 1;
  }
};

class PackMessageCommand : public BaseMessagesSubCommand {
public:
  PackMessageCommand() : BaseMessagesSubCommand("pack", "Packs a WWIV type-2 message area.") {}

  bool AddSubCommands() override final {
    add_argument(BooleanCommandLineArgument{"backup", "make a backup of the subs", true});
    add_argument({"delete_overflow",
                  "Strategy for deleting excess messages when adding new ones. (none|one|all)",
                  "none"});
    return true;
  }

  std::string GetUsage() const override final {
    std::ostringstream ss;
    ss << "Usage:   pack <base sub filename>" << endl;
    ss << "Example: post general" << endl;
    return ss.str();
  }

  static bool backup(const Config& config, const string& name) {
    const auto sb = backup_file(FilePath(config.datadir(), StrCat(name, ".sub")));
    const auto db = backup_file(FilePath(config.msgsdir(), StrCat(name, ".dat")));
    return sb && db;
  }

   int Execute() override {
    if (remaining().empty()) {
      clog << "Missing sub basename." << endl;
      cout << GetUsage() << GetHelp();
      return 2;
    }

    const string basename(remaining().front());
    if (!CreateMessageApiMap(basename)) {
      clog << "Error Creating message apis." << endl;
      return 1;
    }
    
    // Ensure we can open it.
    {
      try {
        unique_ptr<MessageArea> area(api().CreateOrOpen(sub(), -1));
      } catch (const bad_message_area&) {
        clog << "Error opening message area: '" << basename << "'." << endl;
        return 1;
      }
    }

    if (barg("backup")) {
      backup(*config()->config(), basename);
    }

    auto newsub = sub();
    newsub.filename = StrCat(basename, ".new");
    {
      unique_ptr<MessageArea> area(api().Open(sub(), -1));
      if (!api().Create(newsub, -1)) {
        clog << "Unable to create new area: " << newsub.filename;
        return 1;
      }
      unique_ptr<MessageArea> newarea(api().Open(newsub, -1));
      const auto total = area->number_of_messages();
      for (auto i = 1; i <= total; i++) {
        auto message(area->ReadMessage(i));
        if (!message) {
          LOG(ERROR) << "Unable to load message #" << i;
          continue;
        }
        if (!newarea->AddMessage(*message, {})) {
          LOG(ERROR) << "Error adding message: " << message->header().title();
        } else {
          cout << "[" << i << "]";
        }
      }
    }

    // Copy "new" versions back to sub and dat
    const auto orig_sub_fn = FilePath(config()->config()->datadir(), StrCat(basename, ".sub"));
    const auto new_sub_fn =
        FilePath(config()->config()->datadir(), StrCat(newsub.filename, ".sub"));
    File::Remove(orig_sub_fn);
    if (!File::Rename(new_sub_fn, orig_sub_fn)) {
      clog << "Unable to move sub";
    }
    const auto orig_dat_fn = FilePath(config()->config()->msgsdir(), StrCat(newsub.filename, ".dat"));
    const auto new_dat_fn =
        FilePath(config()->config()->msgsdir(), StrCat(newsub.filename, ".dat"));
    File::Remove(orig_dat_fn);
    if (!File::Rename(new_dat_fn, orig_dat_fn)) {
      clog << "Unable to move dat";
    }

    return 0;
  }
};

bool MessagesCommand::AddSubCommands() {
  if (!add(make_unique<MessagesDumpCommand>())) {
    return false;
  }
  if (!add(make_unique<DeleteMessageCommand>())) {
    return false;
  }
  if (!add(make_unique<PostMessageCommand>())) {
    return false;
  }
  if (!add(make_unique<PackMessageCommand>())) {
    return false;
  }
  
  return true;
}

MessagesDumpCommand::MessagesDumpCommand()
    : BaseMessagesSubCommand("dump", "Displays message header and text information.") {}

std::string MessagesDumpCommand::GetUsage() const {
  std::ostringstream ss;
  ss << "Usage:   dump <base sub filename>" << endl;
  ss << "Example: dump general" << endl;
  return ss.str();
}

bool MessagesDumpCommand::AddSubCommands() {
  add_argument({"start", "Starting message number.", "1"});
  add_argument({"end", "Last message number.", "-1"});
  add_argument({"start_date", "Date for starting message in format yyyy-mm-dd[ h:m:s].", ""});
  add_argument({"end_date", "Date for ending message in format yyyy-mm-dd[ h:m:s].", ""});
  add_argument(BooleanCommandLineArgument("all", "dumps everything, control lines too", false));

  return true;
}

int MessagesDumpCommand::ExecuteImpl(MessageArea* area, const string& basename, int start, int end, bool all) {


  const auto last_message = (end >= 0) ? end : area->number_of_messages();
  cout << "Message Sub: '" << basename << "' has " << area->number_of_messages() << " messages."
       << endl;
  cout << string(72, '-') << endl;
  for (auto current = start; current <= last_message; current++) {
    auto message = area->ReadMessage(current);
    if (!message) {
      cout << "#" << current << "  ERROR " << endl;
      VLOG(1) << "Failed to read message number: " << current;
      cout << string(72, '-') << endl;
      continue;
    }
    const auto& header = message->header();
    cout << "#" << setw(5) << std::left << current << " From: " << setw(20) << header.from()
         << "date: " << daten_to_wwivnet_time(header.daten()) << endl
         << "title: " << header.title();
    if (header.local()) {
      cout << "[LOCAL]";
    }
    if (header.deleted()) {
      cout << "[DELETED]";
    }
    if (header.locked()) {
      cout << "[LOCKED]";
    }
    if (header.private_msg()) {
      cout << "[PRIVATE]";
    }
    cout << endl;
    if (all) {
      cout << "qscan: " << header.last_read() << endl;
    }
    if (header.deleted()) {
      // Don't try to read the text of deleted messages.
      continue;
    }
    const auto& text = message->text();
    cout << string(72, '-') << endl;
    auto lines = wwiv::strings::SplitString(text.text(), "\n", false);
    for (const auto& line : lines) {
      if (line.empty()) {
        continue;
      }
      if (line.front() != CD || all) {
        for (const auto ch : line) {
          dump_char(cout, ch);
        }
        cout << endl;
      }
    }
    cout << "------------------------------------------------------------------------" << endl;
  }
  return 0;
}

int MessagesDumpCommand::Execute() {
  if (remaining().empty()) {
    clog << "Missing sub basename." << endl;
    cout << GetUsage() << GetHelp();
    return 2;
  }

  const string basename(remaining().front());
  if (!CreateMessageApiMap(basename)) {
    clog << "Error Creating message apis." << endl;
    return 1;
  }
    
  auto start = iarg("start");
  auto end = iarg("end");
  auto start_date = sarg("start_date");
  auto end_date = sarg("end_date");
  const auto all = barg("all");

  unique_ptr<MessageArea> area(api().Open(sub_, -1));
  if (!area) {
    clog << "Error opening message area: '" << sub().filename << "'." << endl;
    return 1;
  }
  area->set_storage_type(sub().storage_type);
  area->set_max_messages(sub().maxmsgs);

  // If we have dates, update the start and end numbers based
  // on the dates.
  const auto last_message = (end >= 0) ? end : area->number_of_messages();
  if (!start_date.empty()) {
    auto start_dt = parse_yyyymmdd_with_optional_hms(start_date).to_daten_t();
    for (start = 1; start <= last_message; start++) {
      auto h = area->ReadMessageHeader(start);
      if (start_dt < h->daten()) {
        // We're past the start date, so use last message number.
        break;
      }
    }
    // Find the closest message to the start date, or leave it -1
  }
  if (!end_date.empty()) {
    const auto end_dt = parse_yyyymmdd_with_optional_hms(end_date).to_daten_t();
    // Find the closest message to the end date, or leave it -1
    for (auto i = 1, before_end = 1; i<= last_message; i++) {
      auto h = area->ReadMessageHeader(i);
      if (end_dt < h->daten()) {
        // We're past the end date, so use last message number.
        end = before_end;
        break;
      }
      before_end = i;
    }
  }
  VLOG(1) << "start: " << start << "; end: " << end << std::endl;
  return ExecuteImpl(area.get(), basename, start, end, all);
}

} // namespace wwiv
