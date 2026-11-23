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
#ifndef INCLUDED_COMMON_INPUT_LINE_EDITOR_H
#define INCLUDED_COMMON_INPUT_LINE_EDITOR_H

#include <string>

namespace wwiv::common {

enum class InputMode { UPPER, MIXED, PROPER, FILENAME, FULL_PATH_NAME, CMDLINE, DATE, PHONE };

/**
 * The editing state of Input1's one-row ANSI input field, with horizontal
 * scrolling. Text longer than the field scrolls; a cell at either edge then
 * shows an arrow ('«' or '»') in place of the hidden text on that side.
 *
 * This holds no I/O, so it can be tested directly; Input1 maps keys onto it
 * and draws cell() for each cell of the field.
 */
class InputLineEditor final {
public:
  /**
   * text may already be longer than max_length (e.g. a value edited outside
   * the BBS): it is shown and can be edited, but not made longer.
   * width is the number of screen cells in the field.
   */
  InputLineEditor(std::string text, int max_length, int width, InputMode mode, bool insert);

  // Editing keys.
  void left();
  void right();
  void home();
  void end();
  void backspace();
  void del();
  void clear();
  void toggle_insert();
  /** Types ch, applying the mode's filtering and formatting. */
  void type(unsigned char ch);

  /** What a cell of the field shows. */
  enum class CellKind { text, empty, more_left, more_right };
  struct Cell {
    CellKind kind{CellKind::empty};
    char ch{0};
    bool operator==(const Cell& o) const { return kind == o.kind && ch == o.ch; }
    bool operator!=(const Cell& o) const { return !(*this == o); }
  };
  [[nodiscard]] Cell cell(int i) const;

  [[nodiscard]] const std::string& text() const noexcept { return text_; }
  [[nodiscard]] int pos() const noexcept { return pos_; }
  [[nodiscard]] int width() const noexcept { return width_; }
  [[nodiscard]] int offset() const noexcept { return offset_; }
  [[nodiscard]] bool insert() const noexcept { return insert_; }
  /** The cell the cursor is in. */
  [[nodiscard]] int cursor_cell() const noexcept { return pos_ - offset_; }
  /** true when text is hidden to the left of the field. */
  [[nodiscard]] bool more_left() const noexcept;
  /** true when text is hidden to the right of the field. */
  [[nodiscard]] bool more_right() const noexcept;

  /** Arrows drawn at the field's edges when text is scrolled off. */
  static constexpr char more_left_char = '\xAE';  // « in CP437
  static constexpr char more_right_char = '\xAF'; // » in CP437

private:
  [[nodiscard]] int len() const noexcept { return static_cast<int>(text_.size()); }
  [[nodiscard]] bool can_grow() const noexcept { return len() < max_length_; }
  [[nodiscard]] bool arrows() const noexcept { return width_ >= 4; }
  [[nodiscard]] bool more_left_at(int offset) const noexcept;
  [[nodiscard]] bool more_right_at(int offset) const noexcept;
  void insert_at(int pos, char ch);
  void scroll_to_cursor();
  void pull_back();

  std::string text_;
  const int max_length_;
  const int width_;
  const InputMode mode_;
  bool insert_;
  int pos_{0};
  int offset_{0};
};

} // namespace wwiv::common

#endif
