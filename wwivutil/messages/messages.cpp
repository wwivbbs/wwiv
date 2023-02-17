/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2022, WWIV Software Services             */
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
#include "sdk/names.h"
#include "sdk/msgapi/message_api_wwiv.h"
#include "sdk/msgapi/msgapi.h"
#include "sdk/net/networks.h"
#include "wwivutil/util.h"

#include <ctime>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

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
static std::optional<subboard_t> find_sub(const wwiv::sdk::Subs& subs, const std::string& filename) {
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

  subs_ = std::make_unique<Subs>(datadir, nets, config()->config()->max_backups());
  if (!subs_->Load()) {
    LOG(ERROR) << "Unable to open subs. ";
    return false;
  }
  if (!subs_->exists(basename)) {
    LOG(ERROR) << "No sub exists with filename: " << basename;
    return false;
  }

  const MessageApiOptions options;
  auto* x = new NullLastReadImpl();
  sub_ = find_sub(*subs_, basename).value_or(default_sub(basename));

  apis_[sub_.storage_type] = std::make_unique<WWIVMessageApi>(options, *config()->config(),
                                                            config()->networks().networks(), x);

  if (!stl::contains(apis_, 2)) {
    // We always want to add type 2 
    apis_[2] = std::make_unique<WWIVMessageApi>(options, *config()->config(),
                                                config()->networks().networks(), x);
  }

  // We try to open or create the sub later.

  //if (!apis_[sub_.storage_type]->Exist(sub_)) {
  //  clog << "Message area: '" << sub_.filename << "' does not exist." << std::endl;
  //  return false;
  //}
  return true;
}

class DeleteMessageCommand final : public BaseMessagesSubCommand {
public:
  DeleteMessageCommand()
      : BaseMessagesSubCommand("delete", "Deletes message number specified by '--num'.") {}

  ~DeleteMessageCommand() override = default;

  [[nodiscard]] std::string GetUsage() const override {
    std::ostringstream ss;
    ss << "Usage:   delete --num=NN <base sub filename>" << std::endl;
    ss << "Example: delete --num=10 general" << std::endl;
    return ss.str();
  }

  int Execute() override {
    if (remaining().empty()) {
      std::clog << "Missing sub basename." << std::endl;
      std::cout << GetUsage() << GetHelp() << std::endl;
      return 2;
    }

    const auto basename(remaining().front());
    if (!CreateMessageApiMap(basename)) {
      std::clog << "Error Creating message apis." << std::endl;
      return 1;
    }
    
    auto area(api().CreateOrOpen(sub(), -1));
    if (!area) {
      std::clog << "Unable to Open message area: '" << sub().filename << "'." << std::endl;
      return 1;
    }

    const auto num_messages = area->number_of_messages();
    const auto message_number = arg("num").as_int();
    std::cout << "Message Sub: '" << basename << "' has " << num_messages << " messages." << std::endl;
    std::cout << std::string(72, '-') << std::endl;

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

  bool AddSubCommands() override {
    add_argument({"num", "Message Number to delete.", "-1"});
    return true;
  }
};

class PostMessageCommand final : public BaseMessagesSubCommand {
public:
  PostMessageCommand() : BaseMessagesSubCommand("post", "Posts a new message.") {}

  bool AddSubCommands() override {
    add_argument({"title", "title of post", ""});
    add_argument({"from", "user name sending the post", ""});
    add_argument({"from_usernum", "usernum sending the post", ""});
    add_argument({"to", "who the post is addressed to", ""});
    add_argument({"date", "date of post", ""});
    add_argument({"in_reply_to", "who this post is a reply to", ""});
    add_argument({"delete_overflow",
                  "Strategy for deleting excess messages when adding new ones. (none|one|all)",
                  "none"});
    add_argument({"use_re_and_by","use re and by lines", ""});

    return true;
  }

  [[nodiscard]] std::string GetUsage() const override {
    std::ostringstream ss;
    ss << "Usage:   post --title=\"Welcome\" --from_usernum=1 <base sub filename> <text filename>"
       << std::endl;
    ss << "Example: post general mymessage.txt" << std::endl;
    return ss.str();
  }

