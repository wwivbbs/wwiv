/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*              Copyright (C)2016-2021, WWIV Software Services            */
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
#ifndef INCLUDED_COMMON_FULL_SCREEN_H
#define INCLUDED_COMMON_FULL_SCREEN_H

#include "local_io/local_io.h"
#include <string>

namespace wwiv::common {
class Input;
class Output;

class FullScreenView final {
public:
  FullScreenView(Output& output, Input& input, int numlines, int swidth, int slength);
  ~FullScreenView();

  /** Displays a timeout warning in the command line area */
  void PrintTimeoutWarning(int);
  /** Clears the command line area */
  void ClearCommandLine();
  /** Displays a text in the command line area */
  void PutsCommandLine(const std::string& text);
  /** Clears the message area (the main body of the full screen view */
  void ClearMessageArea();
  /** Displays the horizontal bar between the header area and body */
  void DrawTopBar();
  /** Displays the horizontal bar between the body and command line*/
  void DrawBottomBar(const std::string& text);
  /** Places the cursor at the top of the body */
  void GotoContentAreaTop();

  [[nodiscard]] int message_height() const noexcept { return message_height_; }
  [[nodiscard]] int lines_start() const noexcept { return lines_start_; }
  [[nodiscard]] int lines_end() const noexcept { return lines_end_; }
  [[nodiscard]] int num_header_lines() const noexcept { return num_header_lines_; }
  [[nodiscard]] int screen_width() const noexcept { return screen_width_; }

  // Runs bgetch_event with error message and warning displayed
  // on the status line.
  int bgetch();

  // Read through to this full screen's input.
  [[nodiscard]] Input& in() const { return bin_; }
  // Write through to this full screen's output.
  [[nodiscard]] Output& out() const { return bout_; }

private:
  Input& bin_;
  Output& bout_;

  int num_header_lines_{0};
  int screen_width_{0};
  int screen_length_{0};
  int message_height_{0};
  int lines_start_{0};
  int lines_end_{0};
  int command_line_{0};

  LocalIO::topdata_t saved_topdata{LocalIO::topdata_t::none};
};

} // namespace wwiv::common

#endif
