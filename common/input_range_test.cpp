/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5.x                            */
/*          Copyright (C)2020-2022, WWIV Software Services                */
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
/**************************************************************************/
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "common/input_range.h"

using namespace wwiv::common;
using namespace testing;

TEST(InputRangeTest, ConsecutiveRange_Numbers) {
  EXPECT_TRUE(is_consecutive_range("1-3"));

  EXPECT_FALSE(is_consecutive_range("1,3"));
  EXPECT_FALSE(is_consecutive_range("1,2"));
  EXPECT_FALSE(is_consecutive_range(""));
  EXPECT_FALSE(is_consecutive_range(",2"));
  EXPECT_FALSE(is_consecutive_range("1,"));
}

TEST(InputRangeTest, ConsecutiveRange_Chars) {
  EXPECT_TRUE(is_consecutive_range("A-Z"));

  EXPECT_FALSE(is_consecutive_range("A,C"));
  EXPECT_FALSE(is_consecutive_range("A,B"));
  EXPECT_FALSE(is_consecutive_range(""));
  EXPECT_FALSE(is_consecutive_range(",B"));
  EXPECT_FALSE(is_consecutive_range("Z,"));
}

TEST(InputRangeTest, SplitDistinctRanges) {
  auto r = split_distinct_ranges("1-3,5");
  EXPECT_THAT(r, ElementsAre("1-3", "5"));

  r = split_distinct_ranges(",5,8");
  EXPECT_THAT(r, ElementsAre("5", "8"));

  r = split_distinct_ranges("");
  EXPECT_THAT(r, IsEmpty());

  r = split_distinct_ranges(",5,8,9-10,11");
  EXPECT_THAT(r, ElementsAre("5", "8", "9-10", "11"));
}

TEST(InputRangeTest, SplitDistinctRanges_Char) {
  auto r = split_distinct_ranges("A-C,E");
  EXPECT_THAT(r, ElementsAre("A-C", "E"));
}

TEST(InputRangeTest, ExpandConsecutiveRange) {
  auto r = expand_consecutive_range<int>("1-3");
  EXPECT_THAT(r, ElementsAre(1, 2, 3));
}

TEST(InputRangeTest, ExpandConsecutiveRange_Char) {
  auto r = expand_consecutive_range<char>("B-D");
  EXPECT_THAT(r, ElementsAre('B', 'C', 'D'));
}

TEST(InputRangeTest, ExpandRanges) {
  auto r = expand_ranges<int>("2,5-7,9", std::set<int>{1,2,3,4,5,6,7,8,9,10});
  EXPECT_THAT(r, ElementsAre(2, 5, 6, 7, 9));
}

TEST(InputRangeTest, ExpandRanges_Char) {
  auto r = expand_ranges<char>("A,C-E,G", std::set<char>{'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'});
  EXPECT_THAT(r, ElementsAre('A', 'C', 'D', 'E', 'G'));
}
