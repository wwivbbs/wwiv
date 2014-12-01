/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*                 Copyright (C)2014, WWIV Software Services              */
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
#include "file_helper.h"
#include "gtest/gtest.h"
#include "core/file.h"
#include "core/textfile.h"
#include "core/strings.h"

#include <iostream>
#include <memory>
#include <string>

using std::string;
using wwiv::strings::StringPrintf;

class TextFileTest : public ::testing::Test {
protected:
  virtual void SetUp() {
    const ::testing::TestInfo* const test_info = 
        ::testing::UnitTest::GetInstance()->current_test_info();
    test_name_ = test_info->name();
    hello_world_path_ = helper_.CreateTempFile(test_name_, kHelloWorld);
  }

  const string& test_name() const { return test_name_; }

  FileHelper helper_;
  string hello_world_path_;
  string test_name_;
  static const string kHelloWorld;
};

const string TextFileTest::kHelloWorld = "Hello World\n";

TEST_F(TextFileTest, Constructor_SunnyCase) {
  TextFile file(hello_world_path_, "rt");
  string s;
  EXPECT_TRUE(file.ReadLine(&s));
  EXPECT_STREQ("Hello World", s.c_str());
}

TEST_F(TextFileTest, Constructor_Path_And_Name) {
  TextFile file(helper_.TempDir(), this->test_name(), "rt");
  string s;
  EXPECT_TRUE(file.ReadLine(&s));
  EXPECT_STREQ("Hello World", s.c_str());
}

TEST_F(TextFileTest, Append) {
  std::unique_ptr<TextFile> file(new TextFile(helper_.TempDir(), this->test_name(), "a+t"));
  EXPECT_EQ(3, file->Write("abc"));
  const string filename = file->full_pathname();
  file.reset();

  const string actual = helper_.ReadFile(filename);
  EXPECT_STREQ("Hello World\nabc", actual.c_str());
}

TEST_F(TextFileTest, ReadLine_CA) {
  const string path = helper_.CreateTempFile(this->test_name(), "a\nb\nc\n");
  TextFile file(path, "rt");
  char s[255];
  EXPECT_TRUE(file.ReadLine(s, sizeof(s)));
  EXPECT_STREQ("a\n", s);
  EXPECT_TRUE(file.ReadLine(s, sizeof(s)));
  EXPECT_STREQ("b\n", s);
  EXPECT_TRUE(file.ReadLine(s, sizeof(s)));
  EXPECT_STREQ("c\n", s);
  EXPECT_FALSE(file.ReadLine(s, sizeof(s)));
}

TEST_F(TextFileTest, ReadLine_String) {
  const string path = helper_.CreateTempFile(this->test_name(), "a\nb\nc\n");
  TextFile file(path, "rt");
  string s;
  EXPECT_TRUE(file.ReadLine(&s));
  EXPECT_STREQ("a", s.c_str());
  EXPECT_TRUE(file.ReadLine(&s));
  EXPECT_STREQ("b", s.c_str());
  EXPECT_TRUE(file.ReadLine(&s));
  EXPECT_STREQ("c", s.c_str());
  EXPECT_FALSE(file.ReadLine(&s));
}

TEST_F(TextFileTest, Write) {
  string filename;
  {
    TextFile file(helper_.TempDir(), this->test_name(), "wt");
    file.Write("Hello");
    filename = file.full_pathname();
    // Let the textfile close.
  }
  const string actual = helper_.ReadFile(filename);
  EXPECT_STREQ("Hello", actual.c_str());
}

TEST_F(TextFileTest, WriteFormatted) {
  string filename;
  {
    TextFile file(helper_.TempDir(), this->test_name(), "wt");
    file.WriteFormatted("%s %s", "Hello", "World");
    filename = file.full_pathname();
    // Let the textfile close.
  }
  const string actual = helper_.ReadFile(filename);
  EXPECT_STREQ("Hello World", actual.c_str());
}

TEST_F(TextFileTest, WriteChar) {
  string filename;
  {
    TextFile file(helper_.TempDir(), this->test_name(), "wt");
    file.WriteChar('H');
    filename = file.full_pathname();
    // Let the textfile close.
  }
  const string actual = helper_.ReadFile(filename);
  EXPECT_STREQ("H", actual.c_str());
}


TEST_F(TextFileTest, WriteBinary) {
  string filename;
  {
    TextFile file(helper_.TempDir(), this->test_name(), "wt");
    file.WriteBinary(kHelloWorld.c_str(), kHelloWorld.size() - 1);  // trim off \n
    filename = file.full_pathname();
    // Let the textfile close.
  }
  const string actual = helper_.ReadFile(filename);
  EXPECT_STREQ("Hello World", actual.c_str());
}

TEST_F(TextFileTest, Close) {
  TextFile file(hello_world_path_, "rt");
  ASSERT_TRUE(file.IsOpen());
  EXPECT_TRUE(file.Close());
  EXPECT_FALSE(file.IsOpen());
}

TEST_F(TextFileTest, Close_SecondCloseReturnsFalse) {
  TextFile file(hello_world_path_, "rt");
  ASSERT_TRUE(file.IsOpen());
  EXPECT_TRUE(file.Close());
  EXPECT_FALSE(file.Close());
  EXPECT_FALSE(file.IsOpen());
}

TEST_F(TextFileTest, IsEOF) {
  TextFile file(hello_world_path_, "rt");
  string s;
  EXPECT_TRUE(file.ReadLine(&s));
  EXPECT_STREQ("Hello World", s.c_str());

  EXPECT_FALSE(file.ReadLine(&s));
  EXPECT_TRUE(file.IsEndOfFile());
}

TEST_F(TextFileTest, GetPosition) {
  const string path = helper_.CreateTempFile(test_name_, "a\nb\nc\n");
  TextFile file(path, "rt");
  ASSERT_EQ(0, file.GetPosition());
  string s;
  EXPECT_TRUE(file.ReadLine(&s));
  EXPECT_STREQ("a", s.c_str());
#ifdef _WIN32
  EXPECT_EQ(3, file.GetPosition());
#else  // _WIN32
  EXPECT_EQ(2, file.GetPosition());
#endif  // _WIN32
}



