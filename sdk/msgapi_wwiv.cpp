/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.0x                             */
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
#include "sdk/msgapi_wwiv.h"

#include <memory>
#include <string>
#include <utility>

#include "core/file.h"
#include "bbs/subacc.h"
#include "sdk/vardec.h"

namespace wwiv {
namespace sdk {
namespace msgapi {


WWIVMessageApi::WWIVMessageApi(const std::string& subs_directory,
  const std::string& messages_directory): MessageApi(subs_directory, messages_directory) {}
WWIVMessageApi::~WWIVMessageApi() {}

bool WWIVMessageApi::Exist(const std::string& name) const {
  File subs(subs_directory_, name);
  return subs.Exists();
}

WWIVMessageArea::WWIVMessageArea(WWIVMessageApi * api): MessageArea(api) {}
WWIVMessageArea::~WWIVMessageArea() {}


// todo(rushfan): should this be create *OR* open instead?
WWIVMessageArea* WWIVMessageApi::Create(const std::string& name) {
  if (File::Exists(subs_directory_, name)) {
    return nullptr;
  }
  File fileSub(subs_directory_, name);
  if (fileSub.Open(File::modeReadOnly | File::modeBinary)) {
    return nullptr;
  }

  if (!fileSub.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite)) {
    return nullptr;
  }

  postrec p;
  memset(&p, 0, sizeof(postrec));
  p.owneruser = 0;
  fileSub.Write(&p, sizeof(postrec));
  return new WWIVMessageArea(this);
}

bool WWIVMessageApi::Remove(const std::string& name) {
  // todo: delete lastread/qscan
  // todo: delete sub files.
  // todo: delete from conf
  return false;
}

WWIVMessageArea* WWIVMessageApi::Open(const std::string& name) {
  return nullptr;
}


}  // namespace msgapi
}  // namespace sdk
}  // namespace wwiv
