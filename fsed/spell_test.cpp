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

#include "core/file.h"
#include "core/test/file_helper.h"
#include "fsed/line.h"
#include "fsed/spell.h"
#include "fsed/spell_text.h"
#include <filesystem>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::core::test;
using namespace wwiv::fsed;
using namespace testing;

namespace {

// A dictionary that knows only the words it was given.
class FakeSpellDictionary final : public SpellDictionary {
public:
  explicit FakeSpellDictionary(std::set<std::string> words) : words_(std::move(words)) {}

  [[nodiscard]] bool available() const override { return true; }
  [[nodiscard]] bool check(const std::string& word) const override {
    checked_.push_back(word);
    return words_.count(word) > 0;
  }
  [[nodiscard]] std::vector<std::string> suggest(const std::string&) const override { return {}; }
  bool add(const std::string& word) override {
    words_.insert(word);
    return true;
  }

  std::set<std::string> words_;
  mutable std::vector<std::string> checked_;
};

} // namespace

TEST(SpellTest, NullDictionary) {
  NullSpellDictionary dict;
  EXPECT_FALSE(dict.available());
  EXPECT_TRUE(dict.check("wrold"));
  EXPECT_TRUE(dict.suggest("wrold").empty());
  EXPECT_FALSE(dict.add("wrold"));
}

TEST(SpellTest, Misspelled) {
  FakeSpellDictionary dict(std::set<std::string>{"quick", "brown", "fox"});
  const auto words = spell_tokenize({line_t{"Teh quick brown fox"}});
  const auto bad = misspelled_words(words, dict);
  ASSERT_EQ(1u, bad.size());
  EXPECT_EQ("Teh", bad[0].word);
  EXPECT_EQ(0, bad[0].line);
  EXPECT_EQ(0, bad[0].x);
  EXPECT_EQ(3, bad[0].end_x);
}

TEST(SpellTest, Misspelled_AfterAdd) {
  FakeSpellDictionary dict(std::set<std::string>{"quick"});
  spell_tokenizer_options_t options{};
  options.skip_mixed_case = false;
  const auto words = spell_tokenize({line_t{"quick WWIVnet"}}, options);
  EXPECT_EQ(1u, misspelled_words(words, dict).size());
  EXPECT_TRUE(dict.add("WWIVnet"));
  EXPECT_TRUE(misspelled_words(words, dict).empty());
}

TEST(SpellTest, Misspelled_NotAvailable) {
  NullSpellDictionary dict;
  const auto words = spell_tokenize({line_t{"Teh qiuck"}});
  EXPECT_TRUE(misspelled_words(words, dict).empty());
}

TEST(SpellTest, Misspelled_Utf8) {
  FakeSpellDictionary dict(std::set<std::string>{});
  // 0x82 is CP437 e-acute, U+00E9 is C3 A9 in UTF-8.
  const auto words = spell_tokenize({line_t{"caf\x82"}});
  const auto bad = misspelled_words(words, dict);
  ASSERT_EQ(1u, bad.size());
  // The word in the result is still CP437, the dictionary sees UTF-8.
  EXPECT_EQ("caf\x82", bad[0].word);
  EXPECT_THAT(dict.checked_, ElementsAre("caf\xc3\xa9"));
}

TEST(SpellTest, SearchPath) {
  const auto dirs = spell_dictionary_search_path("/home/dict", "/bbs");
  ASSERT_GE(dirs.size(), 2u);
#ifndef _WIN32
  EXPECT_EQ("/home/dict", dirs[0].string());
#endif
  EXPECT_EQ(FilePath("/bbs", "dict").string(), dirs[1].string());
#ifdef _WIN32
  // No system directories on Windows.
  EXPECT_EQ(2u, dirs.size());
#else
  ASSERT_EQ(6u, dirs.size());
  EXPECT_EQ("/usr/share/hunspell", dirs[2].string());
  EXPECT_EQ("/usr/local/share/hunspell", dirs[3].string());
  EXPECT_EQ("/usr/share/myspell/dicts", dirs[4].string());
  EXPECT_EQ("/usr/share/myspell", dirs[5].string());
#endif
}

