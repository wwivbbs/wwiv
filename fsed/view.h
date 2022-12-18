/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*              Copyright (C)2020-2022, WWIV Software Services            */
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
#ifndef INCLUDED_FSED_VIEW_H
#define INCLUDED_FSED_VIEW_H

#include "common/full_screen.h"
#include "common/message_editor_data.h"
#include "fsed/model.h"

namespace wwiv {
namespace common {
class Context;
}
}

namespace wwiv::fsed {

class FsedView final : public editor_viewport_t {
public:
  FsedView(const common::FullScreenView& fs, common::MessageEditorData& data, bool file);
  ~FsedView() override = default;

  common::FullScreenView& fs();
  [[nodiscard]] int max_view_lines() const override { return max_view_lines_; }
  [[nodiscard]] int max_view_columns() const override { return max_view_columns_; }
  // Draws the current line without colors, and redraws the previous
  // line with colors.
  void draw_current_line(FsedModel& e, int previous_line);
  // Updates the editor line number based on the cy and fs view of the 
  // world.
  void handle_editor_invalidate(FsedModel&, editor_range_t t);
  void draw_header();
  void redraw();
  void redraw(const FsedModel& ed);
  void draw_bottom_bar(const FsedModel& ed);
  [[nodiscard]] int bgetch(FsedModel& ed);
  void outchr(int color, char ch);
  void cls();
  void ansic(int c);
  [[nodiscard]] int top_line() const override { return top_line_; }
  void set_top_line(int l) override { top_line_ = l; }
  void gotoxy(const FsedModel& ed) override;
  void ClearCommandLine();
  void macro(wwiv::common::Context& ctx, int cc);

  // Top editor line number visible in the viewport.
  int top_line_{0};
  bool debug{false};

private:
  common::FullScreenView fs_;
  common::Output& bout_;
  common::Input& bin_;
  int max_view_lines_;
  int max_view_columns_;
  common::MessageEditorData& data_;
  bool file_{false};
  //  Saved positions for the bottom bar caching.
  int sx{-1};
  int sy{-1};
  int sl{-1};
};

}

#endif