/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2018-2026, WWIV Software Services             */
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
#include "gtest/gtest.h"

#include "bbs/bbs_helper.h"
#include "common/input.h"
#include "common/output.h"
#include "local_io/keycodes.h"
#include "sdk/ansi/ansi.h"
#include "sdk/ansi/framebuffer.h"
#include <string>

using wwiv::sdk::User;
using namespace wwiv::sdk::ansi;

namespace {
// Extended keys as the local keyboard sends them: 0 then the scan code.
// (Ctrl-A, the other Home key, is a user macro key.)
const std::string kHome{'\0', static_cast<char>(wwiv::local::io::HOME)};
const std::string kEnd{'\0', static_cast<char>(wwiv::local::io::END)};
const std::string kLeft{'\0', static_cast<char>(wwiv::local::io::LARROW)};
} // namespace

// Drives the ANSI input field (Input1) with keys and replays what it sends
// the caller through an ANSI interpreter, to check what the caller sees.
class InputTest : public ::testing::Test {
protected:
  void SetUp() override {
    helper.SetUp();
    helper.user()->set_flag(User::flag_ansi, true);
    helper.sess().ok_modem_stuff(true);
    helper.sess().outcom(true);
  }

  // Writes `col` prompt characters, then runs input_cmdline on orig with
  // keys (which should end with RETURN), returning the result.
  std::string cmdline(int col, const std::string& orig, const std::string& keys, int max_length) {
    // \r puts the tracked column back to 0 (bout.nl() leaves it at 1).
    bout.outstr("\r");
    helper.io()->rcaptured();
    bout.outstr(std::string(col, '>'));
    helper.io()->push_local_keys(keys);
    auto result = bin.input_cmdline(orig, max_length);
    bout.flush();
    screen_ = helper.io()->rcaptured();
    return result;
  }

  // The first row the caller sees, as it was just before RETURN.
  std::string row0() const {
    FrameBuffer fb{80};
    Ansi ansi(&fb, {}, 0x07);
    for (const auto c : screen_) {
      // Stop at the newline Input1 writes after RETURN.
      if (c == '\r' || c == '\n') {
        break;
      }
      ansi.write(c);
    }
    fb.close();
    return fb.row_as_text(0);
  }

  BbsHelper helper{};
  std::string screen_;
};

TEST_F(InputTest, ShortText) {
  EXPECT_EQ("hello", cmdline(1, "", "hello\r", 79));
  EXPECT_EQ(">hello", row0().substr(0, 6));
}

TEST_F(InputTest, LongText_TypedPastTheEdge_ScrollsWithLeftArrow) {
  std::string typed;
  for (auto i = 0; i < 100; i++) {
    typed.push_back(static_cast<char>('a' + i % 26));
  }
  EXPECT_EQ(typed, cmdline(1, "", typed + "\r", 160));
  const auto row = row0();
  ASSERT_GE(row.size(), 3u);
  EXPECT_EQ('>', row[0]);
  EXPECT_EQ('\xAE', row[1]) << row;
  EXPECT_EQ(std::string::npos, row.find('\xAF')) << row;
  // The end of what was typed is visible.
  EXPECT_NE(std::string::npos, row.find(typed.substr(typed.size() - 20))) << row;
  EXPECT_LE(row.size(), 79u) << "field wrapped past the row";
}

TEST_F(InputTest, LongOriginal_Home_ShowsRightArrow) {
  const std::string orig(120, 'x');
  EXPECT_EQ(orig, cmdline(1, orig, kHome + "\r", 160));
  const auto row = row0();
  ASSERT_EQ(79u, row.size()) << row;
  EXPECT_EQ(std::string(77, 'x'), row.substr(1, 77));
  EXPECT_EQ('\xAF', row.back());
}

TEST_F(InputTest, LongOriginal_LongerThanMax_NoOverflow) {
  // Used to strcpy 300 bytes into a max_length + 1 buffer.
  const std::string orig(300, 'y');
  EXPECT_EQ(orig, cmdline(1, orig, kEnd + "z\r", 79));
}

TEST_F(InputTest, EditInMiddleOfScrolledText) {
  const std::string orig(100, 'a');
  auto keys = kHome;
  keys += "B";
  keys += kEnd;
  keys += kLeft;
  keys += "C\r";
  auto expected = orig;
  expected.insert(0, "B");
  expected.insert(expected.size() - 1, "C");
  EXPECT_EQ(expected, cmdline(1, orig, keys, 160));
}

TEST_F(InputTest, WideUser_NarrowLocalConsole_Scrolls) {
  // Local session: the 80 column local console limits the field.
  helper.user()->screen_width(150);
  std::string typed;
  for (auto i = 0; i < 120; i++) {
    typed.push_back(static_cast<char>('a' + i % 26));
  }
  EXPECT_EQ(typed, cmdline(1, "", typed + "\r", 160));
  const auto row = row0();
  EXPECT_LE(row.size(), 79u) << row;
  EXPECT_EQ('\xAE', row[1]) << row;
  EXPECT_NE(std::string::npos, row.find(typed.substr(typed.size() - 20))) << row;
}

