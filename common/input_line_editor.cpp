/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                 Copyright (C)2026, WWIV Software Services              */
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
#include "common/input_line_editor.h"

#include "core/strings.h"
#include <algorithm>
#include <cstring>
#include <string>

using namespace wwiv::strings;

namespace wwiv::common {

static const std::string valid_letters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
static constexpr char date_separator = '/';
static constexpr char phone_separator = '-';

InputLineEditor::InputLineEditor(std::string text, int max_length, int width, InputMode mode,
                                 bool insert)
    : text_(std::move(text)), max_length_(std::max(0, max_length)), width_(std::max(1, width)),
      mode_(mode), insert_(insert) {
  if (mode_ == InputMode::DATE || mode_ == InputMode::PHONE) {
    insert_ = false;
  }
  end();
}

bool InputLineEditor::more_left_at(int offset) const noexcept { return arrows() && offset > 0; }

bool InputLineEditor::more_right_at(int offset) const noexcept {
  return arrows() && len() - offset > width_;
}

bool InputLineEditor::more_left() const noexcept { return more_left_at(offset_); }
bool InputLineEditor::more_right() const noexcept { return more_right_at(offset_); }

// Keeps the cursor inside the field, off the arrow cells. When it hits an
// edge the view jumps by a third of the field rather than one character, so
// typing past the edge doesn't redraw the whole field on every key. After a
// jump to the right, up to a third of the field may be empty past the end of
// the text.
void InputLineEditor::scroll_to_cursor() {
  // With arrows the jump must be at least 1 (to get the cursor off the left
  // arrow); without them, fields under 3 cells can't jump at all.
  const auto jump = arrows() ? std::max(1, width_ / 3) : width_ / 3;
  const auto min_cell = [this](int o) { return more_left_at(o) ? 1 : 0; };
  const auto max_cell = [this](int o) { return more_right_at(o) ? width_ - 2 : width_ - 1; };
  if (pos_ - offset_ < min_cell(offset_)) {
    offset_ = std::max(0, pos_ - jump);
  } else if (pos_ - offset_ > max_cell(offset_)) {
    offset_ = pos_ - (width_ - 1) + jump;
  }
}

InputLineEditor::Cell InputLineEditor::cell(int i) const {
  if (i == 0 && more_left()) {
    return {CellKind::more_left, more_left_char};
  }
  if (i == width_ - 1 && more_right()) {
    return {CellKind::more_right, more_right_char};
  }
  if (const auto idx = offset_ + i; idx < len()) {
    return {CellKind::text, text_[idx]};
  }
  return {CellKind::empty, 0};
}

void InputLineEditor::left() {
  if (mode_ != InputMode::DATE && mode_ != InputMode::PHONE && pos_ > 0) {
    --pos_;
    scroll_to_cursor();
  }
}

void InputLineEditor::right() {
  if (mode_ != InputMode::DATE && mode_ != InputMode::PHONE && pos_ < len()) {
    ++pos_;
    scroll_to_cursor();
  }
}

void InputLineEditor::home() {
  pos_ = 0;
  scroll_to_cursor();
}

void InputLineEditor::end() {
  pos_ = len();
  // Show as much text as fits, with the cursor in the last cell.
  offset_ = std::max(0, len() - (width_ - 1));
  scroll_to_cursor();
}

void InputLineEditor::clear() {
  text_.clear();
  pos_ = 0;
  offset_ = 0;
}

void InputLineEditor::toggle_insert() {
  // Historically only UPPER mode honors the Insert key.
  if (mode_ == InputMode::UPPER) {
    insert_ = !insert_;
  }
}

// After text is deleted, scrolls back so no text stays hidden to the left
// that would now fit.
void InputLineEditor::pull_back() {
  offset_ = std::min(offset_, std::max(0, len() - (width_ - 1)));
}

void InputLineEditor::del() {
  if (pos_ == len() || mode_ == InputMode::DATE || mode_ == InputMode::PHONE) {
    return;
  }
  text_.erase(pos_, 1);
  pull_back();
  scroll_to_cursor();
}

void InputLineEditor::backspace() {
  if (pos_ == 0) {
    return;
  }
  if (pos_ != len()) {
    if (mode_ != InputMode::DATE && mode_ != InputMode::PHONE) {
      text_.erase(--pos_, 1);
    }
  } else {
    text_.pop_back();
    pos_ = len();
    // Also remove the separator before the character just erased.
    if ((mode_ == InputMode::DATE && (pos_ == 2 || pos_ == 5)) ||
        (mode_ == InputMode::PHONE && (pos_ == 3 || pos_ == 7))) {
      text_.pop_back();
      pos_ = len();
    }
  }
  pull_back();
  scroll_to_cursor();
}

void InputLineEditor::insert_at(int pos, char ch) {
  text_.insert(text_.begin() + pos, ch);
}

void InputLineEditor::type(unsigned char ch) {
  if (ch < 32 || ch == 255) {
    return;
  }
  // Overwriting existing text is always allowed; anything that makes the text
  // longer needs room.
  const auto overwrite = !insert_ && pos_ < len();
  if (!overwrite && !can_grow()) {
    return;
  }
  if (mode_ != InputMode::MIXED && mode_ != InputMode::FILENAME && mode_ != InputMode::CMDLINE &&
      mode_ != InputMode::FULL_PATH_NAME) {
    ch = upcase(ch);
  }
  if (mode_ == InputMode::FILENAME && std::strchr("/\\<>|*?\";:", ch)) {
    return;
  }
  if (mode_ == InputMode::FULL_PATH_NAME && std::strchr("<>|*?\";", ch)) {
    return;
  }
  if (mode_ == InputMode::PROPER && pos_ > 0) {
    // Lowercase letters that don't start a word.
    if (valid_letters.find(static_cast<char>(ch)) != std::string::npos &&
        text_[pos_ - 1] != ' ') {
      ch = locase(ch);
    }
  }

  auto separator = '\0';
  if (mode_ == InputMode::DATE && (pos_ == 2 || pos_ == 5)) {
    separator = date_separator;
  } else if (mode_ == InputMode::PHONE && (pos_ == 3 || pos_ == 7)) {
    separator = phone_separator;
  }
  if (separator) {
    if (!can_grow()) {
      return;
    }
    insert_at(pos_++, separator);
    if (ch == static_cast<unsigned char>(separator)) {
      // The caller typed the separator themselves.
      scroll_to_cursor();
      return;
    }
    if (pos_ >= len() && !can_grow()) {
      scroll_to_cursor();
      return;
    }
  }

  if (!insert_ && pos_ < len()) {
    text_[pos_++] = static_cast<char>(ch);
  } else {
    insert_at(pos_++, static_cast<char>(ch));
  }
  scroll_to_cursor();
}

} // namespace wwiv::common