  [[nodiscard]] int Execute() override {
    if (remaining().size() < 2) {
      std::clog << "Missing sub basename." << std::endl;
      std::cout << GetUsage() << GetHelp();
      return 2;
    }

    const auto basename(remaining().front());
    if (!CreateMessageApiMap(basename)) {
      std::clog << "Error Creating message apis." << std::endl;
      return 1;
    }
    
    auto area(api().CreateOrOpen(sub_, -1));
    if (!area) {
      std::clog << "Error opening message area: '" << basename << "'." << std::endl;
      return 1;
    }

    const auto& filename = stl::at(remaining(),1);
    auto from = arg("from").as_string();
    auto title = arg("title").as_string();
    auto to = arg("to").as_string();
    auto daten = DateTime::now();
    if (auto date_str = arg("date").as_string(); !date_str.empty()) {
      std::istringstream ss(date_str);
      std::tm dt = {};
      ss >> std::get_time(&dt, "Www Mmm dd hh:mm:ss yyyy");
      if (!ss.fail()) {
        daten = DateTime::from_time_t(mktime(&dt));
      }
    }
    auto in_reply_to = arg("in_reply_to").as_string();
    auto from_usernum = arg("from_usernum").as_int();
    if (from_usernum >= 1 && from.empty()) {
      Names names(*config()->config());
      from = names.UserName(from_usernum);
    }

    TextFile text_file(filename, "r");
    auto raw_text = text_file.ReadFileIntoString();
    auto lines = SplitString(raw_text, "\n", false);

    auto msg = area->CreateMessage();
    auto& header = msg.header();
    header.set_from_system(0);
    header.set_from_usernum(static_cast<uint16_t>(from_usernum));
    header.set_title(title);
    header.set_from(from);
    header.set_to(to);
    header.set_daten(daten.to_daten_t());
    header.set_in_reply_to(in_reply_to);
    msg.set_text(JoinStrings(lines, "\r\n"));

    MessageAreaOptions area_options{};
    area_options.send_post_to_network = true;
    area_options.add_re_and_by_line = arg("use_re_and_by").as_bool();
    return area->AddMessage(msg, area_options) ? 0 : 1;
  }
};

class PackMessageCommand final : public BaseMessagesSubCommand {
public:
  PackMessageCommand() : BaseMessagesSubCommand("pack", "Packs a WWIV type-2 message area.") {}

  bool AddSubCommands() override {
    add_argument(BooleanCommandLineArgument{"backup", "make a backup of the subs", true});
    add_argument({"delete_overflow",
                  "Strategy for deleting excess messages when adding new ones. (none|one|all)",
                  "none"});
    return true;
  }

  [[nodiscard]] std::string GetUsage() const override {
    std::ostringstream ss;
    ss << "Usage:   pack <base sub filename>" << std::endl;
    ss << "Example: post general" << std::endl;
    return ss.str();
  }

  static bool backup(const Config& config, const std::string& name) {
    const auto max_backups = config.max_backups();
    const auto sb = backup_file(FilePath(config.datadir(), StrCat(name, ".sub")), max_backups);
    const auto db = backup_file(FilePath(config.msgsdir(), StrCat(name, ".dat")), max_backups);
    return sb && db;
  }

   int Execute() override {
    if (remaining().empty()) {
      std::clog << "Missing sub basename." << std::endl;
      std::cout << GetUsage() << GetHelp();
      return 2;
    }

    const auto basename(remaining().front());
    if (!CreateMessageApiMap(basename)) {
      std::clog << "Error Creating message apis." << std::endl;
      return 1;
    }

    // Ensure we can open it.
    {
      try {
        auto area(api().CreateOrOpen(sub(), -1));
      } catch (const bad_message_area&) {
        std::clog << "Error opening message area: '" << basename << "'." << std::endl;
        return 1;
      }
    }

    if (barg("backup")) {
      backup(*config()->config(), basename);
    }

    auto newsub = sub();
    newsub.filename = StrCat(basename, ".new");
    {
      auto area(api().Open(sub(), -1));
      if (!api().Create(newsub, -1)) {
        std::clog << "Unable to create new area: " << newsub.filename;
        return 1;
      }
      auto newarea(api().Open(newsub, -1));
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
          std::cout << "[" << i << "]";
        }
      }
    }

    // Copy "new" versions back to sub and dat
    const auto orig_sub_fn = FilePath(config()->config()->datadir(), StrCat(basename, ".sub"));
    const auto new_sub_fn =
        FilePath(config()->config()->datadir(), StrCat(newsub.filename, ".sub"));
    File::Remove(orig_sub_fn);
    if (!File::Rename(new_sub_fn, orig_sub_fn)) {
      std::clog << "Unable to move sub";
    }
    const auto orig_dat_fn =
        FilePath(config()->config()->msgsdir(), StrCat(newsub.filename, ".dat"));
    const auto new_dat_fn =
        FilePath(config()->config()->msgsdir(), StrCat(newsub.filename, ".dat"));
    File::Remove(orig_dat_fn);
    if (!File::Rename(new_dat_fn, orig_dat_fn)) {
      std::clog << "Unable to move dat";
    }

