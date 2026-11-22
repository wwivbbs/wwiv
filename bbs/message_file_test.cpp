/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                Copyright (C)2026, WWIV Software Services               */
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
#include "bbs/message_file.h"

#include "bbs/bbs_helper.h"
#include "sdk/msgapi/type2_text.h"
#include "sdk/vardec.h"
#include "gtest/gtest.h"

#include <string>

using namespace wwiv::sdk::msgapi;

class MessageFileTest : public ::testing::Test {
protected:
  void SetUp() override {
    helper.SetUp();
    ASSERT_TRUE(helper.files().Mkdir("msgs"));
    a()->init_status_and_instances_for_test();
  }

  messagerec save(const std::string& text) {
    messagerec m{};
    m.storage_type = 2;
    savefile(text, &m, kFileName);
    return m;
  }

  std::string read(const messagerec& m) {
    const auto text = readfile(&m, kFileName);
    return text.value_or("<<NONE>>");
  }

  static constexpr const char* kFileName = "testsub";
  BbsHelper helper;
};

// TODO: readfile only strips the trailing ^Z from messages longer than one
// block, because "out.size() - MSG_BLOCK_SIZE" wraps around for shorter
// ones. These tests expect the current behavior (kShortEnd on the end of
// short messages); drop it from them when readfile is fixed.
static constexpr const char* kShortEnd = "\x1a";

TEST_F(MessageFileTest, SaveAndRead) {
  const auto m = save("Hello\r\n\x1a");
  EXPECT_EQ(std::string("Hello\r\n") + kShortEnd, read(m));
}

TEST_F(MessageFileTest, LineAdd_Short) {
  const auto m = save("Hello\r\n\x1a");
  lineadd(&m, "World", kFileName);
  EXPECT_EQ(std::string("Hello\r\nWorld\r\n") + kShortEnd, read(m));
}

TEST_F(MessageFileTest, LineAdd_Twice) {
  const auto m = save("Hello\r\n\x1a");
  lineadd(&m, "World", kFileName);
  lineadd(&m, "Again", kFileName);
  EXPECT_EQ(std::string("Hello\r\nWorld\r\nAgain\r\n") + kShortEnd, read(m));
}

TEST_F(MessageFileTest, LineAdd_SpillsIntoNextBlock) {
  const std::string text(500, 'a');
  const std::string line(100, 'b');
  const auto m = save(text + "\x1a");
  lineadd(&m, line, kFileName);
  EXPECT_EQ(text + line + "\r\n", read(m));
}

// The line used to be strcpy'd into a fixed size buffer, so a long enough
// line overflowed the heap. Now it's truncated to fit in two blocks.
TEST_F(MessageFileTest, LineAdd_VeryLongLineIsTruncated) {
  const auto m = save("Hello\r\n\x1a");
  lineadd(&m, std::string(5000, 'x'), kFileName);

  // Two blocks, less the existing text, "\r\n^Z" and a null terminator.
  constexpr auto expected_x = 2 * MSG_BLOCK_SIZE - 7 - 4;
  EXPECT_EQ("Hello\r\n" + std::string(expected_x, 'x') + "\r\n", read(m));
}

// The block chain comes from disk. Following an invalid chain used to read
// past the end of the GAT, or loop forever.
TEST_F(MessageFileTest, LineAdd_InvalidChainIsIgnored) {
  const auto m = save("Hello\r\n\x1a");
  auto bad = m;
  // A block that's not in use, so its GAT entry is 0.
  bad.stored_as = m.stored_as + 100;
  lineadd(&bad, "World", kFileName);
  EXPECT_EQ(std::string("Hello\r\n") + kShortEnd, read(m));
}
