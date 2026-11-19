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

#include "core/file.h"
#include "core/jsonfile.h"
#include "core/test/file_helper.h"
#include "fsed/spell_config.h"
#include "sdk/config.h"
#include "sdk/config_cereal.h"
#include "sdk/menus/menu.h"
#include "sdk/menus/menus_cereal.h"
#include <filesystem>
#include <string>

using namespace wwiv::core;
using namespace wwiv::core::test;
using namespace wwiv::fsed;
using namespace testing;

TEST(SpellConfigTest, ResolveDictionary_Order) {
  EXPECT_EQ("de_DE", resolve_spell_dictionary("de_DE", "fr_FR", "en_GB"));
  EXPECT_EQ("fr_FR", resolve_spell_dictionary("", "fr_FR", "en_GB"));
  EXPECT_EQ("en_GB", resolve_spell_dictionary("", "", "en_GB"));
  EXPECT_EQ("en_US", resolve_spell_dictionary("", "", ""));
  EXPECT_EQ("en_US", resolve_spell_dictionary(" ", "  ", ""));
}

TEST(SpellConfigTest, ValidDictionaryName) {
  EXPECT_TRUE(is_valid_spell_dictionary_name("en_US"));
  EXPECT_TRUE(is_valid_spell_dictionary_name("de_DE_frami"));
  EXPECT_TRUE(is_valid_spell_dictionary_name("pt-BR"));
  EXPECT_FALSE(is_valid_spell_dictionary_name(""));
  EXPECT_FALSE(is_valid_spell_dictionary_name("../../etc/passwd"));
  EXPECT_FALSE(is_valid_spell_dictionary_name("en US"));
  EXPECT_FALSE(is_valid_spell_dictionary_name("C:en_US"));
  EXPECT_FALSE(is_valid_spell_dictionary_name(std::string(33, 'a')));
  // Skipped when resolving.
  EXPECT_EQ("en_GB", resolve_spell_dictionary("../x", "", "en_GB"));
}

TEST(SpellConfigTest, BoardDefaults) {
  const wwiv::sdk::spell_config_t c{};
  EXPECT_TRUE(c.enabled);
  EXPECT_TRUE(c.check_on_save);
  EXPECT_EQ("en_US", c.dictionary);
  EXPECT_TRUE(c.dict_dir.empty());
}

TEST(SpellConfigTest, Effective) {
  wwiv::sdk::spell_config_t board{};
  board.dictionary = "en_GB";
  board.dict_dir = "/dicts";
  spell_user_config_t user{};

  auto s = effective_spell_settings(board, "", user);
  EXPECT_TRUE(s.board_enabled);
  EXPECT_TRUE(s.check_on_save);
  EXPECT_EQ("en_GB", s.dictionary);
  EXPECT_EQ("/dicts", s.dict_dir);

  EXPECT_EQ("fr_FR", effective_spell_settings(board, "fr_FR", user).dictionary);

  user.dictionary = "de_DE";
  user.enabled = false;
  s = effective_spell_settings(board, "fr_FR", user);
  EXPECT_EQ("de_DE", s.dictionary);
  EXPECT_FALSE(s.check_on_save);

  user.enabled = true;
  board.check_on_save = false;
  EXPECT_FALSE(effective_spell_settings(board, "", user).check_on_save);

  board.check_on_save = true;
  board.enabled = false;
  s = effective_spell_settings(board, "", user);
  EXPECT_FALSE(s.board_enabled);
  EXPECT_FALSE(s.check_on_save);
}

TEST(SpellConfigTest, Paths) {
  const std::filesystem::path data{"data"};
  EXPECT_EQ(data / "spell" / "users" / "12.json", spell_user_config_path(data, 12));
  EXPECT_EQ(data / "spell" / "users" / "12.dic", spell_user_words_path(data, 12));
  EXPECT_EQ(data / "spell" / "board.dic", spell_board_words_path(data));
}

TEST(SpellConfigTest, UserConfig_Missing) {
  FileHelper helper;
  const auto c = load_spell_user_config(helper.TempDir(), 1);
  EXPECT_TRUE(c.enabled);
  EXPECT_TRUE(c.dictionary.empty());
}

TEST(SpellConfigTest, UserConfig_SaveLoad) {
  FileHelper helper;
  spell_user_config_t c{};
  c.enabled = false;
  c.dictionary = "en_GB";
  ASSERT_TRUE(save_spell_user_config(helper.TempDir(), 7, c));
  ASSERT_TRUE(File::Exists(spell_user_config_path(helper.TempDir(), 7)));

  const auto l = load_spell_user_config(helper.TempDir(), 7);
  EXPECT_FALSE(l.enabled);
  EXPECT_EQ("en_GB", l.dictionary);

  // Other users still get the defaults.
  EXPECT_TRUE(load_spell_user_config(helper.TempDir(), 8).enabled);
}

TEST(SpellConfigTest, UserConfig_NotARealUser) {
  FileHelper helper;
  EXPECT_FALSE(save_spell_user_config(helper.TempDir(), 0, spell_user_config_t{}));
  EXPECT_TRUE(load_spell_user_config(helper.TempDir(), 0).enabled);
}

