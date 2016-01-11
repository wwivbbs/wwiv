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
#include "sdk/msgapi/msgapi_wwiv.h"
#include "sdk/msgapi/message_api_wwiv.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/datafile.h"
#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "bbs/subacc.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/datetime.h"
#include "sdk/vardec.h"

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
static constexpr int GAT_NUMBER_ELEMENTS = 2048;
static constexpr int GAT_SECTION_SIZE = GAT_NUMBER_ELEMENTS * sizeof(uint16_t);
static constexpr int MSG_BLOCK_SIZE = 512;

static constexpr int  GATSECLEN = GAT_SECTION_SIZE + GAT_NUMBER_ELEMENTS * MSG_BLOCK_SIZE;
#define MSG_STARTING(section__) (section__ * GATSECLEN + GAT_SECTION_SIZE)


WWIVMessageArea::WWIVMessageArea(WWIVMessageApi* api, const std::string& sub_filename, const std::string& text_filename)
  : MessageArea(api),
    sub_filename_(sub_filename), text_filename_(text_filename) {
  DataFile<postrec> sub(sub_filename_, File::modeBinary | File::modeReadOnly);
  if (!sub) {
    // TODO: throw exception
  }
  postrec header;
  sub.Read(0, &header);
  last_num_messages_ = header.owneruser;
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
  // Not implemented on wwiv.
}

void WWIVMessageArea::WriteMessageAreaHeader(const MessageAreaHeader & header) {
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
  postrec header;
  sub.Read(0, &header);
  last_num_messages_ = header.owneruser;
  return header.owneruser;
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

  // Some of the message header information ends up in the text.
  // line1: From username (i.e. rushfan #1 @5161)
  // line2: Date (again, same as daten but is formatted by the sender)
  // optional lines:
  // RE: Title (title this is a reply to, mostly redundant since the title will contain it too)
  // BY: Author (author of the post this is a reply to, could be considered the "to" person for this message.
  // ^DControl Lines (we have many)
  // ^D# (0 = network, >0 = tag lines)

  string raw_text;
  if (!readfile(&header.msg, this->text_filename_, &raw_text)) {
    return nullptr;
  }

  vector<string> lines = SplitString(raw_text, "\n");
  auto it = lines.begin();
  string from_username = StringTrim(*it++);
  string date = StringTrim(*it++);
  string to;
  string in_reply_to;
  string text;
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
  unique_ptr<WWIVMessageHeader> wwiv_header(new WWIVMessageHeader(header, from_username, to, in_reply_to, api_));
  unique_ptr<WWIVMessageText> wwiv_text(new WWIVMessageText(text));

  return new WWIVMessage(std::move(wwiv_header), std::move(wwiv_text));
}

WWIVMessageHeader* WWIVMessageArea::ReadMessageHeader(int message_number) {
  unique_ptr<WWIVMessage> msg(ReadMessage(message_number));
  return msg->release_header();
}

WWIVMessageText* WWIVMessageArea::ReadMessageText(int message_number) {
  unique_ptr<WWIVMessage> msg(ReadMessage(message_number));
  return msg->release_text();
}

