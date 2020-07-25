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
#include "local_io/keycodes.h"

namespace wwiv::bbs::fsed {


std::map<int, FsedCommand> CreateDefaultKeyMap() { 
  std::map<int, FsedCommand> map;
  map.emplace(COMMAND_UP, FsedCommand::cursor_up);
  map.emplace(COMMAND_DOWN, FsedCommand::cursor_down);
  map.emplace(COMMAND_PAGEUP, FsedCommand::cursor_pgup);
  map.emplace(COMMAND_PAGEDN, FsedCommand::cursor_pgdown);
  map.emplace(COMMAND_LEFT, FsedCommand::cursor_left);
  map.emplace(COMMAND_RIGHT, FsedCommand::cursor_right);
  map.emplace(COMMAND_HOME, FsedCommand::cursor_home);
  map.emplace(CA, FsedCommand::cursor_home);
  map.emplace(CD, FsedCommand::delete_line);
  map.emplace(COMMAND_END, FsedCommand::cursor_end);
  map.emplace(CE, FsedCommand::cursor_end);
  map.emplace(CK, FsedCommand::delete_to_eol);
  map.emplace(CL, FsedCommand::view_redraw);
  map.emplace(COMMAND_DELETE, FsedCommand::delete_right);
  map.emplace(BACKSPACE, FsedCommand::backspace);
  map.emplace(ESC, FsedCommand::menu);
  map.emplace(CI, FsedCommand::toggle_insovr);
  map.emplace(RETURN, FsedCommand::key_return);
  map.emplace(CP, FsedCommand::input_wwiv_color);

  return map;
}


} // namespace wwiv::bbs::fsed