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
#include "core/wfile.h"
#include "core/strings.h"

#include <iostream>
#include <string>

using std::string;
using namespace wwiv::strings;

TEST(FileTest, DoesNotExist) {
    FileHelper file;
    string tmp = file.TempDir();
    GTEST_ASSERT_NE("", tmp);
    WFile dne(tmp, "doesnotexist");
    ASSERT_FALSE(dne.Exists());
}

TEST(FileTest, DoesNotExist_Static) {
    FileHelper file;
    string tmp = file.TempDir();
    GTEST_ASSERT_NE("", tmp);
    WFile dne(tmp, "doesnotexist");
    ASSERT_FALSE(WFile::Exists(dne.full_pathname()));
}

TEST(FileTest, Exists) {
    FileHelper file;
    string tmp = file.TempDir();
    GTEST_ASSERT_NE("", tmp);
    ASSERT_TRUE(file.Mkdir("newdir"));
    WFile dne(tmp, "newdir");
    ASSERT_TRUE(dne.Exists()) << dne.full_pathname(); 
}

TEST(FileTest, Exists_Static) {
    FileHelper file;
    string tmp = file.TempDir();
    GTEST_ASSERT_NE("", tmp);
    ASSERT_TRUE(file.Mkdir("newdir"));
    WFile dne(tmp, "newdir");
    ASSERT_TRUE(WFile::Exists(dne.full_pathname())) << dne.full_pathname(); 
}

TEST(FileTest, Exists_TrailingSlash) {
    FileHelper file;
    string tmp = file.TempDir();
    GTEST_ASSERT_NE("", tmp);
    ASSERT_TRUE(file.Mkdir("newdir"));
    WFile dne(tmp, "newdir");
    string path = StringPrintf("%s%c", dne.full_pathname().c_str(), WFile::pathSeparatorChar);
    ASSERT_TRUE(WFile::Exists(path)) << path; 
}

TEST(FileTest, Length_Open) {
    static const string kHelloWorld = "Hello World";
    FileHelper helper;
    string path = helper.CreateTempFile(this->test_info_->name(), kHelloWorld);
    WFile file(path);
    file.Open(WFile::modeBinary | WFile::modeReadOnly);
    ASSERT_EQ(kHelloWorld.size(), file.GetLength());
}

TEST(FileTest, Length_NotOpen) {
    static const string kHelloWorld = "Hello World";
    FileHelper helper;
    string path = helper.CreateTempFile(this->test_info_->name(), kHelloWorld);
    WFile file(path);
    ASSERT_EQ(kHelloWorld.size(), file.GetLength());
}

TEST(FileTest, IsDirectory_NotOpen) {
    static const string kHelloWorld = "Hello World";
    FileHelper helper;
    string path = helper.CreateTempFile(this->test_info_->name(), kHelloWorld);
    WFile file(path);
    ASSERT_FALSE(file.IsDirectory());
    ASSERT_TRUE(file.IsFile());
}

TEST(FileTest, IsDirectory_Open) {
    static const string kHelloWorld = "Hello World";
    FileHelper helper;
    string path = helper.CreateTempFile(this->test_info_->name(), kHelloWorld);
    WFile file(path);
    file.Open(WFile::modeBinary | WFile::modeReadOnly);
    ASSERT_FALSE(file.IsDirectory());
    ASSERT_TRUE(file.IsFile());
}

TEST(FileTest, LastWriteTime_NotOpen) {
    static const string kHelloWorld = "Hello World";
    FileHelper helper;
    time_t now = time(nullptr);
    string path = helper.CreateTempFile(this->test_info_->name(), kHelloWorld);
    WFile file(path);
    ASSERT_LE(now, file.last_write_time());
}

TEST(FileTest, LastWriteTime_Open) {
    static const string kHelloWorld = "Hello World";
    FileHelper helper;
    time_t now = time(nullptr);
    string path = helper.CreateTempFile(this->test_info_->name(), kHelloWorld);
    WFile file(path);
    file.Open(WFile::modeBinary | WFile::modeReadOnly);
    ASSERT_LE(now, file.last_write_time());
}

