/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2026, WWIV Software Services                  */
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
#include "bbs/basic/next/script_keys.h"

#include "common/context.h"
#include "common/input.h"
#include "local_io/keycodes.h"

#include <string>

namespace wwiv::bbs::basic {

std::string ScriptKeyName(int key) {
  using namespace wwiv::local::io;
  switch (key) {
  case COMMAND_UP:
    return "UP";
  case COMMAND_DOWN:
    return "DOWN";
  case COMMAND_LEFT:
    return "LEFT";
  case COMMAND_RIGHT:
    return "RIGHT";
  case COMMAND_HOME:
    return "HOME";
  case COMMAND_END:
    return "END";
  case COMMAND_PAGEUP:
    return "PGUP";
  case COMMAND_PAGEDN:
    return "PGDN";
  case COMMAND_INSERT:
    return "INS";
  case COMMAND_DELETE:
    return "DEL";
  case COMMAND_F1:
    return "F1";
  case COMMAND_F2:
    return "F2";
  case COMMAND_F3:
    return "F3";
  case COMMAND_F4:
    return "F4";
  case COMMAND_F5:
    return "F5";
  case COMMAND_F6:
    return "F6";
  case COMMAND_F7:
    return "F7";
  case COMMAND_F8:
    return "F8";
  case COMMAND_F9:
    return "F9";
  case COMMAND_F10:
    return "F10";
  default:
    return {};
  }
}

std::string ReadScriptKey(common::Input& in, const std::function<int()>& get_key) {
  while (true) {
    const auto key = get_key();
    if (key == 0) {
      // bgetch_event also returns 0 for keys it handled and for broken
      // escape sequences, so check that the caller really hung up.
      if (in.sess().hangup()) {
        return {};
      }
      continue;
    }
    if (key < 256) {
      return std::string(1, static_cast<char>(key));
    }
    if (auto name = ScriptKeyName(key); !name.empty()) {
      return name;
    }
  }
}

} // namespace wwiv::bbs::basic
