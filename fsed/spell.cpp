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
#include "fsed/spell.h"

#include "core/cp437.h"
#include "core/file.h"
#include "core/log.h"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#ifdef WWIV_HAS_HUNSPELL
#include "hunspell.hxx"
#endif

namespace wwiv::fsed {

using namespace wwiv::core;

#ifdef WWIV_HAS_HUNSPELL

namespace {

bool is_ascii(const std::string& s) {
  for (const auto c : s) {
    if (static_cast<unsigned char>(c) > 0x7f) {
      return false;
    }
  }
  return true;
}

/**
 * Hunspell backed dictionary. Hunspell expects words in the encoding of the
 * dictionary (the SET line in the .aff file). Most dictionaries today are
 * UTF-8; for the ones that are not, only ASCII words are checked.
 */
class HunspellDictionary final : public SpellDictionary {
public:
  explicit HunspellDictionary(const spell_dictionary_files_t& files)
      : hunspell_(
            std::make_unique<Hunspell>(files.aff.string().c_str(), files.dic.string().c_str())) {
    utf8_ = hunspell_->get_dict_encoding() == "UTF-8";
  }
  ~HunspellDictionary() override = default;

  [[nodiscard]] bool available() const override { return true; }

  [[nodiscard]] bool check(const std::string& word) const override {
    if (!utf8_ && !is_ascii(word)) {
      return true;
    }
    return hunspell_->spell(word);
  }

  [[nodiscard]] std::vector<std::string> suggest(const std::string& word) const override {
    if (!utf8_ && !is_ascii(word)) {
      return {};
    }
    auto all = hunspell_->suggest(word);
    if (utf8_) {
      return all;
    }
    std::vector<std::string> out;
    for (auto& s : all) {
      if (is_ascii(s)) {
        out.emplace_back(std::move(s));
      }
    }
    return out;
  }

  bool add(const std::string& word) override {
    if (!utf8_ && !is_ascii(word)) {
      return false;
    }
    return hunspell_->add(word) == 0;
  }

private:
  std::unique_ptr<Hunspell> hunspell_;
  bool utf8_{true};
};

} // namespace

bool spell_check_compiled_in() { return true; }

#else

bool spell_check_compiled_in() { return false; }

#endif // WWIV_HAS_HUNSPELL

std::vector<std::filesystem::path>
spell_dictionary_search_path(const std::string& dict_dir, const std::filesystem::path& bbsdir) {
  std::vector<std::filesystem::path> dirs;
  if (!dict_dir.empty()) {
    dirs.emplace_back(dict_dir);
  }
  if (!bbsdir.empty()) {
    dirs.emplace_back(FilePath(bbsdir, "dict"));
  }
#ifndef _WIN32
  dirs.emplace_back("/usr/share/hunspell");
  dirs.emplace_back("/usr/share/myspell/dicts");
#endif
  return dirs;
}

std::optional<spell_dictionary_files_t>
find_spell_dictionary(const std::string& name, const std::vector<std::filesystem::path>& dirs) {
  if (name.empty()) {
    return std::nullopt;
  }
  for (const auto& dir : dirs) {
    const auto aff = FilePath(dir, name + ".aff");
    const auto dic = FilePath(dir, name + ".dic");
    if (File::Exists(aff) && File::Exists(dic)) {
      return spell_dictionary_files_t{aff, dic};
    }
  }
  return std::nullopt;
}

std::unique_ptr<SpellDictionary>
create_spell_dictionary(const std::string& name, const std::vector<std::filesystem::path>& dirs) {
#ifdef WWIV_HAS_HUNSPELL
  if (const auto files = find_spell_dictionary(name, dirs); files) {
    VLOG(1) << "Using spell check dictionary: " << files->dic.string();
    return std::make_unique<HunspellDictionary>(files.value());
  }
  LOG(INFO) << "Unable to find spell check dictionary: " << name;
#else
  // Hunspell was not compiled in, there is nothing to load.
  (void)name;
  (void)dirs;
#endif
  return std::make_unique<NullSpellDictionary>();
}

std::string spell_to_utf8(const std::string& in) {
  // This does not depend on the C locale the way
  // cp437_to_utf8(const std::string&) (wcstombs) does.
  std::string out;
  for (const auto ch : in) {
    const auto cp = static_cast<uint32_t>(cp437_to_utf8(static_cast<uint8_t>(ch)));
    if (cp < 0x80) {
      out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
      out.push_back(static_cast<char>(0xc0 | (cp >> 6)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
    } else {
      out.push_back(static_cast<char>(0xe0 | ((cp >> 12) & 0x0f)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3f)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
    }
  }
  return out;
}

std::string spell_from_utf8(const std::string& in) { return utf8_to_cp437(in); }

std::vector<spell_word_t> misspelled_words(const std::vector<spell_word_t>& words,
                                           const SpellDictionary& dict) {
  std::vector<spell_word_t> out;
  if (!dict.available()) {
    return out;
  }
  for (const auto& w : words) {
    if (!dict.check(spell_to_utf8(w.word))) {
      out.push_back(w);
    }
  }
  return out;
}

} // namespace wwiv::fsed
