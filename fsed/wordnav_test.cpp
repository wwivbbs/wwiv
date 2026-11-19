/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*              Copyright (C)2020-2022, WWIV Software Services            */
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
#include "fsed/wordnav.h"

#include "gtest/gtest.h"

using namespace wwiv::fsed;

TEST(WordNav, WordRightSameLine) {
  const std::vector<std::string> lines{"The quick brown"};
  auto p = word_right(lines, word_pos_t{0, 0});
  EXPECT_EQ(0, p.line);
  EXPECT_EQ(4, p.x); // 'q' of quick
  p = word_right(lines, p);
  EXPECT_EQ(10, p.x); // 'b' of brown
  p = word_right(lines, p);
  EXPECT_EQ(15, p.x); // end
  p = word_right(lines, p);
  EXPECT_EQ(15, p.x);
}

TEST(WordNav, WordLeftSameLine) {
  const std::vector<std::string> lines{"The quick brown"};
  auto p = word_left(lines, word_pos_t{0, 15});
  EXPECT_EQ(0, p.line);
  EXPECT_EQ(10, p.x); // 'b' of brown
  p = word_left(lines, p);
  EXPECT_EQ(4, p.x); // 'q'
  p = word_left(lines, p);
  EXPECT_EQ(0, p.x); // 'T'
  p = word_left(lines, p);
  EXPECT_EQ(0, p.x);
}

TEST(WordNav, MidWordGoesToStartOrNext) {
  const std::vector<std::string> lines{"The quick"};
  // From inside "quick"
  auto p = word_left(lines, word_pos_t{0, 6});
  EXPECT_EQ(4, p.x);
  p = word_right(lines, word_pos_t{0, 6});
  EXPECT_EQ(9, p.x); // end of line
}

TEST(WordNav, CrossesLines) {
  const std::vector<std::string> lines{"The quick", "brown fox"};
  // From end of first line, word right lands on "brown"
  auto p = word_right(lines, word_pos_t{0, 9});
  EXPECT_EQ(1, p.line);
  EXPECT_EQ(0, p.x);
  // From start of second line, word left lands on "quick"
  p = word_left(lines, word_pos_t{1, 0});
  EXPECT_EQ(0, p.line);
  EXPECT_EQ(4, p.x);
}

TEST(WordNav, SkipsBlankLines) {
  const std::vector<std::string> lines{"Hello", "", "World"};
  auto p = word_right(lines, word_pos_t{0, 0});
  EXPECT_EQ(2, p.line);
  EXPECT_EQ(0, p.x);
  p = word_left(lines, word_pos_t{2, 0});
  EXPECT_EQ(0, p.line);
  EXPECT_EQ(0, p.x);
}

TEST(WordNav, LeadingAndTrailingSpace) {
  const std::vector<std::string> lines{"  foo  bar  "};
  auto p = word_right(lines, word_pos_t{0, 0});
  EXPECT_EQ(2, p.x); // 'f'
  p = word_right(lines, p);
  EXPECT_EQ(7, p.x); // 'b'
  p = word_right(lines, p);
  EXPECT_EQ(12, p.x); // end
  p = word_left(lines, word_pos_t{0, 12});
  EXPECT_EQ(7, p.x);
}

TEST(WordNav, EmptyDocument) {
  const std::vector<std::string> lines{};
  auto p = word_right(lines, word_pos_t{0, 0});
  EXPECT_EQ(0, p.line);
  EXPECT_EQ(0, p.x);
  p = word_left(lines, word_pos_t{0, 0});
  EXPECT_EQ(0, p.line);
  EXPECT_EQ(0, p.x);
}
