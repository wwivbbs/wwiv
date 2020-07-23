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
#include "bbs/fsed/line.h"

#include "bbs/bbs.h"
#include "bbs/bgetch.h"
#include "bbs/full_screen.h"
#include "bbs/message_editor_data.h"
#include "bbs/output.h"
#include "bbs/pause.h"
#include "bbs/quote.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/format.h"
#include "local_io/keycodes.h"

namespace wwiv::bbs::fsed {

using namespace wwiv::stl;
using namespace wwiv::strings;

line_add_result_t line_t::add(int x, char c, ins_ovr_mode_t mode) {
  while (ssize(text) < x) {
    text.push_back(' ');
  }
  if (x == ssize(text)) {
    text.push_back(c);
    return line_add_result_t::no_redraw;
  } else if (mode == ins_ovr_mode_t::ins) {
    wwiv::stl::insert_at(text, x, c);
    return line_add_result_t::needs_redraw;
  } else {
    text[x] = c;
    return line_add_result_t::no_redraw;
  }
}

line_add_result_t line_t::del(int x, ins_ovr_mode_t) {
  if (x < 0) {
    return line_add_result_t::error;
  }
  auto result = (x == ssize(text)) ? line_add_result_t::no_redraw : line_add_result_t::needs_redraw;
  if (!wwiv::stl::erase_at(text, x)) {
    return line_add_result_t ::error;
  }
  return result;
}

line_add_result_t line_t::bs(int x, ins_ovr_mode_t mode) { 
  if (x <= 0) {
    return line_add_result_t::error;
  }
  return del(x - 1, mode);
}

int line_t::size() const { 
  return ssize(text);
}

} // namespace wwiv::bbs::fsed