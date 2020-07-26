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
#include "bbs/fsed/fsed.h"

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
#include "bbs/fsed/editor.h"
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

/////////////////////
// LOCALS

static void advance_cy(editor_t& ed, FsedView& view, bool invalidate = true) {
  // advancy cy if we have room, scroll region otherwise
  if (ed.cy < view.max_view_lines()) {
    ++ed.cy;
  } else {
    // scroll
    view.top_line = ed.curli - ed.cy;
    if (invalidate) {
      ed.invalidate_to_eof(view.top_line);
    }
  }
}

static void gotoxy(const editor_t& ed, const FullScreenView& fs) {
  bout.GotoXY(ed.cx + 1, ed.cy + fs.lines_start());
}

/////////////////////////////////////////////////////////////////////////////
// MAIN

static FsedView create_frame(MessageEditorData& data, bool file) {
  const auto screen_width = a()->user()->GetScreenChars();
  const auto screen_length = a()->user()->GetScreenLines() - 1;
  const auto num_header_lines = 4;
  auto fs = FullScreenView(bout, num_header_lines, screen_width, screen_length);
  auto view = FsedView(fs, data, file);
  view.redraw();
  return view;
}

bool fsed(const std::filesystem::path& path) {
  MessageEditorData data{};
  data.title = path.string();
  editor_t ed{};
  auto file_lines = read_file(path, ed.maxli);
  if (!file_lines.empty()) {
    ed.set_lines(std::move(file_lines));
  }

  auto save = fsed(ed, data, true);
  if (!save) {
    return false;
  }

  TextFile f(path, "wt");
  if (!f) {
    return false;
  }
  for (const auto& l : ed.to_lines()) {
    f.WriteLine(l);
  }
  return true;
}

bool fsed(std::vector<std::string>& lin, int maxli, MessageEditorData& data, bool file) {
  editor_t ed{};
  ed.maxli = maxli;
  // TODO anon
  for (auto l : lin) {
    bool wrapped = !l.empty() && l.back() == '\x1';
    ed.emplace_back(line_t{ wrapped, l });
  }
  if (!fsed(ed, data, file)) {
    return false;
  }

  lin = ed.to_lines();
  return true;
}


