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
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "fsed/line.h"
#include "fsed/model.h"
#include "fsed/spell.h"
#include "fsed/spell_text.h"
#include "fsed/spell_walk.h"
#include <deque>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace wwiv::fsed;
using namespace testing;

namespace {

// Knows only its words, suggestions come from a map.
class FakeDictionary final : public SpellDictionary {
public:
  FakeDictionary(std::set<std::string> words,
                 std::map<std::string, std::vector<std::string>> suggestions = {})
      : words_(std::move(words)), suggestions_(std::move(suggestions)) {}

  [[nodiscard]] bool available() const override { return available_; }
  [[nodiscard]] bool check(const std::string& word) const override {
    return words_.count(word) > 0;
  }
  [[nodiscard]] std::vector<std::string> suggest(const std::string& word) const override {
    if (const auto it = suggestions_.find(word); it != suggestions_.end()) {
      return it->second;
    }
    return {};
  }
  bool add(const std::string& word) override {
    words_.insert(word);
    added_.push_back(word);
    return true;
  }

  bool available_{true};
  std::set<std::string> words_;
  std::map<std::string, std::vector<std::string>> suggestions_;
  std::vector<std::string> added_;
};

// Plays back keys and replacement text. Hangs up when out of keys.
class FakeIO final : public SpellCheckIO {
public:
  explicit FakeIO(std::deque<int> keys, std::deque<std::string> replacements = {})
      : keys_(std::move(keys)), replacements_(std::move(replacements)) {}

  void show_word(FsedModel&, const spell_word_t& word,
                 const std::vector<std::string>& suggestions) override {
    shown_.push_back(word.word);
    suggestions_.push_back(suggestions);
    ++visible_;
  }
  void hide_word(FsedModel&, const spell_word_t&) override { --visible_; }
  int getkey(FsedModel&) override {
    if (keys_.empty()) {
      hangup_ = true;
      return 0;
    }
    const auto k = keys_.front();
    keys_.pop_front();
    return k;
  }
  std::string input_replacement(FsedModel&, const std::string& word) override {
    prompted_.push_back(word);
    if (replacements_.empty()) {
      return {};
    }
    auto r = replacements_.front();
    replacements_.pop_front();
    return r;
  }
  void after_edit(FsedModel&, int) override { ++edits_; }
  [[nodiscard]] bool hangup() const override { return hangup_; }

  std::deque<int> keys_;
  std::deque<std::string> replacements_;
  std::vector<std::string> shown_;
  std::vector<std::vector<std::string>> suggestions_;
  std::vector<std::string> prompted_;
  int visible_{0};
  int edits_{0};
  bool hangup_{false};
};

constexpr int ESC = 27;

std::vector<std::string> text_of(const FsedModel& ed) {
  std::vector<std::string> out;
  for (auto i = 0; i < static_cast<int>(ed.size()); i++) {
    std::string s;
    for (const auto& c : ed.line(i).cells()) {
      s.push_back(c.ch);
    }
    out.push_back(s);
  }
  return out;
}

std::vector<bool> wraps_of(const FsedModel& ed) {
  std::vector<bool> out;
  for (auto i = 0; i < static_cast<int>(ed.size()); i++) {
    out.push_back(ed.line(i).wrapped());
  }
  return out;
}

FsedModel model_of(const std::vector<std::pair<bool, std::string>>& lines, int width = 80) {
  FsedModel ed(255);
  ed.set_max_line_len(width);
  for (const auto& [w, t] : lines) {
    ed.emplace_back(line_t(w, t));
  }
  return ed;
}

spell_word_t find_word(const FsedModel& ed, const std::string& w) {
  for (const auto& t : spell_tokenize(ed)) {
    if (t.word == w) {
      return t;
    }
  }
  ADD_FAILURE() << "No word: " << w;
  return {};
}

const std::set<std::string> kWords{"the", "quick", "brown", "fox", "jumps", "over", "lazy",
                                   "dog", "hello", "world", "a",   "is",    "this", "test"};

} // namespace

/////////////////////////////////////////////////////////////////////////////
// spell_replace_word

