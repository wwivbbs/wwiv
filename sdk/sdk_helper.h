/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2014-2021, WWIV Software Services             */
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
#ifndef INCLUDED_SDK_TEST_SDK_HELPER_H
#define INCLUDED_SDK_TEST_SDK_HELPER_H

#include <string>

#include <filesystem>
#include "core/test/file_helper.h"

class SdkHelper {
public:
  SdkHelper();
  ~SdkHelper();
  bool SetUp() { return true; }

  [[nodiscard]] std::string root() const { return root_.string(); }
  [[nodiscard]] std::string data() const { return data_.string(); }
  [[nodiscard]] std::string dloads() const { return dloads_.string(); }
  [[nodiscard]] std::string msgs() const { return msgs_.string(); }
  [[nodiscard]] std::string gfiles() const { return gfiles_.string(); }
  [[nodiscard]] std::string menus() const { return menus_.string(); }
  [[nodiscard]] std::string scripts() const { return scripts_.string(); }
  [[nodiscard]] std::string logs() const { return logs_.string(); }
  [[nodiscard]] std::string scratch() const { return scratch_.string(); }
  [[nodiscard]] wwiv::core::test::FileHelper& files() { return files_; }

  std::filesystem::path CreatePath(const std::string& name);
  wwiv::core::test::FileHelper files_;
  std::filesystem::path data_;
  std::filesystem::path dloads_;
  std::filesystem::path msgs_;
  std::filesystem::path menus_;
  std::filesystem::path gfiles_;
  std::filesystem::path scripts_;
  std::filesystem::path logs_;
  std::filesystem::path scratch_;

private:
  const std::filesystem::path saved_dir_;
  const std::filesystem::path root_;
};

#endif  // INCLUDED_SDK_TEST_SDK_HELPER_H
