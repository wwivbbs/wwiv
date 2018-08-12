/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*               Copyright (C)2018, WWIV Software Services                */
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

#include <cstdint>
#include <vector>

namespace wwiv {
namespace sdk {
namespace ansi {

static constexpr int FRAMEBUFFER_DEFAULT_ATTRIBUTE = 0x07;

class FrameBufferCell {
public:
  FrameBufferCell();
  FrameBufferCell(FrameBufferCell& o) : a_(o.a_), c_(o.c_) {}
  FrameBufferCell(char c, uint8_t a);
  char c() const noexcept { return c_; }
  void c(char ch) { c_ = ch; }
  uint8_t a() const noexcept { return a_; }

  // As a composite word (high 8 bits are attribute, low are char)
  uint16_t w() const noexcept { return (a_ << 8) | c_; }

  FrameBufferCell& operator=(FrameBufferCell& o) {
    if (this == &o) {
      return *this;
    }
    a_ = o.a();
    c_ = o.c();
    return *this;
  }

private:
  char c_;
  uint8_t a_;
};

class FrameBuffer {
public:
  explicit FrameBuffer(int cols);
  /** Writes c using a, handles \r and \n */
  bool write(char c, uint8_t a);
  /** Moves the cursor to x,y */
  bool gotoxy(int x, int y);
  // Like CLS, clears everything viewable.
  bool clear();
  // Finalizes this frambuffer and shrinks the size to fit, etc.
  void close();

  /**
   * Puts c using a at cursor position pos.
   * Does not handle movement chars like \r or \n
   */
  bool put(int pos, char c, uint8_t a);

  bool write(char c) { return write(c, a_); }
  void set_attr(uint8_t a) { a_ = a; }

  int cols() const noexcept { return cols_; }
  int pos() const noexcept { return pos_; }
  uint8_t curatr() const noexcept { return a_; }
  int x() const noexcept { return pos_ % cols_; }
  int y() const noexcept { return pos_ / cols_; }

  // Mostly used for debugging and tests.

  // Number of total rows after the framebuffer has been closed.
  int rows() const { return b_.empty() ? 0 : (1 + (b_.size() / cols_)); }
  // The raw text (without attributes) for row # 'row'
  std::string row_as_text(int row) const;
  std::vector<uint16_t> row_char_and_attr(int row) const;

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
