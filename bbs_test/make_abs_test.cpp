/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2014-2021, WWIV Software Services               */
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
/*                                                                        */
/**************************************************************************/
#include "bbs/make_abs_cmd.h"
#include "bbs_test/bbs_helper.h"
#include "core/file.h"
#include "core/strings.h"

#include "gtest/gtest.h"
#include <filesystem>
#include <string>

using wwiv::strings::StrCat;
using namespace wwiv::core;

class MakeAbsTest : public ::testing::Test {
protected:
  void SetUp() override {
    helper.SetUp();
    root = helper.app_->bbspath().string();
  }
  BbsHelper helper{};
  std::string root;
};

#ifdef _WIN32

TEST_F(MakeAbsTest, NotUnderRoot) {
  const std::string expected = "c:\\windows\\system32\\cmd.exe foo";
  std::string cmdline = "cmd foo";
  make_abs_cmd(root, &cmdline);
  EXPECT_STRCASEEQ(expected.c_str(), cmdline.c_str());
}

TEST_F(MakeAbsTest, UnderRoot) {
  const auto foo = helper.files().CreateTempFile("foo.exe", "");
  const auto expected = StrCat(foo.string(), " bar");
  std::string cmdline = "foo bar";
  make_abs_cmd(root, &cmdline);
  EXPECT_STRCASEEQ(expected.c_str(), cmdline.c_str());
}

TEST_F(MakeAbsTest, UnderRoot_WithExt) {
  const auto foo = helper.files().CreateTempFile("foo.exe", "");
  const auto expected = StrCat(foo.string(), " bar");
  std::string cmdline = "foo.exe bar";
  make_abs_cmd(root, &cmdline);
  EXPECT_STRCASEEQ(expected.c_str(), cmdline.c_str());
}

TEST_F(MakeAbsTest, UnderRoot_SlashInParam) {
  const auto foo = helper.files().CreateTempFile("foo.exe", "");
  const auto arg = StrCat(" bar", File::pathSeparatorChar, "baz");
  const auto expected = StrCat(foo.string(), arg);
  auto cmdline = StrCat("foo", arg);
  make_abs_cmd(root, &cmdline);
  EXPECT_STRCASEEQ(expected.c_str(), cmdline.c_str());
}

TEST_F(MakeAbsTest, UnderRoot_SlashInParam_DoesNotExist) {
  const auto foo = helper.files().CreateTempFilePath("foo");
  const auto arg = StrCat(" bar", File::pathSeparatorChar, "baz");
  const auto expected = StrCat(foo.string(), arg);
  auto cmdline = StrCat("foo", arg);
  make_abs_cmd(root, &cmdline);
  EXPECT_STRCASEEQ(expected.c_str(), cmdline.c_str());
}

TEST_F(MakeAbsTest, DoesNotExist) {
  const auto foo = helper.files().CreateTempFilePath("foo");
  const auto expected = StrCat(foo.string(), " bar");
  std::string cmdline = "foo bar";
  make_abs_cmd(root, &cmdline);
  EXPECT_STRCASEEQ(expected.c_str(), cmdline.c_str());
}

#else 

TEST_F(MakeAbsTest, Smoke) {
  File f(FilePath(helper.files().TempDir(), "ls foo"));
  std::string expected = f.full_pathname();
  std::string cmdline = "ls foo";
  make_abs_cmd(root, &cmdline);
  EXPECT_STRCASEEQ(expected.c_str(), cmdline.c_str());
}

#endif  // _WIN32
