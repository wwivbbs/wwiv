/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*               Copyright (C)2014-2019, WWIV Software Services           */
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
#include "core/file.h"
#include "core/strings.h"
#include "file_helper.h"
#include "gtest/gtest.h"

#include <iostream>
#include <string>

using std::string;
using namespace wwiv::core;
using namespace wwiv::strings;

TEST(FileTest, DoesNotExist) {
  FileHelper file;
  string tmp = file.TempDir();
  GTEST_ASSERT_NE("", tmp);
  const auto fn = FilePath(tmp, "doesnotexist");
  ASSERT_FALSE(File::Exists(fn));
}

TEST(FileTest, DoesNotExist_Static) {
  FileHelper file;
  string tmp = file.TempDir();
  GTEST_ASSERT_NE("", tmp);
  File dne(FilePath(tmp, "doesnotexist"));
  ASSERT_FALSE(File::Exists(dne.full_pathname()));
}

TEST(FileTest, Exists) {
  FileHelper file;
  const auto tmp{file.TempDir()};
  GTEST_ASSERT_NE("", tmp);
  ASSERT_TRUE(file.Mkdir("newdir"));
  const auto f{FilePath(tmp, "newdir")};
  ASSERT_TRUE(File::Exists(f)) << f;
}

TEST(FileTest, ExistsWildCard) {
  FileHelper helper;
  const string path = helper.CreateTempFile("msg00000.001", "msg00000.001");
  ASSERT_TRUE(File::Exists(path));

  string wildcard_path = FilePath(helper.TempDir(), "msg*");
  ASSERT_TRUE(File::ExistsWildcard(wildcard_path)) << path << "; w: " << wildcard_path;

  wildcard_path = FilePath(helper.TempDir(), "msg*.*");
  EXPECT_TRUE(File::ExistsWildcard(wildcard_path)) << path << "; w: " << wildcard_path;

  wildcard_path = FilePath(helper.TempDir(), "msg*.???");
  EXPECT_TRUE(File::ExistsWildcard(wildcard_path)) << path << "; w: " << wildcard_path;
}

TEST(FileTest, ExistsWildCard_Extension) {
  FileHelper helper;
  const string path = helper.CreateTempFile("msg00000.001", "msg00000.001");
  ASSERT_TRUE(File::Exists(path));

  auto wildcard_path = FilePath(helper.TempDir(), "msg*.001");
  ASSERT_TRUE(File::ExistsWildcard(wildcard_path)) << path << "; w: " << wildcard_path;

  wildcard_path = FilePath(helper.TempDir(), "msg*.??1");
  ASSERT_TRUE(File::ExistsWildcard(wildcard_path)) << path << "; w: " << wildcard_path;
}

TEST(FileTest, Exists_Static) {
  FileHelper file;
  string tmp = file.TempDir();
  GTEST_ASSERT_NE("", tmp);
  ASSERT_TRUE(file.Mkdir("newdir"));
  File dne(FilePath(tmp, "newdir"));
  ASSERT_TRUE(File::Exists(dne.full_pathname())) << dne.full_pathname();
}

TEST(FileTest, Exists_TrailingSlash) {
  FileHelper file;
  const auto tmp = file.TempDir();
  GTEST_ASSERT_NE("", tmp);
  ASSERT_TRUE(file.Mkdir("newdir"));
  File dne(FilePath(tmp, "newdir"));
  const auto path = File::EnsureTrailingSlash(dne.full_pathname());
  ASSERT_TRUE(File::Exists(path)) << path;
}

TEST(FileTest, Length_Open) {
  static const string kHelloWorld = "Hello World";
  FileHelper helper;
  string path = helper.CreateTempFile(this->test_info_->name(), kHelloWorld);
  File file(path);
  ASSERT_TRUE(file.Open(File::modeBinary | File::modeReadOnly));
  ASSERT_EQ(static_cast<long>(kHelloWorld.size()), file.length());
}

TEST(FileTest, Length_NotOpen) {
  static const string kHelloWorld = "Hello World";
  FileHelper helper;
  string path = helper.CreateTempFile(this->test_info_->name(), kHelloWorld);
  File file(path);
  ASSERT_EQ(static_cast<long>(kHelloWorld.size()), file.length());
}

TEST(FileTest, IsDirectory_NotOpen) {
  static const string kHelloWorld = "Hello World";
  FileHelper helper;
  string path = helper.CreateTempFile(this->test_info_->name(), kHelloWorld);
  File file(path);
  ASSERT_FALSE(File::is_directory(path));
  ASSERT_TRUE(File::is_regular_file(path));
}

TEST(FileTest, IsDirectory_Open) {
  static const string kHelloWorld = "Hello World";
  FileHelper helper;
  string path = helper.CreateTempFile(this->test_info_->name(), kHelloWorld);
  File file(path);
  ASSERT_TRUE(file.Open(File::modeBinary | File::modeReadOnly));
  ASSERT_FALSE(File::is_directory(path));
  ASSERT_TRUE(File::is_regular_file(path));
}