    return 0;
  }
};


bool MessagesCommand::AddSubCommands() {
  if (!add(std::make_unique<MessagesDumpCommand>())) {
    return false;
  }
  if (!add(std::make_unique<DeleteMessageCommand>())) {
    return false;
  }
  if (!add(std::make_unique<PostMessageCommand>())) {
    return false;
  }
  if (!add(std::make_unique<PackMessageCommand>())) {
    return false;
  }
  
  return true;
}

MessagesDumpCommand::MessagesDumpCommand()
    : BaseMessagesSubCommand("dump", "Displays message header and text information.") {}

std::string MessagesDumpCommand::GetUsage() const {
  std::ostringstream ss;
  ss << "Usage:   dump <base sub filename>" << std::endl;
  ss << "Example: dump general" << std::endl;
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

// ReSharper disable once CppMemberFunctionMayBeConst
int MessagesDumpCommand::ExecuteImpl(MessageArea* area, const std::string& basename, int start, int end,
                                     bool all) {
  const auto total = area->number_of_messages();
  if (start < 1) {
    start = 1;
  } else if (start > total) {
    start = total;
  }
  if (end <= 0) {
    end = total;
  } else if (end < start) {
    end = start;
  } else if (end > total) {
    end = total;
  }
  std::cout << "Message Sub: '" << basename << "' has " << total << " messages." << std::endl;
  std::cout << std::string(72, '-') << std::endl;
  for (auto current = start; current <= end; current++) {
    const auto message = area->ReadMessage(current);
    if (!message) {
      std::cout << "#" << current << "  ERROR " << std::endl;
      VLOG(1) << "Failed to read message number: " << current;
      std::cout << std::string(72, '-') << std::endl;
      continue;
    }
    const auto& header = message->header();
    std::cout << "#" << std::setw(5) << std::left << current << " From: " << std::setw(20) << header.from()
         << "date: " << daten_to_wwivnet_time(header.daten()) << std::endl
         << "title: " << header.title();
    if (header.local()) {
      std::cout << "[LOCAL]";
    }
    if (header.deleted()) {
      std::cout << "[DELETED]";
    }
    if (header.locked()) {
      std::cout << "[LOCKED]";
    }
    if (header.private_msg()) {
      std::cout << "[PRIVATE]";
    }
    std::cout << std::endl;
    if (all) {
      std::cout << "qscan: " << header.last_read() << std::endl;
    }
    if (header.deleted()) {
      // Don't try to read the text of deleted messages.
      continue;
    }
    const auto& text = message->text();
    std::cout << std::string(72, '-') << std::endl;
    auto lines = wwiv::strings::SplitString(text.string(), "\n", false);
    for (const auto& line : lines) {
      if (line.empty()) {
        continue;
      }
      if (line.front() != CD || all) {
        for (const auto ch : line) {
          dump_char(std::cout, ch);
        }
        std::cout << std::endl;
      }
    }
    std::cout << "------------------------------------------------------------------------" << std::endl;
  }
  return 0;
}

int MessagesDumpCommand::Execute() {
  if (remaining().empty()) {
    std::clog << "Missing sub basename." << std::endl;
    std::cout << GetUsage() << GetHelp();
    return 2;
  }

  const auto basename(remaining().front());
  if (!CreateMessageApiMap(basename)) {
    std::clog << "Error Creating message apis." << std::endl;
    return 1;
  }
    
  auto start = iarg("start");
  auto end = iarg("end");
  const auto start_date = sarg("start_date");
  const auto end_date = sarg("end_date");
  const auto all = barg("all");

  auto area(api().Open(sub_, -1));
  if (!area) {
    std::clog << "Error opening message area: '" << sub().filename << "'." << std::endl;
    return 1;
  }
  area->set_storage_type(sub().storage_type);
  area->set_max_messages(sub().maxmsgs);

  // If we have dates, update the start and end numbers based
  // on the dates.
  const auto last_message = (end >= 0) ? end : area->number_of_messages();
  if (!start_date.empty()) {
    const auto start_dt = parse_yyyymmdd_with_optional_hms(start_date).to_daten_t();
    for (start = 1; start <= last_message; start++) {
      if (const auto h = area->ReadMessageHeader(start); start_dt < h->daten()) {
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
      if (const auto h = area->ReadMessageHeader(i); end_dt < h->daten()) {
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
