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
#include "sdk/msgapi/message_api_wwiv.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/file.h"
#include "core/strings.h"
#include "core/version.h"
#include "bbs/subacc.h"
#include "sdk/filenames.h"
#include "sdk/vardec.h"

using std::string;
using std::unique_ptr;
using namespace wwiv::strings;

namespace wwiv {
namespace sdk {
namespace msgapi {

static constexpr int  GAT_NUMBER_ELEMENTS = 2048;
static constexpr int GAT_SECTION_SIZE = 4096;
static constexpr int MSG_BLOCK_SIZE = 512;

WWIVMessageApi::WWIVMessageApi(
  const wwiv::sdk::msgapi::MessageApiOptions& options,
  const wwiv::sdk::Config& config,
  const std::vector<net_networks_rec>& net_networks,
  WWIVLastReadImpl* last_read)
  : WWIVMessageApi(options, config.root_directory(), 
    config.datadir(), config.msgsdir(), net_networks, last_read) {}


WWIVMessageApi::WWIVMessageApi(
  const wwiv::sdk::msgapi::MessageApiOptions& options,
  const std::string& bbs_directory,
  const std::string& data_directory,
  const std::string& messages_directory,
  const std::vector<net_networks_rec>& net_networks,
  WWIVLastReadImpl* last_read)
  : MessageApi(options, bbs_directory, data_directory, messages_directory, net_networks),
  last_read_(last_read) {}
WWIVMessageApi::~WWIVMessageApi() {}

bool WWIVMessageApi::Exist(const std::string& name) const {
  const std::string sub_filename = StrCat(name, ".sub");
  File subs(subs_directory_, sub_filename);
  return subs.Exists();
}

WWIVMessageArea* WWIVMessageApi::Create(const std::string& name, int subnum) {
  return Create(name, ".sub", ".dat", subnum);
}

WWIVMessageArea* WWIVMessageApi::Create(const wwiv::sdk::subboard_t& sub, int subnum) {
  const string name = sub.name;
  auto area = Create(name, ".sub", ".dat", subnum);
  area->set_max_messages(sub.maxmsgs);
  area->set_storage_type(sub.storage_type);
  return area;
}

// todo(rushfan): should this be create *OR* open instead?
WWIVMessageArea* WWIVMessageApi::Create(const std::string& name, const std::string& sub_ext, const std::string& text_ext, int subnum) {
  const std::string sub_filename = StrCat(name, sub_ext);
  File fileSub(subs_directory_, sub_filename);
  if (fileSub.Exists()) {
    // Don't create if it already exists.
    return nullptr;
  }
  if (fileSub.Open(File::modeReadOnly | File::modeBinary)) {
    // Don't create if we can open it.
    return nullptr;
  }

  if (!fileSub.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite)) {
    // Don't create if we fail to write it.
    return nullptr;
  }

  const std::string text_filename = StrCat(name, text_ext);
  File msgs_file(messages_directory_, text_filename);
  if (msgs_file.Open(File::modeReadOnly | File::modeBinary)) {
    // Don't create since we have this file already.
    return nullptr;
  }

  {
    // Create new Message Text (.DAT ) file.
    msgs_file.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
    // I think we can skip this too since it doesn't do anything as we
    // setlength the file which fills it with zeros.
    uint16_t gat[GAT_NUMBER_ELEMENTS] = {0};
    msgs_file.Write(gat, GAT_SECTION_SIZE);

    msgs_file.SetLength(GAT_SECTION_SIZE + (75L * 1024L));
  }

  // Create 5.1 style sub header.
  WWIVMessageAreaHeader header(wwiv_num_version, 0);
  fileSub.Write(&header.header(), sizeof(subfile_header_t));
  // Need to close the files before creating a new WWIVMessageArea since we
  // have thm locked for write, and we need to open it in the constructor. 
  fileSub.Close();
  msgs_file.Close();
  return new WWIVMessageArea(this, fileSub.full_pathname(), msgs_file.full_pathname(), subnum);
}

bool WWIVMessageApi::Remove(const std::string& name) {
  // todo: delete lastread/qscan
  // todo: delete sub files.
  // todo: delete from conf
  return false;
}

WWIVMessageArea* WWIVMessageApi::Open(const std::string& name, int subnum) {
  return Open(name, ".sub", ".dat", subnum);
}

WWIVMessageArea* WWIVMessageApi::Open(const wwiv::sdk::subboard_t& sub, int subnum) {
  const string name = sub.name;
  auto area = Open(name, ".sub", ".dat", subnum);
  area->set_max_messages(sub.maxmsgs);
  area->set_storage_type(sub.storage_type);
  return area;
}

WWIVMessageArea* WWIVMessageApi::Open(const std::string& name, const std::string& sub_ext, const std::string& text_ext, int subnum) {
    const std::string sub_filename = StrCat(name, sub_ext);
  const string msgs_filename = StrCat(name, text_ext);
  string sub;
  string msgs;
  {
    File fileSub(subs_directory_, sub_filename);
    File msgs_file(messages_directory_, msgs_filename);
    if (!fileSub.Exists()) {
      return nullptr;
    }
    if (!fileSub.Open(File::modeReadOnly | File::modeBinary)) {
      return nullptr;
    }

    if (!msgs_file.Exists()) {
      File create_msgs_file(messages_directory_, msgs_filename);
      if (!create_msgs_file.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite)) {
        return nullptr;
      }
    }
    if (!msgs_file.Open(File::modeReadOnly | File::modeBinary)) {
      return nullptr;
    }

    sub = fileSub.full_pathname();
    msgs = msgs_file.full_pathname();
  }

  // We have to return this after the last block exited so that
  // fileSub and msgs_file will both be closed.
  return new WWIVMessageArea(this, sub, msgs, subnum);
}

WWIVEmail* WWIVMessageApi::OpenEmail() {
  string data;
  string text;
  {
    File datafile(subs_directory_, EMAIL_DAT);
    File textfile(messages_directory_, EMAIL_DAT);
    if (!datafile.Exists()) {
      return nullptr;
    }
    if (!datafile.Open(File::modeReadOnly | File::modeBinary)) {
      return nullptr;
    }
    if (!textfile.Exists()) {
      return nullptr;
    }
    if (!textfile.Open(File::modeReadOnly | File::modeBinary)) {
      return nullptr;
    }
    data = datafile.full_pathname();
    text = textfile.full_pathname();
  }

  return new WWIVEmail(root_directory_, data, text, net_networks_.size());
}

uint32_t WWIVMessageApi::last_read(int area) const {
  if (last_read_) {
    return last_read_->last_read(area);
  }
  return 0;
}

void WWIVMessageApi::set_last_read(int area, uint32_t last_read) {
  if (last_read_) {
    return last_read_->set_last_read(area, last_read);
  }
}

}  // namespace msgapi
}  // namespace sdk
}  // namespace wwiv
