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
#include "core/strings.h"
#include <algorithm>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#ifdef WWIV_HAS_HUNSPELL
#include "hunspell.hxx"
#endif

namespace wwiv::fsed {

using namespace wwiv::core;

namespace {

bool is_ascii(const std::string& s) {
  for (const auto c : s) {
    if (static_cast<unsigned char>(c) > 0x7f) {
      return false;
    }
  }
  return true;
}

bool is_ascii_letter(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }

// UTF-8 form of p. path::u8string() returns std::u8string in C++20. Unlike
// path::string(), this never depends on (or throws because of) the Windows
// ANSI code page, so it is safe for logging any path.
std::string path_to_utf8(const std::filesystem::path& p) {
  const auto u8 = p.u8string();
  return std::string(u8.begin(), u8.end());
}

} // namespace

#ifdef WWIV_HAS_HUNSPELL

namespace {

// The path to give Hunspell for p. See spell_hunspell_path.
std::string hunspell_path(const std::filesystem::path& p) {
#ifdef _WIN32
  std::error_code ec;
  auto abs = std::filesystem::absolute(p, ec);
  if (ec) {
    abs = p;
  }
  return spell_hunspell_path(path_to_utf8(abs.lexically_normal()), true);
#else
  return spell_hunspell_path(path_to_utf8(p), false);
#endif
}

/**
 * Hunspell backed dictionary. Hunspell expects words in the encoding of the
 * dictionary (the SET line in the .aff file). Most dictionaries today are
 * UTF-8; for the ones that are not, only ASCII words are checked.
 */
class HunspellDictionary final : public SpellDictionary {
public:
  explicit HunspellDictionary(const spell_dictionary_files_t& files)
      : hunspell_(std::make_unique<Hunspell>(hunspell_path(files.aff).c_str(),
                                             hunspell_path(files.dic).c_str())) {
    utf8_ = hunspell_->get_dict_encoding() == "UTF-8";
  }
  ~HunspellDictionary() override = default;

  [[nodiscard]] bool available() const override { return true; }

