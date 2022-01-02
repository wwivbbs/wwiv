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
#include "gtest/gtest.h"

#include <iostream>
#include <memory>
#include <string>

#include "core/strings.h"
#include "sdk/ansi/framebuffer.h"
#include "sdk_test/sdk_helper.h"

using namespace wwiv::strings;
using namespace wwiv::sdk::ansi;

class FrameBufferTest : public testing::Test {};

TEST_F(FrameBufferTest, Cols) {
  FrameBuffer b{10};
  EXPECT_EQ(10, b.cols());
}

static void write(FrameBuffer& b, const std::string s) {
  for (const auto c : s) {
    b.write(c);
  }
}

TEST_F(FrameBufferTest, SingleLine) {
  FrameBuffer b{10};
  b.curatr(3);
  write(b, "Hello");
  b.close();
  EXPECT_EQ(5, b.pos());
  EXPECT_EQ(1, b.rows());
  EXPECT_EQ("Hello", b.row_as_text(0));
}

TEST_F(FrameBufferTest, MultiLine) {
  FrameBuffer b{10};
  b.curatr(3);
  write(b, "Hello\nWorld");
  b.close();
  EXPECT_EQ(b.cols() + 5, b.pos());
  EXPECT_EQ(2, b.rows());
  EXPECT_EQ("Hello", b.row_as_text(0));
  EXPECT_EQ("World", b.row_as_text(1));
}

TEST_F(FrameBufferTest, Goto) {
  FrameBuffer b{10};
  b.curatr(8);
  b.gotoxy(0, 10);
  write(b, "\xb0""ello");
  b.close();
  EXPECT_EQ(105, b.pos());
  EXPECT_EQ(11, b.rows());
  EXPECT_EQ("\xb0""ello", b.row_as_text(10));
  auto ca = b.row_char_and_attr(10);
  EXPECT_EQ(5u, ca.size());
  const auto f = static_cast<uint8_t>('\xb0');
  EXPECT_EQ((8 << 8) | f, ca[0]);
  EXPECT_EQ((8 << 8) | 'e', ca[1]);
  EXPECT_EQ((8 << 8) | 'l', ca[2]);
  EXPECT_EQ((8 << 8) | 'l', ca[3]);
  EXPECT_EQ((8 << 8) | 'o', ca[4]);
}
