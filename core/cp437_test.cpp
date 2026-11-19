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
#include "core/cp437.h"
#include "gtest/gtest.h"
#include <clocale>
#include <string>

using namespace wwiv::core;

TEST(Cp437Test, Smoke_Char) {
  char s[81];
  ASSERT_EQ(1, cp437_to_utf8('A', s));
  EXPECT_EQ('A', s[0]);
  EXPECT_EQ('\0', s[1]);
}

TEST(Cp437Test, CharMultiple) {
  char s[81];
  EXPECT_EQ(3, cp437_to_utf8(0xfe, s));
  EXPECT_STREQ("\xe2\x96\xa0", s) << s;
}

TEST(Cp437Test, Smoke_String) {
  const auto s = cp437_to_utf8("Hello World");
  EXPECT_EQ("Hello World", s);
}

TEST(Cp437Test, Block_String) {
  const auto s = cp437_to_utf8("\xdb\xdb\xdb");
  EXPECT_EQ(9u, s.length());
  EXPECT_EQ("\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88", s);
}

TEST(Cp437Test, EmptyString) {
  const auto s = cp437_to_utf8("");
  EXPECT_EQ("", s);
}

TEST(Cp437Test, Utf8ToCp437_Ascii) { EXPECT_EQ("Hello World", utf8_to_cp437("Hello World")); }

TEST(Cp437Test, Utf8ToCp437_Accents) {
  // e-acute (U+00E9) is 0x82 and n-tilde (U+00F1) is 0xA4 in CP437.
  EXPECT_EQ("caf\x82 ma\xa4" "ana", utf8_to_cp437("caf\xc3\xa9 ma\xc3\xb1" "ana"));
  // Full block (U+2588) is 0xDB.
  EXPECT_EQ("\xdb", utf8_to_cp437("\xe2\x96\x88"));
}

TEST(Cp437Test, Utf8ToCp437_Unknown) {
  // U+20AC (euro) is not in CP437.
  EXPECT_EQ("1?", utf8_to_cp437("1\xe2\x82\xac"));
  EXPECT_EQ("a_", utf8_to_cp437("a\xff", '_'));
  // Truncated sequence.
  EXPECT_EQ("?", utf8_to_cp437("\xc3"));
}

TEST(Cp437Test, Utf8ToCp437_RoundTrip) {
  std::string all;
  for (auto c = 0x80; c <= 0xff; c++) {
    all.push_back(static_cast<char>(c));
  }
  std::string utf8;
  for (const auto c : all) {
    char buf[8]{};
    const auto cp = static_cast<uint32_t>(cp437_to_utf8(static_cast<uint8_t>(c)));
    if (cp < 0x800) {
      buf[0] = static_cast<char>(0xc0 | (cp >> 6));
      buf[1] = static_cast<char>(0x80 | (cp & 0x3f));
    } else {
      buf[0] = static_cast<char>(0xe0 | (cp >> 12));
      buf[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
      buf[2] = static_cast<char>(0x80 | (cp & 0x3f));
    }
    utf8.append(buf);
  }
  EXPECT_EQ(all, utf8_to_cp437(utf8));
}
