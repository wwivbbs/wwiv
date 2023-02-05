/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*            Copyright (C)2018-2022, WWIV Software Services              */
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
#include "core/stl.h"
#include "core/strings.h"
#include "sdk/ansi/ansi.h"
#include "sdk/ansi/framebuffer.h"
#include "sdk/sdk_helper.h"

#include "gtest/gtest.h"
#include <string>
#include <sdk/msgapi/parsed_message.h>
using namespace wwiv::sdk::msgapi;

using namespace wwiv::strings;
using namespace wwiv::sdk::ansi;

class AnsiTest : public testing::Test {
public:
  AnsiTest()
      : b(10), ansi(&b, {}, 0x07), heart_and_pipe(&ansi, {7, 11, 14, 13, 31, 10, 12, 9, 5, 3}) {}

  void write(const std::string& s) {
    ansi.write(s);
    b.close();
  }

  bool write_hp(const std::string& s) {
    auto result = true;
    for (const auto c : s) {
      if (!heart_and_pipe.write(c)) {
        result = false;
      }
    }
    b.close();
    return result;
  }

  void check(const std::vector<std::string>& lines) const {
    EXPECT_EQ(wwiv::stl::ssize(lines), b.rows());
    for (std::vector<std::string>::size_type i = 0; i < lines.size(); i++) {
      EXPECT_EQ(lines[i], b.row_as_text(i)) << "Line #" << i;
    }
  }

  FrameBuffer b;
  Ansi ansi;
  HeartAndPipeCodeFilter heart_and_pipe;
};

TEST_F(AnsiTest, ColorBlue) {
  write("\x1b[34;1mHello");
  EXPECT_EQ(9, b.curatr());
  EXPECT_EQ(5, b.pos());
  check({"Hello"});
}

TEST_F(AnsiTest, CSI_2J) {
  write("\x1b[34;1mHello\x1b[2JH");
  EXPECT_EQ(9, b.curatr());
  EXPECT_EQ(1, b.pos());
  check({"H"});
}

TEST_F(AnsiTest, CSI_A_MoveUp) {
  write("Hello\n\x1b[1AWorld");
  check({"World"});
}

TEST_F(AnsiTest, CSI_A_MoveUpTooFar) {
  write("Hello\n\x1b[20AWorld");
  check({"World"});
}

TEST_F(AnsiTest, CSI_B_MoveDown) {
  write("Hello\n\x1b[2BWorld");
  check({"Hello", "", "", "World"});
}

TEST_F(AnsiTest, CSI_C_Right) {
  write("H\x1b[4CW");
  check({"H    W"});
}

TEST_F(AnsiTest, CSI_D_Left) {
  write("Hello\x1b[4Da");
  check({"Hallo"});
}

TEST_F(AnsiTest, CSI_D_Left_TooFar) {
  write("Hello\x1b[40DY");
  check({"Yello"});
}

TEST_F(AnsiTest, CSI_K_ClearEOL) {
  write("Hello\nWorld\n\x1b[2A\x1b[4C\x1b[K");
  check({"Hell      ", "World"});
}

TEST_F(AnsiTest, HeartAndPipe_HeartSmoke) {
  write_hp("\x03""1Hello");
  check({"Hello"});
  EXPECT_EQ(11, b.curatr());
}

TEST_F(AnsiTest, HeartAndPipe_StdPipe) {
  write_hp("|12Hello");
  check({"Hello"});
  EXPECT_EQ(12, b.curatr());
}

TEST_F(AnsiTest, HeartAndPipe_WWIVPipe) {
  write_hp("|#5Hello");
  check({"Hello"});
  EXPECT_EQ(10, b.curatr());
}

TEST_F(AnsiTest, HeartAndPipe_WWIVPipeMacro) {
  write_hp("Hello |@N");
  check({"Hello |@N"});
}

TEST_F(AnsiTest, HeartAndPipe_WWIVPipeExpression) {
  write_hp("|{a.b}");
  check({"|{a.b}"});
}


TEST_F(AnsiTest, XenosTagLine) {
  std::string text =
      R"([^D]1
[^D]2[^C]6  A  [^C]2.-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-.[^C]0
[^D]3[^C]6 /[^C]5*[^C]6\ [^C]2|[^C]4 /\liens' /\lcove! WWIV on Linux, Taguig, Metro Manila, Philippines [^C]2|
[^D]4[^C]6/ v \[^C]2`-=-=-=-=-=-=-=-=-=-=-=-[[^C]4 WWIV Development[^C]2]-=-=--=-=-=-=-=-=-=-=-=-=-'
[^D]5
[^D]6
[^D]7)";

  StringReplace(&text, "[^A]", "\x01");
  StringReplace(&text, "[^C]", "\x03");
  StringReplace(&text, "[^D]", "\x04");

  const auto lines = SplitString(text, "\n");

  // Finally render it to the frame buffer to interpret heart codes, ansi, etc.
  FrameBuffer b80(80);
  Ansi ansib80(&b80, {}, 0x07);
  HeartAndPipeCodeFilter heart(&ansib80, {7, 11, 14, 13, 31, 10, 12, 9, 5, 3});
  for (auto& l : lines) {
    for (const auto c : l) {
      heart.write(c);
    }
    // This is the same as in split_wwiv_message
    if (l.front() == 0x04) { // CD
      ansib80.attr(7);
      ansib80.reset();
    }
    heart.write('\n');
  }
  b80.close();
  auto screen_lines = b80.to_screen_as_lines();

  const auto expected = fmt::format("{:c}4{:c}[0;31;40;1m/ v \\{:c}[33m`", 0x04, 0x1b, 0x1b);
  EXPECT_TRUE(starts_with(screen_lines[3], expected)) << JoinStrings(screen_lines, "\n");
}


/*
* 



*/