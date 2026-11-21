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
#ifndef INCLUDED_FSED_SPELL_CONFIG_H
#define INCLUDED_FSED_SPELL_CONFIG_H

#include "sdk/config.h"
#include <filesystem>
#include <set>
#include <string>

namespace wwiv::fsed {

/**
 * Per user spell check settings, stored as a sidecar file next to the
 * user's personal word list (data/spell/users/{usernum}.json) so that
 * userrec does not change.
 */
struct spell_user_config_t {
  // Does this user want spell check when saving a message.
  bool enabled{true};
  // Dictionary override (i.e. en_GB). Empty means no override.
  std::string dictionary;
};

/** The spell check settings that apply to one user in one menu set. */
struct spell_settings_t {
  // Spell check is turned on for the BBS.
  bool board_enabled{true};
  // Run the spell check when this user saves a message.
  bool check_on_save{true};
  // Dictionary to use.
  std::string dictionary{"en_US"};
  // Extra directory to search for dictionaries (may be empty).
  std::string dict_dir;
};

/** Name of the dictionary used when nothing else is configured. */
inline constexpr const char* SPELL_DEFAULT_DICTIONARY = "en_US";

/**
 * True if name can be a dictionary name: 1-32 letters, digits, '_' or '-'
 * (i.e. en_US, de_DE_frami). Keeps names typed by users out of paths.
 */
[[nodiscard]] bool is_valid_spell_dictionary_name(const std::string& name);

/**
 * Picks the dictionary to use, the first one that is not empty of: the
 * user's override, the menu set's dictionary, the board default, en_US.
 * Names that are not valid are skipped.
 */
[[nodiscard]] std::string resolve_spell_dictionary(const std::string& user_dictionary,
                                                   const std::string& menu_set_dictionary,
                                                   const std::string& board_dictionary);

/** Combines the board, menu set and user settings. */
[[nodiscard]] spell_settings_t effective_spell_settings(const sdk::spell_config_t& board,
                                                        const std::string& menu_set_dictionary,
                                                        const spell_user_config_t& user);

/** Directory holding spell check data: {datadir}/spell */
[[nodiscard]] std::filesystem::path spell_data_dir(const std::filesystem::path& datadir);
/** {datadir}/spell/users/{usernum}.json */
[[nodiscard]] std::filesystem::path spell_user_config_path(const std::filesystem::path& datadir,
                                                           int usernum);
/** {datadir}/spell/users/{usernum}.dic */
[[nodiscard]] std::filesystem::path spell_user_words_path(const std::filesystem::path& datadir,
                                                          int usernum);
/** {datadir}/spell/board.dic */
[[nodiscard]] std::filesystem::path spell_board_words_path(const std::filesystem::path& datadir);

/**
 * Loads the user's settings. Returns the defaults when the file does not
 * exist or usernum is not a real user.
 */
[[nodiscard]] spell_user_config_t load_spell_user_config(const std::filesystem::path& datadir,
                                                         int usernum);
/** Saves the user's settings, creating data/spell/users if needed. */
bool save_spell_user_config(const std::filesystem::path& datadir, int usernum,
                            const spell_user_config_t& config);

/**
 * A list of words, one per line in UTF-8, as used for the personal
 * (data/spell/users/{usernum}.dic) and board (data/spell/board.dic) word
 * lists. Lines starting with '#' and blank lines are ignored.
 */
class SpellWordList {
public:
  explicit SpellWordList(std::filesystem::path path);

  // Loads the list. A missing file is an empty list and still succeeds.
  bool Load();
  // Saves the list, creating the directory if needed. Fails without writing
  // anything if the last Load failed, so that a list that could not be read
  // is not replaced by only the words added since.
  bool Save() const;
  // Adds a word. Returns false if the word is empty or already present.
  bool add(const std::string& word);
  [[nodiscard]] bool contains(const std::string& word) const;
  [[nodiscard]] const std::set<std::string>& words() const noexcept { return words_; }
  [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
  std::filesystem::path path_;
  std::set<std::string> words_;
  bool load_failed_{false};
};

} // namespace wwiv::fsed

#endif
