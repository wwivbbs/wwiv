#include "gtest/gtest.h"
#include "core/strings.h"
#include "core_test/file_helper.h"
#include "networkb/transfer_file.h"

#include <chrono>
#include <cstdint>
#include <string>

using std::chrono::system_clock;
using std::chrono::time_point;

using std::string;
using namespace wwiv::net;
using wwiv::strings::StringPrintf;

class TransferFileTest : public testing::Test {
public:
  TransferFileTest() : filename("test1"), contents("ASDF"), now(system_clock::now()), file(filename, contents, now) {}

  time_point<system_clock> now;
  string contents;
  string filename;
  InMemoryTransferFile file;
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
