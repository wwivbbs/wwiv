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
#include "gtest/gtest.h"

#include "bbs_test/bbs_helper.h"
#include "common/output.h"
#include "common/printfile.h"
#include "core/file.h"
#include "core/strings.h"
#include "core_test/file_helper.h"
#include <regex>
#include <string>

using wwiv::sdk::User;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::strings;

class PrintFileTest : public ::testing::Test {
protected:
  void SetUp() override { 
    helper.SetUp(); 
    helper.user()->screen_width(80);
    const auto lang = FilePath(helper.files().TempDir(), "en/gfiles");
    const auto gfiles = FilePath(helper.files().TempDir(), "gfiles");
    dirs.push_back(lang);
    dirs.push_back(gfiles);
  }

  std::filesystem::path CreateTempFile(const std::string& name) {
    return helper.files().CreateTempFile(name, "1");
  }

  std::filesystem::path CreateFullPathToPrint(const std::string& basename) const {
    return wwiv::common::CreateFullPathToPrint(dirs, *helper.user(), basename);
  }

  BbsHelper helper{};
  std::vector<std::filesystem::path> dirs;
  };

TEST_F(PrintFileTest, LanguageDir) {
  const auto expected_ans = CreateTempFile("en/gfiles/one.ans");
  const auto expected_bw = CreateTempFile("en/gfiles/one.b&w");
  const auto expected_msg = CreateTempFile("en/gfiles/one.msg");
  CreateTempFile("gfiles/one.ans");
  CreateTempFile("gfiles/one.b&w");
  CreateTempFile("gfiles/one.msg");

  helper.user()->SetStatus(0);
  const auto actual_msg = CreateFullPathToPrint("one");
  EXPECT_EQ(expected_msg, actual_msg);

  helper.user()->set_flag(User::flag_ansi);
  const auto actual_bw = CreateFullPathToPrint("one");
  EXPECT_EQ(expected_bw, actual_bw);

  helper.user()->set_flag(User::status_color);
  const auto actual_ans = CreateFullPathToPrint("one");
  EXPECT_EQ(expected_ans, actual_ans);
}

TEST_F(PrintFileTest, GFilesOnly_NoExt) {
  const auto expected_ans = CreateTempFile("gfiles/one.ans");
  const auto expected_bw = CreateTempFile("gfiles/one.b&w");
  const auto expected_msg = CreateTempFile("gfiles/one.msg");

  helper.user()->SetStatus(0);
  const auto actual_msg = CreateFullPathToPrint("one");
  EXPECT_EQ(expected_msg, actual_msg);

  helper.user()->set_flag(User::flag_ansi);
  const auto actual_bw = CreateFullPathToPrint("one");
  EXPECT_EQ(expected_bw, actual_bw);

  helper.user()->set_flag(User::status_color);
  const auto actual_ans = CreateFullPathToPrint("one");
  EXPECT_EQ(expected_ans, actual_ans);
}

TEST_F(PrintFileTest, WithExtension) {
  const auto expected_ans = CreateTempFile("gfiles/one.ans");
  const auto expected_bw = CreateTempFile("gfiles/one.b&w");
  const auto expected_msg = CreateTempFile("gfiles/one.msg");

  const auto actual_msg = CreateFullPathToPrint("one.msg");
  EXPECT_EQ(expected_msg, actual_msg);
  const auto actual_bw = CreateFullPathToPrint("one.b&w");
  EXPECT_EQ(expected_bw, actual_bw);
  const auto actual_ans = CreateFullPathToPrint("one.ans");
  EXPECT_EQ(expected_ans, actual_ans);
}

TEST_F(PrintFileTest, FullyQualified) {
  const auto expected = CreateTempFile("gfiles/one.ans");
  const auto actual = CreateFullPathToPrint(expected.string());
  EXPECT_EQ(expected, actual);
}

TEST_F(PrintFileTest, WithCols) {
  const auto base_ans = CreateTempFile("gfiles/one.msg");
  const auto expected_msg = CreateTempFile("gfiles/one.80.msg");
  const auto msg120 = CreateTempFile("gfiles/one.120.msg");
  const auto msg132 = CreateTempFile("gfiles/one.132.msg");
  const auto msg40 = CreateTempFile("gfiles/one.40.msg");

  const auto actual_msg = CreateFullPathToPrintWithCols(base_ans, 80);
  EXPECT_EQ(expected_msg, actual_msg);
}

TEST_F(PrintFileTest, WithCols_None) {
  const auto expected_msg = CreateTempFile("gfiles/one.msg");
  const auto msg100 = CreateTempFile("gfiles/one.100.msg");
  const auto msg120 = CreateTempFile("gfiles/one.120.msg");
  const auto msg132 = CreateTempFile("gfiles/one.132.msg");

  const auto actual_msg = CreateFullPathToPrintWithCols(expected_msg, 80);
  EXPECT_EQ(expected_msg, actual_msg);
}

TEST_F(PrintFileTest, GFilesOnly_WithCols) {
  const auto msg132 = CreateTempFile("gfiles/one.132.msg");
  const auto base_msg = CreateTempFile("gfiles/one.msg");

  helper.user()->screen_width(132);
  const auto actual_msg = CreateFullPathToPrint("one");
  EXPECT_EQ(msg132, actual_msg);

  helper.user()->screen_width(80);
  const auto actual_bw = CreateFullPathToPrint("one");
  EXPECT_EQ(base_msg, actual_bw);
}