TEST(SpellWalkTest, Replace_SameLength) {
  auto ed = model_of({{false, "the qick brown fox"}});
  const auto pos = spell_replace_word(ed, find_word(ed, "qick"), "quik");
  EXPECT_THAT(text_of(ed), ElementsAre("the quik brown fox"));
  EXPECT_EQ(0, pos.line);
  EXPECT_EQ(8, pos.x);
}

TEST(SpellWalkTest, Replace_Longer_NoWrap) {
  auto ed = model_of({{false, "the qick brown fox"}, {false, "next"}});
  const auto pos = spell_replace_word(ed, find_word(ed, "qick"), "quick");
  EXPECT_THAT(text_of(ed), ElementsAre("the quick brown fox", "next"));
  EXPECT_THAT(wraps_of(ed), ElementsAre(false, false));
  EXPECT_EQ(0, pos.line);
  EXPECT_EQ(9, pos.x);
}

TEST(SpellWalkTest, Replace_KeepsColor) {
  FsedModel ed(255);
  ed.emplace_back(line_t(false, "\x03"
                                "3teh dog"));
  spell_replace_word(ed, find_word(ed, "teh"), "the");
  const auto& cells = ed.line(0).cells();
  ASSERT_EQ(7u, cells.size());
  for (const auto& c : cells) {
    EXPECT_EQ(3, c.wwiv_color);
  }
}

TEST(SpellWalkTest, Replace_Rewraps) {
  // Width 20 means lines of up to 19 characters plus the trailing space.
  auto ed = model_of({{true, "the quick brown fox "},
                      {true, "jumps over the lazy "},
                      {false, "dog"},
                      {false, ""},
                      {false, "next paragraph"}},
                     20);
  const auto pos = spell_replace_word(ed, find_word(ed, "fox"), "foxxxxxxxx");
  EXPECT_THAT(text_of(ed), ElementsAre("the quick brown ", "foxxxxxxxx jumps ", "over the lazy dog",
                                       "", "next paragraph"));
  EXPECT_THAT(wraps_of(ed), ElementsAre(true, true, false, false, false));
  EXPECT_EQ(1, pos.line);
  EXPECT_EQ(10, pos.x);
}

TEST(SpellWalkTest, Replace_Shorter_PullsUp) {
  auto ed = model_of({{true, "the quick brown "}, {true, "foxxxxxxxx jumps "}, {false, "dog"}}, 20);
  const auto pos = spell_replace_word(ed, find_word(ed, "foxxxxxxxx"), "fox");
  EXPECT_THAT(text_of(ed), ElementsAre("the quick brown fox ", "jumps dog"));
  EXPECT_THAT(wraps_of(ed), ElementsAre(true, false));
  EXPECT_EQ(0, pos.line);
  EXPECT_EQ(19, pos.x);
}

TEST(SpellWalkTest, Replace_SoftWrapMarker) {
  // A trailing ^A also means soft wrapped (as loaded from a message). Build
  // the cells directly since some line_t versions drop ^A when parsing text.
  auto ed = model_of({{false, "the qick brown "}, {false, "fox"}}, 20);
  auto cells = ed.line(0).cells();
  cells.emplace_back(0, '\x01');
  ed.line(0).assign(cells);
  ASSERT_FALSE(ed.line(0).wrapped());
  spell_replace_word(ed, find_word(ed, "qick"), "quick");
  EXPECT_THAT(text_of(ed), ElementsAre("the quick brown fox"));
  EXPECT_THAT(wraps_of(ed), ElementsAre(false));
}

TEST(SpellWalkTest, Replace_CharacterWrapKeepsWordsApart) {
  // "hello" / "wrold" with no space at the break is saved as "hello wrold";
  // replacing the second word must not glue it onto the first.
  auto ed = model_of({{true, "hello"}, {false, "wrold"}}, 20);
  spell_replace_word(ed, find_word(ed, "wrold"), "world");
  EXPECT_THAT(text_of(ed), ElementsAre("hello world"));
  EXPECT_THAT(wraps_of(ed), ElementsAre(false));
  EXPECT_THAT(ed.to_lines(false), ElementsAre("hello world"));
}

TEST(SpellWalkTest, Replace_CharacterWrap_FirstWord) {
  auto ed = model_of({{true, "helo"}, {false, "world"}}, 20);
  spell_replace_word(ed, find_word(ed, "helo"), "hello");
  EXPECT_THAT(ed.to_lines(false), ElementsAre("hello world"));
}

