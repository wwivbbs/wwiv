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
#include <vector>

// TODO(rushfan): Should these move to jsonfile.h?
#include <cereal/cereal.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>

#include "deps/my_basic-master/core/my_basic.h"

#include "bbs/application.h"
#include "bbs/bbs.h"
#include "bbs/com.h"
#include "bbs/input.h"
#include "bbs/interpret.h"
#include "bbs/menu.h"
#include "bbs/pause.h"
#include "bbs/printfile.h"
#include "core/file.h"
#include "core/jsonfile.h"
#include "core/log.h"
#include "core/textfile.h"
#include "core/strings.h"
#include "core/version.h"

using std::string;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv {
namespace bbs {

static char* BasicStrDup(const std::string& s) {
  return mb_memdup(s.c_str(), s.size() + 1);
}

enum class script_data_type_t { STRING, INT, REAL };

struct wwiv_script_userdata_t {
  string module;
};

#define SERIALIZE(field) { try { ar(cereal::make_nvp(#field, field)); } catch(const cereal::Exception&) { ar.setNextName(nullptr); } }

struct script_data_t {
  script_data_type_t type;
  string s;
  int i;
  float r;

  template <class Archive>
  void serialize(Archive& ar) {
    SERIALIZE(type);
    if (type == script_data_type_t::INT) {
      SERIALIZE(i);
    }
    else if (type == script_data_type_t::REAL) {
      SERIALIZE(r);
    }
    else if (type == script_data_type_t::STRING) {
      SERIALIZE(s);
    }
  }


};

static mb_value_t to_mb_value(const script_data_t& f) {
  mb_value_t t{};
  switch (f.type) {
  case script_data_type_t::INT:
    t.type = MB_DT_INT;
    t.value.integer = f.i;
    return t;
  case script_data_type_t::REAL:
    t.type = MB_DT_REAL;
    t.value.float_point = f.r;
    return t;
  case script_data_type_t::STRING:
    t.type = MB_DT_STRING;
    t.value.string = BasicStrDup(f.s);
    return t;
  }
  return t;
}

static script_data_t to_script_data(const mb_value_t& v) {
  script_data_t result{};
  switch (v.type) {
  case MB_DT_INT:
    result.type = script_data_type_t::INT;
    result.i = v.value.integer;
    return result;
  case MB_DT_REAL:
    result.type = script_data_type_t::REAL;
    result.r = v.value.float_point;
    return result;
  case MB_DT_STRING:
    result.type = script_data_type_t::STRING;
    result.s = v.value.string;
    return result;
  }

  return{};
}

static bool SaveData(const std::string basename, const std::vector<script_data_t>& data) {
  const auto path = FilePath(a()->config()->datadir(), StrCat(basename, ".script.json"));
  JsonFile<decltype(data)> json(path, "data", data);
  return json.Save();
}

static std::vector<script_data_t> LoadData(const std::string basename) {
  std::vector<script_data_t> data;
  const auto path = FilePath(a()->config()->datadir(), StrCat(basename, ".script.json"));
  JsonFile<decltype(data)> json(path, "data", data);
  json.Load();
  return data;
}

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
  const auto path = FilePath(a()->config()->scriptdir(), script_name);
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

static int _version(struct mb_interpreter_t* bas, void** l) {
  mb_check(mb_attempt_open_bracket(bas, l));
  mb_check(mb_attempt_close_bracket(bas, l));
  mb_push_string(bas, l, BasicStrDup(wwiv_version));
  return MB_FUNC_OK;
}

static bool RegisterNamespaceData(mb_interpreter_t* bas, const std::string& basename) {
  mb_begin_module(bas, "WWIV.DATA");

  mb_register_func(bas, "MODULE_NAME", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    mb_check(mb_attempt_close_bracket(bas, l));
    bout << "wwiv.data\r\n";
    return MB_FUNC_OK;
  });

  mb_register_func(bas, "SAVE", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_assert(bas && l);
    mb_check(mb_attempt_open_bracket(bas, l));
    char* scope = nullptr;
    if (mb_has_arg(bas, l)) {
      // Scope: GLOBAL OR USER
      mb_check(mb_pop_string(bas, l, &scope));
    }
    if (mb_has_arg(bas, l)) {
      mb_value_t arg;
      mb_make_nil(arg);
      mb_check(mb_pop_value(bas, l, &arg));
      // arg should be coll.
      if (arg.type != MB_DT_LIST) {
        bout << "|#6Error: Only saving a LIST is currently supported. (not DICT)\r\n";
        return MB_FUNC_WARNING;
      }
      int count = 0;
      mb_check(mb_count_coll(bas, l, arg, &count));
      // bout << " with size: " << count;
      std::vector<script_data_t> data;
      for (int i = 0; i < count; i++) {
        mb_value_t idx{};
        mb_make_int(idx, i);
        mb_value_t val{};
        if (mb_get_coll(bas, l, arg, idx, &val) == MB_FUNC_OK) {
          data.emplace_back(to_script_data(val));
        }
      }
      wwiv_script_userdata_t* wwiv_userdata = nullptr;
      void* x;
      mb_get_userdata(bas, &x);
      wwiv_userdata = (wwiv_script_userdata_t*)x;

      if (!SaveData(wwiv_userdata->module, data)) {
        bout << "#6Error saving data.\r\n";
      }
    }
    mb_check(mb_attempt_close_bracket(bas, l));
    return MB_FUNC_OK;
  });

  // l = LOAD("GLOBAL|USER")
  mb_register_func(bas, "LOAD", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_assert(bas && l);
    mb_check(mb_attempt_open_bracket(bas, l));
    char* scope = nullptr;
    if (mb_has_arg(bas, l)) {
      // Scope: GLOBAL OR USER
      mb_check(mb_pop_string(bas, l, &scope));
    }
    if (mb_has_arg(bas, l)) {
      mb_value_t arg;
      mb_make_nil(arg);
      mb_check(mb_pop_value(bas, l, &arg));
      // arg should be coll.
      if (arg.type != MB_DT_LIST) {
        bout << "|#6Error: Only saving a LIST is currently supported. (not DICT)\r\n";
        return MB_FUNC_WARNING;
      }

      wwiv_script_userdata_t* wwiv_userdata = nullptr;
      void* x;
      mb_get_userdata(bas, &x);
      wwiv_userdata = (wwiv_script_userdata_t*)x;

      int current_count = 0;
      mb_check(mb_count_coll(bas, l, arg, &current_count));
      //bout << " with existing size: " << current_count;
      std::vector<script_data_t> data = LoadData(wwiv_userdata->module);
      for (const auto& d : data) {
        mb_value_t val = to_mb_value(d);
        mb_value_t idx{};
        mb_make_int(idx, current_count++);
        auto ret = mb_set_coll(bas, l, arg, idx, val);
        if (ret != MB_FUNC_OK) {
          bout << "[oops] ";
        }
      }

      if (!SaveData(wwiv_userdata->module, data)) {
        bout << "#6Error loading data.\r\n";
      }
    }
    mb_check(mb_attempt_close_bracket(bas, l));
    return MB_FUNC_OK;
  });


  return mb_end_module(bas) == MB_FUNC_OK;
}

