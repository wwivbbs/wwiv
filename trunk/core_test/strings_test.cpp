/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*           Copyright (C)2008-2014, WWIV Software Services                */
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

#include <string>
#include <vector>

#include "core/strings.h"

using std::cout;
using std::endl;
using std::ostringstream;
using std::string;
using std::vector;

using namespace wwiv::strings;


TEST(StringsTest, StripColors) {
    EXPECT_EQ( string(""), stripcolors(string("")) );
    EXPECT_EQ( string("|"), stripcolors(string("|")) );
    EXPECT_EQ( string("|0"), stripcolors(string("|0")) );
    EXPECT_EQ( string("12345"), stripcolors(string("12345")) );
    EXPECT_EQ( string("abc"), stripcolors(string("abc")) );
    EXPECT_EQ( string("1 abc"), stripcolors(string("\x031 abc")) );
    EXPECT_EQ( string("\x03 abc"), stripcolors(string("\x03 abc")) );
    EXPECT_EQ( string("abc"), stripcolors(string("|15abc")) );
}

TEST(StringsTest, StringColors_CharStarVersion) {
    EXPECT_STREQ( "", stripcolors("") );
    EXPECT_STREQ( "|", stripcolors("|") );
    EXPECT_STREQ( "|0", stripcolors("|0") );
    EXPECT_STREQ( "12345", stripcolors( "12345") );
}

TEST(StringsTest, Properize) {
    EXPECT_EQ( string("Rushfan"), properize( string("rushfan") ) );
    EXPECT_EQ( string("Rushfan"), properize( string("rUSHFAN") ) );
    EXPECT_EQ( string(""), properize( string("") ) );
    EXPECT_EQ( string(" "), properize( string(" ") ) );
    EXPECT_EQ( string("-"), properize( string("-") ) );
    EXPECT_EQ( string("."), properize( string(".") ) );
    EXPECT_EQ( string("R"), properize( string("R") ) );
    EXPECT_EQ( string("R"), properize( string("r") ) );
    EXPECT_EQ( string("Ru"), properize( string("RU") ) );
    EXPECT_EQ( string("R.U"), properize( string("r.u") ) );
    EXPECT_EQ( string("R U"), properize( string("r u") ) );
    EXPECT_EQ( string("Rushfan"), properize( string("Rushfan") ) );
}

TEST(StringsTest, StringPrintf_Smoke) {
  static const string kRushfan = "rushfan";
  EXPECT_EQ(kRushfan, StringPrintf("%s%s", "rush", "fan"));
  EXPECT_EQ(kRushfan, StringPrintf("%s%c%c%c", "rush", 'f', 'a', 'n'));
}

TEST(StringsTest, StrCat_Smoke) {
  static const string kRushfan = "rushfan";
  EXPECT_EQ(kRushfan, StrCat("rush", "fan"));
  EXPECT_EQ(kRushfan, StrCat("ru", "sh", "fan"));
  EXPECT_EQ(kRushfan, StrCat("ru", "sh", "f", "an"));
  EXPECT_EQ(kRushfan, StrCat("r", "u", "sh", "f", "an"));
  EXPECT_EQ(kRushfan, StrCat("r", "u", "s", "h", "f", "an"));
}

TEST(StringsTest, StringReplace_EntireString) {
  string s = "Hello";
  string world = "World";
  EXPECT_EQ(world, StringReplace(&s, "Hello", "World"));
  EXPECT_EQ(world, s);
}

TEST(StringsTest, StringReplace_PartialString) {
  string s = "Hello World";
  string expected = "World World";
  EXPECT_EQ(expected, StringReplace(&s, "Hello", "World"));
  EXPECT_EQ(expected, s);
}

TEST(StringsTest, StringReplace_NotFound) {
  string s = "Hello World";
  string expected(s);
  EXPECT_EQ(expected, StringReplace(&s, "Dude", "Where's my car"));
  EXPECT_EQ(expected, s);
}

TEST(StringsTest, SplitString_Basic) {
  const string s = "Hello World";
  vector<string> expected = { "Hello", "World" };
  vector<string> actual;
  SplitString(s, " ", &actual);
  EXPECT_EQ(expected, actual);
}

TEST(StringsTest, SplitString_BasicReturned) {
  const string s = "Hello World";
  vector<string> expected = { "Hello", "World" };
  vector<string> actual = SplitString(s, " ");
  EXPECT_EQ(expected, actual);
}

