/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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
#include  "bbs/basic.h"

#include <cstdarg>
#include <string>

#include "deps/my_basic-master/core/my_basic.h"

#include "bbs/application.h"
#include "bbs/bbs.h"
#include "bbs/input.h"
#include "core/file.h"
#include "core/log.h"
#include "core/textfile.h"
#include "core/strings.h"

using std::string;
using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv {
namespace bbs {

int my_print(const char* fmt, ...) {
  char buf[1024];

  va_list argptr;
  va_start(argptr, fmt);
  vsnprintf(buf, sizeof(buf), fmt, argptr);
  va_end(argptr);

  bout.bputs(buf);
  bout.nl();
  return MB_FUNC_OK;
}

int my_input(char* buf, int size) {
  auto v = Input1("", size, false, wwiv::bbs::InputMode::MIXED);
  strcpy(buf, v.c_str());
  return v.size();
}

static bool RegisterMyBasicGlobals() {
  mb_check(mb_init());
  return true;
}

static bool LoadBasicFile(mb_interpreter_t* bas, const std::string& script_name) {
  const auto path = FilePath(a()->config()->gfilesdir(), script_name);
  if (!File::Exists(path)) {
    LOG(ERROR) << "|#6Unable to locate script: " << path;
    bout << "|#6Unable to locate script: " << script_name;
    return false;
  }
  TextFile file(path, "r");
  if (!file) {
    LOG(ERROR) << "|#6Unable to read script: " << path;
    bout << "|#6Unable to read script: " << script_name;
  }

  auto lines = file.ReadFileIntoString();
  auto ret = mb_load_string(bas, lines.c_str(), true);
  return ret == MB_FUNC_OK;
}

bool RunBasicScript(const std::string& script_name) {
  static bool pnce = RegisterMyBasicGlobals();

  struct mb_interpreter_t* bas = nullptr;
  mb_open(&bas);
  mb_set_printer(bas, my_print);
  mb_set_inputer(bas, my_input);

  if (!LoadBasicFile(bas, script_name)) {
    bout << "|#6Unable to load script: " << script_name;
    return false;
  }

  auto ret = mb_run(bas);

  mb_close(&bas);
  return ret == MB_FUNC_OK;
}

}
}

