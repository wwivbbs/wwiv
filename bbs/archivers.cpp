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
#include "bbs/archivers.h"

#include "bbs/bbs.h"
#include "bbs/make_abs_cmd.h"
#include "bbs/stuffin.h"
#include "core/file.h"
#include "core/strings.h"
#include "sdk/files/arc.h"
#include <cstring>
#include <string>

using std::string;
using namespace wwiv::core;
using namespace wwiv::strings;

static const std::string DEFAULT_EXT = "ZIP";
constexpr int MAX_ARCS = 15;


// Returns which archive as a number in your archive config
// Returns the first archiver you have listed if unknown
std::optional<arcrec> match_archiver(const std::vector<arcrec>& arcs, const std::string& filename) {
  return wwiv::sdk::files::find_arcrec(arcs, filename, DEFAULT_EXT);
}

std::optional<arc_command_t> get_arc_cmd(const std::string& arc_fn, arc_command_type_t cmdtype, const std::string& ofn) {

  auto oa = wwiv::sdk::files::find_arcrec(a()->arcs, arc_fn);
  if (!oa) {
    return std::nullopt;
  }
  auto arc = oa.value();
  std::string cmdline;
  switch (cmdtype) {
  case arc_command_type_t::list:
    cmdline = arc.arcl;
    break;
  case arc_command_type_t::extract:
    cmdline = arc.arce;
    break;
  case arc_command_type_t::add:
    cmdline = arc.arca;
    break;
  case arc_command_type_t::remove:
    cmdline = arc.arcd;
    break;
  case arc_command_type_t::comment:
    cmdline = arc.arck;
    break;
  case arc_command_type_t::test:
    cmdline = arc.arct;
    break;
  default:
    // Unknown type.
    return std::nullopt;
  }

  if (cmdline.empty()) {
    return std::nullopt;
  }

  if (cmdline == "@internal") {
    arc_command_t c{};
    c.internal = true;
    return {c};
  }

  auto out = stuff_in(cmdline, arc_fn, ofn, "", "", "");
  const auto root = a()->bbspath().string();
  make_abs_cmd(root, &out);
  arc_command_t c{false, out};
  return {c};
}
