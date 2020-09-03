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

  const auto saved_topdata = a()->localIO()->topdata();
  if (a()->localIO()->topdata() != LocalIO::topdata_t::none) {
    a()->localIO()->topdata(LocalIO::topdata_t::none);
    a()->UpdateTopScreen();
  }

  // Draw the initial contents of the file.
  ed.invalidate_to_eof(0);
  // Draw the bottom bar once to start with.
  view->Color(0);
  view->draw_bottom_bar(ed);
  fs.GotoContentAreaTop();
  FsedState state{};

  FsedCommands commands{};
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

  a()->localIO()->topdata(saved_topdata);
  a()->UpdateTopScreen();

  return state.save;
}

} // namespace wwiv::bbs::fsed