TEST(SpellTest, SearchPath_NoDictDir) {
  const auto dirs = spell_dictionary_search_path("", "/bbs");
  ASSERT_FALSE(dirs.empty());
  EXPECT_EQ(FilePath("/bbs", "dict").string(), dirs[0].string());
}

TEST(SpellTest, SearchPath_AbsoluteDictDir) {
  FileHelper helper;
  const auto abs = helper.Dir("mydicts");
  const auto dirs = spell_dictionary_search_path(abs.string(), "/bbs");
  ASSERT_GE(dirs.size(), 2u);
  EXPECT_EQ(abs.string(), dirs[0].string());
}

TEST(SpellTest, SearchPath_RelativeDictDir) {
  // A relative dict_dir is under the BBS directory, not the current directory.
  const auto dirs = spell_dictionary_search_path("dicts", "/bbs");
  ASSERT_GE(dirs.size(), 2u);
  EXPECT_EQ(FilePath("/bbs", "dicts").string(), dirs[0].string());
  EXPECT_EQ(FilePath("/bbs", "dict").string(), dirs[1].string());
}

TEST(SpellTest, SearchPath_NoBbsDir) {
  const auto dirs = spell_dictionary_search_path("", "");
#ifdef _WIN32
  EXPECT_TRUE(dirs.empty());
#else
  ASSERT_EQ(4u, dirs.size());
  EXPECT_EQ("/usr/share/hunspell", dirs[0].string());
#endif
}

TEST(SpellTest, HunspellPath_NotWindows) {
  EXPECT_EQ("/bbs/dict/en_US.aff", spell_hunspell_path("/bbs/dict/en_US.aff", false));
  EXPECT_EQ("/bbs/W\xc3\xb6rter/de_DE.aff",
            spell_hunspell_path("/bbs/W\xc3\xb6rter/de_DE.aff", false));
}

TEST(SpellTest, HunspellPath_Windows_Ascii) {
  // Plain ASCII paths that fit in MAX_PATH work with fopen, leave them alone.
  EXPECT_EQ(R"(C:\wwiv\dict\en_US.aff)", spell_hunspell_path(R"(C:\wwiv\dict\en_US.aff)", true));
  EXPECT_EQ("C:/wwiv/dict/en_US.aff", spell_hunspell_path("C:/wwiv/dict/en_US.aff", true));
  EXPECT_EQ(R"(\\server\bbs\dict\en_US.aff)",
            spell_hunspell_path(R"(\\server\bbs\dict\en_US.aff)", true));
  EXPECT_EQ("", spell_hunspell_path("", true));
}

TEST(SpellTest, HunspellPath_Windows_Utf8) {
  EXPECT_EQ("\\\\?\\C:\\B\xc3\xa4r\\dict\\de_DE.aff",
            spell_hunspell_path("C:\\B\xc3\xa4r\\dict\\de_DE.aff", true));
  // Forward slashes are not allowed after \\?\, so they are changed.
  EXPECT_EQ("\\\\?\\D:\\B\xc3\xa4r\\dict\\de_DE.dic",
            spell_hunspell_path("D:/B\xc3\xa4r/dict/de_DE.dic", true));
}

TEST(SpellTest, HunspellPath_Windows_Unc) {
  EXPECT_EQ("\\\\?\\UNC\\server\\B\xc3\xa4r\\de_DE.aff",
            spell_hunspell_path("\\\\server\\B\xc3\xa4r\\de_DE.aff", true));
}

TEST(SpellTest, HunspellPath_Windows_Long) {
  const std::string dir = "C:\\" + std::string(300, 'a');
  EXPECT_EQ("\\\\?\\" + dir + "\\en_US.aff", spell_hunspell_path(dir + "\\en_US.aff", true));
}

