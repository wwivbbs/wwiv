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
#include "fsed/spell_text.h"

#include "core/stl.h"
#include "fsed/model.h"
#include <string>
#include <vector>

namespace wwiv::fsed {

using namespace wwiv::stl;

namespace {

// A character with the color codes removed, along with the line and cell
// index where it came from.
struct spell_char_t {
  char ch;
  int line;
  int x;
};

// Character classes are spelled out for ASCII and CP437 rather than using
// std::isalpha/std::isdigit, which depend on the C locale and could treat
// bytes 0x80-0xFF differently from one system (or code page) to the next.
bool is_digit(char c) { return c >= '0' && c <= '9'; }
bool is_upper(char c) { return c >= 'A' && c <= 'Z'; }
bool is_lower(char c) { return c >= 'a' && c <= 'z'; }

bool is_color_code_char(char c) { return c == '#' || is_digit(c); }

// ASCII letters and the CP437 accented letters (0x80-0x9A and 0xA0-0xA5, not the
// currency symbols at 0x9B-0x9F) along with sharp s (0xE1).
bool is_letter(char c) {
  if (is_upper(c) || is_lower(c)) {
    return true;
  }
  const auto u = static_cast<unsigned char>(c);
  return (u >= 0x80 && u <= 0x9a) || (u >= 0xa0 && u <= 0xa5) || u == 0xe1;
}

bool is_word_char(char c) { return is_letter(c) || is_digit(c) || c == '\''; }

bool is_space(char c) {
  // Anything below a space (tabs, control characters) breaks words.
  return static_cast<unsigned char>(c) <= ' ';
}

// Removes heart codes, pipe codes and a trailing soft-wrap marker.
template <typename F> std::vector<spell_char_t> strip_codes(int line_num, int len, F char_at) {
  std::vector<spell_char_t> out;
  for (auto i = 0; i < len; i++) {
    const auto c = char_at(i);
    if (c == '\x03' && i + 1 < len && is_digit(char_at(i + 1))) {
      ++i;
      continue;
    }
    if (c == '|' && i + 2 < len && is_color_code_char(char_at(i + 1)) &&
        is_color_code_char(char_at(i + 2))) {
      i += 2;
      continue;
    }
    if (c == '\x01' && i + 1 == len) {
      // WWIV soft-wrap marker.
      continue;
    }
    out.push_back(spell_char_t{c, line_num, i});
  }
  return out;
}

std::vector<spell_char_t> strip_line(const line_t& line, int line_num) {
  const auto& cells = line.cells();
  return strip_codes(line_num, size_int(cells), [&](int i) { return cells[i].ch; });
}

bool is_wrapped(const line_t& line) {
  return line.wrapped() || (!line.cells().empty() && line.cells().back().ch == '\x01');
}

bool starts_with(const std::string& s, const std::string& prefix) {
  return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

std::string to_lower_ascii(std::string s) {
  for (auto& c : s) {
    if (is_upper(c)) {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return s;
}

// True if a whitespace delimited chunk of text looks like a URL, email
// address, path, file name, domain name or abbreviation like "e.g.".
bool is_unchecked_chunk(const std::string& chunk) {
  if (chunk.find_first_of("/\\@_") != std::string::npos) {
    // URLs (http://), paths, emails and identifiers.
    return true;
  }
  if (starts_with(to_lower_ascii(chunk), "www.")) {
    return true;
  }
  // A dot with a letter or digit on both sides: example.com, readme.txt, e.g.
  for (auto i = 1; i + 1 < size_int(chunk); i++) {
    if (chunk[i] == '.' && is_word_char(chunk[i - 1]) && is_word_char(chunk[i + 1])) {
      return true;
    }
  }
  return false;
}

bool is_checked_word(const std::string& word, const spell_tokenizer_options_t& options) {
  auto letters = 0;
  auto has_upper = false;
  auto has_lower = false;
  auto upper_after_first = false;
  for (auto i = 0; i < size_int(word); i++) {
    const auto c = word[i];
    if (is_digit(c)) {
      return false;
    }
    if (!is_letter(c)) {
      continue;
    }
    ++letters;
    if (is_upper(c)) {
      has_upper = true;
      if (i > 0) {
        upper_after_first = true;
      }
    } else if (is_lower(c)) {
      has_lower = true;
    }
  }
  if (letters < options.min_word_length) {
    return false;
  }
  if (options.skip_all_caps && has_upper && !has_lower) {
    return false;
  }
  if (options.skip_mixed_case && has_lower && upper_after_first) {
    return false;
  }
  return true;
}

// Finds the words in one whitespace delimited chunk [start, end) of a paragraph.
void tokenize_chunk(const std::vector<spell_char_t>& chars, int start, int end,
                    const spell_tokenizer_options_t& options, std::vector<spell_word_t>& out) {
  std::string chunk;
  for (auto i = start; i < end; i++) {
    chunk.push_back(chars[i].ch);
  }
  if (is_unchecked_chunk(chunk)) {
    return;
  }
  auto i = start;
  while (i < end) {
    if (!is_word_char(chars[i].ch)) {
      ++i;
      continue;
    }
    auto word_start = i;
    while (i < end && is_word_char(chars[i].ch)) {
      ++i;
    }
    auto word_end = i;
    // Apostrophes are only part of a word when they are inside of it
    // (don't, Frank's), not quotes around it ('word' or dogs').
    while (word_start < word_end && chars[word_start].ch == '\'') {
      ++word_start;
    }
    while (word_end > word_start && chars[word_end - 1].ch == '\'') {
      --word_end;
    }
    if (word_start == word_end) {
      continue;
    }
    spell_word_t w;
    for (auto j = word_start; j < word_end; j++) {
      w.word.push_back(chars[j].ch);
    }
    if (!is_checked_word(w.word, options)) {
      continue;
    }
    w.line = chars[word_start].line;
    w.x = chars[word_start].x;
    w.end_line = chars[word_end - 1].line;
    w.end_x = chars[word_end - 1].x + 1;
    out.emplace_back(std::move(w));
  }
}

// Splits the joined characters of a paragraph into words.
void tokenize_paragraph(const std::vector<spell_char_t>& chars,
                        const spell_tokenizer_options_t& options, std::vector<spell_word_t>& out) {
  const auto len = size_int(chars);
  auto i = 0;
  while (i < len) {
    if (is_space(chars[i].ch)) {
      ++i;
      continue;
    }
    const auto start = i;
    while (i < len && !is_space(chars[i].ch)) {
      ++i;
    }
    tokenize_chunk(chars, start, i, options, out);
  }
}

bool is_skip_line(const line_t& line) {
  std::string text;
  for (const auto& c : strip_line(line, 0)) {
    text.push_back(c.ch);
  }
  return is_spell_skip_line(text);
}

template <typename F>
std::vector<spell_word_t> tokenize(int num_lines, F line_at,
                                   const spell_tokenizer_options_t& options) {
  std::vector<spell_word_t> out;
  // The characters of the current soft-wrapped paragraph. The lines are
  // joined the same way FsedModel::to_lines(false) (when the message is
  // saved) and the editor's paragraph reflow join them: when neither side of
  // the line break is whitespace, there is a space between them. So a
  // character-wrapped "hello" / "world" is checked as the two words that are
  // saved ("hello world"), not as "helloworld", and a word never spans two
  // lines.
  std::vector<spell_char_t> paragraph;
  auto start_of_paragraph = true;
  auto skip_paragraph = false;
  for (auto i = 0; i < num_lines; i++) {
    const auto& line = line_at(i);
    if (start_of_paragraph) {
      skip_paragraph = is_skip_line(line);
      paragraph.clear();
    }
    if (!skip_paragraph) {
      const auto chars = strip_line(line, i);
      if (!paragraph.empty() && !chars.empty() && !is_space(paragraph.back().ch) &&
          !is_space(chars.front().ch)) {
        // The space the saved message has here. It is never part of a word,
        // so its position does not matter.
        paragraph.push_back(spell_char_t{' ', i, 0});
      }
      paragraph.insert(paragraph.end(), chars.begin(), chars.end());
    }
    start_of_paragraph = !is_wrapped(line);
    if (start_of_paragraph && !skip_paragraph) {
      tokenize_paragraph(paragraph, options, out);
    }
  }
  if (!start_of_paragraph && !skip_paragraph) {
    // The last line was marked as wrapped.
    tokenize_paragraph(paragraph, options, out);
  }
  return out;
}

} // namespace

bool is_spell_skip_line(const std::string& text) {
  if (text.empty()) {
    return false;
  }
  if (text.front() == '\x01' || text.front() == '\x04') {
    // WWIV control lines and FidoNet kludges.
    return true;
  }
  std::string::size_type i = 0;
  while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) {
    ++i;
  }
  const auto t = text.substr(i);
  // Signature separators ("--" and "-- ") and tag lines ("---" and longer).
  if (t == "--" || t == "-- " || starts_with(t, "---")) {
    return true;
  }
  if (starts_with(t, "* Origin:") || starts_with(t, "SEEN-BY:")) {
    return true;
  }
  // Quotes: "> text", ">> text", and WWIV's "FR> text" with up to four
  // characters of initials.
  std::string::size_type q = 0;
  while (q < t.size() && q < 4 && is_letter(t[q])) {
    ++q;
  }
  return q < t.size() && t[q] == '>';
}

std::string spell_strip_codes(const std::string& text) {
  const auto chars = strip_codes(0, size_int(text), [&](int i) { return text[i]; });
  std::string out;
  for (const auto& c : chars) {
    out.push_back(c.ch);
  }
  return out;
}

std::vector<spell_word_t> spell_tokenize(const std::vector<line_t>& lines,
                                         const spell_tokenizer_options_t& options) {
  return tokenize(size_int(lines), [&](int i) -> const line_t& { return lines[i]; }, options);
}

std::vector<spell_word_t> spell_tokenize(const std::vector<line_t>& lines) {
  return spell_tokenize(lines, spell_tokenizer_options_t{});
}

std::vector<spell_word_t> spell_tokenize(const FsedModel& ed,
                                         const spell_tokenizer_options_t& options) {
  return tokenize(size_int(ed), [&](int i) -> const line_t& { return ed.line(i); }, options);
}

std::vector<spell_word_t> spell_tokenize(const FsedModel& ed) {
  return spell_tokenize(ed, spell_tokenizer_options_t{});
}

} // namespace wwiv::fsed