TEST(SpellWalkTest, Replace_HardLinesNotJoined) {
  auto ed = model_of({{false, "hello"}, {false, "the qick fox"}, {false, "dog"}}, 20);
  const auto pos = spell_replace_word(ed, find_word(ed, "qick"), "quick");
  EXPECT_THAT(text_of(ed), ElementsAre("hello", "the quick fox", "dog"));
  EXPECT_EQ(1, pos.line);
  EXPECT_EQ(9, pos.x);
}

TEST(SpellWalkTest, Replace_NoSpace_CharacterWrap) {
  auto ed = model_of({{false, "a bbbbbbbbbb"}}, 10);
  spell_replace_word(ed, find_word(ed, "bbbbbbbbbb"), "cccccccccccc");
  // width 9: "a " then 9 c's then 3 c's.
  EXPECT_THAT(text_of(ed), ElementsAre("a ", "ccccccccc", "ccc"));
  EXPECT_THAT(wraps_of(ed), ElementsAre(true, true, false));
}

TEST(SpellWalkTest, Replace_ClampsCursor) {
  auto ed = model_of({{true, "aaaa bbbb "}, {false, "cccc"}}, 20);
  ed.curli = 1;
  ed.cx = 4;
  spell_replace_word(ed, find_word(ed, "bbbb"), "b");
  ASSERT_EQ(1u, ed.size());
  EXPECT_EQ(0, ed.curli);
  EXPECT_LE(ed.cx, 4);
}

/////////////////////////////////////////////////////////////////////////////
// spell_suggestions

TEST(SpellWalkTest, Suggestions_LimitAndConvert) {
  FakeDictionary d(
      {}, {{"teh", {"the", "ten", "tea", "tee", "tech", "tel", "the", "eh", "te", "tea2", "tea3"}},
           {"cafe", {"caf\xc3\xa9", "\xe2\x82\xac"}}});
  EXPECT_THAT(spell_suggestions(d, "teh"),
              ElementsAre("the", "ten", "tea", "tee", "tech", "tel", "eh", "te", "tea2"));
  // e-acute is 0x82 in CP437, the euro sign can not be shown.
  EXPECT_THAT(spell_suggestions(d, "cafe"), ElementsAre("caf\x82"));
}

TEST(SpellWalkTest, Suggestions_TranspositionFirst) {
  // Hunspell 1.7.2 en_US for "Teh" ranks short fragments above "The", so
  // "The" is 12th and would be dropped by the 9-suggestion cap.
  FakeDictionary d({}, {{"Teh",
                         {"Te", "Th", "Eh", "Tet", "Ted", "Meh", "Tex", "Neh", "TeX", "T eh",
                          "Te h", "The", "Teth", "Tech", "Tee"}}});
  EXPECT_THAT(spell_suggestions(d, "Teh"),
              ElementsAre("The", "Te", "Th", "Eh", "Tet", "Ted", "Meh", "Tex", "Neh"));
}

TEST(SpellWalkTest, Suggestions_PreservesHunspellOrder) {
  FakeDictionary d(
      {}, {{"qick",
            {"quick", "click", "kick", "pick", "wick", "sick", "tick", "nick", "lick", "rick"}}});
  // No adjacent transposition, no spaces: Hunspell's order, still capped at 9.
  EXPECT_THAT(spell_suggestions(d, "qick"), ElementsAre("quick", "click", "kick", "pick", "wick",
                                                        "sick", "tick", "nick", "lick"));
}

TEST(SpellWalkTest, Suggestions_SpacesAfterSingleWords) {
  FakeDictionary d({}, {{"ab", {"a b", "ac", "ba", "ad"}}});
  EXPECT_THAT(spell_suggestions(d, "ab"), ElementsAre("ba", "ac", "ad", "a b"));
}

/////////////////////////////////////////////////////////////////////////////
// spell_check_walk

TEST(SpellWalkTest, Walk_Unavailable) {
  auto ed = model_of({{false, "teh"}});
  FakeDictionary d(kWords);
  d.available_ = false;
  FakeIO io({});
  EXPECT_EQ(spell_check_result_t::unavailable, spell_check_walk(ed, d, io).result);
  EXPECT_TRUE(io.shown_.empty());
}

