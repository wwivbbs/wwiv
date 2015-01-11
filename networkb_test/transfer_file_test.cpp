#include "gtest/gtest.h"
#include "core/strings.h"
#include "core_test/file_helper.h"
#include "networkb/transfer_file.h"
#include "networkb/wfile_transfer_file.h"

#include <chrono>
#include <cstdint>
#include <string>

using std::chrono::system_clock;
using std::chrono::time_point;

using std::string;
using std::unique_ptr;
using namespace wwiv::net;
using namespace wwiv::strings;

class TransferFileTest : public testing::Test {
public:
  TransferFileTest() 
    : filename("test1"), contents("ASDF"), now(system_clock::now()), file(filename, contents, system_clock::to_time_t(now)) {
    full_filename = file_helper_.CreateTempFile(filename, contents);
  }

  const time_point<system_clock> now;
  const string contents;
  const string filename;
  InMemoryTransferFile file;
  string full_filename;
  FileHelper file_helper_;
};

TEST_F(TransferFileTest, AsPacketData) {
  const string expected = StringPrintf("test1 4 %lu 0", system_clock::to_time_t(now));
  ASSERT_EQ(expected, file.as_packet_data(0));
}

TEST_F(TransferFileTest, Filename) {
  ASSERT_EQ(filename, file.filename());
}

TEST_F(TransferFileTest, FileSize) {
  ASSERT_EQ(contents.size(), file.file_size());
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
  WFileTransferFile wfile_file(filename, unique_ptr<File>(new File(full_filename)));
  ASSERT_EQ(filename, wfile_file.filename());
  ASSERT_EQ(contents.size(), wfile_file.file_size());

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
  const string empty_filename = StrCat(filename, "_empty");
  const string empty_file_fullpath = file_helper_.CreateTempFilePath(empty_filename);
  {
    WFileTransferFile wfile_file(empty_filename, unique_ptr<File>(new File(empty_file_fullpath)));
    EXPECT_EQ(empty_filename, wfile_file.filename());
    EXPECT_LE(wfile_file.file_size(), 0);

    wfile_file.WriteChunk(contents.c_str(), contents.size());
    EXPECT_EQ(contents.size(), wfile_file.file_size());
    wfile_file.Close();
  }
  // Needed wfile_file to go out of scope before the file can be read.
  EXPECT_EQ(contents, file_helper_.ReadFile(empty_file_fullpath));
}
