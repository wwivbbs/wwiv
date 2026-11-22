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
static void paint_edit_row(Output& bout, int y, int screen_width, int screen_length,
                           const std::vector<cell_t>& cells) {
  // Never paint on or past the last terminal row (the command line).
  // A full-width write there autowraps and scrolls the header off the top.
  if (row_below_frame(y, screen_length) || y >= screen_length) {
    return;
  }
  y = clamp_screen_row(y, screen_length);
  bout.goxy(1, y);
  bout.clreol();
  bout.goxy(1, y);
  // Never write more cells than the wrap width.  A line that somehow
  // grew past max_line_len would otherwise autowrap onto the next row
  // and be painted over by the following model line.
  const int max_cells = editor_wrap_columns(screen_width);
  int last_color = -1;
  int written = 0;
  for (const auto& c : cells) {
    if (written >= max_cells) {
      break;
    }
    if (c.wwiv_color != last_color) {
      last_color = c.wwiv_color;
      bout.ansic(c.wwiv_color);
    }
    bout.outchr(c.ch);
    ++written;
  }
  if (written >= screen_width - 1) {
    bout.goxy(1, y);
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

void FsedView::handle_editor_invalidate(FsedModel& e, editor_range_t) {
  // Always repaint the header and every visible edit row.  If the
  // terminal scrolled (a newline or a wrap on the last row shifts the
  // whole screen up), the header is otherwise never rewritten and
  // leftover message text stays in rows 1-5.
  draw_header();
  fs_.DrawTopBar();

  // Never write at or below lines_end() (the status bar).
  const auto paints = plan_edit_area_paints(top_line(), size_int(e),
                                            fs_.lines_start(), fs_.lines_end());

  for (const auto& p : paints) {
    if (!row_in_edit_area(p.y, fs_.lines_start(), fs_.lines_end())) {
      continue;
    }
    if (p.line < 0) {
      paint_edit_row(bout_, p.y, fs_.screen_width(), fs_.screen_length(), {});
      continue;
    }
    paint_edit_row(bout_, p.y, fs_.screen_width(), fs_.screen_length(), e.line(p.line).cells());
  }

  // Quote return, help, and viewport scroll all redraw lines through
  // this path, and a full-width last row can wipe the status bar.
  invalidate_bottom_bar();
  draw_bottom_bar(e);
  // Rewrite the command line (the last terminal row) carefully, then
  // park the cursor back in the edit area so a later write cannot
  // autowrap off the bottom and scroll the screen.
  ClearCommandLine();
  clear_below_frame();
  gotoxy(e);
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

void FsedView::clear_below_frame() {
  // The command line is the last row of the frame (row 24 on an 80x24
  // terminal).  Do not CUP to row 25 or beyond: Output::goxy only clamps
  // to the *session* line count (often the stock 25), so a 24-row web
  // terminal can be sent "row 25", which scrolls the whole screen and
  // wipes the header.  There is nothing below the command line to clear
  // on a frame that already fills the terminal.
  const int max_y = fs_.screen_length();
  const int first = fs_.command_line() + 1;
  for (int y = first; y <= first + 2; ++y) {
    if (row_below_frame(y, max_y)) {
      break;
    }
    bout_.goxy(1, clamp_screen_row(y, max_y));
    bout_.clreol();
  }
}

void FsedView::redraw() { 
  bout_.cls();
  draw_header(); 
  fs_.DrawTopBar();
  invalidate_bottom_bar();
  fs_.DrawBottomBar("");
  ClearCommandLine();
  clear_below_frame();
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
  clear_below_frame();
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
