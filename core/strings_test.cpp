/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2008-2022, WWIV Software Services                */
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

#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include "core/strings.h"

using namespace wwiv::strings;

TEST(StringsTest, StripColors) {
  EXPECT_EQ(std::string(""), stripcolors(std::string("")));
  EXPECT_EQ(std::string("|"), stripcolors(std::string("|")));
  EXPECT_EQ(std::string("|0"), stripcolors(std::string("|0")));
  EXPECT_EQ(std::string("12345"), stripcolors(std::string("12345")));
  EXPECT_EQ(std::string("abc"), stripcolors(std::string("abc")));
  EXPECT_EQ(std::string("1 abc"), stripcolors(std::string("\x031 abc")));
  EXPECT_EQ(std::string("\x03 abc"), stripcolors(std::string("\x03 abc")));
  EXPECT_EQ(std::string("abc"), stripcolors(std::string("|15abc")));
}

TEST(StringsTest, StripColors_AnsiSeq) {
  EXPECT_EQ(std::string(""), stripcolors(std::string("\x1b[0m")));
  EXPECT_EQ(std::string(""), stripcolors(std::string("\x1b[0;33;46;1m")));
  EXPECT_EQ(std::string("|"), stripcolors(std::string("|\x1b[0;33;46;1m")));
  EXPECT_EQ(std::string("abc"), stripcolors(std::string("|15\x1b[0;33;46;1mabc")));
  EXPECT_EQ(std::string("abc"),
            stripcolors(std::string("\x1b[0m|15\x1b[0;33;46;1ma\x1b[0mb\x1b[0mc\x1b[0m")));
}

TEST(StringsTest, StringColors_CharStarVersion) {
  EXPECT_STREQ("", stripcolors(""));
  EXPECT_STREQ("|", stripcolors("|"));
  EXPECT_STREQ("|0", stripcolors("|0"));
  EXPECT_STREQ("12345", stripcolors("12345"));
}

TEST(StringsTest, Properize) {
  EXPECT_EQ(std::string("Rushfan"), properize(std::string("rushfan")));
  EXPECT_EQ(std::string("Rushfan"), properize(std::string("rUSHFAN")));
  EXPECT_EQ(std::string(""), properize(std::string("")));
  EXPECT_EQ(std::string(" "), properize(std::string(" ")));
  EXPECT_EQ(std::string("-"), properize(std::string("-")));
  EXPECT_EQ(std::string("."), properize(std::string(".")));
  EXPECT_EQ(std::string("R"), properize(std::string("R")));
  EXPECT_EQ(std::string("R"), properize(std::string("r")));
  EXPECT_EQ(std::string("Ru"), properize(std::string("RU")));
  EXPECT_EQ(std::string("R.U"), properize(std::string("r.u")));
  EXPECT_EQ(std::string("R U"), properize(std::string("r u")));
  EXPECT_EQ(std::string("Rushfan"), properize(std::string("Rushfan")));
}

TEST(StringsTest, StrCat_Smoke) {
  static const std::string kRushfan = "rushfan";
  EXPECT_EQ(kRushfan, StrCat("rush", "fan"));
  EXPECT_EQ(kRushfan, StrCat("ru", "sh", "fan"));
  EXPECT_EQ(kRushfan, StrCat("ru", "sh", "f", "an"));
  EXPECT_EQ(kRushfan, StrCat("r", "u", "sh", "f", "an"));
  EXPECT_EQ(kRushfan, StrCat("r", "u", "s", "h", "f", "an"));
}

TEST(StringsTest, StrCat_AlphaNumeric) {
  static const std::string kWoot = "w00t";
  EXPECT_EQ(kWoot, StrCat("w", 0, 0, "t"));
}

TEST(StringsTest, StringReplace_EntireString) {
  std::string s = "Hello";
  const std::string world = "World";
  StringReplace(&s, "Hello", "World");
  EXPECT_EQ(world, s);
}

TEST(StringsTest, StringReplace_PartialString) {
  std::string s = "Hello World";
  const std::string expected = "World World";
  StringReplace(&s, "Hello", "World");
  EXPECT_EQ(expected, s);
}

TEST(StringsTest, StringReplace_NotFound) {
  std::string s = "Hello World";
  const std::string expected(s);
  StringReplace(&s, "Dude", "Where's my car");
  EXPECT_EQ(expected, s);
}

TEST(StringsTest, SplitString_Basic) {
  const std::string s = "Hello World";
  const std::vector<std::string> expected = {"Hello", "World"};
  std::vector<std::string> actual;
  SplitString(s, " ", &actual);
  EXPECT_EQ(expected, actual);
}

