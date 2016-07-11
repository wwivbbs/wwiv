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
  const std::string& bbs_directory,
  const std::string& subs_directory,
  const std::string& messages_directory,
  const std::vector<net_networks_rec>& net_networks)
  : MessageApi(bbs_directory, subs_directory, messages_directory, net_networks) {}
WWIVMessageApi::~WWIVMessageApi() {}

bool WWIVMessageApi::Exist(const std::string& name) const {
  const std::string sub_filename = StrCat(name, ".sub");
  File subs(subs_directory_, sub_filename);
  return subs.Exists();
}

// todo(rushfan): should this be create *OR* open instead?
WWIVMessageArea* WWIVMessageApi::Create(const std::string& name) {
  const std::string sub_filename = StrCat(name, ".sub");
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

  const std::string text_filename = StrCat(name, ".dat");
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
  return new WWIVMessageArea(this, fileSub.full_pathname(), msgs_file.full_pathname());
}

bool WWIVMessageApi::Remove(const std::string& name) {
  // todo: delete lastread/qscan
  // todo: delete sub files.
  // todo: delete from conf
  return false;
}

WWIVMessageArea* WWIVMessageApi::Open(const std::string& name) {
  const std::string sub_filename = StrCat(name, ".sub");
  File fileSub(subs_directory_, sub_filename);
  if (!fileSub.Exists()) {
    return nullptr;
  }
  if (!fileSub.Open(File::modeReadOnly | File::modeBinary)) {
    return nullptr;
  }
  const string msgs_filename = StrCat(name, ".dat");
  File msgs_file(messages_directory_, msgs_filename);
  if (!msgs_file.Exists()) {
    return nullptr;
  }
  if (!msgs_file.Open(File::modeReadOnly | File::modeBinary)) {
    return nullptr;
  }

  return new WWIVMessageArea(this, fileSub.full_pathname(), msgs_file.full_pathname());
}

WWIVEmail* WWIVMessageApi::OpenEmail() {
  File datafile(subs_directory_, EMAIL_DAT);
  if (!datafile.Exists()) {
    return nullptr;
  }
  if (!datafile.Open(File::modeReadOnly | File::modeBinary)) {
    return nullptr;
  }
  File textfile(messages_directory_, EMAIL_DAT);
  if (!textfile.Exists()) {
    return nullptr;
  }
  if (!textfile.Open(File::modeReadOnly | File::modeBinary)) {
    return nullptr;
  }

  return new WWIVEmail(datafile.full_pathname(), textfile.full_pathname(), net_networks_.size());
}

}  // namespace msgapi
}  // namespace sdk
}  // namespace wwiv
