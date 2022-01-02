/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2021-2022, WWIV Software Services             */
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
#ifndef INCLUDED_SDK_BBS_DIRECTORIES_H
#define INCLUDED_SDK_BBS_DIRECTORIES_H

#include <filesystem>
#include <string>

namespace wwiv::sdk {


class BbsDirectories {
public:
  [[nodiscard]] virtual std::string root_directory() const = 0;
  [[nodiscard]] virtual std::string datadir() const = 0;
  [[nodiscard]] virtual std::string msgsdir() const = 0;
  [[nodiscard]] virtual std::string gfilesdir() const = 0;
  [[nodiscard]] virtual std::string menudir() const = 0;
  [[nodiscard]] virtual std::string dloadsdir() const = 0;
  [[nodiscard]] virtual std::string scriptdir() const = 0;
  [[nodiscard]] virtual std::string logdir() const = 0;

  /**
   * Returns the scrarch directory for a given node.
   */
  [[nodiscard]] virtual std::filesystem::path scratch_dir(int node) const = 0;
};

} // namespace wwiv::sdk

#endif
