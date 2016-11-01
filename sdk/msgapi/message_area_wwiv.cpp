/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2016, WWIV Software Services             */
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

#include "core/datafile.h"
#include "core/file.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "bbs/subacc.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/datetime.h"
#include "sdk/vardec.h"
#include "sdk/msgapi/message_api_wwiv.h"

namespace wwiv {
namespace sdk {
namespace msgapi {

using std::string;
using std::make_unique;
using std::unique_ptr;
using std::vector;
using wwiv::core::DataFile;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

constexpr char CD = 4;
constexpr char CZ = 26;

static WWIVMessageAreaHeader ReadHeader(DataFile<postrec>& file) {
  subfile_header_t raw_header;
  if (!file.Read(0, reinterpret_cast<postrec*>(&raw_header))) {
    // Invalid header.
    WWIVMessageAreaHeader header(0, 0);
    header.set_initialized(false);
    return header;
  }
  if (raw_header.active_message_count > file.number_of_records()) {
    /*LOG(INFO) << "Header claims too many messages, raw_header.active_message_count("
      << raw_header.active_message_count << ") > file.number_of_records("
      << file.number_of_records() << ")";
    */
    raw_header.active_message_count = static_cast<uint16_t>(file.number_of_records());
  }
  return WWIVMessageAreaHeader(raw_header);
}

static bool WriteHeader(DataFile<postrec>& file, const WWIVMessageAreaHeader& header) {
  auto p = header.header();
  // Increment the mod_count every time we write the header.
  p.mod_count++;
  return file.Write(0, reinterpret_cast<const postrec*>(&p));
}

WWIVMessageAreaHeader::WWIVMessageAreaHeader(uint16_t wwiv_num_version, uint32_t num_messages)
  : header_(subfile_header_t()) {
  strcpy(header_.signature, "WWIV\x1A");
  header_.revision = 1;
  header_.wwiv_version = wwiv_num_version;
  header_.daten_created = static_cast<uint32_t>(time(nullptr));
  header_.active_message_count = num_messages;
}

WWIVMessageArea::WWIVMessageArea(WWIVMessageApi* api, const std::string& sub_filename, const std::string& text_filename)
  : MessageArea(api), Type2Text(text_filename), sub_filename_(sub_filename) {
  DataFile<postrec> sub(sub_filename_, File::modeBinary | File::modeReadOnly);
  if (!sub) {
    // TODO: throw exception
  }
  open_ = true;
}

WWIVMessageArea::~WWIVMessageArea() {
  Close();
}

bool WWIVMessageArea::Close() {
  open_ = false;
  return true;
}

bool WWIVMessageArea::Lock() {
  return false;
}

bool WWIVMessageArea::Unlock() {
  return false;
}

void WWIVMessageArea::ReadMessageAreaHeader(MessageAreaHeader& header) {
  DataFile<postrec> sub(sub_filename_);
  header = ReadHeader(sub);
}

void WWIVMessageArea::WriteMessageAreaHeader(const MessageAreaHeader& header) {
  // Not implemented on wwiv.
}

int WWIVMessageArea::FindUserMessages(const std::string& user_name) {
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

bool WWIVMessageArea::ParseMessageText(
  const postrec& header,
  int message_number,
  string& from_username,
  string& date, string& to,
  string& in_reply_to, string& text) {

  // Some of the message header information ends up in the text.
  // line1: From username (i.e. rushfan #1 @5161)
  // line2: Date (again, same as daten but is formatted by the sender)
  // optional lines:
  // RE: Title (title this is a reply to, mostly redundant since the title will contain it too)
  // BY: Author (author of the post this is a reply to, could be considered the "to" person for this message.
  // ^DControl Lines (we have many)
  // ^D# (0 = network, >0 = tag lines)

  string raw_text;
  if (!readfile(&header.msg, &raw_text)) {
    return false;
  }

  vector<string> lines = SplitString(raw_text, "\n");
  auto it = lines.begin();
  if (it == std::end(lines)) {
    VLOG(1) << "Malformed message(1) #" << message_number << "; title: '" << header.title << "' " << header.owneruser << "@" << header.ownersys;
    return true; 
  }

  from_username = *it++;
  StringTrim(&from_username);
  if (it == lines.end()) {
    VLOG(1) << "Malformed message(2) #" << message_number << "; title: '" << header.title << "' " << header.owneruser << "@" << header.ownersys;
    return true;
  }

  date = *it++;
  StringTrim(&date);
  if (it == lines.end()) {
    VLOG(1) << "Malformed message(3) #" << message_number << "; title: '" << header.title << "' " << header.owneruser << "@" << header.ownersys;
    return true;
  }

  for (; it != lines.end(); it++) {
    auto line = *it;
    StringTrim(&line);
    if (!line.empty() && line.front() == CD) {
      text += line;
      text += "\r\n";
    } else if (starts_with(line, "RE:")) {
      in_reply_to = line.substr(3);
      StringTrim(&in_reply_to);
    } else if (starts_with(line, "BY:")) {
      to = line.substr(3);
      StringTrim(&to);
    } else {
      // No more special lines, the rest is just text.
      for (; it != std::end(lines); it++) {
        string text_line = *it;
        // Terminate the string with a control-Z.
        string::size_type cz_pos = text_line.find(CZ);
        if (cz_pos != string::npos) {
          text_line = text_line.substr(0, cz_pos);
        }
        // Trim all remaining nulls.
        string::size_type null_pos = text_line.find((char)0);
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

WWIVMessage* WWIVMessageArea::ReadMessage(int message_number) {
  int num_messages = number_of_messages();
  if (message_number < 1) {
    return nullptr;
  } else if (message_number > num_messages) {
    message_number = num_messages;
  }

  DataFile<postrec> sub(sub_filename_);
  if (!sub) {
    // TODO: throw exception
    return nullptr;
  }
  postrec header;
  sub.Read(message_number, &header);
  if (header.msg.storage_type != 2) {
    // We only support type-2 on the WWIV API.
    return nullptr;
  }

  string from_username, date, to, in_reply_to, text;
  if (!ParseMessageText(header, message_number, from_username, date, to, in_reply_to, text)) {
    return nullptr;
  }

  return new WWIVMessage(
    std::unique_ptr<WWIVMessageHeader>(new WWIVMessageHeader(header, from_username, to, in_reply_to, api_)),
    make_unique<WWIVMessageText>(text));
}

WWIVMessageHeader* WWIVMessageArea::ReadMessageHeader(int message_number) {
  unique_ptr<WWIVMessage> msg(ReadMessage(message_number));
  if (!msg) {
    return nullptr;
  }
  return msg->release_header();
}

WWIVMessageText* WWIVMessageArea::ReadMessageText(int message_number) {
  unique_ptr<WWIVMessage> msg(ReadMessage(message_number));
  if (!msg) {
    return nullptr;
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
  DataFile<statusrec_t> file(config.datadir(), STATUS_DAT,
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

bool WWIVMessageArea::AddMessage(const Message& message) {
  messagerec m{STORAGE_TYPE, 0xffffff};

  const WWIVMessageHeader& header = *dynamic_cast<const WWIVMessage&>(message).header();
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
      // TODO(rushfan): Fail here?
    }
  } else {
    // TODO(rushfan): Make this a VLOG(2)
    VLOG(3) << "AddMessage called with existing qscan ptr: title: " << message.header()->title()
            << "; qscan: " << header.data().qscan;
  }
  p.daten = header.daten();
  p.status = header.status();
  //if (session()->user()->IsRestrictionValidate()) {
  //  p.status |= status_unvalidated;
  //}

  const string text = StrCat(header.from(), "\r\n",
    daten_to_humantime(header.daten()), "\r\n",
    message.text()->text());
  if (!savefile(text, &p.msg)) {
    return false;
  }
  return add_post(p);
}

bool WWIVMessageArea::DeleteMessage(int message_number) {
  int num_messages = number_of_messages();
  if (message_number < 1) {
    return false;
  } else if (message_number > num_messages) {
    return false;
  }

  DataFile<postrec> sub(sub_filename_, File::modeBinary | File::modeCreateFile | File::modeReadWrite);
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
  header.owneruser = num_messages - 1;
  sub.Write(0, &header);

  return true;
}

WWIVMessage* WWIVMessageArea::CreateMessage() {
  return new WWIVMessage(
    make_unique<WWIVMessageHeader>(api_),
    make_unique<WWIVMessageText>());
}

// Implementation Details

bool WWIVMessageArea::add_post(const postrec& post) {
  DataFile<postrec> sub(sub_filename_, File::modeBinary|File::modeReadWrite);
  if (!sub) {
    return false;
  }
  if (sub.number_of_records() == 0) {
    return false;
  }
  WWIVMessageAreaHeader wwiv_header = ReadHeader(sub);
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

}  // namespace msgapi
}  // namespace sdk
}  // namespace wwiv