TEST(SpellTest, HunspellPath_Windows_AlreadyPrefixed) {
  const std::string p = "\\\\?\\C:\\B\xc3\xa4r\\en_US.aff";
  EXPECT_EQ(p, spell_hunspell_path(p, true));
}

TEST(SpellTest, HunspellPath_Windows_Relative) {
  // The prefix only works with absolute paths, so relative ones are unchanged.
  EXPECT_EQ("dict\\B\xc3\xa4r.aff", spell_hunspell_path("dict\\B\xc3\xa4r.aff", true));
}

TEST(SpellTest, FindDictionary) {
  FileHelper helper;
  ASSERT_TRUE(helper.Mkdir("one"));
  ASSERT_TRUE(helper.Mkdir("two"));
  // "one" only has half of the dictionary, so "two" is used.
  helper.CreateTempFile("one/en_US.aff", "SET UTF-8\n");
  helper.CreateTempFile("two/en_US.aff", "SET UTF-8\n");
  helper.CreateTempFile("two/en_US.dic", "1\nhello\n");
  const std::vector<std::filesystem::path> dirs{helper.Dir("one"), helper.Dir("two")};

  const auto files = find_spell_dictionary("en_US", dirs);
  ASSERT_TRUE(files.has_value());
  EXPECT_EQ(FilePath(helper.Dir("two"), "en_US.aff").string(), files->aff.string());
  EXPECT_EQ(FilePath(helper.Dir("two"), "en_US.dic").string(), files->dic.string());

  EXPECT_FALSE(find_spell_dictionary("fr_FR", dirs).has_value());
  EXPECT_FALSE(find_spell_dictionary("", dirs).has_value());
}

TEST(SpellTest, CreateDictionary_NotFound) {
  FileHelper helper;
  const auto dict = create_spell_dictionary("xx_XX", {helper.TempDir()});
  ASSERT_NE(nullptr, dict);
  EXPECT_FALSE(dict->available());
  EXPECT_TRUE(dict->check("wrold"));
}

TEST(SpellTest, CreateDictionary) {
  FileHelper helper;
  helper.CreateTempFile("tiny.aff", "SET UTF-8\n");
  helper.CreateTempFile("tiny.dic", "2\nhello\nworld\n");
  auto dict = create_spell_dictionary("tiny", {helper.TempDir()});
  ASSERT_NE(nullptr, dict);
  if (!spell_check_compiled_in()) {
    // Built with -DWWIV_HUNSPELL=OFF or without Hunspell installed.
    EXPECT_FALSE(dict->available());
    EXPECT_TRUE(dict->check("helo"));
    return;
  }
  EXPECT_TRUE(dict->available());
  EXPECT_TRUE(dict->check("hello"));
  EXPECT_TRUE(dict->check("world"));
  EXPECT_FALSE(dict->check("helo"));
  EXPECT_TRUE(dict->add("helo"));
  EXPECT_TRUE(dict->check("helo"));
}

TEST(SpellTest, CreateDictionary_NonAsciiDirectory) {
  // On Windows this needs the \\?\ form of the path for Hunspell, which only
  // MSVC builds of Hunspell understand.
#if defined(_WIN32) && !defined(_MSC_VER)
  GTEST_SKIP() << "Hunspell only supports UTF-8 paths on Windows when built with MSVC";
#endif
  FileHelper helper;
  const std::string dirname = "W\xc3\xb6rterbuch";
  ASSERT_TRUE(helper.Mkdir(dirname));
  helper.CreateTempFile(dirname + "/tiny.aff", "SET UTF-8\n");
  helper.CreateTempFile(dirname + "/tiny.dic", "2\nhello\nworld\n");
  auto dict = create_spell_dictionary("tiny", {helper.Dir(dirname)});
  ASSERT_NE(nullptr, dict);
  if (!spell_check_compiled_in()) {
    EXPECT_FALSE(dict->available());
    return;
  }
  ASSERT_TRUE(dict->available());
  EXPECT_TRUE(dict->check("hello"));
  EXPECT_FALSE(dict->check("helo"));
}
