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

#include <functional>
#include <string>
#include <string_view>

namespace wwiv::common {
class Input;
class Output;
} // namespace wwiv::common

namespace wwiv::bbs::basic {

// The wwivbasic Console for scripts run by the BBS: output goes to bout
// (so pipe and heart color codes work, as they do for my_basic's PRINT) and
// input comes from bin. Once the caller has hung up, input is at its end:
// read_line and read_key return "".
class BbsConsole final : public wwivbasic::Console {
public:
  // What read_line(0), "no limit", reads: the longest line the BBS line
  // editor (Input::input_text) accepts from an ANSI user.
  static constexpr int kMaxLineLength = 78;
  // The most read_line reads. Input::input_text reads a non-ANSI user's line
  // into a 255-byte buffer, so a longer max_length would overflow it.
  static constexpr int kMaxInputLength = 254;

  // hung_up says whether the caller has hung up; empty means never.
  BbsConsole(common::Input& in, common::Output& out, std::function<bool()> hung_up = {})
      : in_(in), out_(out), hung_up_(std::move(hung_up)) {}

  void write(std::string_view text) override;
  void newline() override;
  std::string read_line(int max_length) override;
  std::string read_key() override;
  void clear_screen() override;

private:
  [[nodiscard]] bool hung_up() const { return hung_up_ && hung_up_(); }

  common::Input& in_;
  common::Output& out_;
  std::function<bool()> hung_up_;
};

} // namespace wwiv::bbs::basic

#endif
