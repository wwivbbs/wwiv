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
#include "sdk/ansi/framebuffer.h"

#include "core/stl.h"
#include <algorithm>
#include "sdk/ansi/makeansi.h"

using namespace wwiv::stl;

namespace wwiv::sdk::ansi {

FrameBufferCell::FrameBufferCell() : FrameBufferCell(0, FRAMEBUFFER_DEFAULT_ATTRIBUTE) {}

FrameBufferCell::FrameBufferCell(char c, uint8_t a) : a_(a), c_(c) {}

FrameBuffer::FrameBuffer(int cols) : VScreen(), cols_(cols) { b_.reserve(cols_ * 100); }

bool FrameBuffer::gotoxy(int x, int y) {
  const auto xx = std::max(x, 0);
  const auto yy = std::max(y, 0);
  pos_ = (yy * cols_) + xx;
  if (pos_ >= size_int(b_)) {
    return grow(pos_);
  }
  return true;
}

bool FrameBuffer::clear() {
  b_.clear();
  pos_ = 0;
  return true;
}

bool FrameBuffer::clear_eol() {
  const auto start = pos_;
  const auto end = (y() + 1) * cols_;
  const auto a = curatr();
  for (auto i = start; i < end; i++) {
    put(i, ' ', a);
  }
  pos_ = end;
  return true;
}

bool FrameBuffer::put(int pos, char c, uint8_t a) {
  if (pos >= size_int(b_)) {
    if (!grow(pos)) {
      return false;
    }
  }
  auto& b = b_[pos];
  b.c(c);
  b.a(a);
  return true;
}

bool FrameBuffer::write(char c, uint8_t a) {
  switch (c) {
  case 0: // NOP
    break;
  case '\r': {
    const auto line = y();
    return gotoxy(0, line);
  }
  case '\n': {
    const auto line = y() + 1;
    return gotoxy(0, line);
  }
  default: {
    return put(pos_++, c, a);
  }
  }

  return true;
}

void FrameBuffer::close() {
  open_ = false;
  if (b_.empty()) {
    return;
  }
  for (auto i = size_int(b_) - 1; i >= 0;i--) {
    if (b_[i].c() != 0) {
      // Were at the end.
      b_.resize(i + 1);
      break;
    }
  }

  auto f = false;
  for (auto i = size_int(b_) - 1; i >= 0; i--) {
    if (f && b_[i].c() == 0) {
      b_[i].c(' ');
    } else if (!f && b_[i].c() != 0) {
      f = true;
    }
    if ((i % cols_) == 0) {
      f = false;
    }
  }

}

bool FrameBuffer::grow(int pos) {
  const auto e = std::max(pos + 1000, (y() + 1) * cols_);
  b_.resize(e);
  return true;
}

int FrameBuffer::rows() const {
  return b_.empty() ? 0 : 1 + size_int(b_) / cols_;
}

std::string FrameBuffer::row_as_text(int row) const {
  const auto start = row * cols_;
  const auto end = std::min<int>(size_int(b_), ((row + 1) * cols_));

  std::string s;
  s.reserve(end - start);
  for (auto i = start; i < end; i++) {
    s.push_back(b_[i].c());
  }
  const auto last = s.find_last_not_of('\0');
  if (last == std::string::npos) {
    s.clear();
  } else if (last + 1 != s.size()) {
    s.resize(last + 1);
  }

  return s;
}

std::vector<uint16_t> FrameBuffer::row_char_and_attr(int row) const {
  const auto start = row * cols_;
  const auto end = std::min<int>(size_int(b_), (row + 1) * cols_);

  std::vector<uint16_t> s;
  s.reserve(end - start);
  for (auto i = start; i < end; i++) {
    s.push_back(b_[i].w());
  }
  return s;
}

std::vector<std::string> FrameBuffer::to_screen_as_lines() const { 
  std::vector<std::string> lines;
  lines.reserve(rows());
  int attr = 7;
  for (auto i = 0; i < rows(); i++) {
    auto row = row_char_and_attr(i);
    std::ostringstream ss;
    for (auto cc : row) {
      const uint8_t a = cc >> 8;
      const uint8_t c = cc & 0xff;
      if (a != attr) {
        ss << makeansi(a, attr);
        attr = a;
      }
      ss << static_cast<char>(c);
    }
    lines.push_back(ss.str());
  }
  return lines;
}


} // namespace wwiv
