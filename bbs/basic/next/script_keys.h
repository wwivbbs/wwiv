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
#ifndef INCLUDED_BBS_BASIC_NEXT_SCRIPT_KEYS_H
#define INCLUDED_BBS_BASIC_NEXT_SCRIPT_KEYS_H

#include <functional>
#include <string>

namespace wwiv::common {
class Input;
}

namespace wwiv::bbs::basic {

// The name a script sees for a special key from Input::bgetch_event (UP, DOWN,
// LEFT, RIGHT, HOME, END, PGUP, PGDN, INS, DEL, F1 to F10), or "" for a key
// it doesn't name.
std::string ScriptKeyName(int key);

// Reads keys with get_key (Input::bgetch_event, or FullScreenView::bgetch)
// until one a script sees, as wwiv.io.READKEY returns it: the character, or a
// special key's name (Enter is CHR(13)). Keys without a name, and those the
// BBS handles itself (Ctrl+T, say), are skipped. Returns "" once the caller
// has hung up.
std::string ReadScriptKey(common::Input& in, const std::function<int()>& get_key);

} // namespace wwiv::bbs::basic

#endif
