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

#include "bbs/bbs.h"
#include "bbs/application.h"
#include "core/strings.h"
#include "core/file.h"

using std::string;
using namespace wwiv::core;
using namespace wwiv::strings;

static int check_arc(const std::string& filename) {
  File f(filename);
  if (!f.Open(File::modeReadOnly)) {
    return COMPRESSION_UNKNOWN;
  }

  char header[10];
  const auto num_read = f.Read(&header, 10);
  if (num_read < 10) {
    return COMPRESSION_UNKNOWN;
  }

  switch (header[0]) {
  case 0x60:
    if (static_cast<unsigned char>(header[1]) == static_cast<unsigned char>(0xEA))
      return COMPRESSION_ARJ;
    break;
  case 0x1a:
    return COMPRESSION_PAK;
  case 'P':
    if (header[1] == 'K')
      return COMPRESSION_ZIP;
    break;
  case 'R':
    if (header[1] == 'a')
      return COMPRESSION_RAR;
    break;
  case 'Z':
    if (header[1] == 'O' && header[2] == 'O')
      return COMPRESSION_ZOO;
    break;
  default: // FallThrough
    ;
  }
  if (header[0] == 'P') {
    return COMPRESSION_UNKNOWN;
  }
  header[9] = 0;
  if (strstr(header, "-lh")) {
    return COMPRESSION_LHA;
  }

  // Guess on type, using extension to guess
  auto p = std::filesystem::path(filename);
  auto ext = p.has_extension() ? p.extension().string() : "";
  if (ext.empty()) {
    return COMPRESSION_UNKNOWN;
  }
  ext = ext.substr(1);
  if (iequals(ext, "ZIP")) {
    return COMPRESSION_ZIP;
  }
  if (iequals(ext, "LHA")) {
    return COMPRESSION_LHA;
  }
  if (iequals(ext, "LZH")) {
    return COMPRESSION_LHA;
  }
  if (iequals(ext, "ZOO")) {
    return COMPRESSION_ZOO;
  }
  if (iequals(ext, "ARC")) {
    return COMPRESSION_PAK;
  }
  if (iequals(ext, "PAK")) {
    return COMPRESSION_PAK;
  }
  if (iequals(ext, "ARJ")) {
    return COMPRESSION_ARJ;
  }
  if (iequals(ext, "RAR")) {
    return COMPRESSION_RAR;
  }
  return COMPRESSION_UNKNOWN;
}

// Returns which archive as a number in your archive config
// Returns the first archiver you have listed if unknown
// One thing to note, if an 'arc' is found, it uses pak, and returns that
// The reason being, PAK does all ARC does, plus a little more, I believe
// PAK has its own special modes, but still look like an ARC, thus, an ARC
// Will puke if it sees this
int match_archiver(const std::string& filename) {
  auto x = check_arc(filename);
  if (x == COMPRESSION_UNKNOWN) {
    return 0;
  }
  std::string type;
  switch (x) {
  case COMPRESSION_ZIP:
    type = "ZIP";
    break;
  case COMPRESSION_LHA:
    type = "LHA";
    break;
  case COMPRESSION_ARJ:
    type = "ARJ";
    break;
  case COMPRESSION_PAK:
    type =  "PAK";
    break;
  case COMPRESSION_ZOO:
    type = "ZOO";
    break;
  case COMPRESSION_RAR:
    type = "RAR";
    break;
  default:
    type = "ZIP";
    break;
  }

  x = 0;
  while (x < 4) {
    if (iequals(type, a()->arcs[x].extension)) {
      return x;
    }
    ++x;
  }
  return 0;
}
