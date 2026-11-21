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
#include "fsed/spell_config.h"

#include "core/file.h"
#include "core/jsonfile.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/textfile.h"
#include <algorithm>
#include <exception>
#include <filesystem>
#include <string>
#include <utility>

namespace cereal {

template <class Archive> void serialize(Archive& ar, wwiv::fsed::spell_user_config_t& n) {
  SERIALIZE(n, enabled);
  SERIALIZE(n, dictionary);
}

} // namespace cereal

namespace wwiv::fsed {

using namespace wwiv::core;
using namespace wwiv::strings;

bool is_valid_spell_dictionary_name(const std::string& name) {
  if (name.empty() || name.size() > 32) {
    return false;
  }
  return std::all_of(name.begin(), name.end(), [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
           c == '-';
  });
}

std::string resolve_spell_dictionary(const std::string& user_dictionary,
                                     const std::string& menu_set_dictionary,
                                     const std::string& board_dictionary) {
  for (const auto& d : {user_dictionary, menu_set_dictionary, board_dictionary}) {
    if (auto t = StringTrim(d); is_valid_spell_dictionary_name(t)) {
      return t;
    }
  }
  return SPELL_DEFAULT_DICTIONARY;
}

spell_settings_t effective_spell_settings(const sdk::spell_config_t& board,
                                          const std::string& menu_set_dictionary,
                                          const spell_user_config_t& user) {
  spell_settings_t s{};
  s.board_enabled = board.enabled;
  s.check_on_save = board.enabled && board.check_on_save && user.enabled;
  s.dictionary = resolve_spell_dictionary(user.dictionary, menu_set_dictionary, board.dictionary);
  s.dict_dir = board.dict_dir;
  return s;
}

std::filesystem::path spell_data_dir(const std::filesystem::path& datadir) {
  return FilePath(datadir, "spell");
}

static std::filesystem::path spell_users_dir(const std::filesystem::path& datadir) {
  return FilePath(spell_data_dir(datadir), "users");
}

std::filesystem::path spell_user_config_path(const std::filesystem::path& datadir, int usernum) {
  return FilePath(spell_users_dir(datadir), StrCat(usernum, ".json"));
}

std::filesystem::path spell_user_words_path(const std::filesystem::path& datadir, int usernum) {
  return FilePath(spell_users_dir(datadir), StrCat(usernum, ".dic"));
}

std::filesystem::path spell_board_words_path(const std::filesystem::path& datadir) {
  return FilePath(spell_data_dir(datadir), "board.dic");
}

spell_user_config_t load_spell_user_config(const std::filesystem::path& datadir, int usernum) {
  spell_user_config_t c{};
  if (usernum <= 0) {
    return c;
  }
  try {
    // No minimum version so a hand written file without one still loads.
    JsonFile f(spell_user_config_path(datadir, usernum), "spell", c);
    if (!f.Load()) {
      return spell_user_config_t{};
    }
  } catch (const std::exception& e) {
    LOG(WARNING) << "Unable to read spell check settings for user #" << usernum << ": " << e.what();
    return spell_user_config_t{};
  }
  return c;
}

bool save_spell_user_config(const std::filesystem::path& datadir, int usernum,
                            const spell_user_config_t& config) {
  if (usernum <= 0) {
    return false;
  }
  const auto path = spell_user_config_path(datadir, usernum);
  if (!File::Exists(path.parent_path()) && !File::mkdirs(path.parent_path())) {
    return false;
  }
  auto c = config;
  JsonFile f(path, "spell", c, 1);
  return f.Save();
}

SpellWordList::SpellWordList(std::filesystem::path path) : path_(std::move(path)) {}

bool SpellWordList::Load() {
  words_.clear();
  load_failed_ = false;
  if (!File::Exists(path_)) {
    return true;
  }
  TextFile f(path_, "rt");
  if (!f.IsOpen()) {
    load_failed_ = true;
    return false;
  }
  std::string line;
  while (f.ReadLine(&line)) {
    StringTrim(&line);
    if (line.empty() || line.front() == '#') {
      continue;
    }
    words_.insert(line);
  }
  return true;
}

bool SpellWordList::Save() const {
  if (load_failed_) {
    return false;
  }
  if (const auto dir = path_.parent_path(); !dir.empty() && !File::Exists(dir)) {
    if (!File::mkdirs(dir)) {
      return false;
    }
  }
  TextFile f(path_, "wt");
  if (!f.IsOpen()) {
    return false;
  }
  for (const auto& w : words_) {
    f.WriteLine(w);
  }
  return true;
}

bool SpellWordList::add(const std::string& word) {
  const auto w = StringTrim(word);
  if (w.empty()) {
    return false;
  }
  return words_.insert(w).second;
}

bool SpellWordList::contains(const std::string& word) const { return words_.count(word) > 0; }

} // namespace wwiv::fsed
