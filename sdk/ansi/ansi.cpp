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
#include "sdk/ansi/ansi.h"

#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include <algorithm>
#include <cctype>

using namespace wwiv::stl;
using namespace wwiv::strings;

namespace wwiv {
namespace sdk {
namespace ansi {

static const char clrlst[] = "04261537";

Ansi::Ansi(FrameBuffer* b, uint8_t default_attr) : b_(b), default_attr_(default_attr) {}

bool Ansi::write(char c) {
  if (state_ == AnsiMode::not_in_sequence && c == 27) {
    state_ = AnsiMode::in_sequence;
    return write_in_sequence(c);
  } else if (state_ == AnsiMode::in_sequence) {
    return write_in_sequence(c);
  }
  return write_not_in_sequence(c);
}

static std::vector<int> to_ansi_numbers(const std::string& as) {
  // TODO(rushfan) assert that this starts_with(as, "\x1b["))?
  auto list = SplitString(as.substr(2), ";");
  std::vector<int> out;
  for (const auto& s : list) {
    out.push_back(to_number<int>(s));
  }
  return out;
}

bool Ansi::write_in_sequence(char c) {
  switch (c) {
  case 27: {
    if (!ansi_sequence_.empty()) {
      return ansi_sequence_error(c);
    }
    ansi_sequence_.push_back(c);
  } break;
  case '[': {
    if (ansi_sequence_.size() != 1) {
      return ansi_sequence_error(c);
    }
    ansi_sequence_.push_back(c);
    break;
  }
  case 'A': {
    auto ns = to_ansi_numbers(ansi_sequence_);
    b_->gotoxy(b_->x(), std::max(0, b_->y() - ns[0]));
    return ansi_sequence_done();
  } break;
  case 'B': {
    auto ns = to_ansi_numbers(ansi_sequence_);
    b_->gotoxy(b_->x(), b_->y() + ns[0]);
    return ansi_sequence_done();
  } break;
  case 'C': {
    auto ns = to_ansi_numbers(ansi_sequence_);
    b_->gotoxy(std::min(b_->cols() - 1, b_->x() + ns[0]), b_->y());
    return ansi_sequence_done();
  } break;
  case 'D': {
    auto ns = to_ansi_numbers(ansi_sequence_);
    b_->gotoxy(std::max(0, b_->x() - ns[0]), b_->y());
    return ansi_sequence_done();
  } break;
  case 'H': 
  case 'f': {
    auto ns = to_ansi_numbers(ansi_sequence_);
    if (ns.size() < 2) {
      return ansi_sequence_error(c);
    }
    b_->gotoxy(ns[0]-1, ns[1]-1);
    return ansi_sequence_done();;
  } break;
  case 'J': {
    auto ns = to_ansi_numbers(ansi_sequence_);
    if (ns.size() != 1 || ns.front() != 2) {
      return ansi_sequence_error(c);
    }
    b_->clear();
    return ansi_sequence_done();
  } break;
  case 'm': {
    auto ansi_numbers_ = to_ansi_numbers(ansi_sequence_);
    // https://en.wikipedia.org/wiki/ANSI_escape_code#SGR_(Select_Graphic_Rendition)_parameters
    for (const auto n : ansi_numbers_) {
      const auto a = b_->curatr();
      switch (n) {
      case 0: // Normal/Reset
        b_->set_attr(default_attr_);
        break;
      case 1: // Bold
        b_->set_attr(b_->curatr() | 0x08);
        break;
      case 5: // Slow blink?
        b_->set_attr(b_->curatr() | 0x80);
        break;
      case 7: { // Reverse Video
        const auto ptr = a & 0x77;
        b_->set_attr((a & 0x88) | (ptr << 4) | (ptr >> 4));
      } break;
      default: {
        if (n >= 30 && n <= 37) {
          b_->set_attr((a & 0xf8) | (clrlst[n - 30] - '0'));
        } else if (n >= 40 && n <= 47) {
          b_->set_attr((a & 0x8f) | ((clrlst[n - 40] - '0') << 4));
        }
      }
      }
    }
    return ansi_sequence_done();
  } break;
  default: {
    if (ansi_sequence_.size() < 2) {
      return ansi_sequence_error(c);
    }
    if (std::isdigit(static_cast<unsigned char>(c)) || c == ';') {
      ansi_sequence_.push_back(c);
      return true;
    }
  } break;
  }
  VLOG(2) << "Unknown ansi character: '" << c << "'; full sequence: " << ansi_sequence_;
  return true;
}

bool Ansi::ansi_sequence_done() {

  state_ = AnsiMode::not_in_sequence;
  ansi_sequence_.clear();
  return true;
}

bool Ansi::ansi_sequence_error(char c) {
  VLOG(2) << "Invalid ansi sequence: '" << ansi_sequence_ << "'";
  write_not_in_sequence(ansi_sequence_);
  write_not_in_sequence(c);
  return ansi_sequence_done();
}

bool Ansi::write_not_in_sequence(char c) {
  b_->write(c);
  return true;
}

bool Ansi::attr(uint8_t a) {
  b_->set_attr(a);
  return true;
}

bool Ansi::reset() {
  state_ = AnsiMode::not_in_sequence;
  ansi_sequence_.clear();
  return true;
}

} // namespace ansi
} // namespace sdk
} // namespace wwiv