bool fsed(editor_t& ed, MessageEditorData& data, bool file) {
  auto view = create_frame(data, file);
  auto& fs = view.fs();
  ed.add_callback([&view](editor_t& e, editor_range_t t) {
    view.handle_editor_invalidate(e, t);
    });

  int saved_topdata = a()->topdata;
  if (a()->topdata != LocalIO::topdataNone) {
    a()->topdata = LocalIO::topdataNone;
    a()->UpdateTopScreen();
  }

  // Draw the initial contents of the file.
  ed.invalidate_to_eof(0);
  // Draw the bottom bar once to start with.
  bout.Color(0);
  view.draw_bottom_bar(ed);
  fs.GotoContentAreaTop();
  bool done = false;
  bool save = false;

  const auto keys = CreateDefaultKeyMap();
  // top editor line number in thw viewable area.
  while (!done) {
    gotoxy(ed, fs);

    const auto key = view.bgetch(ed);
    if (key < 0xff && key >= 32) {
      const auto c = static_cast<char>(key & 0xff);
      gotoxy(ed, fs);
      bout.Color(ed.curline().wwiv_color());
      bout.bputch(c);
      if (ed.add(c) == editor_add_result_t::wrapped) {
        advance_cy(ed, view);
      }
      continue;
    }

    const auto it = keys.find(key);
    if (it == std::end(keys)) {
      // No key binding
      continue;
    }
    switch (it->second) {
    case FsedCommand::cursor_up: {
      auto previous_line = ed.curli;
      if (ed.cy > 0) {
        --ed.cy;
        --ed.curli;
        const auto right_max = std::min<int>(view.max_view_columns(), ssize(ed.curline()));
        ed.cx = std::min<int>(ed.cx, right_max);
      }
      else if (ed.curli > 0) {
        // scroll
        --ed.curli;
        const auto right_max = std::min<int>(view.max_view_columns(), ssize(ed.curline()));
        ed.cx = std::min<int>(ed.cx, right_max);
        view.top_line = ed.curli;
        ed.invalidate_to_eof(view.top_line);
      }
      view.draw_current_line(ed, previous_line);
    } break;
    case FsedCommand::cursor_down: {
      auto previous_line = ed.curli;
      if (ed.curli < ssize(ed) - 1) {
        ++ed.curli;
        const auto right_max = std::min<int>(view.max_view_columns(), ssize(ed.curline()));
        ed.cx = std::min<int>(ed.cx, right_max);
        advance_cy(ed, view);
      }
      view.draw_current_line(ed, previous_line);
    } break;
    case FsedCommand::cursor_pgup: {
      auto previous_line = ed.curli;
      const auto up = std::min<int>(ed.curli, view.max_view_lines());
      // nothing to do!
      if (up == 0) {
        break;
      }
      ed.cy = std::max<int>(ed.cy - up, 0);
      ed.curli = std::max<int>(ed.curli - up, 0);
      view.top_line = ed.curli - ed.cy;
      const auto right_max = std::min<int>(view.max_view_columns(), ssize(ed.curline()));
      ed.cx = std::min<int>(ed.cx, right_max);
      ed.invalidate_to_eof(view.top_line);
      view.draw_current_line(ed, previous_line);
    } break;
    case FsedCommand::cursor_pgdown: {
      auto previous_line = ed.curli;
      const auto dn =
          std::min<int>(view.max_view_lines(), std::max<int>(0, ssize(ed) - ed.curli - 1));
      if (dn == 0) {
        // nothing to do!
        break;
      }
      ed.curli += dn;
      ed.cy += dn;
      const auto right_max = std::min<int>(view.max_view_columns(), ssize(ed.curline()));
      ed.cx = std::min<int>(ed.cx, right_max);
      if (ed.cy >= view.max_view_lines()) {
        // will need to scroll
        ed.cy = view.max_view_lines();
        view.top_line = ed.curli - ed.cy;
        ed.invalidate_to_eof(view.top_line);
      }
      view.draw_current_line(ed, previous_line);
    } break;
    case FsedCommand::cursor_left: {
      if (ed.cx > 0) {
        --ed.cx;
      }
    } break;
    case FsedCommand::cursor_right: {
      // TODO: add option to cursor right to end of view
      const auto right_max = std::min<int>(view.max_view_columns(), ssize(ed.curline()));
      if (ed.cx < right_max) {
        ++ed.cx;
      }
    } break;
    case FsedCommand::cursor_home:{
      ed.cx = 0;
    } break;
    case FsedCommand::delete_line: {
      if (ed.remove_line()) {
        ed.invalidate_to_eof(ed.curli);
      }
      view.draw_current_line(ed, ed.curli);
    } break;
    case FsedCommand::cursor_end:{
      ed.cx = ssize(ed.curline());
    } break;
    case FsedCommand::delete_to_eol: {
      if (ed.cx < ssize(ed.curline())) {
        auto& oline = ed.curline();
        oline.assign(oline.substr(0, ed.cx));
        oline.wrapped(false);
        ed.invalidate_to_eol();
      } else if (ssize(ed.curline()) == 0) {
        // delete line
        if (ed.remove_line()) {
          ed.invalidate_to_eof(ed.curli);
          view.draw_current_line(ed, ed.curli);
        }
      }
    } break;
    case FsedCommand::view_redraw: { // redraw
      view.redraw();
      ed.invalidate_to_eof(0);
    } break;
    case FsedCommand::delete_right: {
      // TODO keep mode state;
      ed.del();
      gotoxy(ed, fs);
      ed.invalidate_to_eol();
    } break;
    case FsedCommand::backspace: {
      auto previous_line = ed.curli;
      // TODO keep mode state;
      ed.bs();
      if (ed.cx > 0) {
        --ed.cx;
      } else if (ed.curli > 0 && ssize(ed.curline()) == 0) {
        // If current line is empty then delete it and move up one line
        if (ed.remove_line()) {
          --ed.cy;
          --ed.curli;
          ed.cx = ssize(ed.curline());
          ed.invalidate_to_eof(ed.curli);
        }
      } else if (ed.curli > 0) {
        auto& prev = ed.line(ed.curli - 1);
        auto& cur = ed.curline();
        auto last_pos = std::max<int>(0, ed.max_line_len - ssize(prev) - 1);
        const int new_cx = ssize(prev);
        if (ssize(cur) < last_pos) {
          prev.append(cur.cells());
          ed.remove_line();
        } else if (int space = cur.last_space_before(last_pos) > 0) {
          prev.append(cur.substr(0, space));
          cur.assign(cur.substr(space));
        }
        --ed.cy;
        --ed.curli;
        ed.cx = new_cx;
        ed.invalidate_to_eof(ed.curli);
      }
      view.draw_current_line(ed, previous_line);
      gotoxy(ed, fs);
      auto c = ed.current_cell();
      bout.Color(c.wwiv_color);
      bout.bputch(c.ch);
    } break;
    case FsedCommand::key_return: {
      int orig_start_line = ed.curli;
      ed.curline().wrapped(false);
      // Insert inserts after the current line
      if (ed.cx >= ssize(ed.curline())) {
        ++ed.curli;
        ed.insert_line();
      } else {
        // Wrap
        auto& oline = ed.curline();
        auto ntext = oline.substr(ed.cx);
        oline.assign(oline.substr(0, ed.cx));
        oline.wrapped(false);

        ++ed.curli;
        ed.insert_line();
        ed.curline().assign(ntext);
      }
      advance_cy(ed, view);
      ed.cx = 0;
      ed.invalidate_to_eof(orig_start_line);
      view.draw_current_line(ed, orig_start_line);
    } break;
    case FsedCommand::input_wwiv_color: {
      auto cc = bout.getkey();
      if (cc >= '0' && cc <= '9') {
        ed.curline().set_wwiv_color(cc - '0');
      }
      view.draw_current_line(ed, ed.curli);
    } break;
    case FsedCommand::menu: {
      bout.PutsXY(
          1, fs.command_line_y(),
          "|#9(|#2ESC|#9=Return, |#2A|#9=Abort, |#2Q|#9=Quote, |#2S|#9=Save, |#2?|#9=Help): ");
      switch (fs.bgetch()) { 
      case 's':
        [[fallthrough]];
      case 'S': {
        done = true;
        save = true;
      } break;
      case 'a':
        [[fallthrough]];
      case 'A': {
        done = true;
      } break;
      case 'q':
      case 'Q': {
        // Hacky quote solution for now.
        // TODO(rushfan): Do something less lame here.
        bout.cls();
        auto quoted_lines = query_quote_lines();
        if (quoted_lines.empty()) {
          break;
        }
        while (!quoted_lines.empty()) {
          // Insert all quote lines.
          const auto ql = quoted_lines.front();
          quoted_lines.pop_front();
          ++ed.curli;
          ed.insert_line();
          ed.curline() = line_t(ql);
          advance_cy(ed, view, false);
        }
        // Add blank line afterwards to use to start new text.
        ++ed.curli;
        ed.insert_line();
        advance_cy(ed, view, false);

        // Redraw everything, the whole enchilada!
        view.redraw();
        ed.invalidate_to_eof(0);
      } break;
      case '?': {
        fs.ClearMessageArea();
        if (!print_help_file(FSED_NOEXT)) {
          fs.ClearCommandLine();
          bout << "|#6Unable to find file: " << FSED_NOEXT;
        } else {
          fs.ClearCommandLine();
        }
        pausescr();
        fs.ClearMessageArea();
        view.redraw();
        ed.invalidate_to_eof(0);
      } break;
      case ESC:
        [[fallthrough]];
      default: {
        fs.ClearCommandLine();
      } break;
      }
    } break;
    case FsedCommand::toggle_insovr: {
      ed.toggle_ins_ovr_mode();
    } break;
    default: {
    } break;
    } // switch

  }

  a()->topdata = saved_topdata;
  a()->UpdateTopScreen();

  return save;
}

