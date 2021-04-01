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
#include "sdk/msgapi/message_api_wwiv.h"

#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/version.h"
#include "sdk/filenames.h"
#include "sdk/vardec.h"
#include "sdk/msgapi/message_area_wwiv.h"

#include <memory>
#include <string>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::strings;

namespace wwiv::sdk::msgapi {

WWIVMessageApi::WWIVMessageApi(const MessageApiOptions& options,
                               const Config& config,
                               const std::vector<Network>& net_networks,
                               WWIVLastReadImpl* last_read)
    : MessageApi(options, config.root_directory(), config.datadir(), config.msgsdir(),
                 net_networks),
      last_read_(last_read), config_(config) {}

bool WWIVMessageApi::Exist(const wwiv::sdk::subboard_t& sub) const {
  return File::Exists(FilePath(subs_directory_, StrCat(sub.filename, ".sub")));
}

bool WWIVMessageApi::Create(const wwiv::sdk::subboard_t& sub, int subnum) {
  return Create(sub.filename, ".sub", ".dat", subnum);
}

bool WWIVMessageApi::Create(const std::string& name, const std::string& sub_ext,
                            const std::string& text_ext, int) {
  LOG(INFO) << "Creating: " << name;
  const auto fn = FilePath(subs_directory_, StrCat(name, sub_ext));
  if (File::Exists(fn)) {
    // Don't create if it already exists.
    return false;
  }
  File fileSub(fn);
  if (fileSub.Open(File::modeReadOnly | File::modeBinary)) {
    // Don't create if we can open it.
    return false;
  }

  if (!fileSub.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite)) {
    // Don't create if we fail to write it.
    return false;
  }

  const auto text_filename = StrCat(name, text_ext);
  File msgs_file(FilePath(messages_directory_, text_filename));
  if (msgs_file.Open(File::modeReadOnly | File::modeBinary)) {
    // Don't create since we have this file already.
    return false;
  }

  {
    // Create new Message Text (.DAT ) file.
    msgs_file.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
    // I think we can skip this too since it doesn't do anything as we
    // set the length on the file which fills it with zeros.
    uint16_t gat[GAT_NUMBER_ELEMENTS] = {0};
    msgs_file.Write(gat, GAT_SECTION_SIZE);

    msgs_file.set_length(GAT_SECTION_SIZE + (75L * 1024L));
  }

  if (name != EMAIL_NOEXT) {
    // Create 5.1 style sub header for subs, but not for email.
    const WWIVMessageAreaHeader header(wwiv_config_version(), 0);
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
  std::filesystem::path sub_fullpath;
  std::filesystem::path msgs_fullpath;
  {
    const auto sub_fn = FilePath(subs_directory_, StrCat(sub.filename, ".sub"));
    const auto msgs_fn = FilePath(messages_directory_, StrCat(sub.filename, ".dat"));
    if (!File::Exists(sub_fn)) {
      throw bad_message_area(sub.filename);
    }
    File fileSub(sub_fn);
    if (!fileSub.Open(File::modeReadOnly | File::modeBinary)) {
      throw bad_message_area(sub.filename);
    }

    if (!File::Exists(msgs_fn)) {
      File create_msgs_file(FilePath(messages_directory_, StrCat(sub.filename, ".dat")));
      if (!create_msgs_file.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite)) {
        throw bad_message_area(sub.filename);
      }
    }
    File msgs_file(msgs_fn);
    if (!msgs_file.Open(File::modeReadOnly | File::modeBinary)) {
      throw bad_message_area(sub.filename);
    }

    sub_fullpath = sub_fn;
    msgs_fullpath = msgs_fn;
  }
  auto* area = new WWIVMessageArea(this, sub, sub_fullpath, msgs_fullpath, subnum, net_networks_);
  area->set_max_messages(sub.maxmsgs);
  area->set_storage_type(sub.storage_type);
  return area;
}

std::unique_ptr<WWIVEmail> WWIVMessageApi::OpenEmail() {
  const auto data = FilePath(subs_directory_, EMAIL_DAT);
  const auto text = FilePath(messages_directory_, EMAIL_DAT);
  {
    if (!File::Exists(data)) {
      // Create it if it doesn't exist.  We still can have an odd case
      // where 1 file exists, but that's not ever normal. so we'll
      // complain later about not being able to create this.
      if (!Create("email", ".dat", ".dat", 0)) {
        return {};
      }
      // Return the newly created WWIVEmail object.
      return std::make_unique<WWIVEmail>(config_, data, text, stl::size_int(net_networks_));
    }

    File datafile(data);
    if (!datafile.Open(File::modeReadOnly | File::modeBinary)) {
      LOG(ERROR) << "Unable to open datafile: " << data;
      return {};
    }

    if (!File::Exists(text)) {
      LOG(ERROR) << "'" << data << "' exists, but '" << text << "' does not! ";
      return {};
    }
    File textfile(text);
    if (!textfile.Open(File::modeReadOnly | File::modeBinary)) {
      LOG(ERROR) << "Unable to open msgsfile: " << data;
      return {};
    }
  }

  return std::make_unique<WWIVEmail>(config_, data, text, stl::size_int(net_networks_));
}

uint32_t WWIVMessageApi::last_read(int area) const {
  if (last_read_) {
    return last_read_->last_read(area);
  }
  return 0;
}

void WWIVMessageApi::set_last_read(int area, uint32_t last_read) {
  if (last_read_) {
    last_read_->set_last_read(area, last_read);
  }
}

} // namespace wwiv
