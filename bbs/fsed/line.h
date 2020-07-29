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
#ifndef __INCLUDED_BBS_FSED_LINE_H__
#define __INCLUDED_BBS_FSED_LINE_H__

#include <filesystem>
#include <vector>
#include <string>

namespace wwiv::bbs::fsed {

enum class line_add_result_t { needs_redraw, no_redraw, error };
enum class ins_ovr_mode_t { ins, ovr };

class cell_t {
public:
  cell_t(int co, char cc) : wwiv_color(co), ch(cc) {}
  int wwiv_color{0};
  char ch{0};
};

enum class line_color_code_format_t { heart, pipe };

class line_t {
public:
  line_t() : line_t(false, "") {}
  line_t(bool wrapped, std::string text);
  line_t(std::string text) : line_t(false, text) {}

  line_add_result_t add(int x, char c, ins_ovr_mode_t mode);
  line_add_result_t del(int x, ins_ovr_mode_t mode);
  line_add_result_t bs(int x, ins_ovr_mode_t mode);
  void push_back(char c);

  bool wrapped() const noexcept { return wrapped_; }
  void wrapped(bool b) { wrapped_ = b; }

  std::size_t size() const;

  // Specialized stuff
  int last_space_before(int maxlen);
  void set_wwiv_color(int c);
  int wwiv_color() const noexcept;

  // operators
  line_t& operator=(const line_t& o);

  void assign(const std::vector<cell_t>& cells);
  void append(const std::vector<cell_t>& cells);
  const std::vector<cell_t>& cells() const { return cell_; }
  std::vector<cell_t> substr(int start, int end);
  std::vector<cell_t> substr(int start);
  // Gets a line of text that can be displayed using bputs
  std::string to_colored_text() const;

private:
  bool wrapped_{false};
  std::vector<cell_t> cell_;

  int size_{0};
  int wwiv_color_{0};
};

}

#endif