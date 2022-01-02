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

FsedView::FsedView(const FullScreenView& fs, MessageEditorData& data, bool file)
    : fs_(fs), bout_(fs_.out()), bin_(fs_.in()), data_(data), file_(file) {
  max_view_lines_ = std::min<int>(20, fs.message_height() - 1);
  max_view_columns_ = std::min<int>(fs.screen_width(), 79);
}

FullScreenView& FsedView::fs() { return fs_; }

void FsedView::gotoxy(const FsedModel& ed) {
  bout_.GotoXY(ed.cx + 1, ed.cy + fs_.lines_start()); // - top_line() 
}

void FsedView::ClearCommandLine() { 
    fs_.PutsCommandLine("|#9(|#2ESC|#9=Menu/Help) ");
}

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
    const auto py = previous_line - top_line() + fs_.lines_start();
    bout_.GotoXY(0, py);
    if (previous_line < size_int(ed)) {
      bout_.bputs(ed.line(previous_line).to_colored_text(-1));
    }
    bout_.clreol();
  }

  const auto y = ed.curli - top_line() + fs_.lines_start(); 
  bout_.GotoXY(0, y);
  bout_.Color(0);
  for (const auto& c : ed.curline().cells()) {
    // Draw char by char for the current line so we don't display
    // color codes where we are editing.
    bout_.Color(c.wwiv_color);
    bout_.bputch(c.ch);
  }
  bout_.clreol();
  gotoxy(ed);
}

void FsedView::handle_editor_invalidate(FsedModel& e, editor_range_t t) {
  // TODO: optimize for first line

  // Never go below top line.
  const auto start_line = std::max<int>(t.start.line, top_line());
  auto last_color = -1;

  for (auto i = start_line; i <= t.end.line; i++) {
    auto y = i - top_line() + fs_.lines_start();
    if (y >= fs_.lines_end()) {
      break;
    }
    if (i >= size_int(e)) {
      break;
    }
    bout_.GotoXY(0, y);
    auto& rl = e.line(i);
    if (i == e.curli) {
      for (const auto& c : rl.cells()) {
        // Draw char by char for the current line so we don't display
        // color codes where we are editing.
        if (c.wwiv_color != last_color) {
          last_color = c.wwiv_color;
          bout_.Color(c.wwiv_color);
        }
        bout_.bputch(c.ch);
      }
    } else {
      bout_.bputs(rl.to_colored_text(-1));
    }
    bout_.clreol();
  }

  // Clean up the bottom.
  // clear the current and then remaining
  if (size_int(e) == t.end.line + 1) {
    for (auto z = size_int(e) - top_line() + fs_.lines_start(); z < fs_.lines_end(); z++) {
      bout_.GotoXY(0, z);
      bout_.clreol();
    }
  }

  gotoxy(e);
}

void FsedView::draw_header() {
  const auto oldcuratr = bout_.curatr();
  bout_.cls();
  const auto to = data_.to_name.empty() ? "All" : data_.to_name;
  bout_ << "|#7From: |#2" << data_.from_name << wwiv::endl;
  bout_ << "|#7To:   |#2" << to << wwiv::endl;
  bout_ << "|#7Area: |#2" << data_.sub_name << wwiv::endl;
  bout_ << "|#7" << (file_ ? "File: " : "Subj: ") << "|#2" << data_.title << wwiv::endl;

  bout_.SystemColor(oldcuratr);
}

void FsedView::redraw() { 
  draw_header(); 
  fs_.DrawTopBar();
  fs_.DrawBottomBar("");
  ClearCommandLine();
}

void FsedView::redraw(const FsedModel& ed) {
  draw_header();
  fs_.DrawTopBar();
  // reset the cache that is used by draw_bottom_bar so that
  // the bottom bar will be drawn even if the cursor position
  // has not changed.
  sx = sy = sl = -1;
  draw_bottom_bar(ed);
  ClearCommandLine();
}

void FsedView::draw_bottom_bar(const FsedModel& ed) {
  const auto sc = bout_.curatr();
  static auto smode = ed.mode();
  static auto sdebug = debug;
  static auto sline_color = 0;
  const auto line_color = ed.curline().wwiv_color();
  if (sx != ed.cx || sy != ed.cy || sl != ed.curli || smode != ed.mode() || sdebug != debug ||
      sline_color != line_color) {
    const auto* mode = ed.mode() == ins_ovr_mode_t::ins ? "INS" : "OVR";
    const auto cell = ed.current_cell();
    const auto text = (debug) ? fmt::format("X:{} Y:{} L:{} T:{} C:{} WC:{} WL:{} [{}]", ed.cx, ed.cy, ed.curli,
                                      top_line(), static_cast<int>(cell.ch), cell.wwiv_color, line_color, mode)
                        : fmt::format("[{}]", mode);
    fs_.DrawBottomBar(text);
    sx = ed.cx;
    sy = ed.cy;
    sl = ed.curli;
    smode = ed.mode();
    sdebug = debug;
    sline_color = line_color;
  }
  bout_.SystemColor(sc);
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

void FsedView::bputch(int color, char ch) {
  bout_.Color(color);
  bout_.bputch(ch);
}

void FsedView::cls() { bout_.cls(); }

void FsedView::Color(int c) { bout_.Color(c); }


} // namespace wwiv::fsed