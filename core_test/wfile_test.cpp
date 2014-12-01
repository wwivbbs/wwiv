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
#include "core/strings.h"

#include <iostream>
#include <string>

using std::string;
using namespace wwiv::strings;

TEST(FileTest, DoesNotExist) {
    FileHelper file;
    string tmp = file.TempDir();
    GTEST_ASSERT_NE("", tmp);
    File dne(tmp, "doesnotexist");
    ASSERT_FALSE(dne.Exists());
}

TEST(FileTest, DoesNotExist_Static) {
    FileHelper file;
    string tmp = file.TempDir();
    GTEST_ASSERT_NE("", tmp);
    File dne(tmp, "doesnotexist");
    ASSERT_FALSE(File::Exists(dne.full_pathname()));
}

TEST(FileTest, Exists) {
    FileHelper file;
    string tmp = file.TempDir();
    GTEST_ASSERT_NE("", tmp);
    ASSERT_TRUE(file.Mkdir("newdir"));
    File dne(tmp, "newdir");
    ASSERT_TRUE(dne.Exists()) << dne.full_pathname(); 
}

TEST(FileTest, Exists_Static) {
    FileHelper file;
    string tmp = file.TempDir();
    GTEST_ASSERT_NE("", tmp);
    ASSERT_TRUE(file.Mkdir("newdir"));
    File dne(tmp, "newdir");
    ASSERT_TRUE(File::Exists(dne.full_pathname())) << dne.full_pathname(); 
}

TEST(FileTest, Exists_TrailingSlash) {
    FileHelper file;
    string tmp = file.TempDir();
    GTEST_ASSERT_NE("", tmp);
    ASSERT_TRUE(file.Mkdir("newdir"));
    File dne(tmp, "newdir");
    string path = StringPrintf("%s%c", dne.full_pathname().c_str(), File::pathSeparatorChar);
    ASSERT_TRUE(File::Exists(path)) << path; 
}

TEST(FileTest, Length_Open) {
    static const string kHelloWorld = "Hello World";
    FileHelper helper;
    string path = helper.CreateTempFile(this->test_info_->name(), kHelloWorld);
    File file(path);
    file.Open(File::modeBinary | File::modeReadOnly);
    ASSERT_EQ(kHelloWorld.size(), file.GetLength());
}

TEST(FileTest, Length_NotOpen) {
    static const string kHelloWorld = "Hello World";
    FileHelper helper;
    string path = helper.CreateTempFile(this->test_info_->name(), kHelloWorld);
    File file(path);
    ASSERT_EQ(kHelloWorld.size(), file.GetLength());
}

TEST(FileTest, IsDirectory_NotOpen) {
    static const string kHelloWorld = "Hello World";
    FileHelper helper;
    string path = helper.CreateTempFile(this->test_info_->name(), kHelloWorld);
    File file(path);
    ASSERT_FALSE(file.IsDirectory());
    ASSERT_TRUE(file.IsFile());
}

TEST(FileTest, IsDirectory_Open) {
    static const string kHelloWorld = "Hello World";
    FileHelper helper;
    string path = helper.CreateTempFile(this->test_info_->name(), kHelloWorld);
    File file(path);
    file.Open(File::modeBinary | File::modeReadOnly);
    ASSERT_FALSE(file.IsDirectory());
    ASSERT_TRUE(file.IsFile());
}

TEST(FileTest, LastWriteTime_NotOpen) {
    static const string kHelloWorld = "Hello World";
    FileHelper helper;
    time_t now = time(nullptr);
    string path = helper.CreateTempFile(this->test_info_->name(), kHelloWorld);
    File file(path);
    ASSERT_LE(now, file.last_write_time());
}

TEST(FileTest, LastWriteTime_Open) {
    static const string kHelloWorld = "Hello World";
    FileHelper helper;
    time_t now = time(nullptr);
    string path = helper.CreateTempFile(this->test_info_->name(), kHelloWorld);
    File file(path);
    file.Open(File::modeBinary | File::modeReadOnly);
    ASSERT_LE(now, file.last_write_time());
}

TEST(FileTest, Read) {
  static const string kHelloWorld = "Hello World";
  FileHelper helper;
  string path = helper.CreateTempFile(this->test_info_->name(), kHelloWorld);
  File file(path);
  file.Open(File::modeBinary | File::modeReadOnly);
  char buf[255];
  ASSERT_EQ(kHelloWorld.length(), file.Read(buf, kHelloWorld.length()));
  buf[11] = 0;
  ASSERT_STREQ(kHelloWorld.c_str(), buf);
}

