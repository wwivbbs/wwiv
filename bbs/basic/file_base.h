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
#ifndef INCLUDED_BBS_BASIC_FILE_BASE_H
#define INCLUDED_BBS_BASIC_FILE_BASE_H

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace wwiv::sdk::files {
class Dirs;
class FileArea;
} // namespace wwiv::sdk::files

namespace wwiv::bbs::basic {

// What wwiv.files reads: the file areas (directories) the current user can
// list. BbsFileBase is the BBS's; tests use their own.
class FileBase {
public:
  // A file area the user can list: an index into dirs(), and the key the
  // user types to choose it.
  struct UserDir {
    int dirnum;
    std::string key;
  };

  virtual ~FileBase() = default;

  // The areas the user can list, in the order the BBS lists them (the
  // current conference's directories that pass their ACS).
  [[nodiscard]] virtual std::vector<UserDir> user_dirs() const = 0;
  // The area the user is in (one of user_dirs()), if any.
  [[nodiscard]] virtual std::optional<UserDir> current_user_dir() const = 0;
  [[nodiscard]] virtual const sdk::files::Dirs& dirs() const = 0;
  // Opens a directory's file list, or returns nullptr if it has none yet.
  // Doesn't create it.
  [[nodiscard]] virtual std::unique_ptr<sdk::files::FileArea> open(int dirnum) = 0;
  // Where the directory's files are kept.
  [[nodiscard]] virtual std::filesystem::path path(int dirnum) const = 0;
};

} // namespace wwiv::bbs::basic

#endif
