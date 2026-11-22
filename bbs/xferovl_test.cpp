/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                Copyright (C)2026, WWIV Software Services               */
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
#include "bbs/xferovl.h"

#include "gtest/gtest.h"

#include <string>
#include <vector>

TEST(ParseUploadListTest, Empty) {
  EXPECT_TRUE(parse_upload_list({}, 0).empty());
}

TEST(ParseUploadListTest, FileAndDescription) {
  const auto e = parse_upload_list({"FOO.ZIP   The foo program\n"}, 0);
  ASSERT_EQ(1u, e.size());
  EXPECT_EQ("FOO.ZIP", e[0].file_name);
  EXPECT_EQ("The foo program", e[0].description);
  EXPECT_EQ("", e[0].extended_description);
}

TEST(ParseUploadListTest, SkipsOptionalWords) {
  const auto e = parse_upload_list({"FOO.ZIP 12345 01-02-93 The foo program\n"}, 2);
  ASSERT_EQ(1u, e.size());
  EXPECT_EQ("FOO.ZIP", e[0].file_name);
  EXPECT_EQ("The foo program", e[0].description);
}

TEST(ParseUploadListTest, MissingOptionalWordsOrDescription) {
  EXPECT_TRUE(parse_upload_list({"FOO.ZIP 12345\n"}, 2).empty());
  EXPECT_TRUE(parse_upload_list({"FOO.ZIP\n"}, 0).empty());
}

TEST(ParseUploadListTest, SkipsLinesStartingWithControlChars) {
  const auto e = parse_upload_list({"\tFOO.ZIP desc\n", "\n", "", "BAR.ZIP desc\n"}, 0);
  ASSERT_EQ(1u, e.size());
  EXPECT_EQ("BAR.ZIP", e[0].file_name);
}

TEST(ParseUploadListTest, ExtendedDescription) {
  const auto e = parse_upload_list(
      {"FOO.ZIP The foo program\n", "    line one\n", "  |  line two\n", "BAR.ZIP Bar\n"}, 0);
  ASSERT_EQ(2u, e.size());
  EXPECT_EQ("line one\r\nline two\r\n", e[0].extended_description);
  EXPECT_EQ("", e[1].extended_description);
}

TEST(ParseUploadListTest, ExtendedDescriptionBeforeAnyFileIsIgnored) {
  const auto e = parse_upload_list({"   orphan\n", "FOO.ZIP desc\n"}, 0);
  ASSERT_EQ(1u, e.size());
  EXPECT_EQ("", e[0].extended_description);
}

TEST(ParseUploadListTest, ExtendedDescriptionAfterBadLineGoesToPreviousFile) {
  const auto e = parse_upload_list({"FOO.ZIP desc\n", "BAD\n", "   more\n"}, 0);
  ASSERT_EQ(1u, e.size());
  EXPECT_EQ("more\r\n", e[0].extended_description);
}

// The extended description used to be strcat'd into a 4096 byte buffer, so
// a long one overflowed the heap.
TEST(ParseUploadListTest, LongExtendedDescription) {
  std::vector<std::string> lines{"FOO.ZIP desc\n"};
  const std::string text(200, 'x');
  for (auto i = 0; i < 100; i++) {
    lines.push_back(" " + text + "\n");
  }
  const auto e = parse_upload_list(lines, 0);
  ASSERT_EQ(1u, e.size());
  EXPECT_EQ(100 * (text.size() + 2), e[0].extended_description.size());
}
