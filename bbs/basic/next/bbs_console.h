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
#ifndef INCLUDED_BBS_BASIC_NEXT_BBS_CONSOLE_H
#define INCLUDED_BBS_BASIC_NEXT_BBS_CONSOLE_H

#include "wwivbasic/console.h"

#include <string>
#include <string_view>

namespace wwiv::common {
class Input;
class Output;
} // namespace wwiv::common

namespace wwiv::bbs::basic {

// The wwivbasic Console for scripts run by the BBS: output goes to bout
// (so pipe and heart color codes work, as they do for my_basic's PRINT) and
// input comes from bin.
class BbsConsole final : public wwivbasic::Console {
public:
  // Line length used by read_line when the script gives no limit.
  static constexpr int kDefaultMaxLineLength = 80;

  BbsConsole(common::Input& in, common::Output& out) : in_(in), out_(out) {}

  void write(std::string_view text) override;
  void newline() override;
  std::string read_line(int max_length) override;
  std::string read_key() override;
  void clear_screen() override;

private:
  common::Input& in_;
  common::Output& out_;
};

} // namespace wwiv::bbs::basic

#endif
