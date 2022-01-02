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
#include "file_helper.h"
#include "gtest/gtest.h"
#include "core/file.h"
#include "core/findfiles.h"
#include "core/strings.h"

#include <string>

using namespace wwiv::core;
using namespace wwiv::strings;

TEST(FindFiles, Suffix) {
  FileHelper helper;
  helper.CreateTempFile("msg00000.001", "");
  FindFiles ff(FilePath(helper.TempDir(), "msg*"), FindFiles::FindFilesType::any);
  auto f = ff.begin();
  EXPECT_EQ("msg00000.001", f->name);
  ++f;
  EXPECT_EQ(f, ff.end());
}

TEST(FindFiles, Prefix) {
  FileHelper helper;
  helper.CreateTempFile("msg00000.001", "");
  FindFiles ff(FilePath(helper.TempDir(), "*001"), FindFiles::FindFilesType::files);
  auto f = ff.begin();
  EXPECT_EQ("msg00000.001", f->name);
  ++f;
  EXPECT_EQ(f, ff.end());
}

TEST(FindFiles, SingleCharSuffix) {
  FileHelper helper;
  helper.CreateTempFile("msg00000.001", "");
  FindFiles ff(FilePath(helper.TempDir(), "msg00000.00?"), FindFiles::FindFilesType::files);
  auto f = ff.begin();
  EXPECT_EQ("msg00000.001", f->name);
  ++f;
  EXPECT_EQ(f, ff.end());
}

TEST(FindFiles, SingleCharPrefix) {
  FileHelper helper;
  helper.CreateTempFile("msg00000.001", "");
  FindFiles ff(FilePath(helper.TempDir(), "?sg00000.001"), FindFiles::FindFilesType::files);
  auto f = ff.begin();
  EXPECT_EQ("msg00000.001", f->name);
  ++f;
  EXPECT_EQ(f, ff.end());
}

TEST(FindFiles, SingleCharExtensionPrefix) {
  FileHelper helper;
  helper.CreateTempFile("msg00000.001", "");
  FindFiles ff(FilePath(helper.TempDir(), "msg00000.?01"), FindFiles::FindFilesType::files);
  auto f = ff.begin();
  EXPECT_EQ("msg00000.001", f->name);
  ++f;
  EXPECT_EQ(f, ff.end());
}

TEST(FindFileTest, Smoke) {
  FileHelper helper;
  helper.CreateTempFile("FOO", "");
  EXPECT_TRUE(FindFile(FilePath(helper.TempDir(),"FOO")));
  EXPECT_TRUE(FindFile(FilePath(helper.TempDir(),"foo")));
  EXPECT_FALSE(FindFile(FilePath(helper.TempDir(),"bad")));
}
