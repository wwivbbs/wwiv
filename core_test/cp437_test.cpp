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
