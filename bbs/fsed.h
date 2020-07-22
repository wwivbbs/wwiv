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
#include <filesystem>
#include <functional>
#include <vector>
#include <string>

namespace wwiv::bbs::fsed {

enum class ins_ovr_mode_t { ins, ovr };

enum class line_add_result_t { needs_redraw, no_redraw, error };

enum class editor_add_result_t { added, wrapped, error };

class line_t {
public:
  bool wrapped{false};
  std::string text;

  line_add_result_t add(int x, char c, ins_ovr_mode_t mode);
  line_add_result_t del(int x, ins_ovr_mode_t mode);
  line_add_result_t bs(int x, ins_ovr_mode_t mode);

  int size() const;
};

struct editor_marker_t {
  int line{0};
  int x{0};
};

struct editor_range_t {
  editor_marker_t start;
  editor_marker_t end;
};

class editor_t {
public:
  // Lines of text
  mutable std::vector<line_t> lines;
  // cursor X position
  int cx{0};
  // cursor Y positon
  int cy{0};
  // Current line number
  int curli{0};
  // Max number of lines allowed
  int maxli{255};
  // max line length
  int max_line_len{79};
  // Insert or Overrite mode
  ins_ovr_mode_t mode_{ins_ovr_mode_t::ins};

  // gets the current line
  line_t& curline();
  // inserts a new line after curli.
  bool insert_line();
  // deletes the current line.
  bool remove_line();
  // Adds a char at the current position (cx, curli);
  editor_add_result_t add(char c);
  // deletes current character and shifts left rest
  bool del();
  // backspace over existing character
  bool bs();
  // Gets the current character
  char current_char();
  // Update mode
  void toggle_ins_ovr_mode() {
    mode_ = (mode_ == ins_ovr_mode_t::ins) ? ins_ovr_mode_t::ovr : ins_ovr_mode_t::ins;
  }

  ins_ovr_mode_t mode() const noexcept { return mode_; };

  std::vector<std::string> to_lines();

  // Listeners
  typedef std::function<void(editor_t&, editor_range_t)> editor_range_invalidated_fn;
  bool add_callback(editor_range_invalidated_fn fn);
  void invalidate_to_eol();
  void invalidate_to_eof();
  void invalidate_to_eof(int start_line);
  void invalidate_range(int start_line, int end_line);

  std::vector<editor_range_invalidated_fn> callbacks_;
};



class FsedView {
public:
  FsedView(FullScreenView fs, MessageEditorData& data, bool file);
  ~FsedView() = default;

  FullScreenView& fs();
  void gotoxy(const editor_t& ed);
  int max_view_lines() const { return max_view_lines_; }
  int max_view_columns() const { return max_view_columns_; }
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
bool fsed(editor_t& ed, MessageEditorData& data, int* setanon, bool file);
bool fsed(std::vector<std::string>& lin, int maxli, int* setanon, MessageEditorData& data,
          bool file);

}

#endif