/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*             Copyright (C)2018-2021, WWIV Software Services             */
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
/**************************************************************************/
#ifndef __INCLUDED_SDK_FRAMEBUFFER_H__
#define __INCLUDED_SDK_FRAMEBUFFER_H__

#include "sdk/ansi/vscreen.h"
#include <cstdint>
#include <string>
#include <vector>

namespace wwiv {
namespace sdk {
namespace ansi {

static constexpr int FRAMEBUFFER_DEFAULT_ATTRIBUTE = 0x07;

class FrameBufferCell {
public:
  FrameBufferCell();
  FrameBufferCell(char c, uint8_t a);
  [[nodiscard]] char c() const noexcept { return c_; }
  void c(char ch) { c_ = ch; }
  [[nodiscard]] uint8_t a() const noexcept { return a_; }
  void a(uint8_t aa) { a_ = aa; }

  // As a composite word (high 8 bits are attribute, low are char)
  [[nodiscard]] uint16_t w() const noexcept { return (a_ << 8) | (c_ & 0xff); }

private:
  uint8_t a_;
  char c_;
};

class FrameBuffer : public VScreen {
public:
  explicit FrameBuffer(int cols);
  /** Writes c using a, handles \r and \n */
  bool write(char c, uint8_t a) override;
  /** Moves the cursor to x,y */
  bool gotoxy(int x, int y) override;
  // Like CLS, clears everything viewable.
  bool clear() override;
  // clears from current position to the end of line.
  bool clear_eol() override;
  // Finalizes this frambuffer and shrinks the size to fit, etc.
  void close() override;

  /**
   * Puts c using a at cursor position pos.
   * Does not handle movement chars like \r or \n
   */
  bool put(int pos, char c, uint8_t a) override;

  bool write(char c) override { return write(c, a_); }
  void curatr(uint8_t a) override { a_ = a; }

  [[nodiscard]] int cols() const noexcept override { return cols_; }
  [[nodiscard]] int pos() const noexcept override { return pos_; }
  [[nodiscard]] uint8_t curatr() const noexcept override { return a_; }
  [[nodiscard]] int x() const noexcept override { return pos_ % cols_; }
  [[nodiscard]] int y() const noexcept override { return pos_ / cols_; }

  // Mostly used for debugging and tests.

  // Number of total rows after the framebuffer has been closed.
  [[nodiscard]] int rows() const;
  // The raw text (without attributes) for row # 'row'
  [[nodiscard]] std::string row_as_text(int row) const;
  [[nodiscard]] std::vector<uint16_t> row_char_and_attr(int row) const;
  [[nodiscard]] std::vector<std::string> to_screen_as_lines() const;

private:
  bool grow(int pos);
  const int cols_;
  std::vector<FrameBufferCell> b_;
  uint8_t a_{7};
  int pos_{0};
  bool open_{true};
};

} // namespace ansi
} // namespace sdk
} // namespace wwiv

#endif // __INCLUDED_SDK_FRAMEBUFFER_H__
