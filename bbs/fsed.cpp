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
#include "bbs/fsed.h"

#include "bbs/bbs.h"
#include "bbs/bgetch.h"
#include "bbs/full_screen.h"
#include "bbs/output.h"
#include "bbs/pause.h"
#include "core/stl.h"
#include "fmt/format.h"
#include "local_io/keycodes.h"

namespace wwiv::bbs::fsed {

using namespace wwiv::stl;

bool line_t::add(int x, char c, ins_ovr_mode_t mode) {
  while (ssize(text) < x) {
    text.push_back(' ');
  }
  if (x == ssize(text)) {
    text.push_back(c);
  } else if (mode == ins_ovr_mode_t::ins) {
    return wwiv::stl::insert_at(text, x, c);
  } else {
    text[x] = c;
  }
  return true;
}

bool line_t::del(int x, ins_ovr_mode_t) {
  if (x < 0) {
    return false;
  }
  return wwiv::stl::erase_at(text, x);
}

bool line_t::bs(int x, ins_ovr_mode_t mode) { 
  if (x <= 0) {
    return false;
  }
  return del(x - 1, mode);
}

int line_t::size() const { 
return ssize(text);
}

line_t& editor_t::curline() {
  // TODO: insert return statement here
  while (curli >= ssize(lines)) {
    lines.emplace_back();
  }
  return lines.at(curli);
}

bool editor_t::insert_line() { 
  if (ssize(lines)  >= this->maxli) {
    return false;
  }
  wwiv::stl::insert_at(lines, curli, line_t{});
  return true;
}

bool editor_t::remove_line() { 
  if (lines.empty()) {
    return false;
  }
  wwiv::stl::erase_at(lines, curli);
  return true;
}


bool editor_t::add(char c) { 
  auto& line = curline(); 
  if (!line.add(cx, c, mode_)) {
    return false;
  }
  ++cx;
  if (cx >= max_line_len) {
    // wrap char
    // TODO(rushfan): Add word wrapping here.
    line.wrapped = true;
    ++curli;
    cx %= max_line_len;
  }
  return true;
}

char editor_t::current_char() {
  auto& line = curline();
  if (cx >= wwiv::stl::size_int16(line.text)) {
    return ' ';
  }
  return line.text.at(cx);
}

bool editor_t::del() { 
  auto& line = curline();
  if (!line.del(cx, mode_)) {
    return false;
  }
  return true;
}

bool editor_t::bs() { 
  auto& line = curline();
  if (!line.bs(cx, mode_)) {
    return false;
  }
  return true;
}

bool editor_t::add_callback(editor_range_invalidated_fn fn) {
  callbacks_.emplace_back(fn);
  return true;
}
void editor_t::invalidate_to_eol() {
  editor_range_t r{};
  r.start.line = r.end.line = curli;
  r.start.x = cx;
  r.end.x = ssize(curline().text);
  for (auto& c : callbacks_) {
    c(*this, r);
  }
}

void editor_t::invalidate_to_eof() {
  invalidate_to_eof(curli);
}

void editor_t::invalidate_to_eof(int start_line) {
  editor_range_t r{};
  r.start.line = start_line;
  r.start.x = cx;
  r.end.line = ssize(lines) - 1;
  r.end.x = 0;
  for (auto& c : callbacks_) {
    c(*this, r);
  }
}


static FsedView create_frame(const std::filesystem::path& path) {
  auto oldcuratr = bout.curatr();
  const auto screen_width = a()->user()->GetScreenChars();
  const auto screen_length = a()->user()->GetScreenLines() - 1;
  bout.cls();
  bout << "From: " << wwiv::endl;
  bout << "to:   " << wwiv::endl;
  bout << "Area: " << wwiv::endl;
  bout << "Subj: " << path << wwiv::endl;
  const auto num_header_lines = 4;

  bout.curatr(oldcuratr);
  auto fs = FullScreenView(bout, num_header_lines, screen_width, screen_length);
  fs.DrawTopBar();
  fs.DrawBottomBar("Foo");
  return FsedView(fs);
}

void gotoxy(const editor_t& ed, const FullScreenView& fs) { 
  bout.GotoXY(ed.cx + 1, ed.cy + fs.lines_start());
}

bool fsed(const std::filesystem::path& path) { 
  auto view = create_frame(path);
  auto& fs = view.fs();
  editor_t ed{};
  ed.add_callback([&view](editor_t& e, editor_range_t t) {
    view.handle_editor_invalidate(e, t); });

  int saved_topdata = a()->topdata;
  if (a()->topdata != LocalIO::topdataNone) {
    a()->topdata = LocalIO::topdataNone;
    a()->UpdateTopScreen();
  }

  fs.GotoContentAreaTop();
  bool done = false;
  while (!done) {
    auto key = fs.bgetch();
    switch (key) {
    case COMMAND_UP: {
      if (ed.cy > 0) {
        --ed.cy;
        --ed.curli;
      }
    } break;
    case COMMAND_DOWN: {
      if (ed.cy < view.max_view_lines()) {
        ++ed.cy;
      } else {
        // TODO: scroll region.
      }
      ++ed.curli;
    } break;
    case COMMAND_LEFT: {
      if (ed.cx > 0) {
        --ed.cx;
      }
    } break;
    case COMMAND_RIGHT: {
      if (ed.cx < view.max_view_columns()) {
        ++ed.cx;
      }
    } break;
    case COMMAND_DELETE: {
      // TODO keep mode state;
      ed.del();
      gotoxy(ed, fs);
      //bout.bputch(ed.current_char());
      ed.invalidate_to_eol();
    } break;
    case BACKSPACE: {
      // TODO keep mode state;
      ed.bs();
      if (ed.cx > 0) {
        --ed.cx;
      }
      gotoxy(ed, fs);
      bout.bputch(ed.current_char());
    } break;
    case RETURN: {
      int orig_start_line = ed.curli;
      ed.curline().wrapped = false;
      // Insert inserts after the current line
      if (ed.cx >= ed.curline().size()) {
        ++ed.cy;
        ++ed.curli;
        ed.insert_line();
      } else {
        // Wrap
        auto& oline = ed.curline();
        auto ntext = oline.text.substr(ed.cx);
        oline.text = oline.text.substr(0, ed.cx);
        oline.wrapped = false;

        ++ed.cy;
        ++ed.curli;
        ed.insert_line();
        ed.curline().text = ntext;
      }
      ed.cx = 0;
      ed.invalidate_to_eof(orig_start_line);
    } break;
    case ESC: {
      bout.PutsXY(1, fs.command_line_y(), "|#9(|#2ESC|#9=Return, |#2A|#9=Abort, |#2S|#9=Save, |#2?|#9=Help): ");
      switch (fs.bgetch()) { 
      case 's':
      case 'S': {
        done = true;
      } break;
      case 'a':
      case 'A': {
        done = true;
      } break;
      case ESC:
      default:{
        fs.ClearCommandLine();
      } break;
      }
    } break;
    default: {
      if (key < 0xff) {
        const auto c = static_cast<char>(key & 0xff);
        gotoxy(ed, fs);
        bout.bputch(c);
        ed.add(c);
      }
    } break;
    } // switch
    fs.DrawBottomBar(fmt::format("X:{} Y:{} L:{} C:{}", ed.cx, ed.cy, ed.curli,
                                 static_cast<int>(ed.current_char())));
    gotoxy(ed, fs);
  }

  bout.cls();
  bout << "Text:" << wwiv::endl;
  for (const auto& l : ed.lines) {
    bout << l.text << wwiv::endl;
  }

  a()->topdata = saved_topdata;
  a()->UpdateTopScreen();
  a()->ExitBBSImpl(0, true);

  return true;
}


FsedView::FsedView(FullScreenView fs) : fs_(std::move(fs)) {
  max_view_lines_ = std::min<int>(20, fs.message_height() - 1);
  max_view_columns_ = std::min<int>(fs.screen_width(), 79);
}

FullScreenView& FsedView::fs() { return fs_; }

void FsedView::gotoxy(const editor_t& ed) {
  bout.GotoXY(ed.cx + 1, ed.cy - top_line + fs_.lines_start());
}

void FsedView::handle_editor_invalidate(editor_t& e, editor_range_t t) {
  // TODO: optimize for first line

  for (int i = t.start.line; i <= t.end.line; i++) {
    auto y = i - top_line + fs_.lines_start();
    if (y > fs_.lines_end()) {
      break;
    }
    bout.GotoXY(0, y);
    auto& rl = e.lines.at(i);
    bout.bputs(rl.text);
    bout.clreol();
  }
  gotoxy(e);
}


} // namespace wwiv::bbs::fsed