  /**
   * Hunspell never reports a failed load: with a missing, unreadable or
   * corrupt .dic file it just has no words and every word is misspelled.
   * So check that it accepts at least one of the words listed in the .dic
   * file (sample_words, from spell_dic_sample_words). Some entries can be
   * forbidden or affix-only forms, which is why there is more than one.
   */
  [[nodiscard]] bool accepts_any(const std::vector<std::string>& sample_words) const {
    return std::any_of(sample_words.begin(), sample_words.end(),
                       [this](const std::string& w) { return hunspell_->spell(w); });
  }

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
    // A relative dict_dir (i.e. "dict\\en") is relative to the BBS directory,
    // not to whatever the current directory happens to be.
    const std::filesystem::path d{dict_dir};
    dirs.emplace_back(bbsdir.empty() ? d : File::absolute(bbsdir, d));
  }
  if (!bbsdir.empty()) {
    dirs.emplace_back(FilePath(bbsdir, "dict"));
  }
#ifndef _WIN32
  dirs.emplace_back("/usr/share/hunspell");
  dirs.emplace_back("/usr/local/share/hunspell");
  dirs.emplace_back("/usr/share/myspell/dicts");
  dirs.emplace_back("/usr/share/myspell");
#endif
  return dirs;
}

std::string spell_hunspell_path(const std::string& utf8_path, bool windows) {
  // MAX_PATH is 260, including the terminating NUL.
  static constexpr std::string::size_type kWindowsMaxPath = 259;
  static const std::string kLongPathPrefix = R"(\\?\)";
  if (!windows || utf8_path.empty()) {
    return utf8_path;
  }
  if (utf8_path.rfind(kLongPathPrefix, 0) == 0) {
    // Already has the prefix.
    return utf8_path;
  }
  if (is_ascii(utf8_path) && utf8_path.size() <= kWindowsMaxPath) {
    // fopen with the ANSI code page handles this just fine.
    return utf8_path;
  }
  auto p = utf8_path;
  std::replace(p.begin(), p.end(), '/', '\\');
  if (p.rfind(R"(\\)", 0) == 0) {
    // \\server\share\dir -> \\?\UNC\server\share\dir
    return wwiv::strings::StrCat(kLongPathPrefix, "UNC\\", p.substr(2));
  }
  if (p.size() >= 3 && is_ascii_letter(p[0]) && p[1] == ':' &&
      p[2] == '\\') {
    // C:\dir
    return wwiv::strings::StrCat(kLongPathPrefix, p);
  }
  // Not an absolute path, the prefix only works with absolute paths.
  return utf8_path;
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

bool spell_file_readable(const std::filesystem::path& p) {
  std::error_code ec;
  if (!std::filesystem::is_regular_file(p, ec) || ec) {
    return false;
  }
  // std::ifstream takes the path as a wide string on Windows.
  std::ifstream f(p, std::ios::binary);
  char c;
  return f.good() && f.get(c).good();
}

std::vector<std::string> spell_dic_sample_words(const std::filesystem::path& dic, int max_words) {
  std::vector<std::string> words;
  std::error_code ec;
  if (!std::filesystem::is_regular_file(dic, ec) || ec) {
    return words;
  }
  std::ifstream f(dic, std::ios::binary);
  if (!f) {
    return words;
  }
  std::string line;
  // The first line is the approximate number of entries. Some files start
  // with a UTF-8 byte order mark.
  if (!std::getline(f, line)) {
    return words;
  }
  if (line.rfind("\xEF\xBB\xBF", 0) == 0) {
    line.erase(0, 3);
  }
  std::string::size_type i = 0;
  while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
    ++i;
  }
  const auto digits_start = i;
  while (i < line.size() && line[i] >= '0' && line[i] <= '9') {
    ++i;
  }
  if (i == digits_start) {
    // Not a Hunspell/MySpell .dic file (or a binary or empty one).
    return words;
  }
  while (static_cast<int>(words.size()) < max_words && std::getline(f, line)) {
    // Strip every trailing CR: a file written in text mode on Windows can
    // end its lines with CR CR LF.
    while (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty() || line.front() == '\t' || line.front() == '/') {
      // Blank lines, comments (tab) and entries with no word.
      continue;
    }
    // word[/flags][ morphology]. A backslash escapes a slash in the word.
    std::string w;
    for (std::string::size_type j = 0; j < line.size(); j++) {
      const auto c = line[j];
      if (c == '\\' && j + 1 < line.size() && line[j + 1] == '/') {
        w.push_back('/');
        ++j;
        continue;
      }
      if (c == '/' || c == '\t' || c == ' ' || c == '\r') {
        break;
      }
      w.push_back(c);
    }
    if (!w.empty()) {
      words.push_back(w);
    }
  }
  return words;
}

std::unique_ptr<SpellDictionary>
create_spell_dictionary(const std::string& name, const std::vector<std::filesystem::path>& dirs) {
#ifdef WWIV_HAS_HUNSPELL
  if (const auto files = find_spell_dictionary(name, dirs); files) {
    const auto dic = path_to_utf8(files->dic);
    if (!spell_file_readable(files->aff)) {
      LOG(WARNING) << "Unable to read spell check dictionary: " << path_to_utf8(files->aff);
      return std::make_unique<NullSpellDictionary>();
    }
    const auto sample = spell_dic_sample_words(files->dic);
    if (sample.empty()) {
      LOG(WARNING) << "Spell check dictionary is unreadable or not a Hunspell .dic file: " << dic;
      return std::make_unique<NullSpellDictionary>();
    }
    try {
      auto d = std::make_unique<HunspellDictionary>(files.value());
      if (!d->accepts_any(sample)) {
        LOG(WARNING) << "Spell check dictionary did not load (corrupt .dic or .aff?): " << dic;
        return std::make_unique<NullSpellDictionary>();
      }
      VLOG(1) << "Using spell check dictionary: " << dic;
      return d;
    } catch (const std::exception& e) {
      LOG(WARNING) << "Unable to load spell check dictionary: " << dic << ": " << e.what();
      return std::make_unique<NullSpellDictionary>();
    }
  }
  std::string searched;
  for (const auto& d : dirs) {
    if (!searched.empty()) {
      searched.append("; ");
    }
    searched.append(path_to_utf8(d));
  }
  LOG(INFO) << "Unable to find spell check dictionary: " << name << ".aff/.dic in: " << searched;
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
