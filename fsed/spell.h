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
#ifndef INCLUDED_FSED_SPELL_H
#define INCLUDED_FSED_SPELL_H

#include "fsed/spell_text.h"
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace wwiv::fsed {

/**
 * A dictionary used to spell check the text in the full screen editor.
 *
 * All words passed in and returned are UTF-8. Use the NullSpellDictionary
 * (or a fake in tests) when Hunspell is not available.
 */
class SpellDictionary {
public:
  SpellDictionary() = default;
  SpellDictionary(const SpellDictionary&) = delete;
  SpellDictionary& operator=(const SpellDictionary&) = delete;
  virtual ~SpellDictionary() = default;

  // True if a dictionary was loaded and words can be checked.
  [[nodiscard]] virtual bool available() const = 0;
  // True if word is spelled correctly (or can not be checked).
  [[nodiscard]] virtual bool check(const std::string& word) const = 0;
  // Suggested replacements for word, best first.
  [[nodiscard]] virtual std::vector<std::string> suggest(const std::string& word) const = 0;
  // Adds word to the dictionary for the rest of this session.
  virtual bool add(const std::string& word) = 0;
};

/**
 * Dictionary used when Hunspell was not compiled in or when no dictionary
 * files could be found. Every word is spelled correctly.
 */
class NullSpellDictionary final : public SpellDictionary {
public:
  NullSpellDictionary() = default;
  ~NullSpellDictionary() override = default;

  [[nodiscard]] bool available() const override { return false; }
  [[nodiscard]] bool check(const std::string&) const override { return true; }
  [[nodiscard]] std::vector<std::string> suggest(const std::string&) const override { return {}; }
  bool add(const std::string&) override { return false; }
};

/** The .aff and .dic files for a Hunspell dictionary. */
struct spell_dictionary_files_t {
  std::filesystem::path aff;
  std::filesystem::path dic;
};

/** True if this build of WWIV was compiled with Hunspell (WWIV_HUNSPELL). */
[[nodiscard]] bool spell_check_compiled_in();

/**
 * The directories to search for dictionaries, in order: dict_dir (if not
 * empty), {bbsdir}/dict, then on non-Windows platforms the system
 * directories /usr/share/hunspell and /usr/share/myspell/dicts.
 */
[[nodiscard]] std::vector<std::filesystem::path>
spell_dictionary_search_path(const std::string& dict_dir, const std::filesystem::path& bbsdir);

/**
 * Finds {name}.aff and {name}.dic in the first directory of dirs that has
 * both of them.
 */
[[nodiscard]] std::optional<spell_dictionary_files_t>
find_spell_dictionary(const std::string& name, const std::vector<std::filesystem::path>& dirs);

/**
 * Creates the dictionary named name (i.e. "en_US") from the first directory
 * in dirs that contains it. Returns a NullSpellDictionary when Hunspell was
 * not compiled in or the dictionary can not be found.
 */
[[nodiscard]] std::unique_ptr<SpellDictionary>
create_spell_dictionary(const std::string& name, const std::vector<std::filesystem::path>& dirs);

/**
 * Returns the words that are not in dict. Words are converted from CP437 to
 * UTF-8 before they are checked.
 */
[[nodiscard]] std::vector<spell_word_t> misspelled_words(const std::vector<spell_word_t>& words,
                                                         const SpellDictionary& dict);

} // namespace wwiv::fsed

#endif
