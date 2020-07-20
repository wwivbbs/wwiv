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

#include "core/file.h"
#include "core/strings.h"
#include "sdk/files/arc.h"
#include <cstring>
#include <string>

using std::string;
using namespace wwiv::core;
using namespace wwiv::strings;

static const std::string DEFAULT_EXT = "ZIP";

// Returns which archive as a number in your archive config
// Returns the first archiver you have listed if unknown
std::optional<arcrec> match_archiver(const std::vector<arcrec>& arcs, const std::string& filename) {
  return wwiv::sdk::files::find_arcrec(arcs, filename, DEFAULT_EXT);
}