static bool RegisterNamespaceWWIVIO(mb_interpreter_t* bas, const std::string& basename) {
  mb_begin_module(bas, "WWIV.IO");

  mb_register_func(bas, "MODULE_NAME", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    mb_check(mb_attempt_close_bracket(bas, l));
    bout << "wwiv.io.test\r\n";
    return MB_FUNC_OK;
  });

  mb_register_func(bas, "PUTS", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    while (mb_has_arg(bas, l)) {
      char* arg = nullptr;
      mb_check(mb_pop_string(bas, l, &arg));
      bout.bputs(arg);
    }
    mb_check(mb_attempt_close_bracket(bas, l));
    return MB_FUNC_OK;
  });

  mb_register_func(bas, "PL", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    while (mb_has_arg(bas, l)) {
      char* arg = nullptr;
      mb_check(mb_pop_string(bas, l, &arg));
      bout.bputs(arg);
      bout.nl();
    }
    mb_check(mb_attempt_close_bracket(bas, l));
    return MB_FUNC_OK;
  });

  mb_register_func(bas, "GETS", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    int arg = 0;
    if (mb_has_arg(bas, l)) {
      mb_check(mb_pop_int(bas, l, &arg));
    }
    mb_check(mb_attempt_close_bracket(bas, l));
    if (arg > 0) {
      string s = inputl(arg, true);
      mb_push_string(bas, l, BasicStrDup(s));
    }
    return MB_FUNC_OK;
  });

  mb_register_func(bas, "GETKEY", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    mb_check(mb_attempt_close_bracket(bas, l));
    char ch = bout.getkey();
    char s[2] = { ch, 0 };
    mb_push_string(bas, l, BasicStrDup(s));
    return MB_FUNC_OK;
  });

  mb_register_func(bas, "CLS", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    bout.cls();
    mb_check(mb_attempt_close_bracket(bas, l));
    return MB_FUNC_OK;
  });

  mb_register_func(bas, "NL", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    int num_lines = 1;
    if (mb_has_arg(bas, l)) {
      mb_check(mb_pop_int(bas, l, &num_lines));
    }
    bout.nl(num_lines);
    mb_check(mb_attempt_close_bracket(bas, l));
    return MB_FUNC_OK;
  });

  mb_register_func(bas, "YN", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    while (mb_has_arg(bas, l)) {
      char* arg = nullptr;
      mb_check(mb_pop_string(bas, l, &arg));
      bout.bputs(arg);
    }
    mb_check(mb_attempt_close_bracket(bas, l));
    bool ret = yesno();

    mb_push_int(bas, l, ret ? 1 : 0);
    return MB_FUNC_OK;
  });

  mb_register_func(bas, "NY", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    while (mb_has_arg(bas, l)) {
      char* arg = nullptr;
      mb_check(mb_pop_string(bas, l, &arg));
      bout.bputs(arg);
    }
    mb_check(mb_attempt_close_bracket(bas, l));
    bool ret = noyes();

    mb_push_int(bas, l, ret ? 1 : 0);
    return MB_FUNC_OK;
  });

  mb_register_func(bas, "PAUSE", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    mb_check(mb_attempt_close_bracket(bas, l));
    pausescr();
    return MB_FUNC_OK;
  });

  return mb_end_module(bas) == MB_FUNC_OK;
}

