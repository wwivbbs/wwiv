/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2014-2019, WWIV Software Services             */
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
#ifndef __INCLUDED_SDK_TEST_SDK_HELPER_H__
#define __INCLUDED_SDK_TEST_SDK_HELPER_H__

#include <string>

#include <filesystem>
#include "core_test/file_helper.h"

class SdkHelper {
public:
  SdkHelper();
  ~SdkHelper();
  bool SetUp() { return true; }

  const std::string root() const { return root_.string(); }
  const std::string data() const { return data_.string(); }
  const std::string dloads() const { return dloads_.string(); }
  const std::string msgs() const { return msgs_.string(); }
  const std::string gfiles() const { return gfiles_.string(); }
  const std::string menus() const { return menus_.string(); }
  const std::string scripts() const { return scripts_.string(); }
  const std::string logs() const { return logs_.string(); }
  FileHelper& files() { return files_; }

  std::filesystem::path CreatePath(const std::string& name);
  FileHelper files_;
  std::filesystem::path data_;
  std::filesystem::path dloads_;
  std::filesystem::path msgs_;
  std::filesystem::path menus_;
  std::filesystem::path gfiles_;
  std::filesystem::path scripts_;
  std::filesystem::path logs_;

private:
  const std::filesystem::path saved_dir_;
  const std::filesystem::path root_;
};

#endif  // __INCLUDED_SDK_TEST_SDK_HELPER_H__
