/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2014-2022, WWIV Software Services             */
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
#include "sdk/net/net.h"

class SdkHelper : public wwiv::sdk::BbsDirectories {
public:
  SdkHelper();
  ~SdkHelper();
  bool SetUp() { return true; }

  [[nodiscard]] virtual std::filesystem::path root_directory() const override { return root_; }
  [[nodiscard]] virtual std::filesystem::path datadir() const override { return data_; }
  [[nodiscard]] virtual std::filesystem::path msgsdir() const override { return msgs_; }
  [[nodiscard]] virtual std::filesystem::path gfilesdir() const override { return gfiles_; }
  [[nodiscard]] virtual std::filesystem::path menudir() const override { return menus_; }
  [[nodiscard]] virtual std::filesystem::path dloadsdir() const override { return dloads_; }
  [[nodiscard]] virtual std::filesystem::path scriptdir() const override { return scripts_; }
  [[nodiscard]] virtual std::filesystem::path logdir() const override { return logs_; }
  [[nodiscard]] virtual std::filesystem::path scratch_dir(int) const override {
    return scratch_;
  }

  [[nodiscard]] std::filesystem::path scratch() const { return scratch_; }
  [[nodiscard]] wwiv::core::test::FileHelper& files() { return files_; }
  [[nodiscard]] wwiv::sdk::Config& config() const;
  [[nodiscard]] wwiv::sdk::net::Network CreateTestNetwork(wwiv::sdk::net::network_type_t net_type);


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
