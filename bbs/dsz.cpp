/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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
/*                                                                        */
/**************************************************************************/
#include "bbs/dsz.h"

#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/format.h"
#include "sdk/files/files.h"
#include <string>
#include <tuple>

using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::strings;

std::tuple<dsz_logline_t, std::string, int> handle_dszline(const std::string& l) {

  const auto parts = SplitString(l, " \t");
  if (parts.size() < 11) {
    LOG(ERROR) << "Malformed DSZ Log Line: '" << l << "'";
    return std::make_tuple(dsz_logline_t::error, "", 0);
  }
  const auto code = parts.at(0).front();
  const auto cps = to_number<int>(parts.at(4));
  const auto fn = wwiv::sdk::files::align(parts.at(10));

  switch (code) {
  case 'Z':
  case 'r':
  case 'R':
  case 'B':
  case 'H':
    // received a file
    return std::make_tuple(dsz_logline_t::upload, fn, cps);
  case 'z':
  case 's':
  case 'S':
  case 'b':
  case 'h':
  case 'Q':
    // sent a file
    return std::make_tuple(dsz_logline_t::download, fn, cps);
  case 'E':
  case 'e':
  case 'L':
  case 'l':
  case 'U':
  default:
    return std::make_tuple(dsz_logline_t::error, "", -1);
  }
}

bool ProcessDSZLogFile(const std::string& path, dsz_logline_callback_fn cb) {
  TextFile tf(path, "rt");
  if (!tf) {
    return false;
  }
  const auto lines = tf.ReadFileIntoVector();
  for (const auto& l : lines) {
    auto [type, fn, cps] = handle_dszline(l);
    cb(type, fn, cps);
  }
  return true;
}