TEST(StringsTest, SplitString_BasicReturned) {
  const std::string s = "Hello World";
  const std::vector<std::string> expected = {"Hello", "World"};
  const auto actual = SplitString(s, " ");
  EXPECT_EQ(expected, actual);
}

TEST(StringsTest, SplitString_ExtraSingleDelim) {
  const std::string s = "Hello   World";
  const std::vector<std::string> expected = {"Hello", "World"};
  std::vector<std::string> actual;
  SplitString(s, " ", &actual);
  EXPECT_EQ(expected, actual);
}

TEST(StringsTest, SplitString_ExtraSingleDelim_NoSkipEmpty) {
  const std::string s = "Hello   World";
  const std::vector<std::string> expected = {"Hello", "", "", "World"};
  std::vector<std::string> actual;
  SplitString(s, " ", false, &actual);
  EXPECT_EQ(expected, actual);
}

TEST(StringsTest, SplitString_TwoDelims) {
  const std::string s = "Hello\tWorld Everyone";
  const std::vector<std::string> expected = {"Hello", "World", "Everyone"};
  std::vector<std::string> actual;
  SplitString(s, " \t", &actual);
  EXPECT_EQ(expected, actual);
}

TEST(StringsTest, SplitString_TwoDelimsBackToBack) {
  const std::string s = "Hello\t\tWorld  \t\t  Everyone";
  const std::vector<std::string> expected = {"Hello", "World", "Everyone"};
  std::vector<std::string> actual;
  SplitString(s, " \t", &actual);
  EXPECT_EQ(expected, actual);
}


TEST(StringsTest, SplitOnce_Smoke) {
  auto [p, s] = SplitOnce("user.name", ".");
  EXPECT_EQ("user", p);
  EXPECT_EQ("name", s);
}

TEST(StringsTest, SplitOnce_Empty) {
  auto [p, s] = SplitOnce("", ".");
  EXPECT_EQ("", p);
  EXPECT_EQ("", s);
}

TEST(StringsTest, SplitOnce_End) {
  auto [p, s] = SplitOnce("user.", ".");
  EXPECT_EQ("user", p);
  EXPECT_EQ("", s);
}

TEST(StringsTest, SplitOnce_Start) {
  auto [p, s] = SplitOnce(".user", ".");
  EXPECT_EQ("", p);
  EXPECT_EQ("user", s);
}

TEST(StringsTest, String_int16_t) {
  EXPECT_EQ(1234, to_number<int16_t>("1234"));
  EXPECT_EQ(0, to_number<int16_t>("0"));
  EXPECT_EQ(-1234, to_number<int16_t>("-1234"));

  EXPECT_EQ(std::numeric_limits<int16_t>::max(), to_number<int16_t>("999999"));
  EXPECT_EQ(std::numeric_limits<int16_t>::min(), to_number<int16_t>("-999999"));

  EXPECT_EQ(0, to_number<int16_t>(""));
  EXPECT_EQ(0, to_number<int16_t>("ASDF"));
}

TEST(StringsTest, String_uint16_t) {
  EXPECT_EQ(1234, to_number<uint16_t>("1234"));
  EXPECT_EQ(0, to_number<uint16_t>("0"));

  EXPECT_EQ(std::numeric_limits<uint16_t>::max(), to_number<uint16_t>("999999"));

  EXPECT_EQ(0, to_number<uint16_t>(""));
  EXPECT_EQ(0, to_number<uint16_t>("ASDF"));
}

TEST(StringsTest, String_unsigned_int) {
  EXPECT_EQ(1234u, to_number<unsigned int>("1234"));
  EXPECT_EQ(static_cast<unsigned int>(0), to_number<unsigned int>("0"));

  EXPECT_EQ(999999u, to_number<unsigned int>("999999"));

  EXPECT_EQ(0u, to_number<unsigned int>(""));
  EXPECT_EQ(0u, to_number<unsigned int>("ASDF"));
}

TEST(StringsTest, String_int) {
  EXPECT_EQ(1234, to_number<int>("1234"));
  EXPECT_EQ(0, to_number<int>("0"));

  EXPECT_EQ(999999, to_number<int>("999999"));
  EXPECT_EQ(-999999, to_number<int>("-999999"));

  EXPECT_EQ(0, to_number<int>(""));
  EXPECT_EQ(0, to_number<int>("ASDF"));
}

