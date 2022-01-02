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

#include <filesystem>
#include <memory>
#include <string>

#include "core/test/file_helper.h"
#include "sdk/bbs_directories.h"
#include "sdk/config.h"

class SdkHelper : public wwiv::sdk::BbsDirectories {
public:
  SdkHelper();
  ~SdkHelper();
  bool SetUp() { return true; }

  [[nodiscard]] virtual std::string root_directory() const override { return root_.string(); }
  [[nodiscard]] virtual std::string datadir() const override { return data_.string(); }
  [[nodiscard]] virtual std::string msgsdir() const override { return msgs_.string(); }
  [[nodiscard]] virtual std::string gfilesdir() const override { return gfiles_.string(); }
  [[nodiscard]] virtual std::string menudir() const override { return menus_.string(); }
  [[nodiscard]] virtual std::string dloadsdir() const override { return dloads_.string(); }
  [[nodiscard]] virtual std::string scriptdir() const override { return scripts_.string(); }
  [[nodiscard]] virtual std::string logdir() const override { return logs_.string(); }
  [[nodiscard]] virtual std::filesystem::path scratch_dir(int) const override {
    return scratch_;
  }

  [[nodiscard]] std::string scratch() const { return scratch_.string(); }
  [[nodiscard]] wwiv::core::test::FileHelper& files() { return files_; }
  [[nodiscard]] wwiv::sdk::Config& config() const;

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
  std::unique_ptr<wwiv::sdk::Config> config_;
};

#endif  // INCLUDED_SDK_TEST_SDK_HELPER_H
