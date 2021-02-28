/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2021, WWIV Software Services             */
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
#include "wwivutil/email/email.h"

#include "core/command_line.h"
#include "core/datetime.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "sdk/names.h"
#include "sdk/msgapi/message_api_wwiv.h"
#include "sdk/net/networks.h"
#include "wwivutil/util.h"

#include <ctime>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using std::setw;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::msgapi;
using namespace wwiv::strings;

constexpr char CD = 4;

namespace wwiv::wwivutil {

bool BaseEmailSubCommand::CreateMessageApi() {
  if (api_) {
    return true;
  }
  const MessageApiOptions options;
  auto* x = new NullLastReadImpl();
  api_ = std::make_unique<WWIVMessageApi>(options, *config()->config(),
                                          config()->networks().networks(), x);
  return true;
}

WWIVMessageApi& BaseEmailSubCommand::api() noexcept {
  if (!api_) {
    CreateMessageApi();
  }
  DCHECK(api_);
  return *api_;
}

class DeleteEmailCommand final : public BaseEmailSubCommand {
public:
  DeleteEmailCommand()
      : BaseEmailSubCommand("delete", "Deletes email number specified by '--num'.") {}

  ~DeleteEmailCommand() override = default;

  [[nodiscard]] std::string GetUsage() const override {
    std::ostringstream ss;
    ss << "Usage:   delete --num=NN <base sub filename>" << std::endl;
    ss << "Example: delete --num=10 general" << std::endl;
    return ss.str();
  }

  int Execute() override {

    if (!CreateMessageApi()) {
      std::clog << "Error Creating message apis." << std::endl;
      return 1;
    }

    auto email = api().OpenEmail();
    if (!email) {
      std::clog << "Unable to Open email" << std::endl;
      return 1;
    }

    const auto num_messages = email->number_of_messages();
    const auto message_number = iarg("num");
    std::cout << "Email has " << num_messages << " messages." << std::endl;
    std::cout << std::string(72, '-') << std::endl;

    if (message_number < 0 || message_number > num_messages) {
      LOG(ERROR) << "Invalid message number #" << message_number;
      return 1;
    }
    
    if (const auto success = email->DeleteMessage(message_number); !success) {
      LOG(ERROR) << "Unable to delete message #" << message_number << ".";
      return 1;
    }

    return 0;
  }

  bool AddSubCommands() override {
    add_argument({"num", "Message Number to delete.", "-1"});
    return true;
  }
};

class AddEmailCommand final : public BaseEmailSubCommand {
public:
  AddEmailCommand() : BaseEmailSubCommand("add", "Creates/Adds a new email.") {}

  bool AddSubCommands() override {
    add_argument({"title", "email title to use", ""});
    add_argument({"from", "The sender user number of this email", ""});
    add_argument({"from_usernum", "The sender user name of this email", ""});
    add_argument({"to", "The receiver user number for this email", ""});
    add_argument({"date", "Date to use in 'Www Mmm dd hh:mm:ss yyyy' format (or empty for current date).", ""});

    return true;
  }

  [[nodiscard]] std::string GetUsage() const override {
    std::ostringstream ss;
    ss << "Usage:   add --title=\"Welcome\" --from=1 <text filename>"
       << std::endl;
    ss << "Example: add --from=1 --to=${to_usernum} welcome_email_text.txt" << std::endl;
    return ss.str();
  }

