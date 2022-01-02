/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*               Copyright (C)2014-2022, WWIV Software Services           */
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

#include <cstdio>
#include <string>
#include <vector>

#include "core/file.h"
#include "core/inifile.h"
#include "core/strings.h"
#include "core/test/file_helper.h"

using namespace wwiv::core;

class IniFileTest : public ::testing::Test {
protected:
  void SetUp() override {
    const auto* test_info = ::testing::UnitTest::GetInstance()->current_test_info();
    test_name_ = test_info->name();
  }

  [[nodiscard]] const std::string& test_name() const { return test_name_; }

  // ReSharper disable once CppMemberFunctionMayBeConst
  std::filesystem::path CreateIniFile(const std::string& section, const std::vector<std::string>& lines) {
    auto [file, path] = helper_.OpenTempFile(test_name_);
    fprintf(file, "[%s]\n", section.c_str());
    for (const auto& line : lines) {
      fprintf(file, "%s\n", line.c_str());
    }
    fclose(file);
    return path;
  }

  bool WriteLineToFile(FILE* file, const std::string& line) {
    return fprintf(file, "%s\n", line.c_str()) > 0;
  }

  // ReSharper disable once CppMemberFunctionMayBeConst
  std::filesystem::path CreateIniFile(const std::string& section1, const std::vector<std::string>& lines1,
                                      const std::string& section2, const std::vector<std::string>& lines2) {
    auto [file, path] = helper_.OpenTempFile(test_name_);
    fprintf(file, "[%s]\n", section1.c_str());
    for (const auto& line : lines1) {
      WriteLineToFile(file, line);
    }

    fprintf(file, "[%s]\n", section2.c_str());
    for (const auto& line : lines2) {
      WriteLineToFile(file, line);
    }

    fclose(file);
    return path;
  }

  wwiv::core::test::FileHelper helper_;
  std::string test_name_;
};

TEST_F(IniFileTest, Single_GetValue) {
  const auto path = this->CreateIniFile("TEST", {"FOO=BAR"});
  IniFile ini(FilePath(helper_.TempDir(), this->test_name()), {"TEST"});
  ASSERT_TRUE(ini.IsOpen());
  EXPECT_EQ("BAR", ini.value<std::string>("FOO"));
  ini.Close();
}

TEST_F(IniFileTest, Single_GetValue_Comment) {
  const auto path = this->CreateIniFile("TEST", {"FOO=BAR  ; BAZ"});
  IniFile ini(FilePath(helper_.TempDir(), this->test_name()), {"TEST"});
  ASSERT_TRUE(ini.IsOpen());
  EXPECT_EQ("BAR", ini.value<std::string>("FOO"));
  ini.Close();
}

TEST_F(IniFileTest, Single_GetNumericValue) {
  const auto path = this->CreateIniFile("TEST", {"FOO=1234", "BAR=4321", "baz=12345"});
  IniFile ini(FilePath(helper_.TempDir(), this->test_name()), {"TEST"});
  ASSERT_TRUE(ini.IsOpen());
  EXPECT_EQ(1234, ini.value<int>("FOO"));
  EXPECT_EQ(4321, ini.value<int>("BAR"));
  EXPECT_EQ(12345, ini.value<int>("baz"));
  ini.Close();
}

TEST_F(IniFileTest, Single_GetBooleanValue) {
  const auto path =
      this->CreateIniFile("TEST", {"T1=TRUE", "T2=1", "T3=Y", "F1=FALSE", "F2=0", "F3=N", "U=WTF"});
  IniFile ini(FilePath(helper_.TempDir(), this->test_name()), {"TEST"});
  ASSERT_TRUE(ini.IsOpen());
  EXPECT_TRUE(ini.value<bool>("T1"));
  EXPECT_TRUE(ini.value<bool>("T2"));
  EXPECT_TRUE(ini.value<bool>("T3"));
  EXPECT_FALSE(ini.value<bool>("F1"));
  EXPECT_FALSE(ini.value<bool>("F2"));
  EXPECT_FALSE(ini.value<bool>("F3"));

  EXPECT_FALSE(ini.value<bool>("U"));
  ini.Close();
}

TEST_F(IniFileTest, Reopen_GetValue) {
  const auto path = this->CreateIniFile("TEST", {"FOO=BAR"}, "TEST2", {"BAR=BAZ"});
  {
    IniFile ini(path, {"TEST"});
    ASSERT_TRUE(ini.IsOpen());
    EXPECT_EQ("BAR", ini.value<std::string>("FOO"));
    ini.Close();
  }

  IniFile ini(path, {"TEST2"});
  EXPECT_EQ("BAZ", ini.value<std::string>("BAR"));
  ini.Close();
}

TEST_F(IniFileTest, TwoSection_GetValue) {
  const auto path = this->CreateIniFile("TEST", {"FOO=BAR"}, "TEST-1", {"FOO=BAZ"});
  IniFile ini(FilePath(helper_.TempDir(), this->test_name()), {"TEST-1", "TEST"});
  ASSERT_TRUE(ini.IsOpen());
  EXPECT_EQ("BAZ", ini.value<std::string>("FOO"));
  ini.Close();
}

TEST_F(IniFileTest, TwoSection_GetValue_OnlyInSecondary) {
  const auto path = this->CreateIniFile("TEST", {"FOO=BAR"}, "TEST-1", {"FOO1=BAZ"});
  IniFile ini(FilePath(helper_.TempDir(), this->test_name()), {"TEST-1", "TEST"});
  ASSERT_TRUE(ini.IsOpen());
  EXPECT_EQ("BAR", ini.value<std::string>("FOO"));
  ini.Close();
}

TEST_F(IniFileTest, CommentAtStart) {
  const auto path = this->CreateIniFile("TEST", {";FOO=BAR"});
  IniFile ini(FilePath(helper_.TempDir(), this->test_name()), {"TEST-1", "TEST"});
  ASSERT_TRUE(ini.IsOpen());
  EXPECT_EQ("", ini.value<std::string>("FOO"));
  ini.Close();
}

TEST_F(IniFileTest, MultiLine_Smoke) {
  const auto path = this->CreateIniFile("TEST", {"FOO=\"\"\"\nBAR\nBAZ\n\"\"\""});
  IniFile ini(FilePath(helper_.TempDir(), this->test_name()), {"TEST-1", "TEST"});
  ASSERT_TRUE(ini.IsOpen());
  const auto actual = ini.value<std::string>("FOO");
  EXPECT_EQ("BAR\r\nBAZ\r\n", actual) << "A:" << actual;
  ini.Close();
}

TEST_F(IniFileTest, MultiLine_OnFirstLine) {
  const auto path = this->CreateIniFile("TEST", {"FOO=\"\"\"BAR\nBAZ\n\"\"\""});
  IniFile ini(FilePath(helper_.TempDir(), this->test_name()), {"TEST-1", "TEST"});
  ASSERT_TRUE(ini.IsOpen());
  const auto actual = ini.value<std::string>("FOO");
  EXPECT_EQ("BAR\r\nBAZ\r\n", actual) << "A:" << actual;
  ini.Close();
}

TEST_F(IniFileTest, MultiLine_OnLastLine) {
  const auto path = this->CreateIniFile("TEST", {"FOO=\"\"\"\nBAR\nBAZ\"\"\""});
  IniFile ini(FilePath(helper_.TempDir(), this->test_name()), {"TEST-1", "TEST"});
  ASSERT_TRUE(ini.IsOpen());
  const auto actual = ini.value<std::string>("FOO");
  EXPECT_EQ("BAR\r\nBAZ", actual) << "A:" << actual;
  ini.Close();
}