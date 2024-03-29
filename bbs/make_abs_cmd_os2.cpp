/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services            */
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
#include "bbs/make_abs_cmd.h"
#include "bbs/bbs.h"
#include "bbs/application.h"

#include "core/file.h"
#include "core/strings.h"
#include "fmt/format.h"

#include <direct.h>
#include <filesystem>
#include <string>
#include <vector>

#define __USE_EMX
#include <stdlib.h>

using namespace wwiv::core;
using namespace wwiv::strings;

void make_abs_cmd(const std::filesystem::path& root, std::string* out) {
  if (out->find("\\") == std::string::npos) {
        // Use current path of we don't have an abs path.
    std::string s(*out);
    File f(FilePath(a()->bbspath(), s));
    *out = f.full_pathname();
  }
}