TEST(FileTest, Read) {
  static const string kHelloWorld = "Hello World";
  FileHelper helper;
  string path = helper.CreateTempFile(this->test_info_->name(), kHelloWorld);
  WFile file(path);
  file.Open(WFile::modeBinary | WFile::modeReadOnly);
  char buf[255];
  ASSERT_EQ(kHelloWorld.length(), file.Read(buf, kHelloWorld.length()));
  buf[11] = 0;
  ASSERT_STREQ(kHelloWorld.c_str(), buf);
}

TEST(FileTest, GetName) {
  static const string kFileName = this->test_info_->name();
  FileHelper helper;
  string path = helper.CreateTempFile(kFileName, "Hello World");
  WFile file(path);
  ASSERT_EQ(kFileName, file.GetName());
}

TEST(FileTest, GetParent) {
  static const string kFileName = this->test_info_->name();
  FileHelper helper;
  string path = helper.CreateTempFile(kFileName, "Hello World");
  WFile file(path);
  ASSERT_EQ(helper.TempDir(), file.GetParent());
}

TEST(FileTest, EnsureTrailingSlash) {
    const string single_slash = StringPrintf("temp%c", WFile::pathSeparatorChar);
    const string double_slash = StringPrintf("temp%c%c", WFile::pathSeparatorChar, WFile::pathSeparatorChar);
    const string no_slash = "temp";

    string s = single_slash;
    WFile::EnsureTrailingSlash(&s);
    EXPECT_EQ(single_slash, s);

    s = double_slash;
    WFile::EnsureTrailingSlash(&s);
    EXPECT_EQ(double_slash, s);

    s = no_slash;
    WFile::EnsureTrailingSlash(&s);
    EXPECT_EQ(single_slash, s);
}

TEST(FileTest, CurrentDirectory) {
  char expected[MAX_PATH];
  getcwd(expected, MAX_PATH);
  string actual;
  WFile::CurrentDirectory(&actual);
  EXPECT_STREQ(expected, actual.c_str());
}

TEST(FileTest, MakeAbsolutePath_Relative) {
  static const string kFileName = this->test_info_->name();
  FileHelper helper;
  const string path = helper.CreateTempFile(kFileName, "Hello World");

  string relative(kFileName);
  WFile::MakeAbsolutePath(helper.TempDir(), &relative);
  EXPECT_EQ(path, relative);
}

TEST(FileTest, MakeAbsolutePath_AlreadyAbsolute) {
  static const string kFileName = this->test_info_->name();
  FileHelper helper;
  const string path = helper.CreateTempFile(kFileName, "Hello World");

  string relative(path);  // Note: relative == absolute path (path)
  WFile::MakeAbsolutePath(helper.TempDir(), &relative);
  EXPECT_EQ(path, relative);
}

TEST(FileTest, IsAbsolute) {
  static const string kFileName = this->test_info_->name();
  FileHelper helper;
  const string path = helper.CreateTempFile(kFileName, "Hello World");

  EXPECT_TRUE(WFile::IsAbsolutePath(path));
  EXPECT_FALSE(WFile::IsAbsolutePath(kFileName));
}

TEST(FileTest, IsRelative) {
  static const string kFileName = this->test_info_->name();
  FileHelper helper;
  const string path = helper.CreateTempFile(kFileName, "Hello World");

  EXPECT_TRUE(WFile::IsRelativePath(kFileName));
  EXPECT_FALSE(WFile::IsRelativePath(path));
}

TEST(FileTest, RealPath_Same) {
  static const string kFileName = this->test_info_->name();
  FileHelper helper;
  const string path = helper.CreateTempFile(kFileName, "Hello World");

  string realpath;
  ASSERT_TRUE(WFile::RealPath(path, &realpath));
  EXPECT_EQ(path, realpath);
}

TEST(FileTest, RealPath_Different) {
  static const string kFileName = this->test_info_->name();
  FileHelper helper;
  const string path = helper.CreateTempFile(kFileName, "Hello World");

  string realpath;
  // Add an extra ./ into the path.
  ASSERT_TRUE(WFile::RealPath(StrCat(helper.TempDir(), WFile::pathSeparatorString, ".", WFile::pathSeparatorString, kFileName), &realpath));
  EXPECT_EQ(path, realpath);
}