FsedView::FsedView(FullScreenView fs, MessageEditorData& data, bool file)
    : fs_(std::move(fs)), data_(data), file_(file) {
  max_view_lines_ = std::min<int>(20, fs.message_height() - 1);
  max_view_columns_ = std::min<int>(fs.screen_width(), 79);
}

FullScreenView& FsedView::fs() { return fs_; }

void FsedView::gotoxy(const editor_t& ed) {
  bout.GotoXY(ed.cx + 1, ed.cy - top_line + fs_.lines_start());
}

void FsedView::draw_current_line(editor_t& ed, int previous_line) { 
  if (previous_line != ed.curli) {
    auto py = previous_line - top_line + fs_.lines_start();
    bout.GotoXY(0, py);
    if (previous_line < ssize(ed)) {
      bout.bputs(ed.line(previous_line).to_colored_text());
    }
    bout.clreol();
  }

  auto y = ed.curli - top_line + fs_.lines_start(); 
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

void FsedView::handle_editor_invalidate(editor_t& e, editor_range_t t) {
  // TODO: optimize for first line

  // Never go below top line.
  auto start_line = std::max<int>(t.start.line, top_line);
  auto last_color = -1;

  for (int i = start_line; i <= t.end.line; i++) {
    auto y = i - top_line + fs_.lines_start();
    if (y > fs_.lines_end() || i >= ssize(e)) {
      // clear the current and then remaining
      bout.GotoXY(0, y);
      bout.clreol();
      for (auto z = t.end.line + 1 - top_line + fs_.lines_start(); z < fs_.lines_end(); z++) {
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

void FsedView::draw_bottom_bar(editor_t& ed) {
  auto sc = bout.curatr();
  static int sx = -1, sy = -1, sl = -1;
  if (sx != ed.cx || sy != ed.cy || sl != ed.curli) {
    const auto mode = ed.mode() == ins_ovr_mode_t::ins ? "INS" : "OVR";
    const auto cell = ed.current_cell();
    fs_.DrawBottomBar(fmt::format("X:{} Y:{} L:{} T:{} C:{} W:{} [{}]", ed.cx, ed.cy, ed.curli, top_line,
                                  static_cast<int>(cell.ch), cell.wwiv_color, mode));
    sx = ed.cx;
    sy = ed.cy;
    sl = ed.curli;
  }
  bout.SystemColor(sc);
  gotoxy(ed);
}

int FsedView::bgetch(editor_t& ed) {
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