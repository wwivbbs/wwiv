/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2026, WWIV Software Services                  */
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
#include "bbs/basic/next/bbs_file_base.h"

#include "bbs/application.h"
#include "core/file.h"
#include "core/log.h"
#include "core/stl.h"
#include "sdk/files/dirs.h"
#include "sdk/files/files.h"

#include <exception>
#include <utility>

namespace wwiv::bbs::basic {

std::vector<FileBase::UserDir> BbsFileBase::user_dirs() const {
  std::vector<UserDir> out;
  out.reserve(app_.udir.size());
  const auto num_dirs = app_.dirs().size();
  for (const auto& u : app_.udir) {
    // udir should only hold real directories; skip any that aren't, so every
    // area a script sees has a directory.
    if (u.subnum >= 0 && u.subnum < num_dirs) {
      out.push_back(UserDir{u.subnum, u.keys});
    }
  }
  return out;
}

std::optional<FileBase::UserDir> BbsFileBase::current_user_dir() const {
  // current_user_dir_num() is a position in udir, which user_dirs() filters,
  // so look the entry up here rather than in user_dirs().
  const int n = app_.current_user_dir_num();
  if (n < 0 || n >= stl::size_int(app_.udir)) {
    return std::nullopt;
  }
  const auto& u = app_.udir[n];
  if (u.subnum < 0 || u.subnum >= app_.dirs().size()) {
    return std::nullopt;
  }
  return UserDir{u.subnum, u.keys};
}

const sdk::files::Dirs& BbsFileBase::dirs() const { return std::as_const(app_).dirs(); }

std::unique_ptr<sdk::files::FileArea> BbsFileBase::open(int dirnum) {
  const auto& d = dirs();
  auto* api = app_.fileapi();
  if (!api || dirnum < 0 || dirnum >= d.size()) {
    return nullptr;
  }
  try {
    return api->Open(d.dir(dirnum));
  } catch (const std::exception& e) {
    LOG(ERROR) << "wwiv.files: unable to open directory #" << dirnum << ": " << e.what();
    return nullptr;
  }
}

// As the BBS's file list (printinfo in bbs/xfer.cpp): relative to the BBS.
std::filesystem::path BbsFileBase::path(int dirnum) const {
  const auto& d = dirs();
  if (dirnum < 0 || dirnum >= d.size()) {
    return {};
  }
  return core::File::absolute(app_.bbspath(), d.dir(dirnum).path);
}

} // namespace wwiv::bbs::basic