TEST(StringsTest, String_int8_t) {
  EXPECT_EQ(std::numeric_limits<int8_t>::max(), to_number<int8_t>("1234"));
  EXPECT_EQ(0, to_number<int8_t>("0"));
  EXPECT_EQ(std::numeric_limits<int8_t>::min(), to_number<int8_t>("-1234"));

  EXPECT_EQ(std::numeric_limits<int8_t>::max(), to_number<int8_t>("999999"));
  EXPECT_EQ(std::numeric_limits<int8_t>::min(), to_number<int8_t>("-999999"));

  EXPECT_EQ(0, to_number<int8_t>(""));
  EXPECT_EQ(0, to_number<int8_t>("ASDF"));
}

TEST(StringsTest, String_uint8_t) {
  EXPECT_EQ(12, to_number<uint8_t>("12"));
  EXPECT_EQ(255, to_number<uint8_t>("255"));
  EXPECT_EQ(0, to_number<uint8_t>("0"));

  EXPECT_EQ(std::numeric_limits<uint8_t>::max(), to_number<uint8_t>("999999"));

  EXPECT_EQ(0, to_number<uint8_t>(""));
  EXPECT_EQ(0, to_number<uint8_t>("ASDF"));
}


TEST(StringsTest, StartsWith) {
  EXPECT_TRUE(starts_with("--foo", "--"));
  EXPECT_TRUE(starts_with("asdf", "a"));
  EXPECT_TRUE(starts_with("asdf", "as"));
  EXPECT_TRUE(starts_with("asdf", "asd"));
  EXPECT_TRUE(starts_with("asdf", "asdf"));
  EXPECT_FALSE(starts_with("asdf", "asf"));
  EXPECT_FALSE(starts_with("asdf", "asdfe"));
}

TEST(StringsTest, EndssWith) {
  EXPECT_TRUE(ends_with("--foo", "foo"));
  EXPECT_TRUE(ends_with("asdf", "f"));
  EXPECT_TRUE(ends_with("asdf", "df"));
  EXPECT_TRUE(ends_with("asdf", "sdf"));
  EXPECT_TRUE(ends_with("asdf", "asdf"));
  EXPECT_FALSE(ends_with("asdf", "adf"));
  EXPECT_FALSE(ends_with("asdf", "easdf"));
}

TEST(StringsTest, StringJustify_Left) {
  std::string a("a");
  StringJustify(&a, 2, ' ', JustificationType::LEFT);
  EXPECT_EQ("a ", a);

  std::string b("b");
  StringJustify(&b, 2, ' ', JustificationType::LEFT);
  EXPECT_EQ("b ", b);
}

TEST(StringsTest, StringJustify_LeftOtherChar) {
  std::string a("a");
  StringJustify(&a, 2, '.', JustificationType::LEFT);
  EXPECT_EQ("a.", a);

  std::string b("b");
  StringJustify(&b, 2, '.', JustificationType::LEFT);
  EXPECT_EQ("b.", b);
}

TEST(StringsTest, StringJustify_Right) {
  std::string a("a");
  StringJustify(&a, 2, ' ', JustificationType::RIGHT);
  EXPECT_EQ(" a", a);

  std::string b("b");
  StringJustify(&b, 2, ' ', JustificationType::RIGHT);
  EXPECT_EQ(" b", b);
}

TEST(StringsTest, StringJustify_RightOtherChar) {
  std::string a("a");
  StringJustify(&a, 2, '.', JustificationType::RIGHT);
  EXPECT_EQ(".a", a);

  std::string b("b");
  StringJustify(&b, 2, '.', JustificationType::RIGHT);
  EXPECT_EQ(".b", b);
}

TEST(StringsTest, StringJustify_LongerString) {
  std::string a("aaa");
  StringJustify(&a, 2, ' ', JustificationType::LEFT);
  EXPECT_EQ("aa", a);

  std::string b("bbb");
  StringJustify(&b, 2, ' ', JustificationType::RIGHT);
  EXPECT_EQ("bb", b);
}

TEST(StringsTest, StringTrim) {
  std::string a = " a ";
  StringTrim(&a);
  EXPECT_EQ("a", a);

  std::string b = "b";
  StringTrim(&b);
  EXPECT_EQ("b", b);
}

TEST(StringsTest, StringTrimBegin) {
  std::string a = " a ";
  StringTrimBegin(&a);
  EXPECT_EQ("a ", a);
}

TEST(StringsTest, StringTrimEnd) {
  std::string a = " a ";
  StringTrimEnd(&a);
  EXPECT_EQ(" a", a);
}

