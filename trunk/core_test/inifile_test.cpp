/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*              Copyright (C) 2014, WWIV Software Services                */
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

#include "file_helper.h"
#include "core/inifile.h"
#include "core/strings.h"

using std::string;
using std::vector;


class IniFileTest : public ::testing::Test {
protected:
  virtual void SetUp() {
    const ::testing::TestInfo* const test_info = 
        ::testing::UnitTest::GetInstance()->current_test_info();
    test_name_ = test_info->name();
  }

  const string& test_name() const { return test_name_; }

  string CreateIniFile(const string section, const vector<string> lines) {
    string path;
    FILE* file = helper_.OpenTempFile(test_name_, &path);
    fprintf(file, "[%s]\n", section.c_str());
    for (const auto& line : lines) {
      fprintf(file, "%s\n", line.c_str());
    }
    fclose(file);
    return path;
  }

  string CreateIniFile(const string section1, const vector<string> lines1,
        const string section2, const vector<string> lines2) {
    string path;
    FILE* file = helper_.OpenTempFile(test_name_, &path);
    fprintf(file, "[%s]\n", section1.c_str());
    for (const auto& line : lines1) {
      fprintf(file, "%s\n", line.c_str());
    }

    fprintf(file, "[%s]\n", section2.c_str());
    for (const auto& line : lines2) {
      fprintf(file, "%s\n", line.c_str());
    }

    fclose(file);
    return path;
  }

  FileHelper helper_;
  string test_name_;
};


TEST_F(IniFileTest, Single_GetValue) {
  const string path = this->CreateIniFile("TEST", { "FOO=BAR" } );
  WIniFile ini(helper_.TempDir(), this->test_name());
  ASSERT_TRUE(ini.Open("TEST"));
  EXPECT_STREQ("BAR", ini.GetValue("FOO"));
  ini.Close();
}

TEST_F(IniFileTest, Single_GetNumericValue) {
  const string path = this->CreateIniFile("TEST", { "FOO=1234", "BAR=4321", "baz=12345" } );
  WIniFile ini(helper_.TempDir(), this->test_name());
  ASSERT_TRUE(ini.Open("TEST"));
  EXPECT_EQ(1234, ini.GetNumericValue("FOO"));
  EXPECT_EQ(4321, ini.GetNumericValue("BAR"));
  EXPECT_EQ(12345, ini.GetNumericValue("baz"));
  ini.Close();
}

TEST_F(IniFileTest, Single_GetBooleanValue) {
  const string path = this->CreateIniFile("TEST", { "T1=TRUE", "T2=1", "T3=Y", "F1=FALSE", "F2=0", "F3=N", "U=WTF" } );
  WIniFile ini(helper_.TempDir(), this->test_name());
  ASSERT_TRUE(ini.Open("TEST"));
  EXPECT_TRUE(ini.GetBooleanValue("T1"));
  EXPECT_TRUE(ini.GetBooleanValue("T2"));
  EXPECT_TRUE(ini.GetBooleanValue("T3"));
  EXPECT_FALSE(ini.GetBooleanValue("F1"));
  EXPECT_FALSE(ini.GetBooleanValue("F2"));
  EXPECT_FALSE(ini.GetBooleanValue("F3"));

  EXPECT_FALSE(ini.GetBooleanValue("U"));
  ini.Close();
}

TEST_F(IniFileTest, Reopen_GetValue) {
  const string path = this->CreateIniFile("TEST", { "FOO=BAR" }, "TEST2", { "BAR=BAZ" } );
  WIniFile ini(helper_.TempDir(), this->test_name());
  ASSERT_TRUE(ini.Open("TEST"));
  EXPECT_STREQ("BAR", ini.GetValue("FOO"));
  ini.Close();

  ASSERT_TRUE(ini.Open("TEST2"));
  EXPECT_STREQ("BAZ", ini.GetValue("BAR"));
  ini.Close();
}

TEST_F(IniFileTest, TwoSection_GetValue) {
  const string path = this->CreateIniFile("TEST", { "FOO=BAR" }, "TEST-1", { "FOO=BAZ" } );
  WIniFile ini(helper_.TempDir(), this->test_name());
  ASSERT_TRUE(ini.Open("TEST-1", "TEST"));
  EXPECT_STREQ("BAZ", ini.GetValue("FOO"));
  ini.Close();
}
