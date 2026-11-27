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
#ifndef INCLUDED_BBS_BASIC_NEXT_BBS_FILE_BASE_H
#define INCLUDED_BBS_BASIC_NEXT_BBS_FILE_BASE_H

#include "bbs/basic/file_base.h"

class Application;

namespace wwiv::bbs::basic {

// The BBS's file areas for wwiv.files: the user's directories (udir), opened
// through the BBS's FileApi.
class BbsFileBase final : public FileBase {
public:
  explicit BbsFileBase(Application& app) : app_(app) {}

  [[nodiscard]] std::vector<UserDir> user_dirs() const override;
  [[nodiscard]] std::optional<UserDir> current_user_dir() const override;
  [[nodiscard]] const sdk::files::Dirs& dirs() const override;
  [[nodiscard]] std::unique_ptr<sdk::files::FileArea> open(int dirnum) override;
  [[nodiscard]] std::filesystem::path path(int dirnum) const override;

private:
  Application& app_;
};

} // namespace wwiv::bbs::basic

#endif
