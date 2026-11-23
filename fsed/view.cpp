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
#include "fsed/view.h"
#include "fsed/view_rows.h"

#include "common/full_screen.h"
#include "common/message_editor_data.h"
#include "common/input.h"
#include "common/output.h"
#include "fsed/model.h"
#include "fsed/line.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/format.h"
#include "local_io/keycodes.h"

using namespace wwiv::common;
using namespace wwiv::local::io;
using namespace wwiv::stl;
using namespace wwiv::strings;

namespace wwiv::fsed {

// Clear the row first, then write cells with outchr (never outstr, so a
// '|' in the message cannot be taken as a WWIV pipe code).  After a
// nearly full write, park the cursor on this row so VT autowrap cannot
// leave a tail on the next row.
static void paint_edit_row(Output& out, int y, int screen_width, int screen_length,
                           const std::vector<cell_t>& cells) {
  // Never paint on or past the last terminal row (the command line).
  // A full-width write there autowraps and scrolls the header off the top.
  if (y >= screen_length) {
    return;
  }
  out.goxy(1, y);
  // Clear with the default color, not whatever the previous row or the
  // status bar left set, so the cleared row has no colored background.
  out.ansic(0);
  out.clreol();
  out.goxy(1, y);
  // Never write more cells than the wrap width.  A line that somehow
  // grew past max_line_len would otherwise autowrap onto the next row
  // and be painted over by the following model line.
  const int max_cells = editor_wrap_columns(screen_width);
  int last_color = 0;
  int written = 0;
  for (const auto& c : cells) {
    if (written >= max_cells) {
      break;
    }
    if (c.wwiv_color != last_color) {
      last_color = c.wwiv_color;
      out.ansic(c.wwiv_color);
    }
    out.outchr(c.ch);
    ++written;
  }
  if (written >= screen_width - 1) {
    out.goxy(1, y);
  }
}

FsedView::FsedView(const FullScreenView& fs, MessageEditorData& data, bool file)
    : fs_(fs), bout_(fs_.out()), bin_(fs_.in()), data_(data), file_(file) {
  max_view_lines_ = fs.message_height() - 1;
  max_view_columns_ = fs.screen_width();
}

FullScreenView& FsedView::fs() { return fs_; }

void FsedView::gotoxy(const FsedModel& ed) {
  // Keep the cursor in the message area.  cy can get one row too high
  // (onto the bottom bar / command line) after jumping to the end of
  // a long message.
  auto y = ed.cy + fs_.lines_start();
  if (y >= fs_.lines_end()) {
    y = fs_.lines_end() - 1;
  }
  if (y < fs_.lines_start()) {
    y = fs_.lines_start();
  }
  auto x = ed.cx + 1;
  if (x < 1) {
    x = 1;
  }
  bout_.goxy(x, y);
}

void FsedView::ClearCommandLine() { 
    fs_.PutsCommandLine("|#9(|#2ESC|#9 or |#2/|#9=Menu/Help) "); }

void FsedView::macro(Context& ctx, int cc) {
  if (!ctx.session_context().okmacro() || bin.charbufferpointer_) {
    return;
  }
  std::map<int, int> macro_nums{{CD, 0}, {CF, 1}, {CA, 2}};
  const auto macro_num = macro_nums.at(cc);
  const auto m = ctx.u().macro(macro_num);
  if (m.empty()) {
    return;
  }
  strcpy(&bin.charbuffer[1], m.c_str());
  bin.charbufferpointer_ = 1;

}

void FsedView::draw_current_line(FsedModel& ed, int previous_line) { 
  if (frame_dirty_ || painted_top_line_ != top_line()) {
    // The viewport scrolled (or the frame needs a repaint); every row moved.
    repaint_frame(ed);
    return;
  }
  if (previous_line != ed.curli) {
    if (const auto py = row_for_line(previous_line, top_line(), fs_.lines_start(), fs_.lines_end())) {
      const std::vector<cell_t> cells =
          previous_line < size_int(ed) ? ed.line(previous_line).cells() : std::vector<cell_t>{};
      paint_edit_row(bout_, *py, fs_.screen_width(), fs_.screen_length(), cells);
    }
  }

  const auto yopt = row_for_line(ed.curli, top_line(), fs_.lines_start(), fs_.lines_end());
  if (!yopt) {
    draw_bottom_bar(ed);
    return;
  }
  paint_edit_row(bout_, *yopt, fs_.screen_width(), fs_.screen_length(), ed.curline().cells());
  // Refresh the bar in case a previous full-width write wrapped onto it.
  draw_bottom_bar(ed);
}

void FsedView::repaint_frame(FsedModel& e) {
  // Repaint the header and every visible edit row.  Used when the
  // viewport scrolled, after a paste burst, or when invalidate_frame()
  // was called, so a terminal that scrolled (e.g. from autowrap on the
  // last row) gets its header back.
  draw_header();
  fs_.DrawTopBar();
  for (const auto& p : plan_edit_area_paints(top_line(), size_int(e), fs_.lines_start(),
                                             fs_.lines_end())) {
    paint_edit_row(bout_, p.y, fs_.screen_width(), fs_.screen_length(),
                   p.line < 0 ? std::vector<cell_t>{} : e.line(p.line).cells());
  }
  invalidate_bottom_bar();
  draw_bottom_bar(e);
  ClearCommandLine();
  frame_dirty_ = false;
  painted_top_line_ = top_line();
  gotoxy(e);
}

void FsedView::handle_editor_invalidate(FsedModel& e, editor_range_t t) {
  if (frame_dirty_ || painted_top_line_ != top_line()) {
    repaint_frame(e);
    return;
  }
  // Only repaint the invalidated rows.  Typing a character or changing
  // one line costs one row, not the whole screen.  Never write at or
  // below lines_end() (the status bar).
  const auto doc_lines = size_int(e);
  const bool to_eof = t.end.line >= doc_lines - 1;
  for (const auto& p : plan_range_paints(t.start.line, t.end.line, to_eof, top_line(), doc_lines,
                                         fs_.lines_start(), fs_.lines_end())) {
    paint_edit_row(bout_, p.y, fs_.screen_width(), fs_.screen_length(),
                   p.line < 0 ? std::vector<cell_t>{} : e.line(p.line).cells());
  }

  // Quote return, help, and viewport scroll all redraw lines through
  // this path, and a full-width last row can wipe the status bar.
  invalidate_bottom_bar();
  draw_bottom_bar(e);
}

void FsedView::draw_header() {
  // Position each header row with CUP, never CR/LF.  A newline issued
  // while the cursor is on the last terminal row scrolls the whole
  // screen and the header is replaced by leftover message text.
  const auto oldcuratr = bout_.curatr();
  const auto to = data_.to_name.empty() ? "All" : data_.to_name;
  const std::string lines[4] = {
      fmt::format("|#7From: |#2{}", data_.from_name),
      fmt::format("|#7To:   |#2{}", to),
      fmt::format("|#7Area: |#2{}", data_.sub_name),
      fmt::format("|#7{}|#2{}", (file_ ? "File: " : "Subj: "), data_.title),
  };
  const int last = fs_.screen_length();
  for (int i = 0; i < 4; ++i) {
    const int y = clamp_screen_row(i + 1, last);
    bout_.goxy(1, y);
    bout_.clreol();
    bout_.goxy(1, y);
    bout_.outstr(lines[i]);
    // Long From/Subj lines must not autowrap onto the next header row.
    bout_.goxy(1, y);
  }
  bout_.setc(oldcuratr);
}

void FsedView::invalidate_bottom_bar() {
  sx = sy = sl = -1;
  sline_color_ = -1;
}

void FsedView::redraw() { 
  bout_.cls();
  draw_header(); 
  fs_.DrawTopBar();
  invalidate_bottom_bar();
  fs_.DrawBottomBar("");
  ClearCommandLine();
  // No model here, so the edit rows are still blank: the next
  // invalidate must repaint the whole frame.
  frame_dirty_ = true;
}

void FsedView::redraw(const FsedModel& ed) {
  bout_.cls();
  draw_header();
  fs_.DrawTopBar();
  // reset the cache that is used by draw_bottom_bar so that
  // the bottom bar will be drawn even if the cursor position
  // has not changed.
  invalidate_bottom_bar();
  draw_bottom_bar(ed);
  ClearCommandLine();
  // The frame is fresh.  Callers follow this with invalidate_to_eof(),
  // which repaints the (now blank) edit rows.
  frame_dirty_ = false;
  painted_top_line_ = top_line();
}

void FsedView::draw_bottom_bar(const FsedModel& ed) {
  const auto sc = bout_.curatr();
  const auto line_color = ed.curline().wwiv_color();
  if (sx != ed.cx || sy != ed.cy || sl != ed.curli || smode_ != ed.mode() || sdebug_ != debug ||
      sline_color_ != line_color) {
    const auto* mode = ed.mode() == ins_ovr_mode_t::ins ? "INS" : "OVR";
    const auto cell = ed.current_cell();
    const auto text = (debug) ? fmt::format("X:{} Y:{} L:{} T:{} C:{} WC:{} WL:{} [{}]", ed.cx, ed.cy, ed.curli,
                                      top_line(), static_cast<int>(cell.ch), cell.wwiv_color, line_color, mode)
                        : fmt::format("[{}]", mode);
    fs_.DrawBottomBar(text);
    sx = ed.cx;
    sy = ed.cy;
    sl = ed.curli;
    smode_ = ed.mode();
    sdebug_ = debug;
    sline_color_ = line_color;
  }
  bout_.setc(sc);
  gotoxy(ed);
}

int FsedView::bgetch(FsedModel& ed) {
  // bgetch_event doesn't process macros by design, but
  // we want them in the FSED
  if (bin_.charbufferpointer_) {
    if (!bin_.charbuffer[bin_.charbufferpointer_]) {
      bin_.charbufferpointer_ = 0;
      bin_.charbuffer[0] = 0;
    } else {
      if (bin_.charbuffer[bin_.charbufferpointer_] == CC) {
        bin_.charbuffer[bin_.charbufferpointer_] = CP;
      }
      return bin_.charbuffer[bin_.charbufferpointer_++];
    }
  }

  return bin.bgetch_event(Input::numlock_status_t::NUMBERS, std::chrono::seconds(1),
                      [&](Input::bgetch_timeout_status_t status, int s) {
                        if (status == Input::bgetch_timeout_status_t::WARNING) {
      fs_.PrintTimeoutWarning(s);
                        } else if (status == Input::bgetch_timeout_status_t::CLEAR) {
      ClearCommandLine();
                        } else if (status == Input::bgetch_timeout_status_t::IDLE) {
      draw_bottom_bar(ed);
    }
  });
}

void FsedView::outchr(int color, char ch) {
  bout_.ansic(color);
  bout_.outchr(ch);
}

void FsedView::cls() { bout_.cls(); }

void FsedView::ansic(int c) { bout_.ansic(c); }


} // namespace wwiv::fsed
