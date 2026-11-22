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
#include "fsed/fsed.h"

#include "common/full_screen.h"
#include "common/input.h"
#include "common/message_editor_data.h"
#include "common/output.h"
#include "core/eventbus.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fsed/commands.h"
#include "fsed/common.h"
#include "fsed/line.h"
#include "fsed/model.h"
#include "fsed/newline.h"
#include "fsed/view.h"
#include "fsed/view_rows.h"
#include "local_io/keycodes.h"

#include <memory>
#include <string>

using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::local::io;
using namespace wwiv::stl;
using namespace wwiv::strings;

namespace wwiv::fsed {

static std::shared_ptr<FsedView> create_frame(MessageEditorData& data, bool file, const wwiv::sdk::User* user) {
  const auto screen_width = (user != nullptr) ? user->screen_width() : 80;
  // Do not blindly use screen_lines()-1: a user who already set 24 to
  // match the web terminal would get a 23-row frame and leftover text
  // on the two rows under the status bar.  A 25-line default still
  // becomes 24, which fills an 80x24 web client.
  const auto screen_length = fullscreen_length_for_user_lines(
      (user != nullptr) ? user->screen_lines() : 24);
  const auto num_header_lines = 4;
  auto fs = FullScreenView(bout, bin, num_header_lines, screen_width, screen_length);
  auto view = std::make_shared<FsedView>(fs, data, file);
  view->redraw();
  return view;
}

bool fsed(Context& ctx, const std::filesystem::path& path) {
  MessageEditorData data("<<NO USERNAME>>"); // anonymous username
  data.title = path.string();
  FsedModel ed(1000);
  auto file_lines = read_file(path, ed.maxli());
  if (!file_lines.empty()) {
    ed.set_lines(std::move(file_lines));
  }

  auto save = fsed(ctx, ed, data, true);
  if (!save) {
    return false;
  }

  TextFile f(path, "wt");
  if (!f) {
    return false;
  }
  for (const auto& l : ed.to_lines(true)) {
    f.WriteLine(l);
  }
  return true;
}

bool fsed(Context& ctx, std::vector<std::string>& lin, int maxli, MessageEditorData& data,
          bool file) {
  const auto saved_mci_enabled = bout.mci_enabled();
  const auto saved_okskey = bin.okskey();
  auto at_exit = finally([=] {
    bout.set_mci_enabled(saved_mci_enabled);
    bin.okskey(saved_okskey);
  });
  bout.disable_mci();
  bin.okskey(false);

  FsedModel ed(maxli);
  for (auto l : lin) {
    const auto wrapped = !l.empty() && l.back() == '\x1';
    ed.emplace_back(line_t{ wrapped, l });
  }
  if (!fsed(ctx, ed, data, file)) {
    return false;
  }

  lin = ed.to_lines(true);
  return true;
}

bool fsed(Context& ctx, FsedModel& ed, MessageEditorData& data, bool file) {
  const auto saved_mci_enabled = bout.mci_enabled();
  const auto saved_okskey = bin.okskey();
  auto at_exit = finally([=] {
    bout.set_mci_enabled(saved_mci_enabled);
    bin.okskey(saved_okskey);
  });
  bout.disable_mci();
  bin.okskey(false);

  auto view = create_frame(data, file, &ctx.u());
  ed.set_view(view);
  auto& fs = view->fs();
  // Wrap at width-1 so a full line cannot autowrap onto the status bar.
  ed.set_max_line_len(editor_wrap_columns(view->max_view_columns()));
  ed.add_callback([&view](FsedModel& e, editor_range_t t) {
    view->handle_editor_invalidate(e, t);
    });
  ed.add_callback([&view](FsedModel& e, int previous_line) { 
      view->draw_current_line(e, previous_line);
    });

  // Draw the initial contents of the file.
  ed.invalidate_to_eof(0);
  // Draw the bottom bar once to start with.
  view->ansic(0);
  view->draw_bottom_bar(ed);
  fs.GotoContentAreaTop();
  FsedState state{};

  FsedCommands commands(ctx, data);
  // Add the menu command since that needs the state variables
  // from here.

  // So a CRLF paste does not insert a blank line after every line, and a
  // lone LF (Unix clipboard) still starts a new paragraph.
  bool last_was_cr = false;

  // top editor line number in thw viewable area.
  while (!state.done) {
    // When a paste burst (or any run of keys) has drained, repaint every
    // visible row before waiting for the next key.  That clears leftover
    // terminal autowrap from a long unwrapped line and restores the
    // header if anything still scrolled.
    if (!bin.bkbhit()) {
      ed.invalidate_to_eof(view->top_line());
    }
    view->gotoxy(ed);

    const auto key = view->bgetch(ed);
    if (key == RETURN || key == SOFTRETURN) {
      if (consume_line_break(key, last_was_cr)) {
        commands.InvokeCommand(fsed_command_id::key_return, ed, *view, state);
      }
      continue;
    }
    last_was_cr = false;
    if (key < 0xff && key >= 32) {
      const auto c = static_cast<char>(key & 0xff);

      // '/' at column 0 of an empty line opens the same menu as ESC, so
      // line-editor muscle memory still works (/S save, /Q quote, /A abort).
      // On a line that already has text, '/' is inserted as a normal
      // character so paths and comments can be typed at the start of a line.
      if (c == '/' && ed.cx == 0 && size_int(ed.curline()) == 0) {
        if (commands.InvokeCommand(fsed_command_id::menu, ed, *view, state)) {
          continue;
        }
      }
      // Do not echo a character that would wrap from the last column onto
      // the next row (the status bar, if this is the last edit row).  Let
      // add() reflow and the viewport redraw handle the display instead.
      if (ed.cx < ed.max_line_len()) {
        view->gotoxy(ed);
        view->outchr(ed.curline().wwiv_color(), c);
      }
      ed.add(c);
      continue;
    }
    if (!commands.TryInterpretChar(key, ed, *view, state)) {
      LOG(ERROR) << "Unable to handle key: " << key;
    }
  }

  // Leave a clean screen so the caller can print "Aborted." / "Saving..."
  // without leftover editor text on the same lines.
  view->cls();
  return state.save;
}

} // namespace