TEST(FileTest, LastWriteTime_NotOpen) {
  static const string kHelloWorld = "Hello World";
  FileHelper helper;
  auto now = time(nullptr);
  auto path = helper.CreateTempFile(this->test_info_->name(), kHelloWorld);
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
  ASSERT_EQ(static_cast<int>(kHelloWorld.length()), file.Read(buf, kHelloWorld.length()));
  buf[11] = 0;
  ASSERT_STREQ(kHelloWorld.c_str(), buf);
}

TEST(FileTest, GetName) {
  static const string kFileName = this->test_info_->name();
  FileHelper helper;
  auto path = helper.CreateTempFile(kFileName, "Hello World");
  File file{path};
  ASSERT_EQ(kFileName, file.path().filename().string());
}

TEST(FileTest, EnsureTrailingSlash) {
  const auto single_slash = StringPrintf("temp%c", File::pathSeparatorChar);
  const auto double_slash =
      StringPrintf("temp%c%c", File::pathSeparatorChar, File::pathSeparatorChar);
  const string no_slash = "temp";

  auto s = File::EnsureTrailingSlash(single_slash);
  EXPECT_EQ(single_slash, s);

  s = File::EnsureTrailingSlash(double_slash);
  EXPECT_EQ(double_slash, s);

  s = File::EnsureTrailingSlash(no_slash);
  EXPECT_EQ(single_slash, s);
}

TEST(FileTest, CurrentDirectory) {
  char buf[MAX_PATH];
  char* expected = getcwd(buf, MAX_PATH);
  const auto actual = File::current_directory();
  EXPECT_STREQ(expected, actual.c_str());
}

TEST(FileTest, SetCurrentDirectory) {
  char buf[MAX_PATH];
  char* expected = getcwd(buf, MAX_PATH);
  const auto original_dir = File::current_directory();
  ASSERT_STREQ(expected, original_dir.c_str());

  FileHelper helper;
  File::set_current_directory(helper.TempDir());
  EXPECT_EQ(helper.TempDir(), File::current_directory());

  File::set_current_directory(original_dir);
}

TEST(FileTest, MakeAbsolutePath_Relative) {
  static const string kFileName{this->test_info_->name()};
  FileHelper helper;
  const auto path = helper.CreateTempFile(kFileName, "Hello World");

  const auto relative = File::absolute(helper.TempDir(), kFileName);
  EXPECT_EQ(path, relative);
}

TEST(FileTest, MakeAbsolutePath_AlreadyAbsolute) {
  static const string kFileName = this->test_info_->name();
  FileHelper helper;
  const auto expected = helper.CreateTempFile(kFileName, "Hello World");

  const auto path = File::absolute(helper.TempDir(), expected);
  EXPECT_EQ(expected, path);
}

TEST(FileTest, MakeAbsolutePath_AlreadyAbsolute_Returning) {
  static const string kFileName = this->test_info_->name();
  FileHelper helper;
  const string expected = helper.CreateTempFile(kFileName, "Hello World");

  auto path = File::absolute(helper.TempDir(), expected);
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
  const auto path = helper.CreateTempFile(kFileName, "Hello World");

  const auto canonical = File::canonical(path);
  EXPECT_EQ(path, canonical);
}

TEST(FileTest, RealPath_Different) {
  static const string kFileName{this->test_info_->name()};
  FileHelper helper;
  const auto path = helper.CreateTempFile(kFileName, "Hello World");

  // Add an extra ./ into the path.
  const auto suffix = FilePath(".", kFileName);
  const auto full = FilePath(helper.TempDir(), suffix);
  const auto canonical = File::canonical(full);
  EXPECT_EQ(path, canonical);
}

TEST(FileTest, mkdir) {
  FileHelper helper;
  const auto path = FilePath(helper.TempDir(), "a");
  const auto l = FilePath("b", "c");

  const string path_missing_middle = FilePath(path, l);
  ASSERT_FALSE(File::Exists(path));

  ASSERT_TRUE(File::mkdir(path));
  ASSERT_TRUE(File::mkdir(path));                 // 2nd time should still return true.
  EXPECT_FALSE(File::mkdir(path_missing_middle)); // Can't create missing path elements.

  ASSERT_TRUE(File::Exists(path));
}

TEST(FileTest, mkdirs) {
  FileHelper helper;
  const auto f = FilePath(helper.TempDir(), "a");
  const auto l = FilePath("b", "c");
  const auto path = FilePath(f, l);
  ASSERT_FALSE(File::Exists(path));

  ASSERT_TRUE(File::mkdirs(path));
  ASSERT_TRUE(File::mkdirs(path));

  ASSERT_TRUE(File::Exists(path));
}

