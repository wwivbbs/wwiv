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
#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "fsed/line.h"
#include "fsed/model.h"
#include "fsed/spell_text.h"
#include <clocale>
#include <string>
#include <vector>

using namespace wwiv::fsed;
using namespace testing;

namespace {

std::vector<std::string> words_of(const std::vector<spell_word_t>& words) {
  std::vector<std::string> out;
  for (const auto& w : words) {
    out.push_back(w.word);
  }
  return out;
}

std::vector<std::string> words_of_lines(const std::vector<line_t>& lines) {
  return words_of(spell_tokenize(lines));
}

std::vector<std::string> words_of_text(const std::string& text) {
  return words_of_lines(std::vector<line_t>{line_t{text}});
}

} // namespace

TEST(SpellTextTest, Simple) {
  const auto words = spell_tokenize({line_t{"Hello wrold"}});
  ASSERT_EQ(2u, words.size());
  EXPECT_EQ("Hello", words[0].word);
  EXPECT_EQ(0, words[0].line);
  EXPECT_EQ(0, words[0].x);
  EXPECT_EQ(0, words[0].end_line);
  EXPECT_EQ(5, words[0].end_x);
  EXPECT_EQ("wrold", words[1].word);
  EXPECT_EQ(6, words[1].x);
  EXPECT_EQ(11, words[1].end_x);
}

TEST(SpellTextTest, Empty) {
  EXPECT_TRUE(spell_tokenize(std::vector<line_t>{}).empty());
  EXPECT_TRUE(spell_tokenize({line_t{""}, line_t{"   "}}).empty());
}

TEST(SpellTextTest, Punctuation) {
  const auto words = spell_tokenize({line_t{"Hello, world! (test)"}});
  EXPECT_THAT(words_of(words), ElementsAre("Hello", "world", "test"));
  EXPECT_EQ(15, words[2].x);
  EXPECT_EQ(19, words[2].end_x);
}

TEST(SpellTextTest, Apostrophes) {
  EXPECT_THAT(words_of_text("don't 'quoted' dogs' Frank's"),
              ElementsAre("don't", "quoted", "dogs", "Frank's"));
}

TEST(SpellTextTest, Hyphens) {
  EXPECT_THAT(words_of_text("full-screen editor"), ElementsAre("full", "screen", "editor"));
}

TEST(SpellTextTest, HeartCodes) {
  // line_t removes heart codes from the text when it is created.
  const auto words = spell_tokenize({line_t{"\x03" "5Hello \x03" "1world\x03" "0"}});
  EXPECT_THAT(words_of(words), ElementsAre("Hello", "world"));
  EXPECT_EQ(6, words[1].x);
  EXPECT_EQ(11, words[1].end_x);
}

TEST(SpellTextTest, PipeCodes) {
  const auto words = spell_tokenize({line_t{"|#5Hello |15world |#1te|#2st |#"}});
  EXPECT_THAT(words_of(words), ElementsAre("Hello", "world", "test"));
  EXPECT_EQ(3, words[0].x);
  EXPECT_EQ(8, words[0].end_x);
  EXPECT_EQ(12, words[1].x);
  EXPECT_EQ(17, words[1].end_x);
  // A pipe code inside of the word is part of the word's span.
  EXPECT_EQ(21, words[2].x);
  EXPECT_EQ(28, words[2].end_x);
}

TEST(SpellTextTest, StripCodes) {
  EXPECT_EQ("Hello", spell_strip_codes("\x03" "5Hel|#1lo|15"));
  EXPECT_EQ("Hello ", spell_strip_codes("Hello \x01"));
  EXPECT_EQ("\x01MSGID", spell_strip_codes("\x01MSGID"));
  EXPECT_EQ("a|b", spell_strip_codes("a|b"));
}

TEST(SpellTextTest, Quotes) {
  const std::vector<line_t> lines{
      line_t{"FR> this is qouted"},
      line_t{"> so is thsi"},
      line_t{">> and thsi"},
      line_t{"  JD> indented qoute"},
      // The format common/quote.cpp creates.
      line_t{"\x03" "1FR\x03" "7> \x03" "5qouted text\x03" "0"},
      line_t{"normal text"},
  };
  EXPECT_THAT(words_of_lines(lines), ElementsAre("normal", "text"));
}

