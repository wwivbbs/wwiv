/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2022, WWIV Software Services             */
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
/**************************************************************************/
#include "gtest/gtest.h"

#include "core/stl.h"
#include "core/strings.h"
#include "core/test/file_helper.h"
#include "fmt/printf.h"
#include "binkp/transfer_file.h"
#include "binkp/wfile_transfer_file.h"
#include <chrono>
#include <filesystem>
#include <string>

using std::chrono::system_clock;
using std::chrono::time_point;
using namespace wwiv::core;
using namespace wwiv::net;
using namespace wwiv::strings;

class TransferFileTest : public testing::Test {
public:
  TransferFileTest() 
    : now(system_clock::now()), contents("ASDF"), filename("test1"), file(filename, contents, system_clock::to_time_t(now)) {
    full_filename = file_helper_.CreateTempFile(filename, contents);
  }

  const time_point<system_clock> now;
  const std::string contents;
  const std::string filename;
  InMemoryTransferFile file;
  std::filesystem::path full_filename;
  wwiv::core::test::FileHelper file_helper_;
};

TEST_F(TransferFileTest, AsPacketData) {
  const auto expected = fmt::format("test1 4 {} 0 67BC1E09", system_clock::to_time_t(now));
  ASSERT_EQ(expected, file.as_packet_data(0));
}

TEST_F(TransferFileTest, Filename) {
  ASSERT_EQ(filename, file.filename());
}

TEST_F(TransferFileTest, FileSize) {
  ASSERT_EQ(wwiv::stl::ssize(contents), file.file_size());
}

TEST_F(TransferFileTest, GetChunk) {
  char chunk[100];

  // Partial file.
  memset(chunk, 0, 100);
  ASSERT_TRUE(file.GetChunk(chunk, 0, 1));
  EXPECT_EQ('A', chunk[0]);

  // Entire file.
  memset(chunk, 0, 100);
  ASSERT_TRUE(file.GetChunk(chunk, 0, contents.size()));
  EXPECT_EQ('A', chunk[0]);
  EXPECT_EQ('S', chunk[1]);
  EXPECT_EQ('D', chunk[2]);
  EXPECT_EQ('F', chunk[3]);

  // Goes past the end.
  memset(chunk, 0, 100);
  EXPECT_FALSE(file.GetChunk(chunk, 0, contents.size() + 1));

  // Goes past the end.
  EXPECT_FALSE(file.GetChunk(chunk, contents.size() - 1, 2));
}

TEST_F(TransferFileTest, WriteChunk) {
  char chunk[100];

  // Partial file.
  memset(chunk, 0, 100);
  memcpy(chunk, "AB", 2);
  ASSERT_TRUE(file.WriteChunk(chunk, 2));
  EXPECT_EQ("ASDFAB", file.contents());
}

TEST_F(TransferFileTest, WFileTest_Read) {
  WFileTransferFile wfile_file(filename, std::make_unique<File>(full_filename));
  ASSERT_EQ(filename, wfile_file.filename());
  ASSERT_EQ(wwiv::stl::ssize(contents), wfile_file.file_size());

  char chunk[100];

  // Partial file.
  memset(chunk, 0, 100);
  ASSERT_TRUE(wfile_file.GetChunk(chunk, 0, 1));
  EXPECT_EQ('A', chunk[0]);

  // Entire wfile_file.
  memset(chunk, 0, 100);
  ASSERT_TRUE(wfile_file.GetChunk(chunk, 0, contents.size()));
  EXPECT_EQ('A', chunk[0]);
  EXPECT_EQ('S', chunk[1]);
  EXPECT_EQ('D', chunk[2]);
  EXPECT_EQ('F', chunk[3]);

  // Goes past the end.
  memset(chunk, 0, 100);
  EXPECT_FALSE(wfile_file.GetChunk(chunk, 0, contents.size() + 1));

  // Goes past the end.
  EXPECT_FALSE(wfile_file.GetChunk(chunk, contents.size() - 1, 2));
  wfile_file.Close();
}

TEST_F(TransferFileTest, WFileTest_Write) {
  const std::string empty_filename = StrCat(filename, "_empty");
  const auto empty_file_fullpath = file_helper_.CreateTempFilePath(empty_filename);
  {
    WFileTransferFile wfile_file(empty_filename, std::make_unique<File>(empty_file_fullpath));
    EXPECT_EQ(empty_filename, wfile_file.filename());
    EXPECT_LE(wfile_file.file_size(), 0);

    wfile_file.WriteChunk(contents.c_str(), contents.size());
    EXPECT_EQ(wwiv::stl::ssize(contents), wfile_file.file_size());
    wfile_file.Close();
  }
  // Needed wfile_file to go out of scope before the file can be read.
  EXPECT_EQ(contents, file_helper_.ReadFile(empty_file_fullpath));
}
