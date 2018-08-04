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
#include "sdk/msgapi/message_api_wwiv.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/version.h"
#include "bbs/subacc.h"
#include "sdk/filenames.h"
#include "sdk/vardec.h"

using std::string;
using std::unique_ptr;
using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv {
namespace sdk {
namespace msgapi {

WWIVMessageApi::WWIVMessageApi(
  const wwiv::sdk::msgapi::MessageApiOptions& options,
  const wwiv::sdk::Config& config,
  const std::vector<net_networks_rec>& net_networks,
  WWIVLastReadImpl* last_read)
  : MessageApi(options, config.root_directory(), 
    config.datadir(), 
    config.msgsdir(), 
    net_networks),
  last_read_(last_read), config_(config) {}

bool WWIVMessageApi::Exist(const wwiv::sdk::subboard_t& sub) const {
  const std::string sub_filename = StrCat(sub.filename, ".sub");
  File subs(FilePath(subs_directory_, sub_filename));
  return subs.Exists();
}

bool WWIVMessageApi::Create(const wwiv::sdk::subboard_t& sub, int subnum) {
  return Create(sub.filename, ".sub", ".dat", subnum);
}

bool WWIVMessageApi::Create(const std::string& name, const std::string& sub_ext, const std::string& text_ext, int subnum) {
  LOG(INFO) << "Creating: " << name;
  const std::string sub_filename = StrCat(name, sub_ext);
  File fileSub(FilePath(subs_directory_, sub_filename));
  if (fileSub.Exists()) {
    // Don't create if it already exists.
    return false;
  }
  if (fileSub.Open(File::modeReadOnly | File::modeBinary)) {
    // Don't create if we can open it.
    return false;
  }

  if (!fileSub.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite)) {
    // Don't create if we fail to write it.
    return false;
  }

  const std::string text_filename = StrCat(name, text_ext);
  File msgs_file(FilePath(messages_directory_, text_filename));
  if (msgs_file.Open(File::modeReadOnly | File::modeBinary)) {
    // Don't create since we have this file already.
    return false;
  }

  {
    // Create new Message Text (.DAT ) file.
    msgs_file.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
    // I think we can skip this too since it doesn't do anything as we
    // setlength the file which fills it with zeros.
    uint16_t gat[GAT_NUMBER_ELEMENTS] = {0};
    msgs_file.Write(gat, GAT_SECTION_SIZE);

    msgs_file.set_length(GAT_SECTION_SIZE + (75L * 1024L));
  }

  if (name != EMAIL_NOEXT) {
    // Create 5.1 style sub header for subs, but not for email.
    WWIVMessageAreaHeader header(wwiv_num_version, 0);
    fileSub.Write(&header.header(), sizeof(subfile_header_t));
  }
  return true;
}

bool WWIVMessageApi::Remove(const std::string&) {
  // todo: delete lastread/qscan
  // todo: delete sub files.
  // todo: delete from conf
  return false;
}


MessageArea* WWIVMessageApi::Open(const wwiv::sdk::subboard_t& sub, int subnum) {
  string sub_fullpath;
  string msgs_fullpath;
  {
    File fileSub(FilePath(subs_directory_, StrCat(sub.filename, ".sub")));
    File msgs_file(FilePath(messages_directory_, StrCat(sub.filename, ".dat")));
    if (!fileSub.Exists()) {
      throw bad_message_area(sub.filename);
    }
    if (!fileSub.Open(File::modeReadOnly | File::modeBinary)) {
      throw bad_message_area(sub.filename);
    }

    if (!msgs_file.Exists()) {
      File create_msgs_file(FilePath(messages_directory_, StrCat(sub.filename, ".dat")));
      if (!create_msgs_file.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite)) {
        throw bad_message_area(sub.filename);
      }
    }
    if (!msgs_file.Open(File::modeReadOnly | File::modeBinary)) {
      throw bad_message_area(sub.filename);
    }

    sub_fullpath = fileSub.full_pathname();
    msgs_fullpath = msgs_file.full_pathname();
  }
  auto area = new WWIVMessageArea(this, sub, sub_fullpath, msgs_fullpath, subnum);
  area->set_max_messages(sub.maxmsgs);
  area->set_storage_type(sub.storage_type);
  return area;
}


WWIVEmail* WWIVMessageApi::OpenEmail() {
  string data;
  string text;
  {
    File datafile(FilePath(subs_directory_, EMAIL_DAT));
    File textfile(FilePath(messages_directory_, EMAIL_DAT));
    data = datafile.full_pathname();
    text = textfile.full_pathname();

    if (!datafile.Exists()) {
      // Create it if it doesn't exist.  We still can have an odd case
      // where 1 file exists, but that's not ever normal. so we'll 
      // complain later about not being able to create this.
      auto created = Create("email", ".dat", ".dat", 0);
      if (!created) {
        return nullptr;
      }
      // Return the newly created WWIVEmail object.
      return new WWIVEmail(config_, data, text, net_networks_.size());
    }

    if (!datafile.Open(File::modeReadOnly | File::modeBinary)) {
      LOG(ERROR) << "Unable to open datafile: " << data;
      return nullptr;
    }
    if (!textfile.Exists()) {
      LOG(ERROR) << "'" << data << "' exists, but '" << text << "' does not! ";
      return nullptr;
    }
    if (!textfile.Open(File::modeReadOnly | File::modeBinary)) {
      LOG(ERROR) << "Unable to open msgsfile: " << data;
      return nullptr;
    }
  }

  return new WWIVEmail(config_, data, text, net_networks_.size());
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
