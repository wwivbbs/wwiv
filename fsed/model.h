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
#ifndef __INCLUDED_BBS_FSED_MODEL_H__
#define __INCLUDED_BBS_FSED_MODEL_H__

#include "fsed/line.h"
#include <deque>
#include <functional>
#include <vector>
#include <string>
#include <memory>

namespace wwiv::bbs::fsed {


class FsedState {
public:
  // mods are "edit", "menu", "command"
  std::string mode{"edit"};
  bool save{false};
  bool done{false};
};

enum class editor_add_result_t { added, wrapped, error };

struct editor_marker_t {
  int line{0};
  int x{0};
};

struct editor_range_t {
  editor_marker_t start;
  editor_marker_t end;
};

class FsedModel;

class editor_viewport_t {
public:
  virtual int max_view_lines() const = 0;
  virtual int max_view_columns() const = 0;
  virtual int top_line() const = 0;
  virtual void set_top_line(int l) = 0;
  virtual void gotoxy(const FsedModel& ed) = 0;
};

class FsedModel {

public:
  
  // Constructor and Destructors

  explicit FsedModel(int max_lines) : maxli_(max_lines) {}
  FsedModel() : FsedModel(255) {}
  ~FsedModel() = default;
  void set_view(const std::shared_ptr<editor_viewport_t>& view);

  //
  // Line Operations  
  //

  // gets the current line
  line_t& curline() const;
  // Gets the line at a position n or throws.
  line_t& line(int n) const;
  bool set_lines(std::vector<line_t>&& n);
  // return the number of lines.
  std::size_t size() const { return lines_.size(); }
  // Adds a new line to the end of the list of lines.
  void emplace_back(line_t&& n);
  // inserts a new line after curli.
  bool insert_line();
  // Inserts all of the lines into the editor at the current
  // position.  These lines may contain heart codes, or pipe
  // codes or both.
  bool insert_lines(std::vector<std::string>& lines);
  // deletes the current line.
  bool remove_line();
  /**
   * Return the text as a vector of strings in WWIV format (meaning the last
   * character is a \x1 if the line is wrapped.
   */
  std::vector<std::string> to_lines();

  //
  // Character Operations
  //
  // Adds a char at the current position (cx, curli);
  editor_add_result_t add(char c);
  // deletes current character and shifts left rest
  bool del();
  // backspace over existing character.  Does not handle wrapping the
  // line onto the previous line
  bool bs_nowrap();
  // backspace over existing character.  Wraps the current line onto
  // the previousline.
  bool bs();
  // handles the enter key
  bool enter();
  // Gets the current character
  cell_t current_cell() const;

  //
  // Cursor Operations
  //
  bool cursor_up();
  bool cursor_down();
  bool cursor_left();
  bool cursor_right();
  bool cursor_pgup();
  bool cursor_pgdown();
  bool cursor_home();
  bool cursor_end();

  bool delete_line();
  bool delete_to_eol();
  bool delete_line_left();
  bool delete_word_left();
  bool delete_right();

  // Toggles the internal state if the editor is in INSERT or OVERWRITE mode. This
  // matters on add, bs, and del
  void toggle_ins_ovr_mode();
  // Get the internal state if the editor is in INSERT or OVERWRITE mode.
  ins_ovr_mode_t mode() const noexcept;

  //
  // Maximums
  //
  void set_max_line_len(int n) noexcept { max_line_len_ = n; }
  int max_line_len() const noexcept { return max_line_len_;  }
  // Max number of lines
  int maxli() const noexcept { return maxli_; }

  //
  // Listeners, Callbacks and invalidations
  //
  typedef std::function<void(FsedModel&, editor_range_t)> editor_range_invalidated_fn;
  typedef std::function<void(FsedModel&, int)> editor_current_line_redraw_fn;
  bool add_callback(editor_range_invalidated_fn fn);
  bool add_callback(editor_current_line_redraw_fn fn);
  void invalidate_to_eol();
  void invalidate_to_eof();
  void invalidate_to_eof(int start_line);
  void invalidate_range(int start_line, int end_line);
  void current_line_dirty(int previous_line);

  //
  // Public values
  //

  // cursor X position
  int cx{0};
  // cursor Y positon
  int cy{0};
  // Current line number
  int curli{0};

private:
  // Max number of lines allowed
  int maxli_{255};
  // Lines of text.  mark mutable so we can add the current line
  // into the array and stay logically const.
  mutable std::vector<line_t> lines_;
  // Insert or Overrite mode
  ins_ovr_mode_t mode_{ins_ovr_mode_t::ins};
  // Max number of lines allowed.
  int max_line_len_{79};

  std::vector<editor_range_invalidated_fn> range_callbacks_;
  std::vector<editor_current_line_redraw_fn> line_callbacks_;
  // Viewport for display
  std::shared_ptr<editor_viewport_t> view_;
};

}

#endif