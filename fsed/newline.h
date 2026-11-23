/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                Copyright (C)2026, WWIV Software Services               */
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
#ifndef INCLUDED_FSED_NEWLINE_H
#define INCLUDED_FSED_NEWLINE_H

#include "local_io/keycodes.h"

namespace wwiv::fsed {

// CR always starts a new line.  LF does too, unless it immediately follows
// a CR, so a Windows CRLF paste does not insert a blank line between every
// line.  A Unix LF-only paste used to be ignored (SOFTRETURN was mapped to
// "ignore"), which joined every paragraph into one and left long hard-broken
// lines looking unwrapped when a later Enter finally arrived.
inline bool consume_line_break(int key, bool& last_was_cr) {
  if (key == wwiv::local::io::RETURN) {
    last_was_cr = true;
    return true;
  }
  if (key == wwiv::local::io::SOFTRETURN) {
    const bool enter = !last_was_cr;
    last_was_cr = false;
    return enter;
  }
  last_was_cr = false;
  return false;
}

} // namespace wwiv::fsed

#endif
