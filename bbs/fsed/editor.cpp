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
#include "bbs/fsed/editor.h"

#include "core/os.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/format.h"
#include <stdexcept>

namespace wwiv::bbs::fsed {

using namespace wwiv::stl;
using namespace wwiv::strings;


/////////////////////////////////////////////////////////////////////////////
// EDITOR

line_t& editor_t::curline() {
  // TODO: insert return statement here
  while (curli >= ssize(lines_)) {
    lines_.emplace_back();
  }
  try {
    return lines_.at(curli);
  } catch (const std::exception& e) {
    LOG(ERROR) << "Exception trying to get line: " << curli << "; what: " << e.what();
    LOG(ERROR) << wwiv::os::stacktrace();
    throw;
  }
}

line_t& editor_t::line(int n) { 
  try {
    return lines_.at(n);
  } catch (const std::exception& e) {
    LOG(ERROR) << "Exception trying to get line: " << n << "; what: " << e.what();
    LOG(ERROR) << wwiv::os::stacktrace();
    throw;
  }
}

bool editor_t::set_lines(std::vector<line_t>&& n) {
  lines_ = n;
  return true;
}

void editor_t::emplace_back(line_t&& n) { lines_.emplace_back(n); }


bool editor_t::insert_line() { 
  if (ssize(lines_)  >= this->maxli) {
    return false;
  }
  return wwiv::stl::insert_at(lines_, curli, line_t());
}

bool editor_t::remove_line() { 
  if (lines_.empty()) {
    return false;
  }
  return wwiv::stl::erase_at(lines_, curli);
}

editor_add_result_t editor_t::add(char c) { 
  auto& line = curline(); 
  auto line_result = line.add(cx, c, mode_);
  if (line_result == line_add_result_t::error) {
    return editor_add_result_t::error;
  }
  auto start_line = curli;
  ++cx;
  if (cx < max_line_len) {
    // no  wrapping is needed
    if (line_result == line_add_result_t::needs_redraw) {
      invalidate_range(start_line, curli);
    }
    return editor_add_result_t::added;
  }
  int last_space = line.last_space_before(ssize(line));
  line.wrapped(true);
  if (last_space != -1 && (max_line_len - last_space) < (max_line_len / 2)) {
    // Word Wrap at the position after the last space. That way the space
    // end up on the previous line
    const auto wrap_position = last_space + 1;
    const auto nline = line.substr(wrap_position);
    line.assign(line.substr(0, wrap_position));
    ++curli;
    curline().assign(nline);
    cx = ssize(curline());
  } else {
    // Character wrap    
    ++curli;
    cx %= max_line_len;
  }
  // Create the new line.
  curline();
  invalidate_range(start_line, curli);
  return editor_add_result_t::wrapped;
}

cell_t editor_t::current_cell() {
  const auto& line = curline();
  if (cx >= ssize(line)) {
    return cell_t(0, ' ');
  }
  try {
    return line.cells().at(cx);
  } catch (const std::exception& e) {
    LOG(ERROR) << "Exception trying to get cx: " << cx << "; what: " << e.what();
    LOG(ERROR) << wwiv::os::stacktrace();
    throw;
  }
}

bool editor_t::del() { 
  auto r = curline().del(cx, mode_);
  if (r == line_add_result_t::error) {
    return false;
  }
  if (r == line_add_result_t::needs_redraw) {
    invalidate_to_eof(curli);
  }
  return true;
}

bool editor_t::bs() { 
  auto r = curline().bs(cx, mode_);
  if (r == line_add_result_t::error) {
    return false;
  }
  if (r == line_add_result_t::needs_redraw) {
    invalidate_to_eof(curli);
  }
  return true;
}

bool editor_t::add_callback(editor_range_invalidated_fn fn) {
  callbacks_.emplace_back(fn);
  return true;
}

void editor_t::invalidate_to_eol() {
  editor_range_t r{};
  r.start.line = r.end.line = curli;
  r.start.x = cx;
  r.end.x = ssize(curline());
  for (auto& c : callbacks_) {
    c(*this, r);
  }
}

void editor_t::invalidate_to_eof() {
  invalidate_to_eof(curli);
}

void editor_t::invalidate_to_eof(int start_line) {
  invalidate_range(start_line, ssize(lines_));
}

void editor_t::invalidate_range(int start_line, int end_line) {
  editor_range_t r{};
  r.start.line = start_line;
  r.start.x = cx;
  r.end.line = end_line;
  for (auto& c : callbacks_) {
    c(*this, r);
  }
}

std::vector<std::string> editor_t::to_lines() { 
  std::vector<std::string> out;
  for (auto l : lines_) {
    auto t = l.to_colored_text();
    wwiv::strings::StringTrimCRLF(&t);
    if (l.wrapped()) {
      // wwiv line wrapping character.
      t.push_back('\x1');
    }
    out.emplace_back(t);
  }

  return out;
}


} // namespace wwiv::bbs::fsed