/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*              Copyright (C)2018, WWIV Software Services                 */
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

#include <iostream>
#include <memory>
#include <string>

#include "core/strings.h"
#include "sdk/ansi/ansi.h"
#include "sdk/ansi/framebuffer.h"
#include "sdk_test/sdk_helper.h"

using namespace wwiv::strings;
using namespace wwiv::sdk::ansi;

class AnsiTest : public testing::Test {
public:
  AnsiTest() : b(10), ansi(&b, 0x07) {}

  void write(const std::string& s) {
    ansi.write(s);
    b.close();
  }

  void check(const std::vector<std::string>& lines) { 
    EXPECT_EQ(lines.size(), b.rows());
    for (std::vector<std::string>::size_type i = 0; i < lines.size(); i++) {
      EXPECT_EQ(lines[i], b.row_as_text(i)) << "Line #" << i;
    }
  }

  FrameBuffer b;
  Ansi ansi;
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
