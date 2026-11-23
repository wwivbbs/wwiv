/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*              Copyright (C)2020-2022, WWIV Software Services            */
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
#include "fsed/wordnav.h"

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
    view.set_top_line(std::max(0, ed.curli - ed.cy));
    if (invalidate) {
      ed.invalidate_to_eof(view.top_line());
    }
  }
  if (ed.cy > view.max_view_lines()) {
    ed.cy = view.max_view_lines();
    view.set_top_line(std::max(0, ed.curli - ed.cy));
  }
  if (ed.cy < 0) {
    ed.cy = 0;
  }
}

// Move the cursor up one visual row after curli has already been decremented.
// Scrolls the viewport when the cursor was on the top visible row so cy
// never goes negative.
static void retreat_cy(FsedModel& ed, editor_viewport_t& view) {
  if (ed.cy > 0) {
    --ed.cy;
  } else {
    view.set_top_line(ed.curli);
    // Scrolling the viewport must repaint the whole visible window;
    // otherwise the first rows keep stale text (and a paste of many
    // lines leaves the original first line frozen at the top).
    ed.invalidate_to_eof(view.top_line());
  }
}

/////////////////////////////////////////////////////////////////////////////
// EDITOR

void FsedModel::set_view(const std::shared_ptr<editor_viewport_t>& view) { view_ = view; }

