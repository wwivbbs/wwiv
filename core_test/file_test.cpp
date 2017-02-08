/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*               Copyright (C)2014-2017, WWIV Software Services           */
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
    File f(tmp, "newdir");
    ASSERT_TRUE(f.Exists()) << f.full_pathname();
}

TEST(FileTest, ExistsWildCard) {
  FileHelper helper;
  const string path = helper.CreateTempFile("msg00000.001", "msg00000.001");
  ASSERT_TRUE(File::Exists(path));

  string wildcard_path = StrCat(helper.TempDir(), File::pathSeparatorString, "msg*");
  ASSERT_TRUE(File::ExistsWildcard(wildcard_path)) << path << "; w: " << wildcard_path;

  wildcard_path = StrCat(helper.TempDir(), File::pathSeparatorString, "msg*.*");
  EXPECT_TRUE(File::ExistsWildcard(wildcard_path)) << path << "; w: " << wildcard_path;

  wildcard_path = StrCat(helper.TempDir(), File::pathSeparatorString, "msg*.???");
  EXPECT_TRUE(File::ExistsWildcard(wildcard_path)) << path << "; w: " << wildcard_path;
}

TEST(FileTest, ExistsWildCard_Extension) {
  FileHelper helper;
  const string path = helper.CreateTempFile("msg00000.001", "msg00000.001");
  ASSERT_TRUE(File::Exists(path));

  string wildcard_path = StrCat(helper.TempDir(), File::pathSeparatorString, "msg*.001");
  ASSERT_TRUE(File::ExistsWildcard(wildcard_path)) << path << "; w: " << wildcard_path;

  wildcard_path = StrCat(helper.TempDir(), File::pathSeparatorString, "msg*.??1");
  ASSERT_TRUE(File::ExistsWildcard(wildcard_path)) << path << "; w: " << wildcard_path;
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
    ASSERT_TRUE(file.Open(File::modeBinary | File::modeReadOnly));
    ASSERT_EQ(static_cast<long>(kHelloWorld.size()),
    		  file.length());
}

TEST(FileTest, Length_NotOpen) {
    static const string kHelloWorld = "Hello World";
    FileHelper helper;
    string path = helper.CreateTempFile(this->test_info_->name(), kHelloWorld);
    File file(path);
    ASSERT_EQ(static_cast<long>(kHelloWorld.size()),
    		  file.length());
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
    ASSERT_TRUE(file.Open(File::modeBinary | File::modeReadOnly));
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
    ASSERT_TRUE(file.Open(File::modeBinary | File::modeReadOnly));
    ASSERT_LE(now, file.last_write_time());
}