static uint32_t next_qscan_value(const string& bbsdir) {
  statusrec status;
  uint32_t next_qscan = 0;
  Config config(bbsdir);
  if (!config.IsInitialized()) {
    // LOG << "Unable to load CONFIG.DAT.";
    return 1;
  }
  DataFile<statusrec> file(config.datadir(), STATUS_DAT,
    File::modeBinary | File::modeReadWrite);
  if (!file) {
    return 0;
  }
  if (!file.Read(0, &status)) {
    return 0;
  }
  next_qscan = status.qscanptr++;
  if (!file.Write(0, &status)) {
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
  p.qscan = next_qscan_value(api_->root_directory());
  if (p.qscan == 0) {
    // TODO(rushfan): Fail here?
  }
  p.daten = header.daten();
  p.status = header.status();
  //if (session()->user()->IsRestrictionValidate()) {
  //  p.status |= status_unvalidated;
  //}

  string text = StrCat(header.from(), "\r\n",
    daten_to_humantime(header.daten()), "\r\n");
  text += message.text()->text();
  savefile(text, &p.msg, text_filename_);
  add_post(p);
  return true;
}

bool WWIVMessageArea::DeleteMessage(int message_number) {
  int num_messages = number_of_messages();
  if (message_number < 1) {
    return nullptr;
  } else if (message_number > num_messages) {
    return nullptr;
  }

  DataFile<postrec> sub(sub_filename_, File::modeBinary | File::modeCreateFile | File::modeReadWrite);
  if (!sub) {
    // TODO: throw exception
    return nullptr;
  }
  postrec post;
  sub.Read(message_number, &post);
  if (post.msg.storage_type != 2) {
    // We only support type-2 on the WWIV API.
    return nullptr;
  }

  // Remove text
  remove_link(post.msg, text_filename_);

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

void WWIVMessageArea::remove_link(messagerec& msg, const string& filename) {
  unique_ptr<File> file(OpenMessageFile(filename));
  if (file->IsOpen()) {
    size_t section = static_cast<int>(msg.stored_as / GAT_NUMBER_ELEMENTS);
    vector<uint16_t> gat = load_gat(*file, section);
    uint32_t current_section = msg.stored_as % GAT_NUMBER_ELEMENTS;
    while (current_section > 0 && current_section < GAT_NUMBER_ELEMENTS) {
      uint32_t next_section = static_cast<long>(gat[current_section]);
      gat[current_section] = 0;
      current_section = next_section;
    }
    save_gat(*file, section, gat);
    file->Close();
  }
}

bool WWIVMessageArea::add_post(const postrec& post) {
  DataFile<postrec> sub(sub_filename_, File::modeBinary|File::modeReadWrite);
  if (!sub) {
    return false;
  }
  postrec header;
  if (!sub.Read(0, &header)) {
    return false;
  }
  // increment the post count.
  header.owneruser++;

  // add the new post
  if (!sub.Write(header.owneruser, &post)) {
    return false;
  }
  // Write the header now.
  return sub.Write(0, &header);
}

/**
* Opens the message area file {messageAreaFileName} and returns the file handle.
* Note: This is a Private method to this module.
*/
File* WWIVMessageArea::OpenMessageFile(const string msgs_filename) {
  // TODO(rushfan): Pass in the status manager. this is needed to
  // set session()->subchg if any of the subs receive a post so that 
  // resynch can work right on multi node configs.
  // 
  // session()->status_manager()->RefreshStatusCache();

  unique_ptr<File> message_file(new File(msgs_filename));
  if (!message_file->Open(File::modeReadWrite | File::modeBinary)) {
    // Create should have created this.
    // TODO(rushfan): Set error code
    return nullptr;
  }
  return message_file.release();
}

std::vector<uint16_t> WWIVMessageArea::load_gat(File& file, size_t section) {
  std::vector<uint16_t> gat(GAT_NUMBER_ELEMENTS);
  auto file_size = file.GetLength();
  auto section_pos = section * GATSECLEN;
  if (file_size < static_cast<long>(section_pos)) {
    file.SetLength(section_pos);
    file_size = section_pos;
  }
  file.Seek(section_pos, File::seekBegin);
  if (file_size < static_cast<long>(section_pos + GAT_SECTION_SIZE)) {
    // TODO(rushfan): Check that gat is loaded.
    file.Write(&gat[0], GAT_SECTION_SIZE);
  } else {
    // TODO(rushfan): Check that gat is loaded.
    file.Read(&gat[0], GAT_SECTION_SIZE);
  }
  return gat;
}

void WWIVMessageArea::save_gat(File& file, size_t section, const std::vector<uint16_t>& gat) {
  long section_pos = static_cast<long>(section * GATSECLEN);
  file.Seek(section_pos, File::seekBegin);
  file.Write(&gat[0], GAT_SECTION_SIZE);

  // TODO(rushfan): Pass in the status manager. this is needed to
  // set session()->subchg if any of the subs receive a post so that 
  // resynch can work right on multi node configs.
  // 
  // WStatus *pStatus = session()->status_manager()->BeginTransaction();
  // pStatus->IncrementFileChangedFlag(WStatus::fileChangePosts);
  // session()->status_manager()->CommitTransaction(pStatus);
}

bool WWIVMessageArea::readfile(const messagerec* msg, string msgs_filename, string* out) {
  out->clear();
  unique_ptr<File> file(OpenMessageFile(msgs_filename));
  if (!file) {
    // TODO(rushfan): set error code,
    return false;
  }
  const size_t gat_section = msg->stored_as / GAT_NUMBER_ELEMENTS;
  vector<uint16_t> gat = load_gat(*file, gat_section);

  uint32_t current_section = msg->stored_as % GAT_NUMBER_ELEMENTS;
  while (current_section > 0 && current_section < GAT_NUMBER_ELEMENTS) {
    file->Seek(MSG_STARTING(gat_section) + MSG_BLOCK_SIZE * static_cast<uint32_t>(current_section), File::seekBegin);
    char b[MSG_BLOCK_SIZE + 1];
    file->Read(b, MSG_BLOCK_SIZE);
    b[MSG_BLOCK_SIZE] = 0;
    out->append(b);
    current_section = gat[current_section];
  }

  string::size_type last_cz = out->find_last_of(CZ);
  std::string::size_type last_block_start = out->length() - MSG_BLOCK_SIZE;
  if (last_cz != string::npos && last_block_start >= 0 && last_cz > last_block_start) {
    // last block has a Control-Z in it.  Make sure we add a 0 after it.
    out->resize(last_cz);
  }
  return true;
}

void WWIVMessageArea::savefile(const string& text, messagerec* msg, const string& msgs_filename) {
  vector<uint16_t> gati;
  unique_ptr<File> msgfile(OpenMessageFile(msgs_filename));
  if (!msgfile->IsOpen()) {
    // Unable to write to the message file.
    msg->stored_as = 0xffffffff;
    return;
  }
  size_t section = 0;
  for (section = 0; section < 1024; section++) {
    vector<uint16_t> gat = load_gat(*msgfile, section);
    int nNumBlocksRequired = static_cast<int>((text.length() + 511L) / MSG_BLOCK_SIZE);
    int i4 = 1;
    gati.clear();
    while (size_int(gati) < nNumBlocksRequired && i4 < GAT_NUMBER_ELEMENTS) {
      if (gat[i4] == 0) {
        gati.push_back(i4);
      }
      ++i4;
    }
    if (size_int(gati) >= nNumBlocksRequired) {
      gati.push_back(-1);
      for (int i = 0; i < nNumBlocksRequired; i++) {
        msgfile->Seek(MSG_STARTING(section) + MSG_BLOCK_SIZE * static_cast<long>(gati[i]), File::seekBegin);
        msgfile->Write((&text[i * MSG_BLOCK_SIZE]), MSG_BLOCK_SIZE);
        gat[gati[i]] = static_cast<uint16_t>(gati[i + 1]);
      }
      save_gat(*msgfile, section, gat);
      break;
    }
  }
  msg->stored_as = static_cast<uint32_t>(gati[0]) + static_cast<uint32_t>(section) * GAT_NUMBER_ELEMENTS;
}

}  // namespace msgapi
}  // namespace sdk
}  // namespace wwiv