static bool RegisterNamespaceWWIV(mb_interpreter_t* bas, const std::string& basename) {
  mb_begin_module(bas, "WWIV");
  mb_register_func(bas, "VERSION", _version);

  mb_register_func(bas, "MODULE_NAME", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    mb_check(mb_attempt_close_bracket(bas, l));
    bout << "wwiv.test\r\n";
    return MB_FUNC_OK;
  }); 

  mb_register_func(bas, "COMMAND", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    char* arg = nullptr;
    if (mb_has_arg(bas, l)) {
      mb_check(mb_pop_string(bas, l, &arg));
    }
    mb_check(mb_attempt_close_bracket(bas, l));
    if (arg) {
      wwiv::menus::InterpretCommand(nullptr, arg);
    }
    return MB_FUNC_OK;
  });

  mb_register_func(bas, "PRINTFILE", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    if (mb_has_arg(bas, l)) {
      char* arg = nullptr;
      mb_check(mb_pop_string(bas, l, &arg));
      printfile(arg);
    }
    mb_check(mb_attempt_close_bracket(bas, l));
    return MB_FUNC_OK;
  });

  // Crappy, awful API way to get data out of WWIVs' macros.
  mb_register_func(bas, "INTERPRET", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    if (!mb_has_arg(bas, l)) {
      return MB_FUNC_ERR;
    }
    char* arg = nullptr;
    mb_check(mb_pop_string(bas, l, &arg));
    BbsMacroContext ctx(a()->user());
    string s = interpret(*arg, ctx);
    mb_check(mb_attempt_close_bracket(bas, l));
    mb_push_string(bas, l, BasicStrDup(s));
    return MB_FUNC_OK;
  });

  return mb_end_module(bas) == MB_FUNC_OK;
}

bool RunBasicScript(const std::string& script_name) {
  static bool pnce = RegisterMyBasicGlobals();

  auto path = FilePath(a()->config()->scriptdir(), script_name);
  if (!File::Exists(path)) {
    bout << "|#6Unable to locate script: " << script_name;
    return false;
  }
  string basename = ToStringLowerCase(script_name);
  if (ends_with(basename, ".bas")) {
    basename = basename.substr(0, basename.find_last_of('.'));
  }

  wwiv_script_userdata_t wwiv_userdata;
  wwiv_userdata.module = basename;
  struct mb_interpreter_t* bas = nullptr;
  mb_open(&bas);
  mb_set_userdata(bas, &wwiv_userdata);
  mb_debug_set_stepped_handler(bas, _on_stepped);
  mb_set_error_handler(bas, _on_error);

  mb_set_printer(bas, my_print);
  mb_set_inputer(bas, my_input);

  RegisterNamespaceWWIV(bas, basename);
  RegisterNamespaceWWIVIO(bas, basename);
  RegisterNamespaceData(bas, basename);

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

