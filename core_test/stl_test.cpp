/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*               Copyright (C)2014-2020, WWIV Software Services           */
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
#include <deque>
#include <map>
#include <string>
#include <vector>

using std::map;
using std::string;
using std::vector;

using namespace wwiv::stl;

TEST(StlTest, Contains_VectorInts) {
  const vector<int> ints = { 1, 2, 3 };

  EXPECT_TRUE(contains(ints, 1));
  EXPECT_FALSE(contains(ints, 0));
  EXPECT_FALSE(contains(ints, 4));
}

TEST(StlTest, Contains_VectorStrings) {
  const vector<string> strings = { "one", "two", "three" };

  EXPECT_TRUE(contains(strings, "one"));
  EXPECT_FALSE(contains(strings, ""));
  EXPECT_FALSE(contains(strings, "four"));
}

TEST(StlTest, Contains_MapStringInts) {
  const map<string, int> ints = { { "one", 1}, {"two", 2}, {"three", 3} };

  EXPECT_TRUE(contains(ints, "one"));
  EXPECT_FALSE(contains(ints, "zero"));
}

TEST(StlTest, Contains_MapStringStrings) {
  const map<string, string> strings = { { "one", "1" }, { "two", "2" }, { "three", "3" } };

  EXPECT_TRUE(contains(strings, "one"));
  EXPECT_FALSE(contains(strings, "zero"));
}

TEST(StlTest, Contains_MapIntInts) {
  const map<int, int> ints = { { 0, 1}, {1, 2}, {2, 3} };

  EXPECT_TRUE(contains(ints, 1));
  EXPECT_FALSE(contains(ints, 3));
}

TEST(StlTest, Contains_MapConstStringStrings) {
  const map<const string, string> strings = {{"one", "1"},{"two", "2"},{"three", "3"}};

  EXPECT_TRUE(contains(strings, "one"));
  EXPECT_FALSE(contains(strings, "zero"));
}

TEST(StlTest, SizeAsInt) {
  const vector<int> v = {1, 2, 3};
  const auto vs = ssize(v);
  EXPECT_EQ(3, vs);
}

TEST(StlTest, SizeAsInt32) {
  const vector<int> v = {1, 2, 3};
  const auto vs = size_int32(v);
  EXPECT_EQ(3, vs);
}

TEST(StlTest, SizeAsInt16) {
  const vector<int> v = {1, 2, 3};
  const auto vs = size_int16(v);
  EXPECT_EQ(3, vs);
}

TEST(StlTest, SizeAsInt8) {
  const vector<int> v = {1, 2, 3};
  const auto vs = size_int8(v);
  EXPECT_EQ(3, vs);
}

TEST(StlTest, InsertAt) {
  struct Foo { int a; };
  vector<Foo> v = {Foo{1}};
  const vector<Foo>::size_type pos = 0;
  insert_at(v, pos, Foo{0});
  EXPECT_EQ(0, v[0].a);
  EXPECT_EQ(1, v[1].a);
}

TEST(StlTest, EraseAt) {
  struct Foo { int a; };
  vector<Foo> v = {Foo{0}, Foo{1}, Foo{2}};
  const vector<Foo>::size_type pos = 1;
  erase_at(v, pos);
  EXPECT_EQ(0, v[0].a);
  EXPECT_EQ(2, v[1].a);
}


TEST(StlTest, At_Vector_Smoke) {
  // ReSharper disable once CppLocalVariableMayBeConst
  /**NOCONST */ std::vector<int> v{ 1, 2, 3 };

  EXPECT_EQ(1, at(v, 0));
}

TEST(StlTest, At_Vector_Smoke_Const) {
  const std::vector<int> v{ 1, 2, 3 };

  EXPECT_EQ(1, at(v, 0));
}

TEST(StlTest, At_Dequeue_Smoke_Const) {
  const std::deque<int> v{ 1, 2, 3 };

  EXPECT_EQ(1, at(v, 0));
}

TEST(StlTest, At_Map_Smoke_Const) {
  const std::map<int, char> v{ {1,'a'}, {2,'b'}, {3,'c'} };

  EXPECT_EQ('a', at(v, 1));
}

TEST(StlTest, At_Map_Death) {
  const std::map<int, char> v{ {1,'a'}, {2,'b'}, {3,'c'} };

  EXPECT_DEATH_IF_SUPPORTED(at(v, 20), ".*Terminating in debug build.");
}
