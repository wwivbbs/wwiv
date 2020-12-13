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
#include "fsed/model.h"

#include "core/os.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/format.h"

namespace wwiv::fsed {

using namespace wwiv::stl;
using namespace wwiv::strings;

/////////////////////////////////////////////////////////////////////////////
// LOCALS

static void advance_cy(FsedModel& ed, editor_viewport_t& view, bool invalidate = true) {
  // advance cy if we have room, scroll region otherwise
  if (ed.cy < view.max_view_lines()) {
    ++ed.cy;
  } else {
    // scroll
    view.set_top_line(ed.curli - ed.cy);
    if (invalidate) {
      ed.invalidate_to_eof(view.top_line());
    }
  }
}

/////////////////////////////////////////////////////////////////////////////
// EDITOR

void FsedModel::set_view(const std::shared_ptr<editor_viewport_t>& view) { view_ = view; }

line_t& FsedModel::curline() const {
  // TODO: insert return statement here
  while (curli >= ssize(lines_)) {
    lines_.emplace_back();
  }
  try {
    return lines_.at(curli);
  } catch (const std::exception& e) {
    LOG(ERROR) << "Exception trying to get line: " << curli << "; what: " << e.what();
    LOG(ERROR) << wwiv::os::stacktrace();
    throw;
  }
}

line_t& FsedModel::line(int n) const {
  try {
    return lines_.at(n);
  } catch (const std::exception& e) {
    LOG(ERROR) << "Exception trying to get line: " << n << "; what: " << e.what();
    LOG(ERROR) << wwiv::os::stacktrace();
    throw;
  }
}

bool FsedModel::set_lines(std::vector<line_t>&& n) {
  lines_ = n;
  return true;
}

void FsedModel::emplace_back(line_t&& n) { lines_.emplace_back(n); }

bool FsedModel::insert_line() {
  if (ssize(lines_) >= maxli()) {
    return false;
  }
  return wwiv::stl::insert_at(lines_, curli, line_t());
}

bool FsedModel::insert_lines(std::vector<std::string>& lines) {
  for (const auto& ql : lines) {
    // Insert all quote lines.
    ++curli;
    insert_line();
    curline() = line_t(ql);
    advance_cy(*this, *view_, false);
  }
  // Add blank line afterwards to use to start new text.
  ++curli;
  insert_line();
  advance_cy(*this, *view_, false);
  // Redraw everything, the whole enchilada!
  invalidate_to_eof(0);
  return true;
}

bool FsedModel::remove_line() {
  if (lines_.empty()) {
    return false;
  }
  return wwiv::stl::erase_at(lines_, curli);
}

editor_add_result_t FsedModel::add(char c) {
  auto& line = curline();
  const auto line_result = line.add(cx, c, mode_);
  if (line_result == line_add_result_t::error) {
    return editor_add_result_t::error;
  }
  const auto start_line = curli;
  ++cx;
  if (cx < max_line_len_) {
    // no  wrapping is needed
    if (line_result == line_add_result_t::needs_redraw) {
      invalidate_range(start_line, curli);
    }
    return editor_add_result_t::added;
  }
  const auto last_space = line.last_space_before(size_int(line));
  line.wrapped(true);
  const auto wwiv_color = line.wwiv_color();
  if (last_space != -1 && (max_line_len_ - last_space) < (max_line_len_ / 2)) {
    // Word Wrap at the position after the last space. That way the space
    // end up on the previous line
    const auto wrap_position = last_space + 1;
    const auto nline = line.substr(wrap_position);
    line.assign(line.substr(0, wrap_position));
    ++curli;
    curline().assign(nline);
    cx = size_int(curline());
  } else {
    // Character wrap.
    ++curli;
    cx %= max_line_len_;
  }
  // Create the new line and carry over line color since we wrapped (either
  // line or character).
  curline().set_wwiv_color(wwiv_color);

  invalidate_range(start_line, curli);
  advance_cy(*this, *view_);
  return editor_add_result_t::wrapped;
}

cell_t FsedModel::current_cell() const {
  const auto& line = curline();
  if (cx >= ssize(line)) {
    return cell_t(0, ' ');
  }
  try {
    return line.cells().at(cx);
  } catch (const std::exception& e) {
    LOG(ERROR) << "Exception trying to get cx: " << cx << "; what: " << e.what();
    LOG(ERROR) << wwiv::os::stacktrace();
    throw;
  }
}

bool FsedModel::cursor_up() {
  const auto previous_line = curli;
  if (cy > 0) {
    --cy;
    --curli;
    const auto right_max = std::min<int>(max_line_len(), size_int(curline()));
    cx = std::min<int>(cx, right_max);
  } else if (curli > 0) {
    // scroll
    --curli;
    const auto right_max = std::min<int>(max_line_len(), size_int(curline()));
    cx = std::min<int>(cx, right_max);
    view_->set_top_line(curli);
    invalidate_to_eof(view_->top_line());
  }
  current_line_dirty(previous_line);
  return true;
}

bool FsedModel::cursor_down() {
  const auto previous_line = curli;
  if (curli < ssize(lines_) - 1) {
    ++curli;
    const auto right_max = std::min<int>(max_line_len(), size_int(curline()));
    cx = std::min<int>(cx, right_max);
    advance_cy(*this, *view_);
  }
  current_line_dirty(previous_line);
  return true;
}

bool FsedModel::cursor_left() {
  if (cx > 0) {
    --cx;
  }
  return true;
}

bool FsedModel::cursor_right() {
  // TODO: add option to cursor right to end of view
  const auto right_max = std::min<int>(max_line_len(), size_int(curline()));
  if (cx < right_max) {
    ++cx;
  }
  return true;
}

bool FsedModel::cursor_pgup() {
  const auto previous_line = curli;
  const auto up = std::min<int>(curli, view_->max_view_lines());
  // nothing to do!
  if (up == 0) {
    return true;
  }
  cy = std::max<int>(cy - up, 0);
  curli = std::max<int>(curli - up, 0);
  view_->set_top_line(curli - cy);
  const auto right_max = std::min<int>(max_line_len(), size_int(curline()));
  cx = std::min<int>(cx, right_max);
  invalidate_to_eof(view_->top_line());
  current_line_dirty(previous_line);
  return true;
}

bool FsedModel::cursor_pgdown() {
  const auto previous_line = curli;
  const auto dn =
      std::min<int>(view_->max_view_lines(), std::max<int>(0, size_int(lines_) - curli - 1));
  if (dn == 0) {
    // nothing to do!
    return true;
  }
  curli += dn;
  cy += dn;
  const auto right_max = std::min<int>(max_line_len(), size_int(curline()));
  cx = std::min<int>(cx, right_max);
  if (cy >= view_->max_view_lines()) {
    // will need to scroll
    cy = view_->max_view_lines();
    view_->set_top_line(curli - cy);
    invalidate_to_eof(view_->top_line());
  }
  current_line_dirty(previous_line);
  return true;
}

bool FsedModel::cursor_home() {
  cx = 0;
  return true;
}

bool FsedModel::cursor_end() {
  cx = size_int(curline());
  return true;
}

bool FsedModel::delete_line() {
  if (remove_line()) {
    invalidate_to_eof(curli);
  }
  current_line_dirty(curli);
  return true;
}

bool FsedModel::delete_to_eol() {
  if (cx < ssize(curline())) {
    auto& oline = curline();
    oline.assign(oline.substr(0, cx));
    oline.wrapped(false);
    invalidate_to_eol();
  } else if (ssize(curline()) == 0) {
    // delete line
    if (remove_line()) {
      invalidate_to_eof(curli);
      current_line_dirty(curli);
    }
  }
  return true;
}

bool FsedModel::delete_line_left() {
  auto& line = curline();
  const auto remainder = line.substr(cx);
  line.assign(remainder);
  cx = 0;
  line.wrapped(false);
  invalidate_to_eol();
  return true;
}

bool FsedModel::delete_word_left() {
  if (cx <= 0) {
    return true;
  }
  auto& line = curline();
  const auto last_space = line.last_space_before(cx);
  if (last_space == cx) {
    return true;
  }
  const auto remainder = line.substr(cx);
  cx = last_space;
  if (last_space == 0) {
    line.assign(remainder);
  } else {
    line.assign(line.substr(0, cx));
    line.append(remainder);
  }
  // TODO(rushfan): Should reflow paragraph here.
  line.wrapped(false);
  invalidate_to_eol();
  return true;
}

bool FsedModel::delete_right() {
  // TODO keep mode state;
  del();
  invalidate_to_eol();
  view_->gotoxy(*this);
  return true;
}

// Toggles the internal state if the editor is in INSERT or OVERWRITE mode. This
// matters on add, bs, and del

void FsedModel::toggle_ins_ovr_mode() {
  mode_ = mode_ == ins_ovr_mode_t::ins ? ins_ovr_mode_t::ovr : ins_ovr_mode_t::ins;
}

// Get the internal state if the editor is in INSERT or OVERWRITE mode.

ins_ovr_mode_t FsedModel::mode() const noexcept { return mode_; }

bool FsedModel::del() {
  const auto r = curline().del(cx, mode_);
  if (r == line_add_result_t::error) {
    return false;
  }
  if (r == line_add_result_t::needs_redraw) {
    invalidate_to_eof(curli);
  }
  return true;
}

bool FsedModel::bs_nowrap() {
  auto r = curline().bs(cx, mode_);
  if (r == line_add_result_t::error) {
    return false;
  }
  if (r == line_add_result_t::needs_redraw) {
    invalidate_to_eof(curli);
  }
  return true;
}

bool FsedModel::bs() {
  const auto previous_line = curli;
  // TODO keep mode state;
  bs_nowrap();
  if (cx > 0) {
    --cx;
  } else if (curli > 0 && ssize(curline()) == 0) {
    // If current line is empty then delete it and move up one line
    if (remove_line()) {
      --cy;
      --curli;
      cx = size_int(curline());
      invalidate_to_eof(curli);
    }
  } else if (curli > 0) {
    auto& prev = line(curli - 1);
    auto& cur = curline();
    const auto last_pos = std::max<int>(0, max_line_len() - size_int(prev) - 1);
    const auto new_cx = size_int(prev);
    if (ssize(cur) < last_pos) {
      prev.append(cur.cells());
      remove_line();
    } else if (const int space = cur.last_space_before(last_pos) > 0) {
      prev.append(cur.substr(0, space));
      cur.assign(cur.substr(space));
    }
    --cy;
    --curli;
    cx = new_cx;
    invalidate_to_eof(curli);
  }
  current_line_dirty(previous_line);
  view_->gotoxy(*this);
  curline().set_wwiv_color(current_cell().wwiv_color);
  return true;
}

bool FsedModel::enter() {
  const auto orig_start_line = curli;
  curline().wrapped(false);
  // Insert inserts after the current line
  if (cx >= ssize(curline())) {
    ++curli;
    insert_line();
  } else {
    // Wrap
    auto& oline = curline();
    const auto ntext = oline.substr(cx);
    oline.assign(oline.substr(0, cx));
    oline.wrapped(false);

    ++curli;
    insert_line();
    curline().assign(ntext);
  }
  advance_cy(*this, *view_);
  cx = 0;
  invalidate_to_eof(orig_start_line);
  current_line_dirty(orig_start_line);
  return true;
}

bool FsedModel::add_callback(const editor_range_invalidated_fn& fn) {
  range_callbacks_.emplace_back(fn);
  return true;
}

bool FsedModel::add_callback(const editor_current_line_redraw_fn& fn) {
  line_callbacks_.emplace_back(fn);
  return true;
}

void FsedModel::invalidate_to_eol() {
  editor_range_t r;
  r.start.line = r.end.line = curli;
  r.start.x = cx;
  r.end.x = size_int(curline());
  for (auto& c : range_callbacks_) {
    c(*this, r);
  }
}

void FsedModel::invalidate_to_eof() { invalidate_to_eof(curli); }

void FsedModel::invalidate_to_eof(int start_line) {
  invalidate_range(start_line, size_int(lines_) - 1);
}

void FsedModel::invalidate_range(int start_line, int end_line) {
  editor_range_t r{};
  r.start.line = start_line;
  r.start.x = cx;
  r.end.line = end_line;
  for (auto& c : range_callbacks_) {
    c(*this, r);
  }
}

void FsedModel::current_line_dirty(int previous_line) {
  for (auto& c : line_callbacks_) {
    c(*this, previous_line);
  }
}

std::vector<std::string> FsedModel::to_lines() {
  std::vector<std::string> out;
  for (const auto& l : lines_) {
    auto t = l.to_colored_text(0);
    StringTrimCRLF(&t);
    if (l.wrapped()) {
      // wwiv line wrapping character.
      t.push_back('\x1');
    }
    out.emplace_back(t);
  }

  return out;
}

} // namespace wwiv::fsed
