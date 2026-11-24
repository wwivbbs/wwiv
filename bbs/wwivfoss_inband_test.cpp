/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                 Copyright (C)2026, WWIV Software Services              */
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

#include "bbs/wwivfoss_inband.h"
#include <string>

using namespace wwiv::bbs;
using namespace std::string_literals;

namespace {
std::string escape(const std::string& s) { return wwivfoss_escape(s.data(), s.size()); }
std::string decode(WWIVFossDecoder& d, const std::string& s) { return d.decode(s.data(), s.size()); }
} // namespace

TEST(WWIVFossInbandTest, Escape_NoFF) {
  EXPECT_EQ("hello", escape("hello"));
  EXPECT_EQ("", escape(""));
}

TEST(WWIVFossInbandTest, Escape_DoublesFF) {
  EXPECT_EQ("a\xFF\xFF" "b\xFF\xFF\xFF\xFF"s, escape("a\xFF" "b\xFF\xFF"s));
}

TEST(WWIVFossInbandTest, LineState) {
  EXPECT_EQ("\xFF\x03"s, wwivfoss_line_state(WWIVFOSS_LINE_UP));
  EXPECT_EQ("\xFF\x01"s, wwivfoss_line_state(WWIVFOSS_LINE_RTS));
}

TEST(WWIVFossInbandTest, Decode_Plain) {
  WWIVFossDecoder d;
  EXPECT_EQ("hello", decode(d, "hello"));
  EXPECT_TRUE(d.dtr());
  EXPECT_EQ(WWIVFOSS_LINE_UP, d.line_state());
}

TEST(WWIVFossInbandTest, Decode_LiteralFF) {
  WWIVFossDecoder d;
  EXPECT_EQ("a\xFF" "b"s, decode(d, "a\xFF\xFF" "b"s));
  EXPECT_TRUE(d.dtr());
}

TEST(WWIVFossInbandTest, Decode_DtrDropAndRaise) {
  WWIVFossDecoder d;
  EXPECT_EQ("ab", decode(d, "a\xFF\x01" "b"s));
  EXPECT_FALSE(d.dtr());
  EXPECT_EQ(WWIVFOSS_LINE_RTS, d.line_state());
  EXPECT_EQ("", decode(d, "\xFF\x03"s));
  EXPECT_TRUE(d.dtr());
}

TEST(WWIVFossInbandTest, Decode_LastLineStateWins) {
  WWIVFossDecoder d;
  EXPECT_EQ("ab", decode(d, "a\xFF\x01\xFF\x07" "b"s));
  EXPECT_EQ(0x07, d.line_state());
  EXPECT_TRUE(d.dtr());
}

TEST(WWIVFossInbandTest, HungUp_AfterData) {
  WWIVFossDecoder d;
  EXPECT_EQ("bye", decode(d, "bye\xFF\x01"s));
  EXPECT_TRUE(d.hung_up());
}

TEST(WWIVFossInbandTest, HungUp_StaleDropBeforeData_Ignored) {
  WWIVFossDecoder d;
  EXPECT_EQ("Welcome!", decode(d, "\xFF\x01\xFF\x03" "Welcome!"s));
  EXPECT_FALSE(d.hung_up());
  EXPECT_TRUE(d.dtr());
}

TEST(WWIVFossInbandTest, HungUp_StaleDropThenBanner_Ignored) {
  WWIVFossDecoder d;
  EXPECT_EQ("Welcome!", decode(d, "\xFF\x01" "Welcome!"s));
  EXPECT_FALSE(d.hung_up());
}

TEST(WWIVFossInbandTest, HungUp_AfterDtrRaise) {
  WWIVFossDecoder d;
  EXPECT_EQ("", decode(d, "\xFF\x03\xFF\x01"s));
  EXPECT_TRUE(d.hung_up());
}

TEST(WWIVFossInbandTest, HungUp_PulseStaysHungUp) {
  WWIVFossDecoder d;
  EXPECT_EQ("x", decode(d, "x\xFF\x01"s));
  EXPECT_EQ("", decode(d, "\xFF\x03"s));
  EXPECT_TRUE(d.dtr());
  EXPECT_TRUE(d.hung_up());
}

TEST(WWIVFossInbandTest, HungUp_DropSplitAcrossReads) {
  WWIVFossDecoder d;
  EXPECT_EQ("x", decode(d, "x\xFF"s));
  EXPECT_FALSE(d.hung_up());
  EXPECT_EQ("", decode(d, "\x01"s));
  EXPECT_TRUE(d.hung_up());
}

TEST(WWIVFossInbandTest, Decode_LineStateZero) {
  WWIVFossDecoder d;
  EXPECT_EQ("x", decode(d, "x\xFF\x00"s));
  EXPECT_FALSE(d.dtr());
}

TEST(WWIVFossInbandTest, Decode_EscapeSplitAcrossReads) {
  WWIVFossDecoder d;
  EXPECT_EQ("a", decode(d, "a\xFF"s));
  EXPECT_EQ("\xFF" "b"s, decode(d, "\xFF" "b"s));
  EXPECT_EQ("c", decode(d, "c\xFF"s));
  EXPECT_EQ("d", decode(d, "\x01" "d"s));
  EXPECT_FALSE(d.dtr());
}

TEST(WWIVFossInbandTest, RoundTrip_AllBytes) {
  std::string all;
  for (int i = 0; i < 256; i++) {
    all.push_back(static_cast<char>(i));
  }
  const auto escaped = escape(all);
  EXPECT_EQ(257u, escaped.size());

  // One byte at a time, to exercise every split point.
  WWIVFossDecoder d;
  std::string out;
  for (const auto ch : escaped) {
    out += d.decode(&ch, 1);
  }
  EXPECT_EQ(all, out);
  EXPECT_TRUE(d.dtr());
}
