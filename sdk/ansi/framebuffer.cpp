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
#include "sdk/ansi/framebuffer.h"

#include "core/stl.h"
#include <algorithm>

using namespace wwiv::stl;

namespace wwiv {
namespace sdk {
namespace ansi {

FrameBufferCell::FrameBufferCell() : FrameBufferCell(0, FRAMEBUFFER_DEFAULT_ATTRIBUTE) {}

FrameBufferCell::FrameBufferCell(char c, uint8_t a) : c_(c), a_(a) {}

FrameBuffer::FrameBuffer(int cols) : cols_(cols) { b_.reserve(cols_ * 100); }

bool FrameBuffer::gotoxy(int x, int y) {
  pos_ = (y * cols_) + x;
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

bool FrameBuffer::put(int pos, char c, uint8_t a) {
  if (pos >= size_int(b_)) {
    if (!grow(pos)) {
      return false;
    }
  }
  b_[pos] = std::move(FrameBufferCell(c, a));
  return true;
}

bool FrameBuffer::write(char c, uint8_t a) {
  switch (c) {
  case 0: // NOP
    break;
  case '\r': {
    auto line = y();
    return gotoxy(0, line);
  } break;
  case '\n': {
    auto line = y() + 1;
    return gotoxy(0, line);
  } break;
  default: { return put(pos_++, c, a); } break;
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

  bool f = false;
  for (auto i = size_int(b_) - 1; i >= 0; i--) {
    if (f && b_[i].c() == 0) {
      b_[i].c(' ');
    } else {
      f = true;
    }
    if ((i % cols_) == 0) {
      f = false;
    }
  }

}

bool FrameBuffer::grow(int pos) {
  auto current = std::max(pos, size_int(b_));

  b_.resize((y() + 1) * cols_);
  return true;
}

std::string FrameBuffer::row_as_text(int row) const {
  int start = row * cols_;
  int end = std::min(size_int(b_), ((row + 1) * cols_));

  std::string s;
  for (int i = start; i < end; i++) {
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
  int start = row * cols_;
  int end = std::min(size_int(b_), (row + 1) * cols_);

  std::vector<uint16_t> s;
  for (int i = start; i < end; i++) {
    s.push_back(b_[i].w());
  }
  return s;
}

} // namespace ansi
} // namespace sdk
} // namespace wwiv
