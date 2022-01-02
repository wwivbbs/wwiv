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
#include "fsed/common.h"

#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"

namespace wwiv::fsed {

using namespace wwiv::stl;
using namespace wwiv::strings;

/////////////////////
// LOCALS

std::vector<line_t> read_file(const std::filesystem::path& path, int line_length) {
  TextFile f(path, "rt");
  if (!f) {
    return {};
  }
  auto lines = f.ReadFileIntoVector();

  std::vector<line_t> out;
  for (auto l : lines) {
    do {
      const auto size_wc = size_without_colors(l);
      if (size_wc <= line_length) {
        out.emplace_back(false, l);
        break;
      }
      // We have a long line
      auto pos = line_length;
      while (pos > 0 && l[pos] > 32) {
        pos--;
      }
      if (pos == 0) {
        pos = line_length;
      }
      auto subset_of_l = l.substr(0, pos);
      l = l.substr(pos + 1);
      out.emplace_back(true, l);
    } while (true);
  }
  return out;
}



} // namespace wwiv::fsed