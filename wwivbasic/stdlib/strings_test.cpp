#include "gtest/gtest.h"
#include "wwivbasic/stdlib/strings.h"

using namespace wwivbasic::stdlib;

TEST(StringsTest, ASC) {
  ASSERT_EQ(13, asc("\r"));
  ASSERT_EQ(254, asc("\xFE"));
}

TEST(StringsTest, CHR) {
  ASSERT_STREQ("\r", chr(13).c_str());
  ASSERT_STREQ("A", chr(65).c_str());
  ASSERT_STREQ("\xFE", chr(254).c_str());
  ASSERT_STREQ("", chr(999).c_str());
}
// LEFT, RIGHT and MID raise errors where my_basic does.

TEST(StringsTest, LEFT) {
  EXPECT_EQ("ab", left("abc", 2));
  EXPECT_EQ("abc", left("abc", 10));
  EXPECT_THROW(left("abc", 0), wwivbasic::BasicError);
  EXPECT_THROW(left("abc", -1), wwivbasic::BasicError);
}

TEST(StringsTest, RIGHT) {
  EXPECT_EQ("bc", right("abc", 2));
  EXPECT_EQ("abc", right("abc", 10));
  EXPECT_THROW(right("abc", 0), wwivbasic::BasicError);
  EXPECT_THROW(right("abc", -1), wwivbasic::BasicError);
}

TEST(StringsTest, MID) {
  using wwivbasic::BasicError;
  using wwivbasic::Value;
  auto mid3 = [](const char* s, int start, int len) {
    return mid({Value(s), Value(start), Value(len)}).toString();
  };
  EXPECT_EQ("bc", mid3("abcd", 1, 2));
  EXPECT_EQ("cd", mid3("abcd", 2, 10));
  EXPECT_EQ("bcd", mid({Value("abcd"), Value(1)}).toString());
  EXPECT_THROW(mid3("abcd", 4, 1), BasicError);
  EXPECT_THROW(mid3("abcd", -1, 2), BasicError);
  EXPECT_THROW(mid3("abcd", 1, 0), BasicError);
  EXPECT_THROW(mid({Value("abcd")}), BasicError);
}
