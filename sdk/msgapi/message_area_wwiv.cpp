/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*               Copyright (C)2015, WWIV Software Services                */
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

#include "core/datafile.h"
#include "core/file.h"
#include "core/strings.h"
#include "bbs/subacc.h"
#include "sdk/filenames.h"
#include "sdk/vardec.h"

namespace wwiv {
namespace sdk {
namespace msgapi {

using std::string;
using std::unique_ptr;
using std::vector;
using wwiv::core::DataFile;
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
    sub_filename_(sub_filename), text_filename_(text_filename),
    gat(new uint16_t[GAT_NUMBER_ELEMENTS]()) {
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
  vector<string> control_lines;
    for (; it != std::end(lines); it++) {
    auto line = StringTrim(*it);
    if (!line.empty() && line.front() == CD) {
      control_lines.push_back(line);
    } else if (starts_with(line, "RE:")) {
      in_reply_to = StringTrim(line.substr(3));
    } else if (starts_with(line, "BY:")) {
      to = StringTrim(line.substr(3));
    } else {
      // No more special lines, the rest is just text.
      for (;  it != std::end(lines); it++) {
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
  unique_ptr<WWIVMessageHeader> wwiv_header(new WWIVMessageHeader(header, from_username, to, date, in_reply_to, control_lines, api_));
  unique_ptr<WWIVMessageText> wwiv_text(new WWIVMessageText(text));

  return new WWIVMessage(wwiv_header.release(), wwiv_text.release());
}

WWIVMessageHeader* WWIVMessageArea::ReadMessageHeader(int message_number) {
  unique_ptr<WWIVMessage> msg(ReadMessage(message_number));
  return msg->release_header();
}

WWIVMessageText* WWIVMessageArea::ReadMessageText(int message_number) {
  unique_ptr<WWIVMessage> msg(ReadMessage(message_number));
  return msg->release_text();
}

bool WWIVMessageArea::AddMessage(const Message & message) {
  return false;
}

bool WWIVMessageArea::DeleteMessage(int message_number) {
  return false;
}


// Implementation Details

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
  message_file->Seek(0L, File::seekBegin);
  message_file->Read(gat.get(), GAT_SECTION_SIZE);

  gat_section = 0;
  return message_file.release();
}

void WWIVMessageArea::set_gat_section(File *pMessageFile, int section) {
  if (gat_section != section) {
    long file_size = pMessageFile->GetLength();
    long section_pos = static_cast<long>(section) * GATSECLEN;
    if (file_size < section_pos) {
      pMessageFile->SetLength(section_pos);
      file_size = section_pos;
    }
    pMessageFile->Seek(section_pos, File::seekBegin);
    if (file_size < (section_pos + GAT_SECTION_SIZE)) {
      for (std::size_t i = 0; i < GAT_NUMBER_ELEMENTS; i++) {
        gat[i] = 0;
      }
      pMessageFile->Write(gat.get(), GAT_SECTION_SIZE);
    } else {
      pMessageFile->Read(gat.get(), GAT_SECTION_SIZE);
    }
    gat_section = section;
  }
}

void WWIVMessageArea::save_gat(File *pMessageFile) {
  long section_pos = static_cast<long>(gat_section) * GATSECLEN;
  pMessageFile->Seek(section_pos, File::seekBegin);
  pMessageFile->Write(gat.get(), GAT_SECTION_SIZE);

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
  set_gat_section(file.get(), msg->stored_as / GAT_NUMBER_ELEMENTS);
  int current_section = msg->stored_as % GAT_NUMBER_ELEMENTS;
  long message_length = 0;
  while (current_section > 0 && current_section < GAT_NUMBER_ELEMENTS) {
    message_length += MSG_BLOCK_SIZE;
    current_section = gat[current_section];
  }
  if (message_length == 0) {
    // TODO(rushfan): set error code,
    return false;
  }

  current_section = msg->stored_as % GAT_NUMBER_ELEMENTS;
  while (current_section > 0 && current_section < GAT_NUMBER_ELEMENTS) {
    file->Seek(MSG_STARTING(gat_section) + MSG_BLOCK_SIZE * static_cast<uint32_t>(current_section), File::seekBegin);
    char b[MSG_BLOCK_SIZE];
    file->Read(b, MSG_BLOCK_SIZE);
    out->append(string(b, MSG_BLOCK_SIZE));
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

void  WWIVMessageArea::savefile(const string& text, messagerec* pMessageRecord, const string msgs_filename) {
  int gati[128];
  unique_ptr<File> pMessageFile(OpenMessageFile(msgs_filename));
  if (pMessageFile->IsOpen()) {
    for (int section = 0; section < 1024; section++) {
      set_gat_section(pMessageFile.get(), section);
      int gatp = 0;
      int nNumBlocksRequired = static_cast<int>((text.length() + 511L) / MSG_BLOCK_SIZE);
      int i4 = 1;
      while (gatp < nNumBlocksRequired && i4 < GAT_NUMBER_ELEMENTS) {
        if (gat[i4] == 0) {
          gati[gatp++] = i4;
        }
        ++i4;
      }
      if (gatp >= nNumBlocksRequired) {
        gati[gatp] = -1;
        for (int i = 0; i < nNumBlocksRequired; i++) {
          pMessageFile->Seek(MSG_STARTING(gat_section) + MSG_BLOCK_SIZE * static_cast<long>(gati[i]), File::seekBegin);
          pMessageFile->Write((&text[i * MSG_BLOCK_SIZE]), MSG_BLOCK_SIZE);
          gat[gati[i]] = static_cast<uint16_t>(gati[i + 1]);
        }
        save_gat(pMessageFile.get());
        break;
      }
    }
    pMessageFile->Close();
  }
  pMessageRecord->stored_as = static_cast<long>(gati[0]) + static_cast<long>(gat_section) * GAT_NUMBER_ELEMENTS;
}


}  // namespace msgapi
}  // namespace sdk
}  // namespace wwiv