TEST(StringsTest, SplitString_ExtraSingleDelim) {
  const string s = "Hello   World";
  vector<string> expected = { "Hello", "World" };
  vector<string> actual;
  SplitString(s, " ", &actual);
  EXPECT_EQ(expected, actual);
}

TEST(StringsTest, SplitString_TwoDelims) {
  const string s = "Hello\tWorld Everyone";
  vector<string> expected = { "Hello", "World", "Everyone" };
  vector<string> actual;
  SplitString(s, " \t", &actual);
  EXPECT_EQ(expected, actual);
}

TEST(StringsTest, SplitString_TwoDelimsBackToBack) {
  const string s = "Hello\t\tWorld  \t\t  Everyone";
  vector<string> expected = { "Hello", "World", "Everyone" };
  vector<string> actual;
  SplitString(s, " \t", &actual);
  EXPECT_EQ(expected, actual);
}

TEST(StringsTest, StringToShort) {
  EXPECT_EQ(1234, StringToShort("1234"));
  EXPECT_EQ(0, StringToShort("0"));
  EXPECT_EQ(-1234, StringToShort("-1234"));

  EXPECT_EQ(std::numeric_limits<int16_t>::max(), StringToShort("999999"));
  EXPECT_EQ(std::numeric_limits<int16_t>::min(), StringToShort("-999999"));

  EXPECT_EQ(0, StringToShort(""));
  EXPECT_EQ(0, StringToShort("ASDF"));
}

TEST(StringsTest, StringToUnsignedShort) {
  EXPECT_EQ(1234, StringToUnsignedShort("1234"));
  EXPECT_EQ(0, StringToUnsignedShort("0"));

  EXPECT_EQ(std::numeric_limits<uint16_t>::max(), StringToUnsignedShort("999999"));
  EXPECT_EQ(std::numeric_limits<uint16_t>::min(), StringToUnsignedShort("-999999"));

  EXPECT_EQ(0, StringToUnsignedShort(""));
  EXPECT_EQ(0, StringToUnsignedShort("ASDF"));
}

TEST(StringsTest, StringToChar) {
  EXPECT_EQ(std::numeric_limits<int8_t>::max(), StringToChar("1234"));
  EXPECT_EQ(0, StringToChar("0"));
  EXPECT_EQ(std::numeric_limits<int8_t>::min(), StringToChar("-1234"));

  EXPECT_EQ(std::numeric_limits<int8_t>::max(), StringToChar("999999"));
  EXPECT_EQ(std::numeric_limits<int8_t>::min(), StringToChar("-999999"));

  EXPECT_EQ(0, StringToChar(""));
  EXPECT_EQ(0, StringToChar("ASDF"));
}

TEST(StringsTest, StringToUnsignedChar) {
  EXPECT_EQ(12, StringToUnsignedChar("12"));
  EXPECT_EQ(255, StringToUnsignedChar("255"));
  EXPECT_EQ(0, StringToUnsignedChar("0"));
  EXPECT_EQ(0, StringToUnsignedChar("-123"));

  EXPECT_EQ(std::numeric_limits<uint8_t>::max(), StringToUnsignedChar("999999"));
  EXPECT_EQ(std::numeric_limits<uint8_t>::min(), StringToUnsignedChar("-999999"));

  EXPECT_EQ(0, StringToUnsignedChar(""));
  EXPECT_EQ(0, StringToUnsignedChar("ASDF"));
}

TEST(StringsTest, RemoveWhitespace_NoSpace) {
  string s("HelloWorld");
  string expected(s);
  RemoveWhitespace(&s);
  EXPECT_EQ(expected, s);
}

TEST(StringsTest, RemoveWhitespace_InnerSpace) {
  string expected("HelloWorld");
  string s("Hello World");
  RemoveWhitespace(&s);
  EXPECT_EQ(expected, s);
}

TEST(StringsTest, RemoveWhitespace_Trailing) {
  string expected("HelloWorld");
  string s("Hello World  ");
  RemoveWhitespace(&s);
  EXPECT_EQ(expected, s);
}

TEST(StringsTest, RemoveWhitespace_Leading) {
  string expected("HelloWorld");
  string s("  Hello World");
  RemoveWhitespace(&s);
  EXPECT_EQ(expected, s);
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
