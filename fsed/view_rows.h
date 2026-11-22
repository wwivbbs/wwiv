/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                Copyright (C)2026, WWIV Software Services               */
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
/*    either  express  or implied.  See  the  License for the specific   */
/*    language governing permissions and limitations under the License.   */
/**************************************************************************/
#ifndef INCLUDED_FSED_VIEW_ROWS_H
#define INCLUDED_FSED_VIEW_ROWS_H

#include <algorithm>
#include <optional>
#include <vector>

namespace wwiv::fsed {

// The FSED message area is the half-open row range [lines_start, lines_end).
// The status bar lives at y == lines_end (see FullScreenView: lines_end_
// equals screen_length_ - 1, which is also DrawBottomBar's row).  Nothing
// in the editor may write at or below that row, or the terminal will
// scroll the header off the screen.

inline bool row_in_edit_area(int y, int lines_start, int lines_end) {
  return y >= lines_start && y < lines_end;
}

// Map a document line onto a screen row, or nullopt if it is not visible
// in the current viewport / would land on the status bar or below.
inline std::optional<int> row_for_line(int line, int top_line, int lines_start, int lines_end) {
  const auto y = line - top_line + lines_start;
  if (!row_in_edit_area(y, lines_start, lines_end)) {
    return std::nullopt;
  }
  return y;
}

// Plan the paints for a visible-window redraw.  Each visible row is either
// a document line or a clear.  Used by the view and by tests that assert
// nothing is scheduled outside [lines_start, lines_end).
struct edit_area_paint {
  int y{0};
  int line{-1}; // -1 means clear the row
};

inline std::vector<edit_area_paint> plan_edit_area_paints(int top_line, int doc_lines,
                                                          int lines_start, int lines_end) {
  std::vector<edit_area_paint> out;
  if (lines_end <= lines_start || top_line < 0) {
    return out;
  }
  for (int y = lines_start; y < lines_end; ++y) {
    const int line = top_line + (y - lines_start);
    edit_area_paint p;
    p.y = y;
    p.line = (line >= 0 && line < doc_lines) ? line : -1;
    out.push_back(p);
  }
  return out;
}

// Never write a character into the last column of the terminal.  A write of
// screen_width cells starting at column 1 leaves the cursor wrap-pending on
// the next row (the status bar, if this is the last edit row).
inline int editor_wrap_columns(int screen_width) {
  return std::max(1, screen_width - 1);
}

// Historical WWIV default is 25 lines (DOS included a local status line),
// and FullScreenView callers used screen_lines()-1 so a 24-row terminal was
// filled correctly.  If the user already set 24 (to match the web client),
// subtracting again leaves the last terminal row unmanaged and wrapped text
// collects under the status bar.
inline int fullscreen_length_for_user_lines(int user_lines) {
  if (user_lines < 16) {
    return 24;
  }
  return (user_lines >= 25) ? user_lines - 1 : user_lines;
}

// The command line lives on row screen_length, which is the last row of
// a 24-line terminal.  Never address a row past that: CUP to row 25 on a
// 24-row terminal (or a write that autowraps on the last row) scrolls the
// whole screen and the header is lost.
inline int clamp_screen_row(int y, int screen_length) {
  if (screen_length < 1) {
    return 1;
  }
  if (y < 1) {
    return 1;
  }
  if (y > screen_length) {
    return screen_length;
  }
  return y;
}

// Plan the paints for an invalidated document range [start_line, end_line].
// Only rows inside the viewport are returned, so a one-line change costs
// one row instead of the whole screen.  When to_eof is set, visible rows
// past the end of the document are cleared (line == -1) so a shrinking
// document does not leave stale text behind.
inline std::vector<edit_area_paint> plan_range_paints(int start_line, int end_line, bool to_eof,
                                                      int top_line, int doc_lines,
                                                      int lines_start, int lines_end) {
  std::vector<edit_area_paint> out;
  if (lines_end <= lines_start || top_line < 0) {
    return out;
  }
  const int first = std::max(start_line, top_line);
  for (int y = lines_start; y < lines_end; ++y) {
    const int line = top_line + (y - lines_start);
    if (line < first) {
      continue;
    }
    if (line < doc_lines && line <= end_line) {
      out.push_back(edit_area_paint{y, line});
    } else if (to_eof && line >= doc_lines) {
      out.push_back(edit_area_paint{y, -1});
    } else {
      break;
    }
  }
  return out;
}

// Detects a run of keys that were already queued when the editor loop
// came round (a paste, or a macro).  drained() returns true once, when
// such a run ends, so the editor can do one full repaint for the whole
// paste instead of repainting the screen for every key.  Normal typing
// never has more than a key or two waiting, so it never triggers this.
class paste_burst_t {
public:
  explicit paste_burst_t(int threshold = 4) : threshold_(threshold) {}
  // Call once per loop pass with whether another key is already waiting.
  bool drained(bool key_waiting) {
    if (key_waiting) {
      ++queued_;
      return false;
    }
    const bool burst = queued_ >= threshold_;
    queued_ = 0;
    return burst;
  }

private:
  int threshold_;
  int queued_{0};
};

} // namespace wwiv::fsed

#endif