TEST(FileTest, Read) {
  static const string kHelloWorld = "Hello World";
  FileHelper helper;
  string path = helper.CreateTempFile(this->test_info_->name(), kHelloWorld);
  File file(path);
  ASSERT_TRUE(file.Open(File::modeBinary | File::modeReadOnly));
  char buf[255];
  ASSERT_EQ(static_cast<int>(kHelloWorld.length()),
		    file.Read(buf, kHelloWorld.length()));
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

TEST(FileTest, parent) {
  static const string kFileName = this->test_info_->name();
  FileHelper helper;
  string path = helper.CreateTempFile(kFileName, "Hello World");
  File file(path);
  ASSERT_EQ(helper.TempDir(), file.parent());
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

TEST(FileTest, SetCurrentDirectory) {
  char expected[MAX_PATH];
  getcwd(expected, MAX_PATH);
  auto original_dir = File::current_directory();
  ASSERT_STREQ(expected, original_dir.c_str());

  FileHelper helper;
  File::set_current_directory(helper.TempDir());
  EXPECT_EQ(helper.TempDir(), File::current_directory());

  File::set_current_directory(original_dir);
}

TEST(FileTest, MakeAbsolutePath_Relative) {
  static const string kFileName = this->test_info_->name();
  FileHelper helper;
  const string path = helper.CreateTempFile(kFileName, "Hello World");

  string relative(kFileName);
  File::absolute(helper.TempDir(), &relative);
  EXPECT_EQ(path, relative);
}

TEST(FileTest, MakeAbsolutePath_Relative_Returning) {
  static const string kFileName = this->test_info_->name();
  FileHelper helper;
  const string path = helper.CreateTempFile(kFileName, "Hello World");

  string relative = File::absolute(helper.TempDir(), kFileName);
  EXPECT_EQ(path, relative);
}

TEST(FileTest, MakeAbsolutePath_AlreadyAbsolute) {
  static const string kFileName = this->test_info_->name();
  FileHelper helper;
  const string expected = helper.CreateTempFile(kFileName, "Hello World");

  string path(expected);
  File::absolute(helper.TempDir(), &path);
  EXPECT_EQ(expected, path);
}

TEST(FileTest, MakeAbsolutePath_AlreadyAbsolute_Returning) {
  static const string kFileName = this->test_info_->name();
  FileHelper helper;
  const string expected = helper.CreateTempFile(kFileName, "Hello World");

  string path = File::absolute(helper.TempDir(), expected);
  EXPECT_EQ(expected, path);
}

TEST(FileTest, IsAbsolute) {
  static const string kFileName = this->test_info_->name();
  FileHelper helper;
  const string path = helper.CreateTempFile(kFileName, "Hello World");

  EXPECT_TRUE(File::is_absolute(path));
  EXPECT_FALSE(File::is_absolute(kFileName));
}

TEST(FileTest, IsRelative) {
  static const string kFileName = this->test_info_->name();
  FileHelper helper;
  const string path = helper.CreateTempFile(kFileName, "Hello World");

  EXPECT_TRUE(File::is_relative(kFileName));
  EXPECT_FALSE(File::is_relative(path));
}

TEST(FileTest, RealPath_Same) {
  static const string kFileName = this->test_info_->name();
  FileHelper helper;
  const string path = helper.CreateTempFile(kFileName, "Hello World");

  string canonical;
  ASSERT_TRUE(File::canonical(path, &canonical));
  EXPECT_EQ(path, canonical);
}

TEST(FileTest, RealPath_Different) {
  static const string kFileName = this->test_info_->name();
  FileHelper helper;
  const string path = helper.CreateTempFile(kFileName, "Hello World");

  string canonical;
  // Add an extra ./ into the path.
  ASSERT_TRUE(File::canonical(StrCat(helper.TempDir(), File::pathSeparatorString, ".", File::pathSeparatorString, kFileName), &canonical));
  EXPECT_EQ(path, canonical);
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

TEST(FileTest, Stream) {
  FileHelper file;
  File f(file.TempDir(), "newdir");
  std::stringstream s;
  s << f;
  ASSERT_EQ(f.full_pathname(), s.str());
}

TEST(FileTest, IsOpen_Open) {
  static const string kHelloWorld = "Hello World";
  FileHelper helper;
  string path = helper.CreateTempFile(this->test_info_->name(), kHelloWorld);
  File file(path);
  ASSERT_TRUE(file.Open(File::modeBinary | File::modeReadOnly));
  EXPECT_TRUE(file.IsOpen());
  EXPECT_TRUE((bool) file);
}

TEST(FileTest, IsOpen_NotOpen) {
  static const string kHelloWorld = "Hello World";
  FileHelper helper;
  string path = helper.CreateTempFile(this->test_info_->name(), kHelloWorld);
  File file(path + "DNE");
  EXPECT_FALSE(file.IsOpen());
  EXPECT_FALSE(file);
}

TEST(FileTest, Seek) {
  static const string kContents = "0123456789";
  FileHelper helper;
  string path = helper.CreateTempFile(this->test_info_->name(), kContents);
  File file(path);
  ASSERT_TRUE(file.Open(File::modeBinary | File::modeReadOnly));

  EXPECT_EQ(0, file.Seek(0, File::Whence::begin));
  char c{};
  file.Read(&c, 1);
  EXPECT_EQ('0', c);

  EXPECT_EQ(3, file.Seek(2, File::Whence::current));
  file.Read(&c, 1);
  EXPECT_EQ('3', c);

  EXPECT_EQ(static_cast<off_t>(kContents.size()),
		  file.Seek(0, File::Whence::end));
  EXPECT_EQ(0, file.Read(&c, 1));
}

TEST(FileTest, CurrentPosition) {
  static const string kContents = "0123456789";
  FileHelper helper;
  string path = helper.CreateTempFile(this->test_info_->name(), kContents);
  File file(path);
  ASSERT_TRUE(file.Open(File::modeBinary | File::modeReadOnly));

  EXPECT_EQ(3, file.Seek(3, File::Whence::begin));
  EXPECT_EQ(3, file.current_position());

  EXPECT_EQ(static_cast<int>(kContents.size()), file.Seek(0, File::Whence::end));
  EXPECT_EQ(static_cast<int>(kContents.size()), file.current_position());
}
