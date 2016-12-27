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
#include "core/version.h"

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
  auto v = inputl(size, false);
  strcpy(buf, v.c_str());
  return v.size();
}

static bool RegisterMyBasicGlobals() {
  mb_init();
  return true;
}

// bobby

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
  // We always want to initialize our variables first.
  mb_load_string(bas, "__initvars()\r\n", false);

  // auto ret = mb_load_file(bas, path.c_str());
  auto ret = mb_load_string(bas, lines.c_str(), true);
  return ret == MB_FUNC_OK;
}

static void _on_error(struct mb_interpreter_t* s, mb_error_e e, char* m, char* f, int p, unsigned short row, unsigned short col, int abort_code) {
  mb_unrefvar(s);
  mb_unrefvar(p);

  if (e != SE_NO_ERR) {
    if (f) {
      if (e == SE_RN_WRONG_FUNCTION_REACHED) {
        printf(
          "Error:\n    Line %d, Col %d in Func: %s\n    Code %d, Abort Code %d\n    Message: %s.\n",
          row, col, f,
          e, abort_code,
          m
        );
      }
      else {
        printf(
          "Error:\n    Line %d, Col %d in File: %s\n    Code %d, Abort Code %d\n    Message: %s.\n",
          row, col, f,
          e, e == SE_EA_EXTENDED_ABORT ? abort_code - MB_EXTENDED_ABORT : abort_code,
          m
        );
      }
    }
    else {
      printf(
        "Error:\n    Line %d, Col %d\n    Code %d, Abort Code %d\n    Message: %s.\n",
        row, col,
        e, e == SE_EA_EXTENDED_ABORT ? abort_code - MB_EXTENDED_ABORT : abort_code,
        m
      );
    }
  }
}

static void _on_stepped(struct mb_interpreter_t* s, void** l, char* f, int p, unsigned short row, unsigned short col) {
  string file = (f) ? f : "(null)";
  VLOG(2) << "p: " << p << "; f: " << file << "; row: " << row << "; col: " << col;

  mb_unrefvar(s);
  mb_unrefvar(l);
  mb_unrefvar(f);
  mb_unrefvar(p);
  mb_unrefvar(row);
  mb_unrefvar(col);
}

static int initvars(struct mb_interpreter_t* bas, void** l) {
  VLOG(1) << "initvars";
  mb_check(mb_attempt_open_bracket(bas, l));
  mb_check(mb_attempt_close_bracket(bas, l));

  mb_value_t w{};
  w.type = MB_DT_STRING;
  w.value.string = strdup(wwiv_version);
  mb_add_var(bas, l, "WWIV", w, true);

  LOG(INFO) << "initvars: " << mb_get_type_string(w.type);
  mb_value_t val;
  mb_get_value_by_name(bas, l, "WWIV", &val);
  LOG(INFO) << "val: " << val.value.string;

  return MB_FUNC_OK;
}

bool RunBasicScript(const std::string& script_name) {
  static bool pnce = RegisterMyBasicGlobals();

  struct mb_interpreter_t* bas = nullptr;
  mb_open(&bas);
  mb_debug_set_stepped_handler(bas, _on_stepped);
  mb_set_error_handler(bas, _on_error);

  mb_set_printer(bas, my_print);
  mb_set_inputer(bas, my_input);
  mb_register_func(bas, "__INITVARS", initvars);

  auto path = FilePath(a()->config()->gfilesdir(), script_name);
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

