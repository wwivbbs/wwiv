/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*               Copyright (C)2014-2015 WWIV Software Services            */
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
#include "core/stl.h"

#include <map>
#include <string>
#include <vector>

using std::map;
using std::string;
using std::vector;

using namespace wwiv::stl;

TEST(StlTest, Contains_VectorInts) {
  vector<int> ints = { 1, 2, 3 };

  EXPECT_TRUE(contains(ints, 1));
  EXPECT_FALSE(contains(ints, 0));
  EXPECT_FALSE(contains(ints, 4));
}

TEST(StlTest, Contains_VectorStrings) {
  vector<string> strings = { "one", "two", "three" };

  EXPECT_TRUE(contains(strings, "one"));
  EXPECT_FALSE(contains(strings, ""));
  EXPECT_FALSE(contains(strings, "four"));
}

TEST(StlTest, Contains_MapStringInts) {
  map<string, int> ints = { { "one", 1}, {"two", 2}, {"three", 3} };

  EXPECT_TRUE(contains(ints, "one"));
  EXPECT_FALSE(contains(ints, "zero"));
}

TEST(StlTest, Contains_MapStringStrings) {
  map<string, string> strings = { { "one", "1" }, { "two", "2" }, { "three", "3" } };

  EXPECT_TRUE(contains(strings, "one"));
  EXPECT_FALSE(contains(strings, "zero"));
}

TEST(StlTest, Contains_MapIntInts) {
  map<int, int> ints = { { 0, 1}, {1, 2}, {2, 3} };

  EXPECT_TRUE(contains(ints, 1));
  EXPECT_FALSE(contains(ints, 3));
}
