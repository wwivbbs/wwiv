/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                   Copyright (C)2020, WWIV Software Services            */
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
#ifndef __INCLUDED_BBS_FSED_H__
#define __INCLUDED_BBS_FSED_H__

#include "bbs/full_screen.h"
#include "bbs/message_editor_data.h"
#include "bbs/fsed/common.h"
#include "bbs/fsed/editor.h"
#include "bbs/fsed/line.h"
#include <filesystem>
#include <functional>
#include <vector>
#include <string>

namespace wwiv::bbs::fsed {

class FsedView {
public:
  FsedView(FullScreenView fs, MessageEditorData& data, bool file);
  ~FsedView() = default;

  FullScreenView& fs();
  void gotoxy(const editor_t& ed);
  int max_view_lines() const { return max_view_lines_; }
  int max_view_columns() const { return max_view_columns_; }
  // Draws the current line without colors, and redraws the previous
  // line with colors.
  void draw_current_line(editor_t& e, int previous_line);
  // Updates the editor line number based on the cy and fs view of the 
  // world.
  void handle_editor_invalidate(editor_t&, editor_range_t t);
  void redraw();
  void draw_bottom_bar(editor_t& ed);
  int bgetch(editor_t& ed);

public:
  // Top editor line number visible in the viewport.
  int top_line{0};

private:
  FullScreenView fs_;
  int max_view_lines_;
  int max_view_columns_;
  MessageEditorData& data_;
  bool file_{false};
};

bool fsed(const std::filesystem::path& path);
bool fsed(editor_t& ed, MessageEditorData& data, bool file);
bool fsed(std::vector<std::string>& lin, int maxli, MessageEditorData& data, bool file);

}

#endif