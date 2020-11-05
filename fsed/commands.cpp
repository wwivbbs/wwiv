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
#include "fsed/commands.h"

#include "common/output.h"
#include "common/quote.h"
#include "core/stl.h"
#include "fsed/model.h"
#include "fsed/view.h"
#include "local_io/keycodes.h"
#include "sdk/filenames.h"
#include <map>
#include <utility>

namespace wwiv::fsed {

std::map<int, fsed_command_id> CreateDefaultEditModeKeyMap() {
  std::map<int, fsed_command_id> map;
  map.emplace(COMMAND_UP, fsed_command_id::cursor_up);
  map.emplace(COMMAND_DOWN, fsed_command_id::cursor_down);
  map.emplace(COMMAND_PAGEUP, fsed_command_id::cursor_pgup);
  map.emplace(COMMAND_PAGEDN, fsed_command_id::cursor_pgdown);
  map.emplace(COMMAND_LEFT, fsed_command_id::cursor_left);
  map.emplace(COMMAND_RIGHT, fsed_command_id::cursor_right);
  map.emplace(COMMAND_HOME, fsed_command_id::cursor_home);
  map.emplace(CA, fsed_command_id::cursor_home);
  map.emplace(CD, fsed_command_id::delete_line);
  map.emplace(COMMAND_END, fsed_command_id::cursor_end);
  map.emplace(CE, fsed_command_id::cursor_end);
  map.emplace(CK, fsed_command_id::delete_to_eol);
  map.emplace(CL, fsed_command_id::view_redraw);
  map.emplace(COMMAND_DELETE, fsed_command_id::delete_right);
  map.emplace(BACKSPACE, fsed_command_id::backspace);
  map.emplace(ESC, fsed_command_id::menu);
  map.emplace(CI, fsed_command_id::toggle_insovr);
  map.emplace(RETURN, fsed_command_id::key_return);
  map.emplace(CP, fsed_command_id::input_wwiv_color);
  map.emplace(CW, fsed_command_id::delete_word_left);
  map.emplace(CX, fsed_command_id::delete_line_left);

  return map;
}

FsedCommand::FsedCommand(fsed_command_id id, std::string name, fsed_command_fn fn)
    : id_(id), name_(std::move(name)), fn_(std::move(fn)) {}

bool FsedCommand::Invoke(FsedModel& model, FsedView& view, FsedState& state) const {
  return fn_(model, view, state);
}

FsedCommands::FsedCommands(wwiv::common::Context& ctx) : ctx_(ctx) {
  AddAll();
  edit_keymap_ = CreateDefaultEditModeKeyMap();
}

std::optional<FsedCommand> FsedCommands::get(fsed_command_id id) {
  if (wwiv::stl::contains(by_id_, id)) {
    return by_id_.at(id);
  }
  return std::nullopt;
}

std::optional<FsedCommand> FsedCommands::get(const std::string& id) {
  if (wwiv::stl::contains(by_name_, id)) {
    return by_name_.at(id);
  }
  return std::nullopt;
}

bool FsedCommands::add(const FsedCommand& cmd) {
  by_id_.emplace(cmd.id(), cmd);
  by_name_.emplace(cmd.name(), cmd);
  return true;
}

static void show_fsed_menu(common::Context& ctx, FsedModel& ed, FsedView& view, bool& done,
                           bool& save) {
  view.fs().PutsCommandLine(
      "|#9(|#2ESC|#9=Return, |#2A|#9=Abort, |#2Q|#9=Quote, |#2S|#9=Save, |#2D|#9=Debug, "
      "|#2?|#9=Help): ");
  const auto cmd = std::toupper(view.fs().bgetch() & 0xff);
  view.ClearCommandLine();
  switch (cmd) {
  case 'S':
    done = save = true;
    break;
  case 'A':
    done = true;
    save = false;
    break;
  case 'D': {
    view.debug = !view.debug;
    view.draw_bottom_bar(ed);
  } break;
  case 'Q': {
    // Hacky quote solution for now.
    // TODO(rushfan): Do something less lame here.
    view.cls();
    auto quoted_lines = query_quote_lines(ctx.session_context());
    if (!quoted_lines.empty()) {
      ed.insert_lines(quoted_lines);
    }
    // Even if we don't insert quotes, we still need to
    // redraw the frame
    view.redraw(ed);
    ed.invalidate_to_eof(0);
  } break;
  case '?': {
    view.ClearCommandLine();
    view.fs().ClearMessageArea();
    if (!view.fs().out().print_help_file(FSED_NOEXT)) {
      view.fs().PutsCommandLine(wwiv::strings::StrCat("|#6Unable to find file: ", FSED_NOEXT));
    }
    view.fs().out().pausescr();
    view.fs().ClearMessageArea();
    view.ClearCommandLine();
    view.redraw(ed);
    ed.invalidate_to_eof(0);
  } break;
  case ESC:
    [[fallthrough]];
  default: {
  } break;
  }
  view.ClearCommandLine();
}

bool FsedCommands::AddAll() {
  add(FsedCommand(fsed_command_id::cursor_up, "cursor_up",
                  [](FsedModel& ed, FsedView&, FsedState&) -> bool { return ed.cursor_up(); }));
  add(FsedCommand(fsed_command_id::cursor_down, "cursor_down",
                  [](FsedModel& ed, FsedView&, FsedState&) -> bool { return ed.cursor_down(); }));
  add(FsedCommand(fsed_command_id::cursor_pgup, "cursor_pgup",
                  [](FsedModel& ed, FsedView&, FsedState&) -> bool { return ed.cursor_pgup(); }));
  add(FsedCommand(fsed_command_id::cursor_pgdown, "cursor_pgdown",
                  [](FsedModel& ed, FsedView&, FsedState&) -> bool { return ed.cursor_pgdown(); }));
  add(FsedCommand(fsed_command_id::cursor_left, "cursor_left",
                  [](FsedModel& ed, FsedView&, FsedState&) -> bool { return ed.cursor_left(); }));
  add(FsedCommand(fsed_command_id::cursor_right, "cursor_right",
                  [](FsedModel& ed, FsedView&, FsedState&) -> bool { return ed.cursor_right(); }));
  add(FsedCommand(fsed_command_id::cursor_home, "cursor_home",
                  [](FsedModel& ed, FsedView&, FsedState&) -> bool { return ed.cursor_home(); }));
  add(FsedCommand(fsed_command_id::delete_line, "delete_line",
                  [](FsedModel& ed, FsedView&, FsedState&) -> bool { return ed.delete_line(); }));
  add(FsedCommand(fsed_command_id::cursor_end, "cursor_end",
                  [](FsedModel& ed, FsedView&, FsedState&) -> bool { return ed.cursor_end(); }));
  add(FsedCommand(fsed_command_id::delete_to_eol, "delete_to_eol",
                  [](FsedModel& ed, FsedView&, FsedState&) -> bool { return ed.delete_to_eol(); }));
  add(FsedCommand(fsed_command_id::view_redraw, "view_redraw",
                  [](FsedModel& ed, FsedView& view, FsedState&) -> bool {
                    view.redraw(ed);
                    ed.invalidate_to_eof(0);
                    return true;
                  }));
  add(FsedCommand(fsed_command_id::delete_right, "delete_right",
                  [](FsedModel& ed, FsedView&, FsedState&) -> bool { return ed.delete_right(); }));
  add(FsedCommand(fsed_command_id::key_return, "key_return",
                  [](FsedModel& ed, FsedView&, FsedState&) -> bool { return ed.enter(); }));
  add(FsedCommand(fsed_command_id::backspace, "backspace",
                  [&](FsedModel& ed, FsedView& view, FsedState&) -> bool {
                    ed.bs();
                    view.bputch(ed.curline().wwiv_color(), ed.current_cell().ch);
                    return true;
                  }));
  add(FsedCommand(fsed_command_id::toggle_insovr, "toggle_insovr",
                  [](FsedModel& ed, FsedView& view, FsedState&) -> bool {
                    ed.toggle_ins_ovr_mode();
                    view.draw_bottom_bar(ed);
                    return true;
                  }));
  add(FsedCommand(fsed_command_id::input_wwiv_color, "input_wwiv_color",
                  [](FsedModel& ed, FsedView& view, FsedState&) -> bool {
                    const auto cc = view.bgetch(ed);
                    if (cc >= '0' && cc <= '9') {
                      ed.curline().set_wwiv_color(cc - '0');
                    }
                    ed.current_line_dirty(ed.curli);
                    return true;
                  }));
  add(FsedCommand(
      fsed_command_id::delete_word_left, "delete_word_left",
      [](FsedModel& ed, FsedView&, FsedState&) -> bool { return ed.delete_word_left(); }));
  add(FsedCommand(
      fsed_command_id::delete_line_left, "delete_line_left",
      [](FsedModel& ed, FsedView&, FsedState&) -> bool { return ed.delete_line_left(); }));
  add(FsedCommand(fsed_command_id::menu, "menu",
                  [&](FsedModel& ed, FsedView& view, FsedState& state) -> bool {
                    show_fsed_menu(ctx_, ed, view, state.done, state.save);
                    return true;
                  }));

  return false;
}

std::optional<fsed_command_id> FsedCommands::get_command_id(int key) {
  const auto key_it = edit_keymap_.find(key);
  if (key_it == std::end(edit_keymap_)) {
    // No key binding
    return std::nullopt;
  }
  return {key_it->second};
}

bool FsedCommands::TryInterpretChar(int key, FsedModel& model, FsedView& view, FsedState& state) {

  auto cmd_id = get_command_id(key);
  if (!cmd_id) {
    return false;
  }

  auto cmd = get(cmd_id.value());
  if (!cmd) {
    // No command bound for this id.
    LOG(WARNING) << "No command bound for command id: " << static_cast<int>(cmd_id.value());
    return false;
  }
  return cmd.value().Invoke(model, view, state);
}

} // namespace