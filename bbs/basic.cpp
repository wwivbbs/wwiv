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
/**************************************************************************/
#include  "bbs/basic.h"

#include <cstdarg>
#include <string>
#include <vector>
#include "bbs/com.h"
#include "bbs/input.h"
#include "bbs/interpret.h"
#include "bbs/menu.h"
#include "bbs/pause.h"
#include "bbs/printfile.h"
#include "core/file.h"
#include "core/jsonfile.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "core/version.h"
#include "deps/my_basic/core/my_basic.h"
#include "sdk/config.h"
#include <cassert>
#include <mutex>

#include <cereal/cereal.hpp>
// ReSharper disable once CppUnusedIncludeDirective
#include <cereal/archives/json.hpp>
// ReSharper disable once CppUnusedIncludeDirective
#include <cereal/types/vector.hpp>
#include <optional>

using std::string;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv::bbs {

namespace {
  std::mutex g_script_mutex;
  // Globals to be used from the interpreter before execution begins
  std::string script_datadir;
  std::string script_scriptdir;

  Output* script_out_{nullptr};

  void set_script_out(Output* o) { script_out_ = o; }

  Output& script_out() {
    return script_out_ != nullptr ? *script_out_ : bout;
  }
}

static char* BasicStrDup(const std::string& s) {
  return mb_memdup(s.c_str(), s.size() + 1);
}

/**
 * Gets the WWIV script "userdata" (wwiv_script_userdata_t) that was applied at the
 * script level for the interpreter session.
 */
static wwiv_script_userdata_t* get_wwiv_script_userdata(struct mb_interpreter_t* bas) {
  void* x{};
  const auto result = mb_get_userdata(bas, &x);
  if (result != MB_FUNC_OK) {
    LOG(ERROR) << "Error getting the script userdata";
    return nullptr;
  }
  return static_cast<wwiv_script_userdata_t*>(x);
}

#define SERIALIZE(field) { \
  try { ar(cereal::make_nvp(#field, field)); \
  } catch(const cereal::Exception&) { ar.setNextName(nullptr); } \
  } 

  template <class Archive>
  void serialize(Archive& ar, script_data_t& t) {
    SERIALIZE(t.type);
    if (t.type == script_data_type_t::INT) {
      SERIALIZE(t.i);
    }
    else if (t.type == script_data_type_t::REAL) {
      SERIALIZE(t.r);
    }
    else if (t.type == script_data_type_t::STRING) {
      SERIALIZE(t.s);
    }
  }

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
  case MB_DT_NIL: 
  case MB_DT_UNKNOWN:
  case MB_DT_NUM:
  case MB_DT_TYPE:
  case MB_DT_USERTYPE:
  case MB_DT_USERTYPE_REF:
  case MB_DT_ARRAY:
  case MB_DT_LIST:
  case MB_DT_LIST_IT:
  case MB_DT_DICT:
  case MB_DT_DICT_IT:
  case MB_DT_COLLECTION:
  case MB_DT_ITERATOR:
  case MB_DT_CLASS:
  case MB_DT_ROUTINE:
  default: ;
    LOG(ERROR) << "Unable to convert type (unknown) for basic type: " << v.type;
    return{};
  }

}

static bool SaveData(const std::string& datadir, const std::string& basename, const std::vector<script_data_t>& data) {
  const auto path = PathFilePath(datadir, StrCat(basename, ".script.json"));
  JsonFile<decltype(data)> json(path, "data", data);
  return json.Save();
}

static std::vector<script_data_t> LoadData(const std::string& datadir, const std::string& basename) {
  std::vector<script_data_t> data;
  const auto path = PathFilePath(datadir, StrCat(basename, ".script.json"));
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

  script_out().bputs(buf);
  script_out().nl();
  return MB_FUNC_OK;
}

int my_input(const char* prompt, char* buf, int size) {
  if (prompt && *prompt) {
    script_out().bputs(prompt);
  }
  const auto v = input_text("", size);
  strcpy(buf, v.c_str());
  return v.size();
}

static bool RegisterMyBasicGlobals() {
  mb_init();
  return true;
}

static std::optional<std::string> ReadBasicFile(const std::string& script_name) {
    if (script_name.find("..") != std::string::npos) {
    LOG(ERROR) << "Invalid script name: " << script_name;
    script_out() << "|#6Invalid script name: " << script_name << "\r\n";
    return std::nullopt;
  }

  const auto path = PathFilePath(script_scriptdir, script_name);
  if (!File::Exists(path)) {
    LOG(ERROR) << "Unable to locate script: " << path;
    script_out() << "|#6Unable to locate script: " << script_name << "\r\n";
    return std::nullopt;
  }
  TextFile file(path, "r");
  if (!file) {
    LOG(ERROR) << "Unable to read script: " << path;
    script_out() << "|#6Unable to read script: " << script_name << "\r\n";
    return std::nullopt;
  }
  const auto lines = file.ReadFileIntoString();
  return {lines};
}

