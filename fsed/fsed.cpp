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
#include "fsed/fsed.h"

#include "common/full_screen.h"
#include "common/input.h"
#include "common/message_editor_data.h"
#include "common/output.h"
#include "core/eventbus.h"
#include "core/scope_exit.h"
#include "fsed/commands.h"
#include "fsed/common.h"
#include "fsed/model.h"
#include "fsed/line.h"
#include "fsed/view.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/format.h"

using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::strings;

namespace wwiv::fsed {

static std::shared_ptr<FsedView> create_frame(MessageEditorData& data, bool file, const wwiv::sdk::User* user) {
  const auto screen_width = (user != nullptr) ? user->GetScreenChars() : 80;
  const auto screen_length = (user != nullptr) ? user->GetScreenLines() - 1 : 24;
  const auto num_header_lines = 4;
  auto fs = FullScreenView(bout, bin, num_header_lines, screen_width, screen_length);
  auto view = std::make_shared<FsedView>(fs, data, file);
  view->redraw();
  return view;
}

bool fsed(Context& ctx, const std::filesystem::path& path) {
  const auto saved_mci_enabled = bout.mci_enabled();
  ScopeExit at_exit([=] { bout.set_mci_enabled(saved_mci_enabled); });
  bout.disable_mci();
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
  for (const auto& l : ed.to_lines()) {
    f.WriteLine(l);
  }
  return true;
}

bool fsed(Context& ctx, std::vector<std::string>& lin, int maxli, MessageEditorData& data,
          bool file) {
  const auto saved_mci_enabled = bout.mci_enabled();
  ScopeExit at_exit([=] { bout.set_mci_enabled(saved_mci_enabled); });
  bout.disable_mci();

  FsedModel ed(maxli);
  for (auto l : lin) {
    const auto wrapped = !l.empty() && l.back() == '\x1';
    ed.emplace_back(line_t{ wrapped, l });
  }
  if (!fsed(ctx, ed, data, file)) {
    return false;
  }

  lin = ed.to_lines();
  return true;
}

bool fsed(Context& ctx, FsedModel& ed, MessageEditorData& data, bool file) {
  const auto saved_mci_enabled = bout.mci_enabled();
  ScopeExit at_exit([=] { bout.set_mci_enabled(saved_mci_enabled); });
  bout.disable_mci();

  auto view = create_frame(data, file, &ctx.u());
  ed.set_view(view);
  auto& fs = view->fs();
  ed.set_max_line_len(view->max_view_columns());
  ed.add_callback([&view](FsedModel& e, editor_range_t t) {
    view->handle_editor_invalidate(e, t);
    });
  ed.add_callback([&view](FsedModel& e, int previous_line) { 
      view->draw_current_line(e, previous_line);
    });

  // Draw the initial contents of the file.
  ed.invalidate_to_eof(0);
  // Draw the bottom bar once to start with.
  view->Color(0);
  view->draw_bottom_bar(ed);
  fs.GotoContentAreaTop();
  FsedState state{};

  FsedCommands commands(ctx, data);
  // Add the menu command since that needs the state variables
  // from here.

  // top editor line number in thw viewable area.
  while (!state.done) {
    view->gotoxy(ed);

    const auto key = view->bgetch(ed);
    if (key < 0xff && key >= 32) {
      const auto c = static_cast<char>(key & 0xff);
      view->gotoxy(ed);
      view->bputch(ed.curline().wwiv_color(), c);
      ed.add(c);
      continue;
    }
    if (!commands.TryInterpretChar(key, ed, *view, state)) {
      LOG(ERROR) << "Unable to handle key: " << key;
    }
  }

  fs.ClearCommandLine();
  fs.PutsCommandLine("");
  return state.save;
}

} // namespace