  [[nodiscard]] int Execute() override {


    if (!CreateMessageApi()) {
      std::clog << "Error Creating message api." << std::endl;
      return 1;
    }

    if (remaining().empty()) {
      std::clog << "Missing text filename." << std::endl;
      return 1;
    }

    const auto filename(remaining().front());
    auto from_usernum = iarg("from_usernum");
    if (from_usernum < 1) {
      std::clog << "--from_usernum must be user number >= 1" << std::endl;
      return 1;
    }
    auto from_name = sarg("from");
    auto title = sarg("title");
    if (title.empty()) {
      std::clog << "--title must be specified." << std::endl;
      return 1;
    }
    auto to_num = iarg("to");
    if (to_num < 1) {
      std::clog << "--to must be user number >= 1" << std::endl;
      return 1;
    }
    auto daten = DateTime::now();
    auto date_str = sarg("date");
    if (!date_str.empty()) {
      std::istringstream ss(date_str);
      std::tm dt = {};
      ss >> std::get_time(&dt, "Www Mmm dd hh:mm:ss yyyy");
      if (!ss.fail()) {
        daten = DateTime::from_time_t(mktime(&dt));
        std::clog << "Error parsing date, defaulting to now: " << daten.to_string() << std::endl;
      }
    }

    auto area = api().OpenEmail();
    if (!area) {
      std::clog << "Error opening email message area." << std::endl;
      return 1;
    }

    TextFile text_file(filename, "r");
    auto raw_text = text_file.ReadFileIntoString();
    auto lines = SplitString(raw_text, "\n", false);

    EmailData header{};
    header.from_system = 0;
    header.from_user = static_cast<uint16_t>(from_usernum);
    header.title = title;
    header.from_network_number = 0;
    header.user_number = static_cast<uint16_t>(to_num);
    header.daten = daten.to_daten_t();

    std::ostringstream ss;
    if (from_usernum >= 1 && from_name.empty()) {
      Names names(*config()->config());
      from_name = names.UserName(from_usernum);
    }
    ss << from_name << "\r\n";
    ss << daten.to_string() << "\r\n";
    ss << JoinStrings(lines, "\r\n");
    header.text = ss.str();

    return area->AddMessage(header) ? 0 : 1;
  }
};


bool EmailCommand::AddSubCommands() {
  if (!add(std::make_unique<EmailDumpCommand>())) {
    return false;
  }
  if (!add(std::make_unique<DeleteEmailCommand>())) {
    return false;
  }
  if (!add(std::make_unique<AddEmailCommand>())) {
    return false;
  }
  
  return true;
}

EmailDumpCommand::EmailDumpCommand()
    : BaseEmailSubCommand("dump", "Displays message header and text information.") {}

std::string EmailDumpCommand::GetUsage() const {
  std::ostringstream ss;
  ss << "Usage:   dump <base sub filename>" << std::endl;
  ss << "Example: dump general" << std::endl;
  return ss.str();
}

bool EmailDumpCommand::AddSubCommands() {
  add_argument({"start", "Starting message number.", "0"});
  add_argument({"end", "Last message number.", "-1"});
  add_argument(BooleanCommandLineArgument("all", "dumps everything, control lines too", false));

  return true;
}

// ReSharper disable once CppMemberFunctionMayBeConst
int EmailDumpCommand::ExecuteImpl(WWIVEmail* area, int start, int end, bool all) {

  const Names names(*config()->config());

  const auto last_message = end >= 0 ? end : area->number_of_messages();
  std::cout << "Email has " << area->number_of_messages() << " messages." << std::endl;
  std::cout << std::string(72, '-') << std::endl;
  for (auto current = start; current < last_message; current++) {
    mailrec m{};
    std::string text;
    if (!area->read_email_header_and_text(current, m, text)) {
      std::cout << "#" << current << "  ERROR " << std::endl;
      VLOG(1) << "Failed to read message number: " << current;
      std::cout << std::string(72, '-') << std::endl;
      continue;
    }
    std::cout << "Email #" << current << std::endl;
    const auto from_name  = names.UserName(m.fromuser);
    std::cout << "From: #" << m.fromuser << " (" << from_name << ")";
    if (m.fromsys) {
      std::cout << "@" << m.fromsys;
      if (m.status & status_source_verified) {
        std::cout << " at network #" << static_cast<int>(m.network.src_verified_msg.net_number) << std::endl;
        std::cout << "Source Verified Type: " << m.network.src_verified_msg.source_verified_type;
      } else {
        std::cout << " at network #" << static_cast<int>(m.network.network_msg.net_number);
      }
    }
    std::cout << std::endl;
    const auto to_name  = names.UserName(m.touser);
    std::cout << "To:   #" << m.touser << " (" << to_name << ")";
    if (m.tosys) {
      std::cout << "@" << m.tosys;
    }
    std::cout << std::endl;
    std::cout << "date:  " << daten_to_wwivnet_time(m.daten) << std::endl;
    std::cout << "title: " << m.title << std::endl;

    if (m.anony) {
      std::cout << "title: " << m.anony << std::endl;
    }
    auto has_status = false;
    if (m.status & status_multimail) {
      std::cout << "[MULTI]";
      has_status = true;
    }
    if (m.status & status_source_verified) {
      std::cout << "[VERIFIED]";
      has_status = true;
    }
    if (m.status & status_new_net) {
      std::cout << "[NET]";
      has_status = true;
    }
    if (m.status & status_seen) {
      std::cout << "[SEEN]";
      has_status = true;
    }
    if (m.status & status_replied) {
      std::cout << "[REPLIED]";
      has_status = true;
    }
    if (m.status & status_forwarded) {
      std::cout << "[FORWARDED]";
      has_status = true;
    }
    if (m.status & status_file) {
      std::cout << "[FILE]";
      has_status = true;
    }
    if (has_status) {
      std::cout << std::endl;
    }
    std::cout << std::string(72, '-') << std::endl;
    auto lines = wwiv::strings::SplitString(text, "\n", false);
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

int EmailDumpCommand::Execute() {
  if (!CreateMessageApi()) {
    std::clog << "Error Creating message api." << std::endl;
    return 1;
  }
    
  const auto start = iarg("start");
  const auto end = iarg("end");
  const auto start_date = sarg("start_date");
  const auto end_date = sarg("end_date");
  const auto all = barg("all");

  if (auto area = api().OpenEmail()) {
    // If we have dates, update the start and end numbers based on the dates.
    const auto last_message = end >= 0 ? end : area->number_of_messages();
    VLOG(1) << "start: " << start << "; end: " << last_message << std::endl;
    return ExecuteImpl(area.get(), start, last_message, all);
  }
  std::clog << "Error opening email area." << std::endl;
  return 1;
}

} // namespace wwiv