TEST(SpellTextTest, SkipLines) {
  EXPECT_TRUE(is_spell_skip_line("FR> quote"));
  EXPECT_TRUE(is_spell_skip_line("> quote"));
  EXPECT_TRUE(is_spell_skip_line("---"));
  EXPECT_TRUE(is_spell_skip_line("--- WWIV 5.9"));
  EXPECT_TRUE(is_spell_skip_line("--"));
  EXPECT_TRUE(is_spell_skip_line("-- "));
  EXPECT_TRUE(is_spell_skip_line(" * Origin: Eagle's Dare (1:2/3)"));
  EXPECT_TRUE(is_spell_skip_line("SEEN-BY: 1/2 3/4"));
  EXPECT_TRUE(is_spell_skip_line("\x04" "0R PATH"));
  EXPECT_TRUE(is_spell_skip_line("\x01MSGID: 1:2/3 12345678"));

  EXPECT_FALSE(is_spell_skip_line(""));
  EXPECT_FALSE(is_spell_skip_line("Hello there"));
  EXPECT_FALSE(is_spell_skip_line("Hello> five letters is not a quote"));
  EXPECT_FALSE(is_spell_skip_line("-- not a sig"));
  EXPECT_FALSE(is_spell_skip_line("- a list item"));
}

TEST(SpellTextTest, SkipLines_NotChecked) {
  const std::vector<line_t> lines{
      line_t{"---"},
      line_t{"-- "},
      line_t{" * Origin: Eagels Dare"},
      line_t{"SEEN-BY: 1/2"},
      line_t{"\x04" "0R PATH"},
      // ^A kludge lines are covered by SkipLines. line_t may drop ^A
      // cells, so use a ^D control line with text here.
      line_t{"\x04" "0MSGID wrold"},
      line_t{"checked"},
  };
  EXPECT_THAT(words_of_lines(lines), ElementsAre("checked"));
}

TEST(SpellTextTest, UrlsEmailsAndPaths) {
  EXPECT_THAT(words_of_text("see https://wwivbbs.org/foo and www.example.com or frank@example.com"),
              ElementsAre("see", "and", "or"));
  EXPECT_THAT(words_of_text("in /opt/wwiv/dict or C:\\wwiv file readme.txt e.g. my_var done"),
              ElementsAre("in", "or", "file", "done"));
}

TEST(SpellTextTest, Digits) {
  EXPECT_THAT(words_of_text("abc123 2nd 80x24 Windows-10 ok"), ElementsAre("Windows", "ok"));
}

TEST(SpellTextTest, Case) {
  EXPECT_THAT(words_of_text("WWIV BBS SyncTERM iPhone Hello"), ElementsAre("Hello"));

  spell_tokenizer_options_t options{};
  options.skip_all_caps = false;
  options.skip_mixed_case = false;
  const auto words = spell_tokenize({line_t{"WWIV BBS SyncTERM iPhone Hello"}}, options);
  EXPECT_THAT(words_of(words), ElementsAre("WWIV", "BBS", "SyncTERM", "iPhone", "Hello"));
}

TEST(SpellTextTest, MinLength) {
  EXPECT_THAT(words_of_text("a I x it"), ElementsAre("it"));
}

TEST(SpellTextTest, Cp437Letters) {
  // 0x82 is CP437 e-acute.
  EXPECT_THAT(words_of_text("caf\x82 ok"), ElementsAre("caf\x82", "ok"));
}

TEST(SpellTextTest, Cp437Letters_All) {
  // Accented letters 0x80-0x9A and 0xA0-0xA5 and sharp s 0xE1 are letters.
  EXPECT_THAT(words_of_text("\x8e" "rger Stra\xe1" "e ni\xa4o"),
              ElementsAre("\x8e" "rger", "Stra\xe1" "e", "ni\xa4o"));
  // The currency symbols 0x9B-0x9F, box drawing and the rest break words.
  EXPECT_THAT(words_of_text("abc\x9c" "def ghi\xc4\xc4jkl mno\xe2pqr"),
              ElementsAre("abc", "def", "ghi", "jkl", "mno", "pqr"));
}

TEST(SpellTextTest, Cp437Letters_IgnoresLocale) {
  // Text is CP437 whatever the C locale says. In a Latin-1 or cp1252 locale
  // std::isalpha(0xC4) is true (A-umlaut), but in CP437 it is box drawing.
  const std::string saved = std::setlocale(LC_CTYPE, nullptr);
  bool set = false;
  for (const char* name : {"de_DE.ISO-8859-1", "de_DE.iso88591", "de_DE@euro", "en_US.ISO-8859-1",
                           "German_Germany.1252", ".1252"}) {
    if (std::setlocale(LC_CTYPE, name) != nullptr) {
      set = true;
      break;
    }
  }
  const auto words = words_of_text("ghi\xc4\xc4jkl \xaa" "bc de\xb2" "fg mno\xe2pqr");
  std::setlocale(LC_CTYPE, saved.c_str());
  EXPECT_THAT(words, ElementsAre("ghi", "jkl", "bc", "de", "fg", "mno", "pqr"));
  if (!set) {
    GTEST_SKIP() << "no Latin-1 or cp1252 locale installed; checked the C locale only";
  }
}

