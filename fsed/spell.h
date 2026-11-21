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
 * The directories to search for dictionaries, in order:
 *   1. dict_dir (if not empty). A relative dict_dir is relative to bbsdir.
 *   2. {bbsdir}/dict
 *   3. On non-Windows platforms the system directories /usr/share/hunspell,
 *      /usr/local/share/hunspell, /usr/share/myspell/dicts and
 *      /usr/share/myspell. Windows has no system location, so dictionaries
 *      go in {bbsdir}\dict (or dict_dir).
 */
[[nodiscard]] std::vector<std::filesystem::path>
spell_dictionary_search_path(const std::string& dict_dir, const std::filesystem::path& bbsdir);

/**
 * Returns the string to pass to Hunspell for the file at utf8_path.
 *
 * On Windows, Hunspell opens files using the ANSI code page unless the path
 * is UTF-8 and starts with the long path prefix \\?\ (see hunspell.hxx).
 * When windows is true and utf8_path (an absolute path) is not plain ASCII
 * or is too long for MAX_PATH, the path is returned with '/' changed to
 * '\' and the \\?\ prefix (\\?\UNC\ for \\server\share paths).
 * Otherwise utf8_path is returned unchanged.
 *
 * windows is a parameter so that this can be tested on every platform.
 */
[[nodiscard]] std::string spell_hunspell_path(const std::string& utf8_path, bool windows);

/**
 * Finds {name}.aff and {name}.dic in the first directory of dirs that has
 * both of them.
 */
[[nodiscard]] std::optional<spell_dictionary_files_t>
find_spell_dictionary(const std::string& name, const std::vector<std::filesystem::path>& dirs);

/** True if p is a regular file that can be opened and has at least one byte. */
[[nodiscard]] bool spell_file_readable(const std::filesystem::path& p);

/**
 * Returns up to max_words words from the Hunspell .dic file dic (without
 * their /flags and morphology fields), in the dictionary's own encoding.
 * Returns nothing when the file can not be read, or when it does not start
 * with the entry count line every Hunspell .dic file has.
 * create_spell_dictionary uses these to check that Hunspell really loaded
 * the dictionary.
 */
[[nodiscard]] std::vector<std::string> spell_dic_sample_words(const std::filesystem::path& dic,
                                                              int max_words = 50);

/**
 * Creates the dictionary named name (i.e. "en_US") from the first directory
 * in dirs that contains it. Returns a NullSpellDictionary when Hunspell was
 * not compiled in, the dictionary can not be found, or it can not be loaded:
 * an unreadable .aff or .dic, a .dic without the entry count line, or a
 * dictionary that Hunspell loads with none of the .dic's own words in it.
 */
[[nodiscard]] std::unique_ptr<SpellDictionary>
create_spell_dictionary(const std::string& name, const std::vector<std::filesystem::path>& dirs);

/** Converts a CP437 word from the editor to UTF-8 for the dictionary. */
[[nodiscard]] std::string spell_to_utf8(const std::string& cp437);

/**
 * Converts a UTF-8 word from the dictionary (i.e. a suggestion) to CP437 for
 * the editor. Characters with no CP437 equivalent become '?'.
 */
[[nodiscard]] std::string spell_from_utf8(const std::string& utf8);

/**
 * Returns the words that are not in dict. Words are converted from CP437 to
 * UTF-8 before they are checked.
 */
[[nodiscard]] std::vector<spell_word_t> misspelled_words(const std::vector<spell_word_t>& words,
                                                         const SpellDictionary& dict);

} // namespace wwiv::fsed

#endif
