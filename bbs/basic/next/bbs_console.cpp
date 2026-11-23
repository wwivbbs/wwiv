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
#include "bbs/basic/next/bbs_console.h"

#include "common/input.h"
#include "common/output.h"

namespace wwiv::bbs::basic {

void BbsConsole::write(std::string_view text) { out_.outstr(std::string(text)); }

void BbsConsole::newline() { out_.nl(); }

std::string BbsConsole::read_line(int max_length) {
  return in_.input_text("", max_length > 0 ? max_length : kDefaultMaxLineLength);
}

std::string BbsConsole::read_key() { return std::string(1, in_.getkey()); }

void BbsConsole::clear_screen() { out_.cls(); }

} // namespace wwiv::bbs::basic