TEST(FileTest, GetName) {
  static const string kFileName = this->test_info_->name();
  FileHelper helper;
  string path = helper.CreateTempFile(kFileName, "Hello World");
  File file(path);
  ASSERT_EQ(kFileName, file.GetName());
}

TEST(FileTest, GetParent) {
  static const string kFileName = this->test_info_->name();
  FileHelper helper;
  string path = helper.CreateTempFile(kFileName, "Hello World");
  File file(path);
  ASSERT_EQ(helper.TempDir(), file.GetParent());
}

TEST(FileTest, EnsureTrailingSlash) {
    const string single_slash = StringPrintf("temp%c", File::pathSeparatorChar);
    const string double_slash = StringPrintf("temp%c%c", File::pathSeparatorChar, File::pathSeparatorChar);
    const string no_slash = "temp";

    string s = single_slash;
    File::EnsureTrailingSlash(&s);
    EXPECT_EQ(single_slash, s);

    s = double_slash;
    File::EnsureTrailingSlash(&s);
    EXPECT_EQ(double_slash, s);

    s = no_slash;
    File::EnsureTrailingSlash(&s);
    EXPECT_EQ(single_slash, s);
}

TEST(FileTest, CurrentDirectory) {
  char expected[MAX_PATH];
  getcwd(expected, MAX_PATH);
  string actual = File::current_directory();
  EXPECT_STREQ(expected, actual.c_str());
}

TEST(FileTest, MakeAbsolutePath_Relative) {
  static const string kFileName = this->test_info_->name();
  FileHelper helper;
  const string path = helper.CreateTempFile(kFileName, "Hello World");

  string relative(kFileName);
  File::MakeAbsolutePath(helper.TempDir(), &relative);
  EXPECT_EQ(path, relative);
}

TEST(FileTest, MakeAbsolutePath_AlreadyAbsolute) {
  static const string kFileName = this->test_info_->name();
  FileHelper helper;
  const string path = helper.CreateTempFile(kFileName, "Hello World");

  string relative(path);  // Note: relative == absolute path (path)
  File::MakeAbsolutePath(helper.TempDir(), &relative);
  EXPECT_EQ(path, relative);
}

TEST(FileTest, IsAbsolute) {
  static const string kFileName = this->test_info_->name();
  FileHelper helper;
  const string path = helper.CreateTempFile(kFileName, "Hello World");

  EXPECT_TRUE(File::IsAbsolutePath(path));
  EXPECT_FALSE(File::IsAbsolutePath(kFileName));
}

TEST(FileTest, IsRelative) {
  static const string kFileName = this->test_info_->name();
  FileHelper helper;
  const string path = helper.CreateTempFile(kFileName, "Hello World");

  EXPECT_TRUE(File::IsRelativePath(kFileName));
  EXPECT_FALSE(File::IsRelativePath(path));
}

TEST(FileTest, RealPath_Same) {
  static const string kFileName = this->test_info_->name();
  FileHelper helper;
  const string path = helper.CreateTempFile(kFileName, "Hello World");

  string realpath;
  ASSERT_TRUE(File::RealPath(path, &realpath));
  EXPECT_EQ(path, realpath);
}

TEST(FileTest, RealPath_Different) {
  static const string kFileName = this->test_info_->name();
  FileHelper helper;
  const string path = helper.CreateTempFile(kFileName, "Hello World");

  string realpath;
  // Add an extra ./ into the path.
  ASSERT_TRUE(File::RealPath(StrCat(helper.TempDir(), File::pathSeparatorString, ".", File::pathSeparatorString, kFileName), &realpath));
  EXPECT_EQ(path, realpath);
}

TEST(FileTest, mkdir) {
  FileHelper helper;
  const string path = StrCat(helper.TempDir(), File::pathSeparatorString, "a");
  const string path_missing_middle = StrCat(helper.TempDir(), File::pathSeparatorString, "a", 
      File::pathSeparatorString, "b", File::pathSeparatorString, "c");
  ASSERT_FALSE(File::Exists(path));

  ASSERT_TRUE(File::mkdir(path));
  ASSERT_TRUE(File::mkdir(path));  // 2nd time should still return true.
  EXPECT_FALSE(File::mkdir(path_missing_middle));  // Can't create missing path elements.

  ASSERT_TRUE(File::Exists(path));
}

TEST(FileTest, mkdirs) {
  FileHelper helper;
  const string path = StrCat(helper.TempDir(), File::pathSeparatorString, "a", 
      File::pathSeparatorString, "b", File::pathSeparatorString, "c");
  ASSERT_FALSE(File::Exists(path));

  ASSERT_TRUE(File::mkdirs(path));
  ASSERT_TRUE(File::mkdirs(path));

  ASSERT_TRUE(File::Exists(path));
}