TEST(SpellWalkTest, Walk_Clean) {
  auto ed = model_of({{false, "the quick brown fox"}});
  FakeDictionary d(kWords);
  FakeIO io({});
  const auto s = spell_check_walk(ed, d, io);
  EXPECT_EQ(spell_check_result_t::clean, s.result);
  EXPECT_EQ(0, s.shown);
}

TEST(SpellWalkTest, Walk_UseSuggestion) {
  auto ed = model_of({{false, "teh quick fox"}});
  FakeDictionary d(kWords, {{"teh", {"ten", "the"}}});
  FakeIO io({'1'});
  const auto s = spell_check_walk(ed, d, io);
  EXPECT_EQ(spell_check_result_t::finished, s.result);
  EXPECT_THAT(text_of(ed), ElementsAre("the quick fox"));
  EXPECT_THAT(io.shown_, ElementsAre("teh"));
  // "the" is a swapped-letter match for "teh", so it is listed first.
  EXPECT_THAT(io.suggestions_.front(), ElementsAre("the", "ten"));
  EXPECT_EQ(1, s.replaced);
  EXPECT_EQ(1, io.edits_);
  EXPECT_EQ(0, io.visible_);
}

TEST(SpellWalkTest, Walk_SuggestionOutOfRange_AsksAgain) {
  auto ed = model_of({{false, "teh fox"}});
  FakeDictionary d(kWords, {{"teh", {"the"}}});
  FakeIO io({'5', 'x', '1'});
  EXPECT_EQ(spell_check_result_t::finished, spell_check_walk(ed, d, io).result);
  EXPECT_THAT(text_of(ed), ElementsAre("the fox"));
  EXPECT_TRUE(io.keys_.empty());
}

TEST(SpellWalkTest, Walk_Replace) {
  auto ed = model_of({{false, "the qwik fox"}});
  FakeDictionary d(kWords);
  FakeIO io({'r'}, {"quick"});
  EXPECT_EQ(spell_check_result_t::finished, spell_check_walk(ed, d, io).result);
  EXPECT_THAT(text_of(ed), ElementsAre("the quick fox"));
  EXPECT_THAT(io.prompted_, ElementsAre("qwik"));
}

TEST(SpellWalkTest, Walk_Replace_Empty_AsksAgain) {
  auto ed = model_of({{false, "the qwik fox"}});
  FakeDictionary d(kWords);
  FakeIO io({'R', 'R'}, {"", "quick"});
  EXPECT_EQ(spell_check_result_t::finished, spell_check_walk(ed, d, io).result);
  EXPECT_THAT(text_of(ed), ElementsAre("the quick fox"));
  // Shown again after the empty answer.
  EXPECT_THAT(io.shown_, ElementsAre("qwik", "qwik"));
}

TEST(SpellWalkTest, Walk_Replace_StillMisspelled_MovesOn) {
  auto ed = model_of({{false, "the qwik fox"}});
  FakeDictionary d(kWords);
  FakeIO io({'R'}, {"qwikk"});
  EXPECT_EQ(spell_check_result_t::finished, spell_check_walk(ed, d, io).result);
  EXPECT_THAT(text_of(ed), ElementsAre("the qwikk fox"));
  EXPECT_THAT(io.shown_, ElementsAre("qwik"));
}

TEST(SpellWalkTest, Walk_Ignore_OnlyOnce) {
  auto ed = model_of({{false, "teh fox teh dog"}});
  FakeDictionary d(kWords);
  FakeIO io({'I', 'I'});
  const auto s = spell_check_walk(ed, d, io);
  EXPECT_EQ(spell_check_result_t::finished, s.result);
  EXPECT_THAT(io.shown_, ElementsAre("teh", "teh"));
  EXPECT_EQ(2, s.shown);
  EXPECT_THAT(text_of(ed), ElementsAre("teh fox teh dog"));
}

TEST(SpellWalkTest, Walk_IgnoreAll) {
  auto ed = model_of({{false, "teh fox teh dog"}, {false, "teh"}, {false, "wrold"}});
  FakeDictionary d(kWords);
  FakeIO io({'g', 'i'});
  EXPECT_EQ(spell_check_result_t::finished, spell_check_walk(ed, d, io).result);
  EXPECT_THAT(io.shown_, ElementsAre("teh", "wrold"));
  // Not added to the dictionary.
  EXPECT_TRUE(d.added_.empty());
}

