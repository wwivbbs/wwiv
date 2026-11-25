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

TEST(StringsTest, UCASE_LCASE) {
  EXPECT_EQ("HELLO, WORLD 1", ucase("Hello, World 1"));
  EXPECT_EQ("hello, world 1", lcase("Hello, World 1"));
  EXPECT_EQ("", ucase(""));
}

TEST(StringsTest, SPACE) {
  EXPECT_EQ("   ", space(3));
  EXPECT_EQ("", space(0));
  EXPECT_EQ("", space(-2));
  EXPECT_THROW(space(1000000), wwivbasic::BasicError);
}

TEST(StringsTest, STRING) {
  using wwivbasic::Value;
  EXPECT_EQ("---", string_of(3, Value("-")));
  // Only the first character is used.
  EXPECT_EQ("aa", string_of(2, Value("abc")));
  // A character code.
  EXPECT_EQ("\xCD\xCD", string_of(2, Value(205)));
  EXPECT_EQ("", string_of(0, Value("-")));
  EXPECT_THROW(string_of(2, Value("")), wwivbasic::BasicError);
  EXPECT_THROW(string_of(2, Value(256)), wwivbasic::BasicError);
  EXPECT_THROW(string_of(1000000, Value("x")), wwivbasic::BasicError);
}

TEST(StringsTest, TRIM) {
  EXPECT_EQ("a b", trim("  a b \t"));
  EXPECT_EQ("a b \t", ltrim("  a b \t"));
  EXPECT_EQ("  a b", rtrim("  a b \t"));
  EXPECT_EQ("", trim("   "));
  EXPECT_EQ("", ltrim("   "));
  EXPECT_EQ("", rtrim("   "));
}

// Positions count from 0, as in MID; -1 means not found.
TEST(StringsTest, INSTR) {
  using wwivbasic::BasicError;
  using wwivbasic::Value;
  auto instr2 = [](const char* s, const char* f) { return instr({Value(s), Value(f)}).toInt(); };
  auto instr3 = [](const char* s, const char* f, int start) {
    return instr({Value(s), Value(f), Value(start)}).toInt();
  };
  EXPECT_EQ(0, instr2("abcabc", "abc"));
  EXPECT_EQ(2, instr2("abcabc", "ca"));
  EXPECT_EQ(-1, instr2("abcabc", "x"));
  EXPECT_EQ(3, instr3("abcabc", "abc", 1));
  EXPECT_EQ(-1, instr3("abcabc", "abc", 4));
  // Past the end isn't an error, just not found.
  EXPECT_EQ(-1, instr3("abc", "a", 10));
  EXPECT_EQ(0, instr2("abc", ""));
  EXPECT_THROW(instr3("abc", "a", -1), BasicError);
  EXPECT_THROW(instr({Value("abc")}), BasicError);
}
