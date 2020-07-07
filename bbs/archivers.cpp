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

#include <cstring>
#include <string>
#include <core/strings.h>
#include "core/file.h"

using std::string;
using namespace wwiv::core;
using namespace wwiv::strings;

static const std::string DEFAULT_EXT = "ZIP";

// One thing to note, if an 'arc' is found, it uses pak, and returns that
// The reason being, PAK does all ARC does, plus a little more, I believe
// PAK has its own special modes, but still look like an ARC, thus, an ARC
// Will puke if it sees this
static std::string derive_arc_extension(const std::string& filename) {
  File f(filename);
  if (!f.Open(File::modeReadOnly | File::modeBinary)) {
    return DEFAULT_EXT;
  }

  char header[10];
  const auto num_read = f.Read(&header, 10);
  if (num_read < 10) {
    return DEFAULT_EXT;
  }

  switch (header[0]) {
  case 0x60:
    if (static_cast<unsigned char>(header[1]) == static_cast<unsigned char>(0xEA))
      return "ARJ";
    break;
  case 0x1a:
    return "PAK";
  case 'P': {
    if (header[1] == 'K') {
      return "ZIP";
    }
  } break;
  case 'R':
    if (header[1] == 'a')
      return "RAR";
    break;
  case 'Z':
    if (header[1] == 'O' && header[2] == 'O')
      return "ZOO";
    break;
  default:
    ;
  }
  header[9] = '\0';
  if (strstr(header, "-lh")) {
    return "LHA";
  }

  // Guess on type, using extension to guess
  const auto p = std::filesystem::path(filename);
  const auto ext = p.has_extension() ? p.extension().string() : "";
  return ext.empty() ? DEFAULT_EXT : ToStringUpperCase(ext.substr(1));
}

// Returns which archive as a number in your archive config
// Returns the first archiver you have listed if unknown
std::optional<arcrec> match_archiver(const std::vector<arcrec>& arcs, const std::string& filename) {
  const auto type = derive_arc_extension(filename);
  auto arc_num = 0;
  for (const auto& arc : arcs) {
    if (iequals(type, arc.extension)) {
      return {arc};
    }
    ++arc_num;
  }
  return std::nullopt;
}
