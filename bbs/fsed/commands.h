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
#ifndef __INCLUDED_BBS_FSED_COMMANDS_H__
#define __INCLUDED_BBS_FSED_COMMANDS_H__

#include <functional>
#include <map>
#include <optional>
#include <string>

namespace wwiv::bbs::fsed {

class FsedModel;
class FsedView;

enum class fsed_command_id {
  cursor_up,
  cursor_down,
  cursor_pgup, 
  cursor_pgdown,
  cursor_left,
  cursor_right,
  cursor_home,
  cursor_end,
  delete_to_eol,
  delete_line,
  delete_line_left,
  delete_word_left,
  delete_right,
  backspace,
  key_return,
  menu,
  toggle_insovr,
  view_redraw,
  input_wwiv_color,
};

typedef std::function<bool(FsedModel&, FsedView& view)> fsed_command_fn;

class FsedCommand {
public:
  FsedCommand() = default;
  FsedCommand(fsed_command_id id, std::string name, fsed_command_fn fn);
  fsed_command_id id() const { return id_; }
  std::string name() const { return name_; }
  bool Invoke(FsedModel& mode, FsedView& view) const;

private:
  fsed_command_id id_;
  std::string name_;
  fsed_command_fn fn_;
};

class FsedCommands {
public:
  FsedCommands();
  std::optional<FsedCommand> get(fsed_command_id id);
  std::optional<FsedCommand> get(const std::string& id);
  bool add(FsedCommand cmd);

  /**
   * Attempts to interpret a key by executing a command.  Returns
   * true if executed as a command, false otherwise.
   */
  bool TryInterpretChar(int key, FsedModel& model, FsedView& view);

private:
  bool AddAll();
  std::map<fsed_command_id, FsedCommand> by_id_;
  std::map<std::string, FsedCommand> by_name_;
  std::map<int, fsed_command_id> keymap_;
};


std::map<int, fsed_command_id> CreateDefaultKeyMap();

}

#endif