/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2014-2020, WWIV Software Services               */
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

#include "bbs/bbs.h"
#include "bbs/printfile.h"
#include "bbs_test/bbs_helper.h"
#include "core_test/file_helper.h"
#include <iostream>
#include <string>

using std::cout;
using std::endl;
using std::string;

using wwiv::sdk::User;

class PrintFileTest : public ::testing::Test {
protected:
  void SetUp() override { helper.SetUp(); }

  std::filesystem::path CreateTempFile(const std::string& name) {
    return helper.files().CreateTempFile(name, "1");
  }
  BbsHelper helper{};
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

  helper.user()->SetStatusFlag(User::ansi);
  const auto actual_bw = CreateFullPathToPrint("one");
  EXPECT_EQ(expected_bw, actual_bw);

  helper.user()->SetStatusFlag(User::status_color);
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

  helper.user()->SetStatusFlag(User::ansi);
  const auto actual_bw = CreateFullPathToPrint("one");
  EXPECT_EQ(expected_bw, actual_bw);

  helper.user()->SetStatusFlag(User::status_color);
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
