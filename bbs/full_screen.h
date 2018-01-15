/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*              Copyright (C)2016-2017, WWIV Software Services            */
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
#include <set>

class FullScreenView {
public:
  FullScreenView(int numlines, int swidth, int slength);
  virtual ~FullScreenView();

  void PrintTimeoutWarning(int);
  void ClearCommandLine();
  void ClearMessageArea();
  void DrawTopBar();
  void DrawBottomBar(const std::string& text);
  void GotoContentAreaTop();
  
  int message_height() const { return message_height_; }
  int lines_start() const { return lines_start_; }
  int lines_end() const { return lines_end_; }
  int num_header_lines() const { return num_header_lines_; }
  int command_line_y() const { return command_line_; }
  int screen_width() const { return screen_width_; }

  int num_header_lines_ = 0;
  int screen_width_ = 0;
  int screen_length_ = 0;
  int message_height_ = 0;
  int lines_start_ = 0;
  int lines_end_ = 0;
  int command_line_ = 0;
};


#endif  // __INCLUDED_BBS_FULL_SCREEN_H__