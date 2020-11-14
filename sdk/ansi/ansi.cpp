/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*             Copyright (C)2018-2020, WWIV Software Services             */
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
#include "core/os.h"
#include "core/stl.h"
#include "core/strings.h"
#include "sdk/ansi/vscreen.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>

using namespace wwiv::stl;
using namespace wwiv::strings;

namespace wwiv::sdk::ansi {

static const char clrlst[] = "04261537";

Ansi::Ansi(VScreen* b, AnsiCallbacks callbacks, uint8_t default_attr)
    : b_(b), callbacks_(std::move(callbacks)), default_attr_(default_attr) {}

bool Ansi::write(char c) {
  if (state_ == AnsiMode::not_in_sequence && c == 27) {
    state_ = AnsiMode::in_sequence;
    return write_in_sequence(c);
  }
  if (state_ == AnsiMode::in_sequence) {
    return write_in_sequence(c);
  }
  return write_not_in_sequence(c);
}

std::vector<int> to_ansi_numbers(const std::string& as, int max_args, std::vector<int> defaults) {
  // TODO(rushfan) assert that this starts_with(as, "\x1b["))?
  auto list = SplitString(as.substr(2), ";", false);
  std::vector<int> out;
  const auto list_size = std::min<int>(max_args, wwiv::stl::size_int(list));
  for (auto i = 0; i < wwiv::stl::ssize(defaults); i++) {
    if (i < list_size) {
      const auto& c = list.at(i);
      if (!c.empty()) {
        out.push_back(to_number<int>(c));
        continue;
      }
    }
    out.push_back(defaults.at(i));
  }

  if (list_size > 0 && list_size > wwiv::stl::ssize(defaults)) {
    const auto start = defaults.size();
    const auto end = list_size - defaults.size();
    for (auto i = start; i < end; i++) {
      const auto& c = list.at(i);
      if (!c.empty()) {
        out.push_back(to_number<int>(c));
      }
    }
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
    return true;
  }
  case '[': {
    if (ansi_sequence_.size() != 1) {
      return ansi_sequence_error(c);
    }
    ansi_sequence_.push_back(c);
    return true;
  }
  case 'A': {
    auto ns = to_ansi_numbers(ansi_sequence_, 1, {1});
    const auto x = b_->x();
    const auto y = std::max(0, b_->y() - ns[0]);
    b_->gotoxy(x, y);
    if (callbacks_.move_) {
      callbacks_.move_(x, y);
    }
    return ansi_sequence_done();
  }
  case 'B': {
    auto ns = to_ansi_numbers(ansi_sequence_, 1, {1});
    const auto x = b_->x();
    const auto y = std::max(0, b_->y() + ns[0]);
    b_->gotoxy(x, y);
    if (callbacks_.move_) {
      callbacks_.move_(x, y);
    }
    return ansi_sequence_done();
  }
  case 'C': {
    auto ns = to_ansi_numbers(ansi_sequence_, 1, {1});
    const auto x = std::min(b_->cols() - 1, b_->x() + ns[0]);
    const auto y = b_->y();
    b_->gotoxy(x, y);
    if (callbacks_.move_) {
      callbacks_.move_(x, y);
    }
    return ansi_sequence_done();
  }
  case 'D': {
    auto ns = to_ansi_numbers(ansi_sequence_, 1, {1});
    const auto x = std::min(b_->cols() - 1, b_->x() - ns[0]);
    const auto y = b_->y();
    b_->gotoxy(x, y);
    if (callbacks_.move_) {
      callbacks_.move_(x, y);
    }
    return ansi_sequence_done();
  }
  case 'H':
  case 'f': {
    auto ns = to_ansi_numbers(ansi_sequence_, 2, {1, 1});
    if (ns.size() < 2) {
      return ansi_sequence_error(c);
    }
    const auto x = ns[1] - 1;
    const auto y = ns[0] - 1;
    if (callbacks_.move_) {
      callbacks_.move_(x, y);
    }
    b_->gotoxy(x, y);
    return ansi_sequence_done();
  }
  case 'h':
  case 'l': { // save or restore wrap at last column.
    // Were' ignoring this.
    return ansi_sequence_done();
  }
  case 'J': {
    auto ns = to_ansi_numbers(ansi_sequence_, 1, {1});
    if (ns.size() != 1 || ns.front() != 2) {
      return ansi_sequence_error(c);
    }
    b_->clear();
    if (callbacks_.move_) {
      callbacks_.move_(0, 0);
    }
    return ansi_sequence_done();
  }
  case 'K':
  case 'k': {
    b_->clear_eol();
    return ansi_sequence_done();
  }
  case 'm': {
    auto ansi_numbers_ = to_ansi_numbers(ansi_sequence_, 10, {});
    // https://en.wikipedia.org/wiki/ANSI_escape_code#SGR_(Select_Graphic_Rendition)_parameters
    for (const auto n : ansi_numbers_) {
      const auto a = b_->curatr();
      switch (n) {
      case 0: // Normal/Reset
        b_->curatr(default_attr_);
        break;
      case 1: // Bold
        b_->curatr(a | 0x08);
        break;
      case 5: // Slow blink?
        b_->curatr(a | 0x80);
        break;
      case 7: { // Reverse Video
        const auto ptr = static_cast<uint8_t>(a & 0x77);
        b_->curatr((a & 0x88) | (ptr << 4) | (ptr >> 4));
      } break;
      default: {
        if (n >= 30 && n <= 37) {
          b_->curatr((a & 0xf8) | (clrlst[n - 30] - '0'));
        } else if (n >= 40 && n <= 47) {
          const uint8_t bg = (clrlst[n - 40] - '0') << 4;
          const uint8_t ct = (a & 0x8f) | bg;
          b_->curatr(ct);
        }
      }
      }
    }
    return ansi_sequence_done();
  }
  case 's': { // Save
    saved_x_ = b_->x();
    saved_y_ = b_->y();
    return ansi_sequence_done();
  }
  case 'u': {
    b_->gotoxy(saved_x_, saved_y_);
    if (callbacks_.move_) {
      callbacks_.move_(saved_x_, saved_y_);
    }
    return ansi_sequence_done();
  }
  default: {
    if (ansi_sequence_.size() < 2) {
      return ansi_sequence_error(c);
    }
    // TODO(rushfan): Really should allow for any of these parameter or
    // intermediate then final bytes:
    // From Wikipedia: https://en.wikipedia.org/wiki/ANSI_escape_code#CSI_sequences
    // The ESC [ is followed by any number (including none) of "parameter bytes" in the
    // range 0x30–0x3F (ASCII 0–9:;<=>?), then by any number of "intermediate bytes"
    // in the range 0x20–0x2F (ASCII space and !"#$%&'()*+,-./), then finally by a
    // single "final byte" in the range 0x40–0x7E (ASCII @A–Z[\]^_`a–z{|}~)
    if (std::isdigit(static_cast<unsigned char>(c)) || c == ';' || c == '?') {
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
  std::ostringstream ss;
  ss << "Previous Sequence: ";
  for (const auto sc : ansi_sequence_) {
    ss << "['" << sc << "':" << static_cast<int>(sc) << "]";
  }
  VLOG(2) << "Invalid ansi char: ['" << c << "':" << static_cast<int>(c) << "] ; " << ss.str();
  write_not_in_sequence(ansi_sequence_);
  write_not_in_sequence(c);
  return ansi_sequence_done();
}

bool Ansi::write_not_in_sequence(char c) {
  b_->write(c);
  return true;
}

bool Ansi::attr(uint8_t a) {
  b_->curatr(a);
  return true;
}

bool Ansi::reset() {
  state_ = AnsiMode::not_in_sequence;
  ansi_sequence_.clear();
  return true;
}

HeartCodeFilter::HeartCodeFilter(AnsiFilter* chain, std::vector<uint8_t> colors)
    : chain_(chain), colors_(std::move(colors)) {}

bool HeartCodeFilter::write(char c) {

  if (has_heart_) {
    const uint8_t color = c - '0';
    has_heart_ = false;
    return attr(color);
  }
  if (c == 3) {
    has_heart_ = true;
    return true;
  }
  return chain_->write(c);
}

bool HeartCodeFilter::attr(uint8_t a) {
  if (a < colors_.size()) {
    return chain_->attr(colors_.at(a));
  }
  return false;
}

} // namespace wwiv
