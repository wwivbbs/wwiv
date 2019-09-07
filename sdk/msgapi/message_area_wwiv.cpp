/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2019, WWIV Software Services             */
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
#include "sdk/msgapi/message_area_wwiv.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "bbs/subacc.h"
#include "core/datafile.h"
#include "core/datetime.h"
#include "core/file.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/version.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/msgapi/message_api_wwiv.h"
#include "sdk/net/packets.h"
#include "sdk/ssm.h"
#include "sdk/usermanager.h"
#include "sdk/vardec.h"

namespace wwiv {
namespace sdk {
namespace msgapi {

using std::make_unique;
using std::string;
using std::unique_ptr;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::stl;
using namespace wwiv::strings;

constexpr char CD = 4;
constexpr char CZ = 26;

static bool WriteHeader(DataFile<postrec>& file, const WWIVMessageAreaHeader& header) {
  auto p = header.header();
  // Increment the mod_count every time we write the header.
  p.mod_count++;
  return file.Write(0, reinterpret_cast<const postrec*>(&p));
}

static WWIVMessageAreaHeader ReadHeader(DataFile<postrec>& file) {
  subfile_header_t raw_header;
  if (!file.Read(0, reinterpret_cast<postrec*>(&raw_header))) {
    // Invalid header.
    WWIVMessageAreaHeader header(0, 0);
    header.set_initialized(false);
    return header;
  }
  if (raw_header.active_message_count > file.number_of_records()) {
    VLOG(1) << "Header claims too many messages, raw_header.active_message_count("
            << raw_header.active_message_count << ") > file.number_of_records("
            << file.number_of_records() << ")";
    raw_header.active_message_count = static_cast<uint16_t>(file.number_of_records());
  }

  if (strncmp(raw_header.signature, "WWIV\x1A", 5) != 0) {
    VLOG(3) << "Missing 5.x header on sub: " << file.file();
    auto saved_count = raw_header.active_message_count;
    memset(&raw_header, 0, sizeof(subfile_header_t));
    // We don't have a modern header. Create one now. Next write
    // from here will have it.
    strcpy(raw_header.signature, "WWIV\x1A");
    raw_header.active_message_count = saved_count;
    raw_header.revision = 1;
    raw_header.wwiv_version = wwiv_num_version;
    raw_header.daten_created = DateTime::now().to_daten_t();

    // We probably can't write the header here since the datafile is usually
    // only open for read only at this point.
    // if (!WriteHeader(file, WWIVMessageAreaHeader(raw_header))) {
    //   VLOG(4) << "Unable to write 5.2 header to file: " << file.file();
    // }
  }

  return WWIVMessageAreaHeader(raw_header);
}

WWIVMessageAreaLastRead::WWIVMessageAreaLastRead(WWIVMessageApi* api, int message_area_number)
    : MessageAreaLastRead(api), wapi_(api), message_area_number_(message_area_number) {}

uint32_t WWIVMessageAreaLastRead::last_read(int) {
  if (message_area_number_ < 0) {
    return 0;
  }
  return wapi_->last_read(message_area_number_);
}

bool WWIVMessageAreaLastRead::set_last_read(int, uint32_t last_read, uint32_t highest_read) {
  if (message_area_number_ < 0) {
    // Fake area - nothing to do.
    return true;
  }
  wapi_->set_last_read(message_area_number_, std::max<uint32_t>(last_read, highest_read));
  return true;
}

bool WWIVMessageAreaLastRead::Close() { return false; }

WWIVMessageAreaHeader::WWIVMessageAreaHeader(uint16_t expected_wwiv_num_version,
                                             uint32_t num_messages)
    : header_(subfile_header_t()) {
  strcpy(header_.signature, "WWIV\x1A");
  header_.revision = 1;
  header_.wwiv_version = expected_wwiv_num_version;
  header_.daten_created = DateTime::now().to_daten_t();
  header_.active_message_count = static_cast<uint16_t>(num_messages);
}

WWIVMessageArea::WWIVMessageArea(WWIVMessageApi* api, const subboard_t sub,
                                 const std::filesystem::path& sub_filename,
                                 const std::filesystem::path& text_filename,
                                 int subnum)
    : MessageArea(api), Type2Text(text_filename), wwiv_api_(api), sub_(sub),
      sub_filename_(sub_filename), header_{}, subnum_(subnum) {
  DataFile<postrec> subfile(sub_filename_, File::modeBinary | File::modeReadOnly);
  if (!subfile) {
    // TODO: throw exception
  } else {
    WWIVMessageAreaHeader h = ReadHeader(subfile);
    header_ = h.raw_header();
  }
  open_ = true;
  last_read_.reset(new WWIVMessageAreaLastRead(api, subnum));
}

WWIVMessageArea::~WWIVMessageArea() { Close(); }

bool WWIVMessageArea::Close() {
  open_ = false;
  return true;
}

bool WWIVMessageArea::Lock() { return false; }

bool WWIVMessageArea::Unlock() { return false; }

void WWIVMessageArea::ReadMessageAreaHeader(MessageAreaHeader& header) {
  DataFile<postrec> sub(sub_filename_);
  WWIVMessageAreaHeader h = ReadHeader(sub);
  header_ = h.raw_header();
  header = h;
}

void WWIVMessageArea::WriteMessageAreaHeader(const MessageAreaHeader&) {
  // Not implemented on wwiv.
}

int WWIVMessageArea::FindUserMessages(const std::string&) {
  // Not implemented on wwiv.
  return 0;
}

int WWIVMessageArea::number_of_messages() {
  DataFile<postrec> sub(sub_filename_);
  if (!sub) {
    // TODO: throw exception
    return 0;
  }

  const int file_num_records = sub.number_of_records();
  WWIVMessageAreaHeader wwiv_header = ReadHeader(sub);
  if (!wwiv_header.initialized()) {
    // TODO: throw exception
    // This is an invalid header.
    return 0;
  }
  int msgs = wwiv_header.active_message_count();
  if (msgs > file_num_records) {
    LOG(ERROR) << "Mismatch between header: " << msgs << " and filesize: " << file_num_records;
    return std::min(msgs, file_num_records);
  }
  return msgs;
}

bool WWIVMessageArea::ParseMessageText(const postrec& header, int message_number,
                                       string& from_username, string& date, string& to,
                                       string& in_reply_to, string& text) {

  // Some of the message header information ends up in the text.
  // line1: From username (i.e. rushfan #1 @5161)
  // line2: Date (again, same as daten but is formatted by the sender)
  // optional lines:
  // RE: Title (title this is a reply to, mostly redundant since the title will contain it too)
  // BY: Author (author of the post this is a reply to, could be considered the "to" person for this
  // message. ^DControl Lines (we have many) ^D# (0 = network, >0 = tag lines)

  string raw_text;
  if (!readfile(&header.msg, &raw_text)) {
    return false;
  }

  // Use the 3 arg form of split string so we don't strip blank lines.
  vector<string> lines = SplitString(raw_text, "\n", false);
  auto it = std::begin(lines);
  if (it == std::end(lines)) {
    VLOG(1) << "Malformed message(1) #" << message_number << "; title: '" << header.title << "' "
            << header.owneruser << "@" << header.ownersys;
    return true;
  }

  from_username = StringTrim(*it++);
  if (it == lines.end()) {
    VLOG(1) << "Malformed message(2) #" << message_number << "; title: '" << header.title << "' "
            << header.owneruser << "@" << header.ownersys;
    return true;
  }

  date = StringTrim(*it++);
  if (it == std::end(lines)) {
    VLOG(1) << "Malformed message(3) #" << message_number << "; title: '" << header.title << "' "
            << header.owneruser << "@" << header.ownersys;
    return true;
  }

  for (; it != std::end(lines); it++) {
    auto line = StringTrim(*it);
    if (!line.empty() && line.front() == CD) {
      text += line;
      text += "\r\n";
    } else if (starts_with(line, "RE:")) {
      in_reply_to = StringTrim(line.substr(3));
    } else if (starts_with(line, "BY:")) {
      to = StringTrim(line.substr(3));
    } else {
      // No more special lines, the rest is just text.
      for (; it != std::end(lines); it++) {
        auto text_line = *it;
        // Terminate the string with a control-Z.
        auto cz_pos = text_line.find(CZ);
        if (cz_pos != string::npos) {
          text_line = text_line.substr(0, cz_pos);
        }
        // Trim all remaining nulls.
        auto null_pos = text_line.find((char)0);
        if (null_pos != string::npos) {
          text_line.resize(null_pos);
        }
        StringTrim(&text_line);
        if (!text_line.empty()) {
          text += text_line;
          text += "\r\n";
        }
      }
      break;
    }
  }
  return true;
}

unique_ptr<Message> WWIVMessageArea::ReadMessage(int message_number) {
  int num_messages = number_of_messages();
  if (message_number < 1) {
    return unique_ptr<Message>();
  } else if (message_number > num_messages) {
    message_number = num_messages;
  }

  DataFile<postrec> sub(sub_filename_);
  if (!sub) {
    // TODO: throw exception
    return {};
  }
  postrec header;
  sub.Read(message_number, &header);
  if (header.msg.storage_type != 2) {
    // We only support type-2 on the WWIV API.
    return {};
  }

  string from_username, date, to, in_reply_to, text;
  if (!ParseMessageText(header, message_number, from_username, date, to, in_reply_to, text)) {
    return {};
  }

  return make_unique<WWIVMessage>(
      make_unique<WWIVMessageHeader>(header, from_username, to, in_reply_to, api_),
      make_unique<WWIVMessageText>(text));
}

unique_ptr<MessageHeader> WWIVMessageArea::ReadMessageHeader(int message_number) {
  auto msg = ReadMessage(message_number);
  if (!msg) {
    return {};
  }
  return msg->release_header();
}

unique_ptr<MessageText> WWIVMessageArea::ReadMessageText(int message_number) {
  auto msg = ReadMessage(message_number);
  if (!msg) {
    return {};
  }
  return msg->release_text();
}

static uint32_t next_qscan_value_and_increment_post(const string& bbsdir) {
  statusrec_t statusrec{};
  uint32_t next_qscan = 0;
  Config config(bbsdir);
  if (!config.IsInitialized()) {
    LOG(ERROR) << "Unable to load CONFIG.DAT.";
    return 1;
  }
  DataFile<statusrec_t> file(PathFilePath(config.datadir(), STATUS_DAT),
                             File::modeBinary | File::modeReadWrite);
  if (!file) {
    return 0;
  }
  if (!file.Read(0, &statusrec)) {
    return 0;
  }
  next_qscan = statusrec.qscanptr++;
  ++statusrec.msgposttoday;
  if (!file.Write(0, &statusrec)) {
    return 0;
  }
  return next_qscan;
}

/**
 * Deletes all excess messages in an area, depending on the
 * overflow strategy set on the API.
 *
 * Note: This should be called by AddMessage *after* posting a new
 * message successfull, since there's no sense in deleting a post if
 * adding a new one hasn't succeeded.
 *
 * Returns the number of messages deleted.
 */
int WWIVMessageArea::DeleteExcess() {
  if (api_->options().overflow_strategy == OverflowStrategy::delete_none) {
    LOG(INFO) << "overflow_strategy is delete_none. Not deleting overflow messages";
    return 0;
  }

  int result = 0;
  bool done = false;
  while (!done) {
    if (api_->options().overflow_strategy == OverflowStrategy::delete_one) {
      LOG(INFO) << "overflow_strategy is delete_one.";
      done = true;
    }
    auto num = number_of_messages();
    if (num <= max_messages_) {
      VLOG(1) << "No overflow messages. " << num << " <= " << max_messages_;
      return result;
    }
    int i = 1;
    int dm = 0;
    while (i <= number_of_messages()) {
      auto pp = ReadMessageHeader(i);
      if (!pp) {
        break;
      } else if (!pp->locked()) {
        dm = i;
        break;
      }
      ++i;
    }
    if (dm == 0) {
      LOG(INFO) << "DeleteExcess: No message to delete.";
      return result;
    }
    if (!DeleteMessage(dm)) {
      LOG(INFO) << "DeleteExcess: Failed to delete message #" << dm;
      return result;
    } else {
      LOG(INFO) << "DeleteExcess: Deleted message #" << dm;
    }
    ++result;
  }
  return result;
}

bool WWIVMessageArea::AddMessage(const Message& message, const MessageAreaOptions& options) {
  messagerec m{STORAGE_TYPE, 0xffffff};

  const auto& header = dynamic_cast<const WWIVMessageHeader&>(message.header());
  postrec p = header.data();
  p.anony = 0;
  p.msg = m;
  p.ownersys = header.from_system();
  p.owneruser = header.from_usernum();
  if (p.qscan == 0) {
    // new message.
    VLOG(3) << "AddMessage needs a qscan";
    p.qscan = next_qscan_value_and_increment_post(api_->root_directory());
    if (p.qscan == 0) {
      LOG(ERROR) << "Failed to get qscan value!";
      return false;
    }
  } else {
    VLOG(2) << "AddMessage called with existing qscan ptr: title: " << message.header().title()
            << "; qscan: " << header.last_read();
  }
  p.daten = header.daten();
  p.status = header.status();
  // if (a()->user()->IsRestrictionValidate()) {
  //  p.status |= status_unvalidated;
  //}
  // If we need network validation, then mark it as such.
  // We force the issue here if it wasn't already done.
  if (!sub_.nets.empty()) {
    if (sub_.anony & anony_val_net && !header.pending_network()) {
      p.status |= status_pending_net;
      UserManager um(wwiv_api_->config());
      SSM ssm(wwiv_api_->config(), um);
      ssm.send_local(1, StrCat("Unvalidated net posts on: ", sub_.name));
    } else if (options.send_post_to_network) {
      LOG(INFO) << "** Sending the newly added message out on all of the networks.";
      auto net = *sub_.nets.begin();
      const auto& wm = dynamic_cast<const WWIVMessage&>(message);
      // Create a base packet from the 1st network entry.
      auto packet = create_packet_from_wwiv_message(wm, net.stype, {});
      // Send the packet to everyone who needs is.
      send_post_to_subscribers(wwiv_api_->network(), net.net_num, net.stype, sub_, packet, {},
                               subscribers_send_to_t::all_subscribers);
    } else {
      VLOG(1) << "Not sending message to the network";
    }
  }

  auto text = StrCat(header.from(), "\r\n", daten_to_wwivnet_time(header.daten()), "\r\n",
                     message.text().text());

  // WWIV 4.x requires a control-Z to terminate the message, WWIV 5.x
  // does not, and removes it on read.
  if (text.back() != CZ) {
    text.push_back(CZ);
  }

  if (!savefile(text, &p.msg)) {
    LOG(ERROR) << "Failed to save message text.";
    return false;
  }
  auto result = add_post(p);
  if (result) {
    DeleteExcess();
  }
  return result;
}

bool WWIVMessageArea::DeleteMessage(int message_number) {
  auto num_messages = number_of_messages();
  if (message_number < 1) {
    return false;
  } else if (message_number > num_messages) {
    return false;
  }

  DataFile<postrec> sub(sub_filename_,
                        File::modeBinary | File::modeCreateFile | File::modeReadWrite);
  if (!sub) {
    // TODO: throw exception
    return false;
  }
  postrec post;
  sub.Read(message_number, &post);
  if (post.msg.storage_type != 2) {
    // We only support type-2 on the WWIV API.
    return false;
  }

  // Remove text
  remove_link(post.msg);

  // Remove post record.
  for (int cur = message_number + 1; cur <= num_messages; cur++) {
    postrec tmp;
    sub.Read(cur, &tmp);
    sub.Write(cur - 1, &tmp);
  }

  // Update header.
  postrec header;
  sub.Read(0, &header);
  // decrement number of posts.
  // TODO(rushfan): Should we check these?
  --header.owneruser;
  header.owneruser = static_cast<uint16_t>(std::max(0, num_messages - 1));
  sub.Write(0, &header);

  return true;
}

bool WWIVMessageArea::ResyncMessage(int& message_number) {
  auto m = ReadMessage(message_number);
  if (!m) {
    auto num_messages = number_of_messages();
    if (message_number > num_messages) {
      message_number = num_messages;
      return true;
    }
    return false;
  }

  if (!HasSubChanged()) {
    // Since we also use resynch to see if were past the last message...
    // return true;
  }

  // remember m is destructed after this message call.
  return ResyncMessageImpl(message_number, *m);
}

bool WWIVMessageArea::HasSubChanged() {
  subfile_header_t last_read_header = this->header_;
  subfile_header_t current_read_header = {};
  {
    DataFile<postrec> sub(sub_filename_, File::modeBinary | File::modeReadOnly);
    WWIVMessageAreaHeader h = ReadHeader(sub);
    current_read_header = h.raw_header();
  }

  return current_read_header.mod_count > last_read_header.mod_count;
}

bool WWIVMessageArea::ResyncMessage(int& message_number, Message& raw_message) {
  if (!HasSubChanged()) {
    // Since we also use resynch to see if were past the last message...
    // return true;
  }

  // Assume it has changed.
  return ResyncMessageImpl(message_number, raw_message);
}

static bool IsSamePost(const postrec& l, const postrec& r) {
  return l.qscan == r.qscan && l.anony == r.anony && l.daten == r.daten &&
         l.ownersys == r.ownersys && l.owneruser == r.owneruser &&
         l.msg.stored_as == r.msg.stored_as;
}

bool WWIVMessageArea::ResyncMessageImpl(int& message_number, Message& raw_message) {
  WWIVMessage& message = dynamic_cast<WWIVMessage&>(raw_message);
  const auto& wwiv_header = dynamic_cast<const WWIVMessageHeader&>(message.header());
  const auto& p = wwiv_header.data();

  auto num_messages = number_of_messages();
  if (message_number > num_messages) {
    message_number = num_messages;
    return true;
  }

  auto pp1 = ReadMessageHeader(message_number);
  if (!pp1) {
    return true;
  }

  auto pp1_header = dynamic_cast<WWIVMessageHeader*>(pp1.get())->header_;

  if (IsSamePost(pp1_header, p)) {
    return true;
  }
  if (p.qscan < pp1_header.qscan) {
    // Search from the current message to the first.
    int num_msgs = number_of_messages();
    if (message_number > num_msgs) {
      message_number = num_msgs + 1;
    }
    for (int i = message_number - 1; i > 0; i--) {
      pp1 = ReadMessageHeader(i);
      pp1_header = dynamic_cast<WWIVMessageHeader*>(pp1.get())->header_;
      if (!pp1) {
        continue;
      }
      if (p.qscan >= pp1_header.qscan || IsSamePost(p, pp1_header)) {
        message_number = i;
        return true;
      }
    }
    message_number = 0;
    return true;
  }

  // Search the full list
  int num_msgs = number_of_messages();
  for (int i = message_number + 1; i <= num_msgs; i++) {
    pp1 = ReadMessageHeader(i);
    if (!pp1) {
      continue;
    }
    if (p.qscan >= pp1_header.qscan || IsSamePost(p, pp1_header)) {
      message_number = i;
      return true;
    }
  }
  message_number = num_msgs;
  return true;
}

std::unique_ptr<Message> WWIVMessageArea::CreateMessage() {
  return make_unique<WWIVMessage>(make_unique<WWIVMessageHeader>(api_),
                                  make_unique<WWIVMessageText>());
}

bool WWIVMessageArea::Exists(daten_t d, const std::string& title, uint16_t from_system,
                             uint16_t from_user) {
  DataFile<postrec> sub(sub_filename_);
  if (!sub) {
    return false;
  }
  std::vector<postrec> headers;
  if (!sub.ReadVector(headers)) {
    return false;
  }

  for (const auto& h : headers) {
    if (h.status & status_delete) {
      continue;
    }
    // Since we don't have a global message id, use the combination of
    // date + title + from system + from user.
    if (h.daten == d && iequals(h.title, title) && h.ownersys == from_system &&
        h.owneruser == from_user) {
      return true;
    }
  }

  return false;
}

MessageAreaLastRead& WWIVMessageArea::last_read() const noexcept { return *last_read_; }

message_anonymous_t WWIVMessageArea::anonymous_type() const noexcept {
  switch (sub_.anony & 0xf0) {
  case anony_none:
    return message_anonymous_t::anonymous_none;
  case anony_enable_anony:
    return message_anonymous_t::anonymous_allowed;
  case anony_enable_dear_abby:
    return message_anonymous_t::anonymous_dear_abby;
  case anony_force_anony:
    return message_anonymous_t::anonymous_forced;
  case anony_real_name:
    return message_anonymous_t::anonymous_real_names_only;
  }
  // WTF CHECK?
  return message_anonymous_t::anonymous_none;
}

// Implementation Details

bool WWIVMessageArea::add_post(const postrec& post) {
  DataFile<postrec> sub(sub_filename_, File::modeBinary | File::modeReadWrite);
  if (!sub) {
    return false;
  }
  if (sub.number_of_records() == 0) {
    return false;
  }
  WWIVMessageAreaHeader wwiv_header(ReadHeader(sub));
  if (!wwiv_header.initialized()) {
    // This is an invalid header.
    return false;
  }
  uint32_t msgnum = wwiv_header.increment_active_message_count();

  // add the new post
  if (!sub.Write(msgnum, &post)) {
    return false;
  }
  // Write the header now.
  return WriteHeader(sub, wwiv_header);
}

} // namespace msgapi
} // namespace sdk
} // namespace wwiv
