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
#include "bbs/fsed/view.h"

#include "bbs/bbs.h"
#include "bbs/bgetch.h"
#include "bbs/full_screen.h"
#include "bbs/message_editor_data.h"
#include "bbs/output.h"
#include "bbs/pause.h"
#include "bbs/printfile.h"
#include "bbs/quote.h"
#include "bbs/fsed/commands.h"
#include "bbs/fsed/common.h"
#include "bbs/fsed/model.h"
#include "bbs/fsed/line.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/format.h"
#include "local_io/keycodes.h"
#include "sdk/filenames.h"

namespace wwiv::bbs::fsed {

using namespace wwiv::stl;
using namespace wwiv::strings;

FsedView::FsedView(FullScreenView fs, MessageEditorData& data, bool file)
    : fs_(std::move(fs)), data_(data), file_(file) {
  max_view_lines_ = std::min<int>(20, fs.message_height() - 1);
  max_view_columns_ = std::min<int>(fs.screen_width(), 79);
}

FullScreenView& FsedView::fs() { return fs_; }

void FsedView::gotoxy(const FsedModel& ed) {
  bout.GotoXY(ed.cx + 1, ed.cy + fs_.lines_start()); // - top_line() 
}

void FsedView::draw_current_line(FsedModel& ed, int previous_line) { 
  if (previous_line != ed.curli) {
    auto py = previous_line - top_line() + fs_.lines_start();
    bout.GotoXY(0, py);
    if (previous_line < ssize(ed)) {
      bout.bputs(ed.line(previous_line).to_colored_text());
    }
    bout.clreol();
  }

  auto y = ed.curli - top_line() + fs_.lines_start(); 
  bout.GotoXY(0, y);
  bout.Color(0);
  for (const auto& c : ed.curline().cells()) {
    // Draw char by char for the current line so we don't display
    // color codes where we are editing.
    bout.Color(c.wwiv_color);
    bout.bputch(c.ch);
  }
  bout.clreol();
  gotoxy(ed);
}

void FsedView::handle_editor_invalidate(FsedModel& e, editor_range_t t) {
  // TODO: optimize for first line

  // Never go below top line.
  auto start_line = std::max<int>(t.start.line, top_line());
  auto last_color = -1;

  for (int i = start_line; i < t.end.line; i++) {
    auto y = i - top_line() + fs_.lines_start();
    if (y >= fs_.lines_end()) {
      break;
    }
    if (i >= ssize(e)) {
      // clear the current and then remaining
      bout.GotoXY(0, y);
      bout.clreol();
      for (auto z = t.end.line + 1 - top_line() + fs_.lines_start(); z < fs_.lines_end(); z++) {
        bout.GotoXY(0, z);
        bout.clreol();
      }
      break;
    }
    bout.GotoXY(0, y);
    auto& rl = e.line(i);
    if (i == e.curli) {
      for (const auto& c : rl.cells()) {
        // Draw char by char for the current line so we don't display
        // color codes where we are editing.
        if (c.wwiv_color != last_color) {
          last_color = c.wwiv_color;
          bout.Color(c.wwiv_color);
        }
        bout.bputch(c.ch);
      }
    } else {
      bout.bputs(rl.to_colored_text());
    }
    bout.clreol();
  }
  gotoxy(e);
}

void FsedView::redraw() {
  auto oldcuratr = bout.curatr();
  bout.cls();
  auto to = data_.to_name.empty() ? "All" : data_.to_name;
  bout << "|#7From: |#2" << data_.from_name << wwiv::endl;
  bout << "|#7To:   |#2" << to << wwiv::endl;
  bout << "|#7Area: |#2" << data_.sub_name << wwiv::endl;
  bout << "|#7" << (file_ ? "File: " : "Subj: ") << "|#2" << data_.title << wwiv::endl;

  bout.SystemColor(oldcuratr);
  fs_.DrawTopBar();
  fs_.DrawBottomBar("");
}

void FsedView::draw_bottom_bar(FsedModel& ed) {
  auto sc = bout.curatr();
  static int sx = -1, sy = -1, sl = -1;
  static auto smode = ed.mode();
  static auto sdebug = debug;
  if (sx != ed.cx || sy != ed.cy || sl != ed.curli || smode != ed.mode() || sdebug != debug) {
    const auto mode = ed.mode() == ins_ovr_mode_t::ins ? "INS" : "OVR";
    const auto cell = ed.current_cell();
    auto text = (debug) ? fmt::format("X:{} Y:{} L:{} T:{} C:{} W:{} [{}]", ed.cx, ed.cy, ed.curli,
                                      top_line(), static_cast<int>(cell.ch), cell.wwiv_color, mode)
                        : fmt::format("[{}]", mode);
    fs_.DrawBottomBar(text);
    sx = ed.cx;
    sy = ed.cy;
    sl = ed.curli;
    smode = ed.mode();
    sdebug = debug;
  }
  bout.SystemColor(sc);
  gotoxy(ed);
}

int FsedView::bgetch(FsedModel& ed) {
  return bgetch_event(numlock_status_t::NUMBERS, std::chrono::seconds(1), [&](bgetch_timeout_status_t status, int s) {
    if (status == bgetch_timeout_status_t::WARNING) {
      fs_.PrintTimeoutWarning(s);
    } else if (status == bgetch_timeout_status_t::CLEAR) {
      fs_.ClearCommandLine();
    } else if (status == bgetch_timeout_status_t::IDLE) {
      draw_bottom_bar(ed);
    }
  });
}

} // namespace wwiv::bbs::fsed