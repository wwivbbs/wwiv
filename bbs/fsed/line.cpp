/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                   Copyright (C)2020, WWIV Software Services            */
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
#include "bbs/fsed/line.h"

#include "bbs/bbs.h"
#include "bbs/bgetch.h"
#include "bbs/full_screen.h"
#include "bbs/message_editor_data.h"
#include "bbs/output.h"
#include "bbs/pause.h"
#include "bbs/quote.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/format.h"
#include "local_io/keycodes.h"

namespace wwiv::bbs::fsed {

using namespace wwiv::stl;
using namespace wwiv::strings;

line_t::line_t(bool wrapped, std::string text) : wrapped_(wrapped) {
  for (const auto c : text) {
    cell_.emplace_back(wwiv_color_, c);
    ++size_;
  }
}

void line_t::push_back(char c) {
  cell_.emplace_back(wwiv_color_, c);
  ++size_;
}

line_add_result_t line_t::add(int x, char c, ins_ovr_mode_t mode) {
  while (size_ < x) {
    push_back(' ');
  }
  if (x == size_) {
    push_back(c);
    return line_add_result_t::no_redraw;
  } else if (mode == ins_ovr_mode_t::ins) {
    wwiv::stl::insert_at(cell_, x, cell_t{wwiv_color_, c});
    ++size_;
    return line_add_result_t::needs_redraw;
  } else {
    cell_[x] = cell_t{wwiv_color_, c};
    return line_add_result_t::no_redraw;
  }
}

line_add_result_t line_t::del(int x, ins_ovr_mode_t) {
  if (x < 0) {
    return line_add_result_t::error;
  }
  auto result = (x == size_) ? line_add_result_t::no_redraw : line_add_result_t::needs_redraw;
  if (!wwiv::stl::erase_at(cell_, x)) {
    return line_add_result_t ::error;
  }
  --size_;

  auto new_x = x - 1;
  if (new_x >= 0 && new_x < size_) {
    // adopt new color
    wwiv_color_ = cell_.at(new_x).wwiv_color;
  }
  return result;
}

line_add_result_t line_t::bs(int x, ins_ovr_mode_t mode) { 
  if (x <= 0) {
    return line_add_result_t::error;
  }
  return del(x - 1, mode);
}

std::size_t line_t::size() const { return size_; }

int line_t::last_space_before(int maxlen) { 
  if (size_ < maxlen) {
    return size_;
  }
  if (cell_.empty()) {
    return 0;
  }
  for (int i = size_ - 1; i > 0; i--) {
    char c = cell_.at(i).ch;
    if (c == '\t' || c == ' ') {
      return i;
    }
  }
  return 0;
}

line_t& line_t::operator=(const line_t& o) {
  wrapped_ = o.wrapped_;
  cell_ = o.cell_;
  size_ = o.size_;
  wwiv_color_ = o.wwiv_color_;
  return *this;
}

void line_t::assign(const std::vector<cell_t>& cells) {
  cell_ = cells;
  size_ = wwiv::stl::ssize(cells);
}

void line_t::append(const std::vector<cell_t>& cells) {
  for (const auto& c : cells) {
    cell_.emplace_back(c);
  }
  size_ += wwiv::stl::ssize(cells);
}


std::vector<cell_t> line_t::substr(int start, int end) {
  return std::vector<cell_t>(cell_.begin() + start, cell_.begin() + end);
}

std::vector<cell_t> line_t::substr(int start) {
  return std::vector<cell_t>(cell_.begin() + start, cell_.end());
}

static void append_wwiv_color(std::string& out, int wwiv_color, line_color_code_format_t format) {
  if (format == line_color_code_format_t::heart) {
    out.push_back('\x3');
    out.push_back(static_cast<char>(wwiv_color + '0'));
  } else {
    out.append(fmt::format("|#{}", wwiv_color));
  }
}

std::string line_t::to_colored_text() const {
  // default the last color to 0 by default)
  bool changed_color = false;
  auto last_color = 0;
  std::string out;
  for (const auto& c : cell_) {
    if (c.wwiv_color != last_color) {
      append_wwiv_color(out, c.wwiv_color, line_color_code_format_t::heart);
      last_color = c.wwiv_color;
      changed_color = true;
    }
    out.push_back(c.ch);
  }
  if (changed_color) {
    append_wwiv_color(out, 0, line_color_code_format_t::heart);
  }
  return out;
}


} // namespace wwiv::bbs::fsed