TEST(StringsTest, StringTrimEnd_NoTrailingSpace) {
  std::string a = " a";
  StringTrimEnd(&a);
  EXPECT_EQ(" a", a);
}

TEST(StringsTest, StringTrimEnd_Empty) {
  std::string a;
  StringTrimEnd(&a);
  EXPECT_TRUE(a.empty());
}

TEST(StringsTest, StringTrimEnd_StringView) {
  const auto a = StringTrimEnd(" a ");
  EXPECT_EQ(" a", a);
}

TEST(StringsTest, StringTrimEnd_StringView_Empty) {
  const auto a = StringTrimEnd("");
  EXPECT_TRUE(a.empty());
}

TEST(StringsTest, StringUpperCase) {
  std::string a = "aB";
  StringUpperCase(&a);
  EXPECT_EQ("AB", a);
}

TEST(StringsTest, StringLowerCase) {
  std::string a = "aB";
  StringLowerCase(&a);
  EXPECT_EQ("ab", a);
}

TEST(StringsTest, IEQuals_charstar) {
  EXPECT_TRUE(iequals("foo", "foo"));
  EXPECT_FALSE(iequals("foo", "fo"));
  EXPECT_FALSE(iequals("fo", "foo"));
  EXPECT_FALSE(iequals("", "foo"));
  EXPECT_FALSE(iequals("foo", ""));
}

TEST(StringsTest, IEQuals) {
  EXPECT_TRUE(iequals(std::string("foo"), std::string("foo")));
  EXPECT_FALSE(iequals(std::string("foo"), std::string("fo")));
  EXPECT_FALSE(iequals(std::string("fo"), std::string("foo")));
  EXPECT_FALSE(iequals(std::string(""), std::string("foo")));
  EXPECT_FALSE(iequals(std::string("foo"), std::string("")));
}

TEST(StringsTest, SizeWithoutColors) {
  EXPECT_EQ(1, size_without_colors("a"));
  EXPECT_EQ(1, size_without_colors("|#1a"));
  EXPECT_EQ(1, size_without_colors("|09a"));
  EXPECT_EQ(1, size_without_colors("|17|10a"));
}

TEST(StringsTest, SizeWithoutColors_AnsiStr) {
  EXPECT_EQ(0, size_without_colors("\x1b[0m"));
  EXPECT_EQ(0, size_without_colors("\x1b[0;33;46;1m"));
  EXPECT_EQ(1, size_without_colors("|\x1b[0;33;46;1m"));
  EXPECT_EQ(3, size_without_colors("|15\x1b[0;33;46;1mabc"));
  EXPECT_EQ(3, size_without_colors("\x1b[0m|15\x1b[0;33;46;1ma\x1b[0mb\x1b[0mc\x1b[0m"));
}

TEST(StringsTest, TrimToSizeIgnoreColors) {
  EXPECT_EQ("a", trim_to_size_ignore_colors("a", 1));
  EXPECT_EQ("|#5a", trim_to_size_ignore_colors("|#5a", 1));
  EXPECT_EQ("|09|16a", trim_to_size_ignore_colors("|09|16a", 1));
  EXPECT_EQ("|09|16a|09", trim_to_size_ignore_colors("|09|16a|09", 1));
  EXPECT_EQ("|09|16a", trim_to_size_ignore_colors("|09|16aa|09", 1));
}

TEST(StringsTest, Test_Compiler_Supports_PutTime) {
  using std::chrono::system_clock;

  auto t = system_clock::to_time_t(system_clock::now());
  auto tm = *std::localtime(&t);
  std::ostringstream ss;
  ss << std::put_time(&tm, "%Z");
  const auto s = ss.str();

  ASSERT_FALSE(s.empty());
}

TEST(StringsTest, Size) {
  EXPECT_EQ(3u, wwiv::strings::size("Yo!"));
  EXPECT_EQ(0u, wwiv::strings::size(""));
  const std::string yo = "Yo!";
  EXPECT_EQ(3u, wwiv::strings::size(yo));
  EXPECT_EQ(0u, wwiv::strings::size(std::string("")));
}

TEST(StringsTest, SSize) {
  EXPECT_EQ(3, wwiv::strings::ssize("Yo!"));
  EXPECT_EQ(0, wwiv::strings::ssize(""));
  const std::string yo = "Yo!";
  EXPECT_EQ(3, wwiv::strings::ssize(yo));
  EXPECT_EQ(0, wwiv::strings::ssize(std::string("")));
}