TEST_F(InputTest, WideRemoteTerminal_UsesFullWidth) {
  // Remote session: the caller's 150 column terminal sets the width.
  helper.sess().incom(true);
  helper.user()->screen_width(150);
  std::string typed;
  for (auto i = 0; i < 120; i++) {
    typed.push_back(static_cast<char>('a' + i % 26));
  }
  EXPECT_EQ(typed, cmdline(1, "", typed + "\r", 160));
  FrameBuffer fb{150};
  Ansi ansi(&fb, {}, 0x07);
  for (const auto c : screen_) {
    if (c == '\r' || c == '\n') {
      break;
    }
    ansi.write(c);
  }
  fb.close();
  const auto row = fb.row_as_text(0);
  EXPECT_EQ(">" + typed, row.substr(0, 121)) << row;
  EXPECT_EQ(std::string::npos, row.find('\xAE')) << row;
}

TEST_F(InputTest, FieldFitsRemainingRoomOnRow) {
  const std::string typed(70, 'q');
  EXPECT_EQ(typed, cmdline(40, "", typed + "\r", 160));
  const auto row = row0();
  // The field starts at column 40 and must not wrap.
  EXPECT_LE(row.size(), 79u) << row;
  EXPECT_EQ('\xAE', row[40]) << row;
}

TEST_F(InputTest, CtrlR_RedrawsPromptAndFieldInPlace) {
  std::string orig;
  for (auto i = 0; i < 12; i++) {
    orig += "this is a test ";
  }
  // Ctrl-W (home), Ctrl-R (redraw line), Ctrl-E (end), Ctrl-R, Ctrl-W.
  // SyncTERM sends Ctrl-R after every Ctrl-E.
  EXPECT_EQ(orig, cmdline(1, orig, "\x17\x12\x05\x12\x17\r", 160));
  const auto out = screen_;
  // Nothing goes to a new row until RETURN.
  EXPECT_EQ(out.find('\n'), out.rfind('\n')) << out;
  EXPECT_EQ(">" + orig.substr(0, 77) + "\xAF", row0());
}

TEST_F(InputTest, LineRestoredOnNewRow_RedrawsPromptAndField) {
  // As after Ctrl-U or an instance message: text on new rows, then the line.
  std::string orig;
  for (auto i = 0; i < 12; i++) {
    orig += "this is a test ";
  }
  bout.outstr("\r");
  helper.io()->rcaptured();
  bout.outstr(">");
  helper.io()->push_local_keys(std::string("\x15\r"));
  EXPECT_EQ(orig, bin.input_cmdline(orig, 160));
  bout.flush();
  FrameBuffer fb{80};
  Ansi ansi(&fb, {}, 0x07);
  for (const auto c : helper.io()->rcaptured()) {
    ansi.write(c);
  }
  fb.close();
  const auto end_row = ">\xAE" + orig.substr(orig.size() - 76) + " ";
  EXPECT_EQ(end_row, fb.row_as_text(0));
  // The last row before RETURN's newline is the redrawn line.
  auto last = -1;
  for (auto r = 0; r < fb.rows(); r++) {
    if (fb.row_as_text(r) == end_row) {
      last = r;
    }
  }
  EXPECT_GT(last, 0);
}

TEST_F(InputTest, Remote_CtrlW_CtrlE_StayOnRow) {
  helper.sess().incom(true);
  std::string orig;
  for (auto i = 0; i < 12; i++) {
    orig += "this is a test ";
  }
  bout.outstr("\r");
  helper.io()->rcaptured();
  bout.outstr(">");
  helper.io()->push_remote_keys("\x17\x05\x17\r");
  EXPECT_EQ(orig, bin.input_cmdline(orig, 160));
  bout.flush();
  const auto out = helper.io()->rcaptured();
  // Nothing goes to a new row until RETURN.
  EXPECT_EQ(out.find('\n'), out.rfind('\n')) << out;
  FrameBuffer fb{80};
  Ansi ansi(&fb, {}, 0x07);
  for (const auto c : out) {
    ansi.write(c);
  }
  fb.close();
  EXPECT_EQ(">" + orig.substr(0, 77) + "\xAF", fb.row_as_text(0));
}


TEST(InputTypeAheadTest, KeysAreReadInOrderAfterThoseAlreadyWaiting) {
  BbsHelper helper{};
  helper.SetUp();
  bin.type_ahead("ab");
  EXPECT_EQ('a', bin.bgetch());
  // Added after "b", which is still waiting.
  bin.type_ahead("c");
  EXPECT_EQ('b', bin.bgetch());
  EXPECT_EQ('c', bin.bgetch());
  EXPECT_FALSE(bin.bkbhit());
  // A NUL would end the buffer; it's dropped.
  bin.type_ahead(std::string{'d', '\0', 'e'});
  EXPECT_EQ('d', bin.bgetch());
  EXPECT_EQ('e', bin.bgetch());
  EXPECT_FALSE(bin.bkbhit());
  bin.charbufferpointer_ = 0;
}

TEST(InputTypeAheadTest, TooManyKeys_AreDropped) {
  BbsHelper helper{};
  helper.SetUp();
  bin.type_ahead(std::string(1000, 'x'));
  int n = 0;
  while (bin.bkbhit() && n < 1000) {
    EXPECT_EQ('x', bin.bgetch());
    ++n;
  }
  EXPECT_EQ(static_cast<int>(sizeof(bin.charbuffer)) - 2, n);
  bin.charbufferpointer_ = 0;
}
