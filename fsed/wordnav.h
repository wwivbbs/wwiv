/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*              Copyright (C)2020-2022, WWIV Software Services            */
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
#ifndef INCLUDED_FSED_WORDNAV_H
#define INCLUDED_FSED_WORDNAV_H

#include <string>
#include <vector>

namespace wwiv::fsed {

// A cursor position in a list of lines. x may equal the line length,
// which represents the gap at the end of the line (the line break).
struct word_pos_t {
  int line{0};
  int x{0};
};

inline bool is_word_space(char c) noexcept { return c == ' ' || c == '\t'; }

inline int line_len(const std::vector<std::string>& lines, int line) {
  if (line < 0 || line >= static_cast<int>(lines.size())) {
    return 0;
  }
  return static_cast<int>(lines[line].size());
}

inline bool at_bof(word_pos_t p) noexcept { return p.line <= 0 && p.x <= 0; }

inline bool at_eof(const std::vector<std::string>& lines, word_pos_t p) {
  if (lines.empty()) {
    return true;
  }
  const int last = static_cast<int>(lines.size()) - 1;
  return p.line >= last && p.x >= line_len(lines, last);
}

// The gap at the end of a line (the line break) counts as whitespace
// so word movement crosses lines the same way it crosses spaces.
inline bool is_space_at(const std::vector<std::string>& lines, word_pos_t p) {
  if (p.line < 0 || p.line >= static_cast<int>(lines.size())) {
    return true;
  }
  if (p.x >= line_len(lines, p.line)) {
    return true;
  }
  if (p.x < 0) {
    return true;
  }
  return is_word_space(lines[p.line][p.x]);
}

inline word_pos_t move_left(const std::vector<std::string>& lines, word_pos_t p) {
  if (p.x > 0) {
    --p.x;
    return p;
  }
  if (p.line > 0) {
    --p.line;
    p.x = line_len(lines, p.line);
  }
  return p;
}

inline word_pos_t move_right(const std::vector<std::string>& lines, word_pos_t p) {
  if (p.x < line_len(lines, p.line)) {
    ++p.x;
    return p;
  }
  if (p.line + 1 < static_cast<int>(lines.size())) {
    ++p.line;
    p.x = 0;
  }
  return p;
}

// Start of the previous word, or the start of the document.
inline word_pos_t word_left(const std::vector<std::string>& lines, word_pos_t p) {
  if (lines.empty() || at_bof(p)) {
    return {0, 0};
  }
  p = move_left(lines, p);
  while (!at_bof(p) && is_space_at(lines, p)) {
    p = move_left(lines, p);
  }
  while (!at_bof(p)) {
    const auto left = move_left(lines, p);
    if (is_space_at(lines, left)) {
      break;
    }
    p = left;
  }
  return p;
}

// Start of the next word, or the end of the document.
inline word_pos_t word_right(const std::vector<std::string>& lines, word_pos_t p) {
  if (at_eof(lines, p)) {
    return p;
  }
  while (!at_eof(lines, p) && !is_space_at(lines, p)) {
    p = move_right(lines, p);
  }
  while (!at_eof(lines, p) && is_space_at(lines, p)) {
    p = move_right(lines, p);
  }
  return p;
}

}

#endif
