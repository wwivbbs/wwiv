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
#include "bbs/fsed/model.h"
#include "bbs/fsed/line.h"
#include "bbs/fsed/view.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/format.h"
#include "local_io/keycodes.h"
#include "sdk/filenames.h"

namespace wwiv::bbs::fsed {

using namespace wwiv::stl;
using namespace wwiv::strings;

static std::shared_ptr<FsedView> create_frame(MessageEditorData& data, bool file) {
  const auto screen_width = a()->user()->GetScreenChars();
  const auto screen_length = a()->user()->GetScreenLines() - 1;
  const auto num_header_lines = 4;
  auto fs = FullScreenView(bout, num_header_lines, screen_width, screen_length);
  auto view = std::make_shared<FsedView>(fs, data, file);
  view->redraw();
  return view;
}

bool fsed(const std::filesystem::path& path) {
  MessageEditorData data{};
  data.title = path.string();
  FsedModel ed(1000);
  auto file_lines = read_file(path, ed.maxli());
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
  FsedModel ed(maxli);
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

bool fsed(FsedModel& ed, MessageEditorData& data, bool file) {
  auto view = create_frame(data, file);
  ed.set_view(view);
  auto& fs = view->fs();
  ed.set_max_line_len(view->max_view_columns());
  ed.add_callback([&view](FsedModel& e, editor_range_t t) {
    view->handle_editor_invalidate(e, t);
    });
  ed.add_callback([&view](FsedModel& e, int previous_line) { 
      view->draw_current_line(e, previous_line);
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
  view->draw_bottom_bar(ed);
  fs.GotoContentAreaTop();
  bool done = false;
  bool save = false;

  const auto keys = CreateDefaultKeyMap();
  // top editor line number in thw viewable area.
  while (!done) {
    view->gotoxy(ed);

    const auto key = view->bgetch(ed);
    if (key < 0xff && key >= 32) {
      const auto c = static_cast<char>(key & 0xff);
      view->gotoxy(ed);
      bout.Color(ed.curline().wwiv_color());
      bout.bputch(c);
      ed.add(c);
      continue;
    }

    const auto it = keys.find(key);
    if (it == std::end(keys)) {
      // No key binding
      continue;
    }
    switch (it->second) {
    case FsedCommand::cursor_up: {
      ed.cursor_up();
    } break;
    case FsedCommand::cursor_down: {
      ed.cursor_down();
    } break;
    case FsedCommand::cursor_pgup: {
      ed.cursor_pgup();
    } break;
    case FsedCommand::cursor_pgdown: {
      ed.cursor_pgdown();
    } break;
    case FsedCommand::cursor_left: {
      ed.cursor_left();
    } break;
    case FsedCommand::cursor_right: {
      ed.cursor_right();
    } break;
    case FsedCommand::cursor_home:{
      ed.cx = 0;
    } break;
    case FsedCommand::delete_line: {
      ed.delete_line();
    } break;
    case FsedCommand::cursor_end:{
      ed.cursor_end();
    } break;
    case FsedCommand::delete_to_eol: {
      ed.delete_to_eol();
    } break;
    case FsedCommand::delete_line_left: {
      ed.delete_line_left();
    } break;
    case FsedCommand::delete_word_left: {
      ed.delete_word_left();
    } break;
    case FsedCommand::view_redraw: { // redraw
      view->redraw();
      ed.invalidate_to_eof(0);
    } break;
    case FsedCommand::delete_right: {
      ed.delete_right();
    } break;
    case FsedCommand::backspace: {
      ed.bs();
      bout.Color(ed.curline().wwiv_color());
      bout.bputch(ed.current_cell().ch);
    } break;
    case FsedCommand::key_return: {
      ed.enter();
    } break;
    case FsedCommand::input_wwiv_color: {
      auto cc = bout.getkey();
      if (cc >= '0' && cc <= '9') {
        ed.curline().set_wwiv_color(cc - '0');
      }
      ed.current_line_dirty(ed.curli);
    } break;
    case FsedCommand::menu: {
      bout.PutsXY(
          1, fs.command_line_y(),
          "|#9(|#2ESC|#9=Return, |#2A|#9=Abort, |#2Q|#9=Quote, |#2S|#9=Save, |#2D|#9=Debug, |#2?|#9=Help): ");
      auto cmd = std::toupper(fs.bgetch() & 0xff);
      fs.ClearCommandLine();
      bout.GotoXY(1, fs.command_line_y());
      switch (cmd) { 
      case 'S': {
        done = true;
        save = true;
      } break;
      case 'A': {
        done = true;
      } break;
      case 'D': {
        view->debug = !view->debug;
        view->draw_bottom_bar(ed);
        fs.ClearCommandLine();
      } break;
      case 'Q': {
        // Hacky quote solution for now.
        // TODO(rushfan): Do something less lame here.
        bout.cls();
        auto quoted_lines = query_quote_lines();
        if (quoted_lines.empty()) {
          break;
        }
        ed.insert_lines(quoted_lines);
        view->redraw();
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
        view->redraw();
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
      view->draw_bottom_bar(ed);
    } break;
    default: {
    } break;
    } // switch

  }

  a()->topdata = saved_topdata;
  a()->UpdateTopScreen();

  return save;
}

} // namespace wwiv::bbs::fsed