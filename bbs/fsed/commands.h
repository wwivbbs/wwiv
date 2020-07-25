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

#include "local_io/keycodes.h"
#include <map>
#include <string>

namespace wwiv::bbs::fsed {

enum class FsedCommand {
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
  delete_right,
  backspace,
  key_return,
  menu,
  toggle_insovr,
  view_redraw,
  input_wwiv_color,
};

std::map<int, FsedCommand> CreateDefaultKeyMap();

}

#endif