static bool LoadBasicFile(mb_interpreter_t* bas, const std::string& script_name) {

  auto o = ReadBasicFile(script_name);
  if (!o) {
    return false;
  }
  const auto ret = mb_load_string(bas, o.value().c_str(), true);
  return ret == MB_FUNC_OK;
}

static void _on_error(struct mb_interpreter_t* s, mb_error_e e, const char* m, const char* f, int p,
                      unsigned short row, unsigned short col, int abort_code) {
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
  const string file = (f) ? f : "(null)";
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

static bool RegisterNamespaceData(mb_interpreter_t* bas) {
  mb_begin_module(bas, "WWIV.DATA");

  mb_register_func(bas, "MODULE_NAME", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    mb_check(mb_attempt_close_bracket(bas, l));
    script_out() << "wwiv.data\r\n";
    mb_check(mb_push_string(bas, l, BasicStrDup("wwiv.data")));
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
        script_out() << "|#6Error: Only saving a LIST is currently supported. (not DICT)\r\n";
        return MB_FUNC_WARNING;
      }
      int count = 0;
      mb_check(mb_count_coll(bas, l, arg, &count));
      // script_out() << " with size: " << count;
      std::vector<script_data_t> data;
      for (auto i = 0; i < count; i++) {
        mb_value_t idx;
        mb_make_int(idx, i);
        mb_value_t val{};
        if (mb_get_coll(bas, l, arg, idx, &val) == MB_FUNC_OK) {
          data.emplace_back(to_script_data(val));
        }
      }
      const auto* d = get_wwiv_script_userdata(bas);

      if (!SaveData(d->datadir, d->module, data)) {
        script_out() << "#6Error saving data.\r\n";
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
        script_out() << "|#6Error: Only saving a LIST is currently supported. (not DICT)\r\n";
        return MB_FUNC_WARNING;
      }

      const auto* sd = get_wwiv_script_userdata(bas);

      auto current_count = 0;
      mb_check(mb_count_coll(bas, l, arg, &current_count));
      auto data = LoadData(sd->datadir, sd->module);
      for (const auto& d : data) {
        const auto val = to_mb_value(d);
        mb_value_t idx;
        mb_make_int(idx, current_count++);
        const auto ret = mb_set_coll(bas, l, arg, idx, val);
        if (ret != MB_FUNC_OK) {
          script_out() << "[oops] ";
        }
      }

      if (!SaveData(sd->datadir, sd->module, data)) {
        script_out() << "#6Error loading data.\r\n";
      }
    }
    mb_check(mb_attempt_close_bracket(bas, l));
    return MB_FUNC_OK;
  });


  return mb_end_module(bas) == MB_FUNC_OK;
}

