/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*              Copyright (C)2016-2020, WWIV Software Services            */
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
#ifndef __INCLUDED_BBS_FULL_SCREEN_H__
#define __INCLUDED_BBS_FULL_SCREEN_H__

#include <string>

class Output;

class FullScreenView {
public:
  FullScreenView(Output& output, int numlines, int swidth, int slength);
  virtual ~FullScreenView();

  /** Displays a timeout warning in the command line area */
  void PrintTimeoutWarning(int);
  /** Clears the command line area */
  void ClearCommandLine();
  /** Displays a text in the command line area */
  void PutsCommandLine(const std::string& text);
  /** Clears the messagea area (the main body of the full screen view */
  void ClearMessageArea();
  /** Displays the horizontal bar between the header area and body */
  void DrawTopBar();
  /** Displays the horizontal bar between the body and command line*/
  void DrawBottomBar(const std::string& text);
  /** Places the cursor at the top of the body */
  void GotoContentAreaTop();
  
  int message_height() const noexcept { return message_height_; }
  int lines_start() const noexcept { return lines_start_; }
  int lines_end() const noexcept { return lines_end_; }
  int num_header_lines() const noexcept { return num_header_lines_; }
  int screen_width() const noexcept { return screen_width_; }

  // Runs bgetch_event with error message and warning displayed
  // on the status line.
  int bgetch();

private:
  Output& bout_;

  int num_header_lines_{0};
  int screen_width_{0};
  int screen_length_{0};
  int message_height_{0};
  int lines_start_{0};
  int lines_end_{0};
  int command_line_{0};
};


#endif  // __INCLUDED_BBS_FULL_SCREEN_H__