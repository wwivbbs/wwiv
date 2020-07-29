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
#include "bbs/fsed/commands.h"

#include "bbs/application.h"
#include "bbs/fsed/model.h"
#include "bbs/fsed/view.h"
#include "core/stl.h"
#include "local_io/keycodes.h"
#include <map>

namespace wwiv::bbs::fsed {

std::map<int, fsed_command_id> CreateDefaultKeyMap() { 
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
: id_(id), name_(std::move(name)), fn_(fn) {}

bool FsedCommand::Invoke(FsedModel& model, FsedView& view) { 
  return fn_(model, view);
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

bool FsedCommands::add(FsedCommand cmd) { 
  by_id_[cmd.id()] = cmd;
  by_name_[cmd.name()] = cmd;
  return true; 
}

bool FsedCommands::AddAll() { 
  add(FsedCommand(fsed_command_id::cursor_up, "cursor_up", [](FsedModel& ed, FsedView &) ->bool {
    return ed.cursor_up();
  }));
  add(FsedCommand(fsed_command_id::cursor_down, "cursor_down",
                  [](FsedModel& ed, FsedView&) -> bool {
                    return ed.cursor_down();
  }));
  add(FsedCommand(fsed_command_id::cursor_pgup, "cursor_pgup", [](FsedModel& ed, FsedView&) -> bool {
    return ed.cursor_pgup();
  }));
  add(FsedCommand(fsed_command_id::cursor_pgdown, "cursor_pgdown",
                  [](FsedModel& ed, FsedView&) -> bool {
    return ed.cursor_pgdown();
  }));
  add(FsedCommand(fsed_command_id::cursor_left, "cursor_left",
                  [](FsedModel& ed, FsedView&) -> bool { return ed.cursor_left(); }));
  add(FsedCommand(fsed_command_id::cursor_right, "cursor_right",
                  [](FsedModel& ed, FsedView&) -> bool { return ed.cursor_right(); }));
  add(FsedCommand(fsed_command_id::cursor_home, "cursor_home",
                  [](FsedModel& ed, FsedView&) -> bool { return ed.cursor_home(); }));
  add(FsedCommand(fsed_command_id::delete_line, "delete_line",
                  [](FsedModel& ed, FsedView&) -> bool { return ed.delete_line(); }));
  add(FsedCommand(fsed_command_id::cursor_end, "cursor_end",
                  [](FsedModel& ed, FsedView&) -> bool { return ed.cursor_end(); }));
  add(FsedCommand(fsed_command_id::delete_to_eol, "delete_to_eol",
                  [](FsedModel& ed, FsedView&) -> bool { return ed.delete_to_eol(); }));
  add(FsedCommand(fsed_command_id::view_redraw, "view_redraw",
                  [](FsedModel& ed, FsedView& view) -> bool {
                    view.redraw();
                    ed.invalidate_to_eof(0);
                    return true;
                  }));

  add(FsedCommand(fsed_command_id::delete_right, "delete_right",
                  [](FsedModel& ed, FsedView&) -> bool { return ed.delete_right(); }));
  add(FsedCommand(fsed_command_id::backspace, "backspace",
                  [&](FsedModel& ed, FsedView&) -> bool { 
    ed.bs();
    bout.Color(ed.curline().wwiv_color());
    bout.bputch(ed.current_cell().ch);
    return true;
    }));
  //add(FsedCommand(fsed_command_id::menu, "menu",
  //                [&](FsedModel& ed, FsedView&) -> bool { 
  //  }));
  add(FsedCommand(fsed_command_id::toggle_insovr, "toggle_insovr",
                  [](FsedModel& ed, FsedView& view) -> bool { 
      ed.toggle_ins_ovr_mode();
                    view.draw_bottom_bar(ed);
                    return true;
    }));
  add(FsedCommand(fsed_command_id::input_wwiv_color, "input_wwiv_color",
                  [](FsedModel& ed, FsedView&) -> bool {
                    auto cc = bout.getkey();
                    if (cc >= '0' && cc <= '9') {
                      ed.curline().set_wwiv_color(cc - '0');
                    }
                    ed.current_line_dirty(ed.curli);
                    return true;
                  }));
  add(FsedCommand(fsed_command_id::delete_word_left, "delete_word_left",
                  [](FsedModel& ed, FsedView&) -> bool { return ed.delete_word_left(); }));
  add(FsedCommand(fsed_command_id::delete_line_left, "delete_line_left",
                  [](FsedModel& ed, FsedView&) -> bool { return ed.delete_line_left(); }));
  return false;
}

} // namespace wwiv::bbs::fsed