TEST(SpellConfigTest, UserConfig_HandWritten) {
  FileHelper helper;
  ASSERT_TRUE(helper.Mkdir("spell"));
  ASSERT_TRUE(helper.Mkdir("spell/users"));
  // No version and a missing key.
  helper.CreateTempFile("spell/users/3.json", R"({"spell":{"enabled":false}})");
  const auto c = load_spell_user_config(helper.TempDir(), 3);
  EXPECT_FALSE(c.enabled);
  EXPECT_TRUE(c.dictionary.empty());
}

TEST(SpellConfigTest, UserConfig_Garbage) {
  FileHelper helper;
  ASSERT_TRUE(helper.Mkdir("spell"));
  ASSERT_TRUE(helper.Mkdir("spell/users"));
  helper.CreateTempFile("spell/users/4.json", "this is not json");
  const auto c = load_spell_user_config(helper.TempDir(), 4);
  EXPECT_TRUE(c.enabled);
  EXPECT_TRUE(c.dictionary.empty());
}

TEST(SpellConfigTest, WordList_Missing) {
  FileHelper helper;
  SpellWordList l(helper.TempDir() / "nope.dic");
  EXPECT_TRUE(l.Load());
  EXPECT_TRUE(l.words().empty());
}

TEST(SpellConfigTest, WordList_Load) {
  FileHelper helper;
  const auto path =
      helper.CreateTempFile("board.dic", "# BBS words\nWWIV\n\n  Fidonet  \nsysop\nsysop\n");
  SpellWordList l(path);
  ASSERT_TRUE(l.Load());
  EXPECT_THAT(l.words(), ElementsAre("Fidonet", "WWIV", "sysop"));
  EXPECT_TRUE(l.contains("sysop"));
  EXPECT_FALSE(l.contains("BBS"));
}

TEST(SpellConfigTest, WordList_AddSave) {
  FileHelper helper;
  const auto path = spell_user_words_path(helper.TempDir(), 5);
  SpellWordList l(path);
  ASSERT_TRUE(l.Load());
  EXPECT_TRUE(l.add("rushfan"));
  EXPECT_FALSE(l.add("rushfan"));
  EXPECT_FALSE(l.add("  "));
  EXPECT_TRUE(l.add("caf\xc3\xa9"));
  // Creates spell/users.
  ASSERT_TRUE(l.Save());

  SpellWordList r(path);
  ASSERT_TRUE(r.Load());
  EXPECT_THAT(r.words(), ElementsAre("caf\xc3\xa9", "rushfan"));
}

TEST(SpellConfigTest, BoardConfig_SaveLoad) {
  FileHelper helper;
  const auto path = helper.TempDir() / "config.json";
  {
    wwiv::sdk::config_t c{};
    c.spell.enabled = false;
    c.spell.check_on_save = false;
    c.spell.dictionary = "en_GB";
    c.spell.dict_dir = "/opt/dict";
    JsonFile f(path, "config", c, 1);
    ASSERT_TRUE(f.Save());
  }
  wwiv::sdk::config_t c{};
  JsonFile f(path, "config", c, 1);
  ASSERT_TRUE(f.Load());
  EXPECT_FALSE(c.spell.enabled);
  EXPECT_FALSE(c.spell.check_on_save);
  EXPECT_EQ("en_GB", c.spell.dictionary);
  EXPECT_EQ("/opt/dict", c.spell.dict_dir);
}

TEST(SpellConfigTest, BoardConfig_OldConfigWithoutSpell) {
  FileHelper helper;
  // A config.json written before spell check existed.
  const auto path = helper.CreateTempFile(
      "config.json", R"({"version":1,"config":{"systemname":"Eagle's Dare","colors":{}}})");
  wwiv::sdk::config_t c{};
  JsonFile f(path, "config", c, 1);
  ASSERT_TRUE(f.Load());
  EXPECT_EQ("Eagle's Dare", c.systemname);
  EXPECT_TRUE(c.spell.enabled);
  EXPECT_TRUE(c.spell.check_on_save);
  EXPECT_EQ("en_US", c.spell.dictionary);
}

TEST(SpellConfigTest, BoardConfig_PartialSpell) {
  FileHelper helper;
  const auto path = helper.CreateTempFile(
      "config.json", R"({"version":1,"config":{"spell":{"dictionary":"fr_FR"}}})");
  wwiv::sdk::config_t c{};
  JsonFile f(path, "config", c, 1);
  ASSERT_TRUE(f.Load());
  EXPECT_TRUE(c.spell.enabled);
  EXPECT_EQ("fr_FR", c.spell.dictionary);
}

TEST(SpellConfigTest, MenuSet_Dictionary) {
  FileHelper helper;
  const auto path = helper.TempDir() / "menuset.json";
  {
    wwiv::sdk::menus::menu_set_t m{};
    m.name = "wwiv";
    m.dictionary = "de_DE";
    JsonFile f(path, "menuset", m, 1);
    ASSERT_TRUE(f.Save());
  }
  wwiv::sdk::menus::menu_set_t m{};
  JsonFile f(path, "menuset", m, 1);
  ASSERT_TRUE(f.Load());
  EXPECT_EQ("de_DE", m.dictionary);

  // Older menu sets do not have it.
  const auto old =
      helper.CreateTempFile("old.json", R"({"version":1,"menuset":{"name":"wwiv","acs":""}})");
  wwiv::sdk::menus::menu_set_t o{};
  JsonFile fo(old, "menuset", o, 1);
  ASSERT_TRUE(fo.Load());
  EXPECT_EQ("wwiv", o.name);
  EXPECT_TRUE(o.dictionary.empty());
}