TEST(SpellTextTest, SoftWrap_WordWrap) {
  // Word wrap leaves the space at the end of the previous line.
  const auto words = spell_tokenize({line_t{true, "Hello "}, line_t{false, "world"}});
  ASSERT_THAT(words_of(words), ElementsAre("Hello", "world"));
  EXPECT_EQ(1, words[1].line);
  EXPECT_EQ(0, words[1].x);
  EXPECT_EQ(1, words[1].end_line);
  EXPECT_EQ(5, words[1].end_x);
}

TEST(SpellTextTest, SoftWrap_CharacterWrap) {
  // A character wrap splits a word with no space, but the saved message
  // (FsedModel::to_lines(false)) and the editor's reflow put a space there,
  // so it is checked as the two words that are saved.
  const auto words = spell_tokenize({line_t{true, "This is char"}, line_t{false, "acter wrapped"}});
  ASSERT_THAT(words_of(words), ElementsAre("This", "is", "char", "acter", "wrapped"));
  EXPECT_EQ(0, words[2].line);
  EXPECT_EQ(8, words[2].x);
  EXPECT_EQ(0, words[2].end_line);
  EXPECT_EQ(12, words[2].end_x);
  EXPECT_EQ(1, words[3].line);
  EXPECT_EQ(0, words[3].x);
  EXPECT_EQ(1, words[3].end_line);
  EXPECT_EQ(5, words[3].end_x);
  EXPECT_EQ(1, words[4].line);
  EXPECT_EQ(6, words[4].x);
}

TEST(SpellTextTest, SoftWrap_HelloWorld) {
  // The example from the review of #1706.
  const auto words = spell_tokenize({line_t{true, "hello"}, line_t{false, "world"}});
  ASSERT_THAT(words_of(words), ElementsAre("hello", "world"));
  for (const auto& w : words) {
    EXPECT_EQ(w.line, w.end_line) << w.word;
  }
}

TEST(SpellTextTest, SoftWrap_LeadingSpaceOnNextLine) {
  EXPECT_THAT(words_of_lines({line_t{true, "hello"}, line_t{false, " world"}}),
              ElementsAre("hello", "world"));
}

TEST(SpellTextTest, SoftWrap_MatchesSavedText) {
  // Whatever the wrapping, the words are the words of the saved text.
  FsedModel model(10);
  model.emplace_back(line_t{true, "Thsi is a lon"});
  model.emplace_back(line_t{true, "g line "});
  model.emplace_back(line_t{false, "wrapped"});
  const auto saved = model.to_lines(false);
  ASSERT_EQ(1u, saved.size());
  EXPECT_EQ(words_of_text(saved.front()), words_of(spell_tokenize(model)));
}

TEST(SpellTextTest, SoftWrap_Marker) {
  // fsed() loads a saved message with wrapped set from the trailing ^A.
  // Whether line_t keeps the ^A as a cell or drops it, the lines are joined
  // with a space, the same as when the message is saved.
  EXPECT_THAT(words_of_lines({line_t{true, "char\x01"}, line_t{false, "acter"}}),
              ElementsAre("char", "acter"));
}

TEST(SpellTextTest, SoftWrap_NotWrapped) {
  EXPECT_THAT(words_of_lines({line_t{false, "char"}, line_t{false, "acter"}}),
              ElementsAre("char", "acter"));
}

TEST(SpellTextTest, SoftWrap_LastLineWrapped) {
  EXPECT_THAT(words_of_lines({line_t{true, "Hello"}}), ElementsAre("Hello"));
}

TEST(SpellTextTest, SoftWrap_QuoteParagraph) {
  // The whole soft-wrapped paragraph is skipped when it starts as a quote,
  // then checking starts again on the next paragraph.
  const std::vector<line_t> lines{
      line_t{true, "FR> a long qouted line that "},
      line_t{false, "wrapped ont the next"},
      line_t{false, "checked"},
  };
  EXPECT_THAT(words_of_lines(lines), ElementsAre("checked"));
}

TEST(SpellTextTest, Model) {
  FsedModel model(10);
  model.emplace_back(line_t{false, "FR> qouted"});
  model.emplace_back(line_t{true, "Hello "});
  model.emplace_back(line_t{false, "wrold"});

  const auto words = spell_tokenize(model);
  ASSERT_THAT(words_of(words), ElementsAre("Hello", "wrold"));
  EXPECT_EQ(1, words[0].line);
  EXPECT_EQ(2, words[1].line);
  EXPECT_EQ(0, words[1].x);
}