static bool RegisterNamespaceWWIVIO(mb_interpreter_t* bas) {
  mb_begin_module(bas, "WWIV.IO");

  mb_register_func(bas, "MODULE_NAME", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    mb_check(mb_attempt_close_bracket(bas, l));
    script_out() << "wwiv.io\r\n";
    mb_check(mb_push_string(bas, l, BasicStrDup("wwiv.io")));
    return MB_FUNC_OK;
  });

  mb_register_func(bas, "PUTS", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    while (mb_has_arg(bas, l)) {
      char* arg = nullptr;
      mb_check(mb_pop_string(bas, l, &arg));
      script_out().bputs(arg);
    }
    mb_check(mb_attempt_close_bracket(bas, l));
    return MB_FUNC_OK;
  });

  mb_register_func(bas, "PL", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    while (mb_has_arg(bas, l)) {
      char* arg = nullptr;
      mb_check(mb_pop_string(bas, l, &arg));
      script_out().bputs(arg);
      script_out().nl();
    }
    mb_check(mb_attempt_close_bracket(bas, l));
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

  mb_register_func(bas, "GETS", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    auto arg = 0;
    if (mb_has_arg(bas, l)) {
      mb_check(mb_pop_int(bas, l, &arg));
    }
    mb_check(mb_attempt_close_bracket(bas, l));
    if (arg > 0) {
      const auto s = input_text("", arg);
      mb_push_string(bas, l, BasicStrDup(s));
    }
    return MB_FUNC_OK;
  });

  mb_register_func(bas, "GETKEY", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    mb_check(mb_attempt_close_bracket(bas, l));
    const auto ch = script_out().getkey();
    char s[2] = { ch, 0 };
    mb_push_string(bas, l, BasicStrDup(s));
    return MB_FUNC_OK;
  });

  mb_register_func(bas, "CLS", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    script_out().cls();
    mb_check(mb_attempt_close_bracket(bas, l));
    return MB_FUNC_OK;
  });

  mb_register_func(bas, "NL", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    int num_lines = 1;
    if (mb_has_arg(bas, l)) {
      mb_check(mb_pop_int(bas, l, &num_lines));
    }
    script_out().nl(num_lines);
    mb_check(mb_attempt_close_bracket(bas, l));
    return MB_FUNC_OK;
  });

  mb_register_func(bas, "YN", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    while (mb_has_arg(bas, l)) {
      char* arg = nullptr;
      mb_check(mb_pop_string(bas, l, &arg));
      script_out().bputs(arg);
    }
    mb_check(mb_attempt_close_bracket(bas, l));
    const auto ret = yesno();

    mb_push_int(bas, l, ret ? 1 : 0);
    return MB_FUNC_OK;
  });

  mb_register_func(bas, "NY", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    while (mb_has_arg(bas, l)) {
      char* arg = nullptr;
      mb_check(mb_pop_string(bas, l, &arg));
      script_out().bputs(arg);
    }
    mb_check(mb_attempt_close_bracket(bas, l));
    const auto ret = noyes();

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

static bool RegisterNamespaceWWIV(mb_interpreter_t* bas) {
  mb_begin_module(bas, "WWIV");
  mb_register_func(bas, "VERSION", _version);

  mb_register_func(bas, "MODULE_NAME", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    mb_check(mb_attempt_close_bracket(bas, l));
    script_out() << "wwiv\r\n";
    mb_check(mb_push_string(bas, l, BasicStrDup("wwiv")));
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

  // Crappy, awful API way to get data out of WWIVs' macros.
  mb_register_func(bas, "INTERPRET", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    if (!mb_has_arg(bas, l)) {
      return MB_FUNC_ERR;
    }
    char* arg = nullptr;
    mb_check(mb_pop_string(bas, l, &arg));

    const auto* d = get_wwiv_script_userdata(bas);

    const auto s = d->ctx->interpret(*arg);
    mb_check(mb_attempt_close_bracket(bas, l));
    mb_push_string(bas, l, BasicStrDup(s));
    return MB_FUNC_OK;
  });

  return mb_end_module(bas) == MB_FUNC_OK;
}

Basic::Basic(Output& o, const wwiv::sdk::Config& config, const MacroContext* ctx)
  : bout_(o), config_(config), ctx_(ctx) {
  // Set the static class level variables used in capture-less lambdas.
  script_datadir = config_.datadir();
  script_scriptdir = config_.scriptdir();

  // Creates the script user-data passed to the interpreter.  This data can be used
  // by the implementation of custom functions.  Values derived from the script name
  // must be set in RunScript.
  script_userdata_.datadir = config_.datadir();
  script_userdata_.ctx = ctx;
  script_userdata_.module = "none";

  [[maybe_unused]] static auto once = RegisterMyBasicGlobals();

  bas_ = SetupBasicInterpreter();
  RegisterDefaultNamespaces();

}

static std::string ScriptBaseName(const std::string& script_name) {
 const auto basename = ToStringLowerCase(script_name);
  if (ends_with(basename, ".bas")) {
    return basename.substr(0, basename.find_last_of('.'));
  }
  return basename;
}

// static
mb_interpreter_t* Basic::SetupBasicInterpreter() {
  struct mb_interpreter_t* bas = nullptr;
  mb_open(&bas);

  // TODO(rushfan): Update to new syntax
  // mb_debug_set_stepped_handler(bas, _on_step-ped);
  mb_set_error_handler(bas, _on_error);

  mb_set_import_handler(bas, [](struct mb_interpreter_t* bas, const char* p) -> int {
    return (LoadBasicFile(bas, p)) ? MB_FUNC_OK : MB_FUNC_ERR;
  });

  mb_set_printer(bas, my_print);
  mb_set_inputer(bas, my_input);

  return bas;  
}

bool Basic::RegisterDefaultNamespaces() {
  RegisterNamespaceWWIV(bas_);
  RegisterNamespaceWWIVIO(bas_);
  RegisterNamespaceData(bas_);

  return true;
}

bool Basic::RunScript(const std::string& module, const std::string& text) {

  script_userdata_.module = module;
  mb_set_userdata(bas_, &script_userdata_);

  if (mb_load_string(bas_, text.c_str(), true) != MB_FUNC_OK) {
    LOG(ERROR) << "Unable to load text: '" << text << "'";
    return false;
  }

  std::lock_guard<std::mutex> guard(g_script_mutex);
  set_script_out(&bout_);
  const auto ret = mb_run(bas_, false);
  mb_close(&bas_);

  // We don't call mb_dispose since we only call mb_init once per execution.
  if (ret != MB_FUNC_OK) {
    LOG(INFO) << "Failure exiting script: '" << module << "' error code (MB_FUNC_XXXX) : " << ret;
    return false;
  }
  return true;
}

bool Basic::RunScript(const std::string& script_name) {
  const auto path = PathFilePath(config_.scriptdir(), script_name);
  if (!File::Exists(path)) {
    script_out() << "|#6Unable to locate script: " << script_name;
    return false;
  }

  const auto module = ScriptBaseName(script_name);
  auto o = ReadBasicFile(script_name);
  if (!o) {
    return false;
  }
  return RunScript(module, o.value());
}


bool RunBasicScript(const std::string& script_name) {
  BbsMacroContext ctx(a()->user(), a()->mci_enabled_);
  Basic basic(bout, *a()->config(), &ctx);
  return basic.RunScript(script_name);
}

}