TEST(SpellWalkTest, Walk_Add) {
  auto ed = model_of({{false, "rushfan fox rushfan caf\x82"}});
  FakeDictionary d(kWords);
  std::vector<std::string> saved;
  FakeIO io({'A', 'A'});
  const auto s = spell_check_walk(ed, d, io, [&](const std::string& w) { saved.push_back(w); });
  EXPECT_EQ(spell_check_result_t::finished, s.result);
  EXPECT_THAT(io.shown_, ElementsAre("rushfan", "caf\x82"));
  // Dictionary and personal word list get UTF-8.
  EXPECT_THAT(d.added_, ElementsAre("rushfan", "caf\xc3\xa9"));
  EXPECT_THAT(saved, ElementsAre("rushfan", "caf\xc3\xa9"));
  EXPECT_EQ(2, s.added);
}

TEST(SpellWalkTest, Walk_Quit) {
  auto ed = model_of({{false, "teh wrold"}});
  FakeDictionary d(kWords, {{"teh", {"the"}}});
  FakeIO io({'1', 'q'});
  const auto s = spell_check_walk(ed, d, io);
  EXPECT_EQ(spell_check_result_t::quit, s.result);
  // The replacement before Q stays.
  EXPECT_THAT(text_of(ed), ElementsAre("the wrold"));
  EXPECT_EQ(0, io.visible_);
}

TEST(SpellWalkTest, Walk_Escape_Cancels) {
  auto ed = model_of({{false, "teh wrold"}});
  FakeDictionary d(kWords);
  FakeIO io({ESC});
  EXPECT_EQ(spell_check_result_t::cancel, spell_check_walk(ed, d, io).result);
  EXPECT_EQ(0, io.visible_);
}

TEST(SpellWalkTest, Walk_Hangup) {
  auto ed = model_of({{false, "teh wrold"}});
  FakeDictionary d(kWords);
  FakeIO io({});
  EXPECT_EQ(spell_check_result_t::hangup, spell_check_walk(ed, d, io).result);
  EXPECT_EQ(0, io.visible_);
}

TEST(SpellWalkTest, Walk_ReplaceThenNextWordAfterReflow) {
  auto ed = model_of({{true, "the qick brown fox "}, {false, "jumps ovr the dog"}}, 20);
  FakeDictionary d(kWords, {{"qick", {"quick"}}, {"ovr", {"over"}}});
  FakeIO io({'1', '1'});
  EXPECT_EQ(spell_check_result_t::finished, spell_check_walk(ed, d, io).result);
  EXPECT_THAT(io.shown_, ElementsAre("qick", "ovr"));
  EXPECT_THAT(text_of(ed), ElementsAre("the quick brown fox ", "jumps over the dog"));
}

TEST(SpellWalkTest, Walk_SkipsQuotesAndCodes) {
  auto ed = model_of({{false, "\x04"
                              "0MSGID wrold"},
                      {false, " > teh quoted"},
                      {false, "|#2teh"}});
  FakeDictionary d(kWords);
  FakeIO io({'I'});
  EXPECT_EQ(spell_check_result_t::finished, spell_check_walk(ed, d, io).result);
  EXPECT_THAT(io.shown_, ElementsAre("teh"));
}

TEST(SpellWalkTest, Walk_ReplaceWithPipeCodeInsideWord) {
  auto ed = model_of({{false, "t|#3eh dog"}});
  FakeDictionary d(kWords, {{"teh", {"the"}}});
  FakeIO io({'1'});
  EXPECT_EQ(spell_check_result_t::finished, spell_check_walk(ed, d, io).result);
  EXPECT_THAT(text_of(ed), ElementsAre("the dog"));
}

TEST(SpellWalkTest, Prompt) {
  EXPECT_STREQ(
      "(1-9)=use suggestion  (R)eplace  (I)gnore  i(G)nore all  (A)dd to dictionary  (Q)uit",
      SPELL_PROMPT);
  EXPECT_LT(std::string(SPELL_PROMPT_NARROW).size(), 80u);
}