TEST(FileTest, Stream) {
  FileHelper file;
  File f(FilePath(file.TempDir(), "newdir"));
  std::stringstream s;
  s << f;
  ASSERT_EQ(f.full_pathname(), s.str());
}

TEST(FileTest, IsOpen_Open) {
  static const string kHelloWorld = "Hello World";
  FileHelper helper;
  const auto path = helper.CreateTempFile(this->test_info_->name(), kHelloWorld);
  File file(path);
  ASSERT_TRUE(file.Open(File::modeBinary | File::modeReadOnly));
  EXPECT_TRUE(file.IsOpen());
  EXPECT_TRUE((bool)file);
}

TEST(FileTest, IsOpen_NotOpen) {
  static const string kHelloWorld = "Hello World";
  FileHelper helper;
  const auto path = helper.CreateTempFile(this->test_info_->name(), kHelloWorld);
  File file(path + "DNE");
  EXPECT_FALSE(file.IsOpen());
  EXPECT_FALSE(file);
}

TEST(FileTest, Seek) {
  static const string kContents = "0123456789";
  FileHelper helper;
  const auto path = helper.CreateTempFile(this->test_info_->name(), kContents);
  File file(path);
  ASSERT_TRUE(file.Open(File::modeBinary | File::modeReadOnly));

  EXPECT_EQ(0, file.Seek(0, File::Whence::begin));
  char c{};
  file.Read(&c, 1);
  EXPECT_EQ('0', c);

  EXPECT_EQ(3, file.Seek(2, File::Whence::current));
  file.Read(&c, 1);
  EXPECT_EQ('3', c);

  EXPECT_EQ(static_cast<off_t>(kContents.size()), file.Seek(0, File::Whence::end));
  EXPECT_EQ(0, file.Read(&c, 1));
}

TEST(FileTest, CurrentPosition) {
  static const string kContents = "0123456789";
  FileHelper helper;
  const auto path = helper.CreateTempFile(this->test_info_->name(), kContents);
  File file(path);
  ASSERT_TRUE(file.Open(File::modeBinary | File::modeReadOnly));

  EXPECT_EQ(3, file.Seek(3, File::Whence::begin));
  EXPECT_EQ(3, file.current_position());

  EXPECT_EQ(static_cast<int>(kContents.size()), file.Seek(0, File::Whence::end));
  EXPECT_EQ(static_cast<int>(kContents.size()), file.current_position());
}

TEST(FileTest, FsCopyFile) {
  namespace fs = wwiv::fs;
  FileHelper file;
  string tmp = file.TempDir();
  GTEST_ASSERT_NE("", tmp);
  ASSERT_TRUE(file.Mkdir("newdir"));
  auto f1 = FilePath(tmp, "f1");
  File f(f1);
  f.Open(File::modeWriteOnly | File::modeCreateFile);
  f.Write("ok");
  f.Close();
  ASSERT_TRUE(File::Exists(f.full_pathname())) << f.full_pathname();

  auto f2 = FilePath(tmp, "f2");
  fs::path from{f1};
  fs::path to{f2};
  std::error_code ec;
  EXPECT_FALSE(File::Exists(to.string()));
  fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
  EXPECT_TRUE(File::Exists(to.string()));
}

TEST(FileTest, CopyFile) {
  namespace fs = wwiv::fs;
  FileHelper file;
  string tmp = file.TempDir();
  GTEST_ASSERT_NE("", tmp);
  ASSERT_TRUE(file.Mkdir("newdir"));
  auto f1 = FilePath(tmp, "f1");
  File f(f1);
  f.Open(File::modeWriteOnly | File::modeCreateFile);
  f.Write("ok");
  f.Close();
  ASSERT_TRUE(File::Exists(f.full_pathname())) << f.full_pathname();

  auto f2 = FilePath(tmp, "f2");
  EXPECT_FALSE(File::Exists(f2));
  File::Copy(f1, f2);
  EXPECT_TRUE(File::Exists(f2));
}

TEST(FileTest, MoveFile) {
  namespace fs = wwiv::fs;
  FileHelper file;
  string tmp = file.TempDir();
  GTEST_ASSERT_NE("", tmp);
  ASSERT_TRUE(file.Mkdir("newdir"));
  auto f1 = FilePath(tmp, "f1");
  File f(f1);
  f.Open(File::modeWriteOnly | File::modeCreateFile);
  f.Write("ok");
  f.Close();
  ASSERT_TRUE(File::Exists(f.full_pathname())) << f.full_pathname();

  auto f2 = FilePath(tmp, "f2");
  EXPECT_TRUE(File::Exists(f1)) << f1;
  EXPECT_FALSE(File::Exists(f2));
  File::Move(f1, f2);
  EXPECT_TRUE(File::Exists(f2));
  EXPECT_FALSE(File::Exists(f1));
}

TEST(FileTest, Free) {
  FileHelper file;
  string tmp = file.TempDir();

  auto fs = File::freespace_for_path(tmp);
  EXPECT_GT(fs, 0);
}