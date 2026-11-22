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
#ifndef INCLUDED_FSED_SPELL_TEXT_H
#define INCLUDED_FSED_SPELL_TEXT_H

#include "fsed/line.h"
#include <string>
#include <vector>

namespace wwiv::fsed {

class FsedModel;

/**
 * A single word found in the editor that should be spell checked.
 *
 * Soft-wrapped lines are joined before splitting into words. Like the saved
 * message, a character wrapped "hello" / "world" (no space at the end of the
 * line) is two words, so a word never spans two lines: end_line == line.
 */
struct spell_word_t {
  // Text of the word in CP437 with any color codes removed.
  std::string word;
  // Line number (index into the editor's lines) of the first character.
  int line{0};
  // Cell index of the first character on that line.
  int x{0};
  // Line number of the last character. Always the same as line, since the
  // tokenizer puts a space between soft-wrapped lines.
  int end_line{0};
  // Cell index one past the last character on end_line. When a pipe code
  // is inside of the word this is more than x + word.size().
  int end_x{0};
};

struct spell_tokenizer_options_t {
  // Words shorter than this many letters are not checked.
  int min_word_length{2};
  // Skip words in ALL CAPS (BBS, WWIV, FTN, ...)
  bool skip_all_caps{true};
  // Skip words with a capital after the first letter (SyncTERM, iPhone).
  bool skip_mixed_case{true};
};

/**
 * Returns true if the line (with color codes already removed) should never
 * be spell checked: quoted text ("FR> text" or "> text"), signature and
 * tag lines ("--", "---"), FidoNet origin and SEEN-BY lines, and WWIV
 * control lines starting with ^A or ^D.
 */
[[nodiscard]] bool is_spell_skip_line(const std::string& text);

/**
 * Removes WWIV heart codes (^C + digit), pipe color codes (|#n, |nn) and
 * a trailing soft-wrap marker (^A) from the text of a line. A ^A at the
 * start of the line (a kludge line) is kept.
 */
[[nodiscard]] std::string spell_strip_codes(const std::string& text);

/**
 * Splits the lines into words that should be spell checked.
 *
 * A soft-wrapped run of lines (line_t::wrapped() or a trailing ^A) is one
 * paragraph, and the skip line rules for the first line of the paragraph
 * apply to all of it. URLs, email addresses, paths, file and domain names
 * and words containing digits are never returned.
 */
[[nodiscard]] std::vector<spell_word_t> spell_tokenize(const std::vector<line_t>& lines,
                                                       const spell_tokenizer_options_t& options);
[[nodiscard]] std::vector<spell_word_t> spell_tokenize(const std::vector<line_t>& lines);

/** Splits the text in the editor into the words that should be spell checked. */
[[nodiscard]] std::vector<spell_word_t> spell_tokenize(const FsedModel& ed,
                                                       const spell_tokenizer_options_t& options);
[[nodiscard]] std::vector<spell_word_t> spell_tokenize(const FsedModel& ed);

} // namespace wwiv::fsed

#endif
