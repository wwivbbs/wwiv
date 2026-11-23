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

#include <algorithm>

namespace wwiv::bbs::basic {

void BbsConsole::write(std::string_view text) { out_.outstr(std::string(text)); }

void BbsConsole::newline() { out_.nl(); }

std::string BbsConsole::read_line(int max_length) {
  if (hung_up()) {
    return {};
  }
  // A positive max_length goes to input_text as is, like the legacy GETS
  // (input_text itself stops an ANSI user at 78 characters).
  const auto len = max_length > 0 ? std::min(max_length, kMaxInputLength) : kMaxLineLength;
  auto line = in_.input_text("", len);
  // Input returns at once after a hangup, with whatever was typed so far.
  return hung_up() ? std::string{} : line;
}

std::string BbsConsole::read_key() {
  if (hung_up()) {
    return {};
  }
  // getkey returns 0 once the caller has hung up.
  const auto ch = in_.getkey();
  if (ch == '\0' || hung_up()) {
    return {};
  }
  return std::string(1, ch);
}

void BbsConsole::clear_screen() { out_.cls(); }

} // namespace wwiv::bbs::basic