line_t& FsedModel::curline() const {
  // TODO: insert return statement here
  while (curli >= size_int(lines_)) {
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
  if (size_int(lines_) >= maxli()) {
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
  // Kept so the character can be taken back out if the reflow below would
  // push the message past the sysop's line limit.
  const auto saved_line = line;
  const auto saved_cx = cx;
  const auto line_result = line.add(cx, c, mode_);
  if (line_result == line_add_result_t::error) {
    return editor_add_result_t::error;
  }
  const auto start_line = curli;
  ++cx;
  // Reflow when the line is at or past the max width (including a mid-line
  // insert that grows a full-width line), or when this line already soft-
  // wraps onto a continuation.  Using "<=" used to skip wrap when the
  // cursor was mid-line and the line was exactly max_line_len_ chars,
  // leaving text past the right edge.
  const bool needs_reflow = cx >= max_line_len_ ||
                            size_int(curline()) >= max_line_len_ ||
                            curline().wrapped();
  if (!needs_reflow) {
    if (line_result == line_add_result_t::needs_redraw) {
      invalidate_range(start_line, curli);
    }
    return editor_add_result_t::added;
  }
  if (!reflow_paragraph()) {
    // The wrapped paragraph would not fit in maxli() lines.  Drop the
    // character rather than leave a line wider than the screen.
    curli = start_line;
    curline() = saved_line;
    cx = saved_cx;
    return editor_add_result_t::error;
  }
  // Lines below may have been pushed down or reflowed.  The old code only
  // invalidated start_line..curli, so the shifted lines stayed put on
  // screen and looked like they had been overwritten (#1343).  If the
  // reflow scrolled the viewport, the view repaints the whole window.
  invalidate_to_eof(start_line);
  return editor_add_result_t::wrapped;
}

bool FsedModel::reflow_paragraph() {
  if (curli < 0) {
    curli = 0;
  }
  // curline() will extend lines_ if curli is past the end.
  (void)curline();
  if (lines_.empty()) {
    return false;
  }
  if (curli >= size_int(lines_)) {
    curli = size_int(lines_) - 1;
  }

  // wrapped() means this line continues onto the next (saved as a trailing
  // SOH / 0x01 marker by to_lines(true)).  Walk back to the start of the
  // soft-wrapped paragraph so deletes can pull words up, then forward to
  // the next hard break.  A blank line is a paragraph separator.
  int start = curli;
  while (start > 0 && line(start - 1).wrapped() && size_int(line(start)) > 0) {
    --start;
  }
  int end = curli + 1;
  while (end < size_int(lines_) && line(end - 1).wrapped()) {
    if (size_int(line(end)) == 0) {
      break;
    }
    ++end;
  }

  std::vector<cell_t> cells;
  int offset = 0;
  bool offset_set = false;
  for (int i = start; i < end; ++i) {
    auto part = line(i).cells();
    while (!part.empty() && part.back().ch == '\x01') {
      part.pop_back();
    }
    if (!cells.empty() && !part.empty()) {
      const auto a = cells.back().ch;
      const auto b = part.front().ch;
      if (a != ' ' && a != '\t' && b != ' ' && b != '\t') {
        cells.emplace_back(cells.back().wwiv_color, ' ');
      }
    }
    if (i == curli && !offset_set) {
      const int local = std::min(std::max(cx, 0), size_int(part));
      offset = size_int(cells) + local;
      offset_set = true;
    }
    cells.insert(cells.end(), part.begin(), part.end());
  }
  if (!offset_set) {
    offset = size_int(cells);
  }
  if (offset < 0) {
    offset = 0;
  }
  if (offset > size_int(cells)) {
    offset = size_int(cells);
  }

  const int width = std::max(1, max_line_len_);
  std::vector<line_t> wrapped;
  int pos = 0;
  const int n = size_int(cells);
  while (pos < n) {
    const int remain = n - pos;
    int take;
    if (remain <= width) {
      take = remain;
    } else {
      int sp = -1;
      for (int k = width - 1; k > 0; --k) {
        const auto ch = cells[pos + k].ch;
        if (ch == ' ' || ch == '\t') {
          sp = k;
          break;
        }
      }
      take = (sp > 0) ? (sp + 1) : width;
    }
    line_t nl;
    nl.assign(std::vector<cell_t>(cells.begin() + pos, cells.begin() + pos + take));
    pos += take;
    nl.wrapped(pos < n);
    wrapped.push_back(std::move(nl));
  }
  if (wrapped.empty()) {
    wrapped.emplace_back();
  }
  if (size_int(lines_) - (end - start) + size_int(wrapped) > maxli() &&
      size_int(wrapped) > (end - start)) {
    // Would exceed the sysop's max lines.  Leave the model untouched;
    // add() then takes the new character back out, so the message never
    // grows past maxli() and no line is left wider than the screen.
    return false;
  }

  lines_.erase(lines_.begin() + start, lines_.begin() + end);
  lines_.insert(lines_.begin() + start, wrapped.begin(), wrapped.end());

  int left = offset;
  curli = start;
  cx = 0;
  for (int i = 0; i < size_int(wrapped); ++i) {
    const int sz = size_int(wrapped[i]);
    if (left <= sz) {
      curli = start + i;
      cx = left;
      break;
    }
    left -= sz;
    curli = start + i;
    cx = sz;
  }
  sync_viewport();
  return true;
}

void FsedModel::sync_viewport() {
  if (!view_) {
    return;
  }
  const int max_cy = std::max(0, view_->max_view_lines());
  if (curli < 0) {
    curli = 0;
  }
  if (!lines_.empty() && curli >= size_int(lines_)) {
    curli = size_int(lines_) - 1;
  }
  if (curli < view_->top_line()) {
    view_->set_top_line(std::max(0, curli));
  } else if (curli - view_->top_line() > max_cy) {
    view_->set_top_line(curli - max_cy);
  }
  cy = curli - view_->top_line();
  if (cy < 0) {
    cy = 0;
    view_->set_top_line(curli);
  }
  if (cy > max_cy) {
    cy = max_cy;
    view_->set_top_line(std::max(0, curli - cy));
  }
}

cell_t FsedModel::current_cell() const {
  const auto& line = curline();
  if (cx >= size_int(line)) {
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
  if (curli < size_int(lines_) - 1) {
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
    return true;
  }
  if (curli == 0) {
    return true;
  }
  // At column 0: wrap to the end of the previous line, scrolling if we
  // were on the top visible row.
  const auto previous_line = curli;
  --curli;
  retreat_cy(*this, *view_);
  cx = std::min<int>(max_line_len(), size_int(curline()));
  current_line_dirty(previous_line);
  return true;
}

bool FsedModel::cursor_right() {
  const auto right_max = std::min<int>(max_line_len(), size_int(curline()));
  if (cx < right_max) {
    ++cx;
    return true;
  }
  if (curli >= size_int(lines_) - 1) {
    return true;
  }
  // At the end of the line: wrap to column 0 of the next line.
  const auto previous_line = curli;
  ++curli;
  cx = 0;
  advance_cy(*this, *view_);
  current_line_dirty(previous_line);
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

bool FsedModel::cursor_eof() {
  const auto previous_line = curli;
  if (lines_.empty()) {
    curli = 0;
    cx = 0;
    cy = 0;
    return true;
  }
  curli = size_int(lines_) - 1;
  cx = size_int(curline());
  sync_viewport();
  if (view_) {
    invalidate_to_eof(view_->top_line());
  }
  current_line_dirty(previous_line);
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
  if (cx < size_int(curline())) {
    auto& oline = curline();
    oline.assign(oline.substr(0, cx));
    oline.wrapped(false);
    invalidate_to_eol();
  } else if (size_int(curline()) == 0) {
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

static std::vector<std::string> editor_line_texts(const FsedModel& ed) {
  std::vector<std::string> texts;
  texts.reserve(ed.size());
  for (int i = 0; i < static_cast<int>(ed.size()); i++) {
    const auto& cells = ed.line(i).cells();
    std::string s;
    s.reserve(cells.size());
    for (const auto& c : cells) {
      s.push_back(c.ch);
    }
    texts.emplace_back(std::move(s));
  }
  return texts;
}

bool FsedModel::delete_word_left() {
  const auto texts = editor_line_texts(*this);
  const auto target = word_left(texts, word_pos_t{curli, cx});
  return delete_to_pos(target.line, target.x);
}

bool FsedModel::delete_right() {
  // TODO keep mode state;
  del();
  invalidate_to_eol();
  view_->gotoxy(*this);
  return true;
}

void FsedModel::settle_cursor(int previous_line) {
  if (view_) {
    const int max_cy = view_->max_view_lines();
    if (curli < view_->top_line()) {
      view_->set_top_line(curli < 0 ? 0 : curli);
      invalidate_to_eof(view_->top_line());
    } else if (curli - view_->top_line() > max_cy) {
      view_->set_top_line(curli - max_cy);
      invalidate_to_eof(view_->top_line());
    }
    cy = curli - view_->top_line();
    if (cy < 0) {
      cy = 0;
      view_->set_top_line(curli);
      invalidate_to_eof(view_->top_line());
    }
  }
  current_line_dirty(previous_line);
}

bool FsedModel::delete_to_pos(int line, int x) {
  int sl = curli;
  int sx = cx;
  int el = line;
  int ex = x;
  if (sl > el || (sl == el && sx > ex)) {
    const int tl = sl;
    const int tx = sx;
    sl = el;
    sx = ex;
    el = tl;
    ex = tx;
  }
  if (sl == el && sx == ex) {
    return true;
  }
  const auto previous_line = curli;
  if (sl == el) {
    auto& ln = this->line(sl);
    const auto prefix = ln.substr(0, sx);
    const auto suffix = ln.substr(ex);
    ln.assign(prefix);
    ln.append(suffix);
    // TODO(rushfan): Should reflow paragraph here.
    ln.wrapped(false);
    curli = sl;
    cx = sx;
    invalidate_to_eol();
    settle_cursor(previous_line);
    return true;
  }
  auto& start = this->line(sl);
  const auto prefix = start.substr(0, sx);
  const bool keep_wrap = this->line(el).wrapped();
  const auto suffix = this->line(el).substr(ex);
  start.assign(prefix);
  start.append(suffix);
  start.wrapped(keep_wrap);
  for (int i = el; i > sl; --i) {
    wwiv::stl::erase_at(lines_, i);
  }
  curli = sl;
  cx = sx;
  invalidate_to_eof(sl);
  settle_cursor(previous_line);
  return true;
}

bool FsedModel::delete_word_right() {
  const auto texts = editor_line_texts(*this);
  const auto target = word_right(texts, word_pos_t{curli, cx});
  return delete_to_pos(target.line, target.x);
}

bool FsedModel::cursor_word_left() {
  const auto previous_line = curli;
  const auto texts = editor_line_texts(*this);
  const auto target = word_left(texts, word_pos_t{curli, cx});
  curli = target.line;
  cx = target.x;
  settle_cursor(previous_line);
  return true;
}

bool FsedModel::cursor_word_right() {
  const auto previous_line = curli;
  const auto texts = editor_line_texts(*this);
  const auto target = word_right(texts, word_pos_t{curli, cx});
  curli = target.line;
  cx = target.x;
  settle_cursor(previous_line);
  return true;
}

bool FsedModel::cursor_bof() {
  const auto previous_line = curli;
  curli = 0;
  cx = 0;
  settle_cursor(previous_line);
  return true;
}

bool FsedModel::cursor_doc_end() {
  const auto previous_line = curli;
  if (lines_.empty()) {
    curli = 0;
    cx = 0;
    cy = 0;
    return true;
  }
  curli = wwiv::stl::size_int(lines_) - 1;
  cx = wwiv::stl::size_int(curline());
  settle_cursor(previous_line);
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
  const bool in_para = curline().wrapped() ||
                       (curli > 0 && line(curli - 1).wrapped()) ||
                       size_int(curline()) >= max_line_len_;
  if (in_para) {
    reflow_paragraph();
    invalidate_to_eof(curli);
    return true;
  }
  if (r == line_add_result_t::needs_redraw) {
    invalidate_to_eof(curli);
  }
  return true;
}

bool FsedModel::bs_nowrap() {
  const auto r = curline().bs(cx, mode_);
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
  const auto previous_cx = std::max<int>(cx - 1, 0);
  const auto previous_cx_color = size_int(curline().cells()) > previous_cx ? curline().cells().at(previous_cx).wwiv_color : 0;
  bs_nowrap();
  if (cx > 0) {
    --cx;
  } else if (curli > 0 && size_int(curline()) == 0) {
    // If current line is empty then delete it and move up one line
    if (remove_line()) {
      --curli;
      retreat_cy(*this, *view_);
      cx = size_int(curline());
      invalidate_to_eof(curli);
    }
  } else if (curli > 0 && line(curli - 1).wrapped()) {
    // Backspace at column 0 of a soft-wrapped continuation: drop the wrap
    // and reflow the paragraph so words pull back up.  Do not use the
    // hard-break join, which can leave a stale wrapped() flag pointing
    // at the next paragraph.
    --curli;
    retreat_cy(*this, *view_);
    cx = size_int(curline());
    reflow_paragraph();
    invalidate_to_eof(curli);
  } else if (curli > 0) {
    auto& prev = line(curli - 1);
    auto& cur = curline();
    // If two hard-broken lines would glue like "Maryland.Trying", put a
    // separating space back.
    const bool need_space =
        size_int(prev) > 0 && size_int(cur) > 0 &&
        prev.cells().back().ch != ' ' && prev.cells().back().ch != '\t' &&
        cur.cells().front().ch != ' ' && cur.cells().front().ch != '\t';
    if (need_space) {
      prev.push_back(' ');
    }
    auto new_cx = size_int(prev);
    const auto last_pos = std::max<int>(0, max_line_len() - size_int(prev) - 1);
    if (ssize(cur) < last_pos) {
      prev.append(cur.cells());
      remove_line();
    } else if (const int space = cur.last_space_before(last_pos); space > 0) {
      // last_space_before returns an index, not a boolean.  The previous
      // "space = expr > 0" form assigned 0 or 1, so a join only pulled one
      // character back onto the line above.
      prev.append(cur.substr(0, space));
      cur.assign(cur.substr(space));
    }
    --curli;
    retreat_cy(*this, *view_);
    cx = new_cx;
    invalidate_to_eof(curli);
  }
  const bool in_para = curline().wrapped() ||
                       (curli > 0 && line(curli - 1).wrapped()) ||
                       size_int(curline()) >= max_line_len_;
  if (in_para) {
    reflow_paragraph();
    invalidate_to_eof(curli);
  }
  current_line_dirty(previous_line);
  view_->gotoxy(*this);
  curline().set_wwiv_color(previous_cx_color);
  return true;
}

bool FsedModel::enter() {
  // Enter always adds a line.  At the sysop's line limit, refuse it
  // before touching anything (insert_line() alone would fail, but the
  // curline() call below would still append a line past the end).
  if (size_int(lines_) >= maxli()) {
    return false;
  }
  const auto orig_start_line = curli;
  curline().wrapped(false);
  const auto previous_color = curline().wwiv_color();
  // Insert inserts after the current line
  if (cx >= size_int(curline())) {
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
  sync_viewport();
  // Restore the wwiv color on the new line so we don't reset to 0.
  curline().set_wwiv_color(previous_color);
  cx = 0;
  // If Enter scrolled the viewport, the view repaints the whole window.
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

std::vector<std::string> FsedModel::to_lines(bool wrap) {
  std::vector<std::string> out;
  std::string curline;
  for (const auto& l : lines_) {
    if (!curline.empty() && curline.back() != ' ') {
      curline.push_back(' ');
    }
    auto x = l.to_colored_text(0);
    StringTrimCRLF(&x);
    curline.append(x);
    if (l.wrapped()) {
      if (wrap) {
        // wwiv line wrapping character.
        curline.push_back('\x1');
        out.emplace_back(curline);
        curline.clear();
        continue;
      }
      // else leave curline as the current line and the next iteration will append to it.
    } else {
      out.emplace_back(curline);
      curline.clear();
    }
  }

  if (!curline.empty()) {
    // Add last line if we have been building one up.
    out.emplace_back(curline);
    curline.clear();
  }
  return out;
}

} // namespace wwiv::fsed
