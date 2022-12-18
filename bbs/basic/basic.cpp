/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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
#include "bbs/basic/basic.h"

#include "bbs/application.h"
#include "bbs/bbs.h"
#include "bbs/basic/util.h"
#include "bbs/basic/wwiv.h"
#include "bbs/basic/wwiv_data.h"
#include "bbs/basic/wwiv_file.h"
#include "bbs/basic/wwiv_io.h"
#include "bbs/basic/wwiv_os.h"
#include "bbs/basic/wwiv_time.h"
#include "common/input.h"
#include "core/file.h"
#include "core/jsonfile.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "deps/my_basic/core/my_basic.h"
#include "sdk/config.h"
#include <cstdarg>
#include <mutex>
#include <optional>
#include <string>

using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv::bbs::basic {

static int my_print(struct mb_interpreter_t *, const char* fmt, ...) {
  char buf[1024];

  va_list argptr;
  va_start(argptr, fmt);
  vsnprintf(buf, sizeof(buf), fmt, argptr);  // NOLINT(clang-diagnostic-format-nonliteral)
  va_end(argptr);

  script_out().puts(buf);
  script_out().nl();
  return MB_FUNC_OK;
}

static int my_input(struct mb_interpreter_t*, const char* prompt, char* buf, int size) {
  if (prompt && *prompt) {
    script_out().puts(prompt);
  }
  const auto v = script_in().input_text("", size);
  strcpy(buf, v.c_str());
  return stl::size_int(v);
}

static bool RegisterMyBasicGlobals() {
  mb_init();
  return true;
}

static std::optional<std::string> ReadBasicFile(const std::filesystem::path& path,
                                                const std::string& script_name) {
    if (script_name.find("..") != std::string::npos) {
    LOG(ERROR) << "Invalid script name: " << script_name;
    script_out().print("|#6Invalid script name: {}\r\n", script_name);
    return std::nullopt;
  }

  if (!File::Exists(path)) {
    LOG(ERROR) << "Unable to locate script: " << path;
    script_out().print("|#6Unable to locate script: {}\r\n", script_name);
    return std::nullopt;
  }
  TextFile file(path, "r");
  if (!file) {
    LOG(ERROR) << "Unable to read script: " << path;
    script_out().print("|#6Unable to read script: {}\r\n", script_name);
    return std::nullopt;
  }
  const auto lines = file.ReadFileIntoString();
  return {lines};
}

static bool LoadBasicFile(mb_interpreter_t* bas, const std::string& script_name) {
  const auto* d = get_wwiv_script_userdata(bas);
  const auto path = FilePath(d->script_dir, script_name);
  auto o = ReadBasicFile(path, script_name);
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
      if (e == SE_RN_REACHED_TO_WRONG_FUNCTION) {
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

static int _on_stepped(struct mb_interpreter_t* s, void** l, const char* f, int p, unsigned short row, unsigned short col) {
  const std::string file = (f) ? f : "(null)";
  VLOG(2) << "p: " << p << "; f: " << file << "; row: " << row << "; col: " << col;

  mb_unrefvar(s);
  mb_unrefvar(l);
  mb_unrefvar(f);
  mb_unrefvar(p);
  mb_unrefvar(row);
  mb_unrefvar(col);

  return MB_FUNC_OK;
}

static int _on_prev_stepped(struct mb_interpreter_t* s, void** l, const char* f, int p,
                            unsigned short row, unsigned short col) {
  mb_unrefvar(s);
  mb_unrefvar(l);
  mb_unrefvar(f);
  mb_unrefvar(p);
  mb_unrefvar(row);
  mb_unrefvar(col);

  return MB_FUNC_OK;
}


Basic::Basic(common::Input& i, common::Output& o, const sdk::Config& config, common::Context* ctx)
    : bin_(i), bout_(o), config_(config), ctx_(ctx) {

  // Creates the script user-data passed to the interpreter.  This data can be used
  // by the implementation of custom functions.  Values derived from the script name
  // must be set in RunScript.
  script_userdata_ = std::make_unique<BasicScriptState>(
    config_.datadir(), config_.scriptdir(), ctx, &bin_, &bout_);

  [[maybe_unused]] static auto once = RegisterMyBasicGlobals();

  bas_ = SetupBasicInterpreter();
  RegisterDefaultNamespaces();

   mb_debug_set_stepped_handler(bas_, _on_prev_stepped, _on_stepped);
}

static std::string ScriptBaseName(const std::string& script_name) {
  auto basename = ToStringLowerCase(script_name);
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

  mb_set_import_handler(bas, [](struct mb_interpreter_t* b, const char* p) -> int {
    return LoadBasicFile(b, p) ? MB_FUNC_OK : MB_FUNC_ERR;
  });

  mb_set_printer(bas, my_print);
  mb_set_inputer(bas, my_input);

  return bas;  
}

// ReSharper disable once CppMemberFunctionMayBeConst
bool Basic::RegisterDefaultNamespaces() {
  RegisterNamespaceWWIV(bas_);
  RegisterNamespaceWWIVIO(bas_);
  RegisterNamespaceData(bas_);
  if (config_.script_package_file_enabled()) {
    RegisterNamespaceWWIVFILE(bas_);
  }
  if (config_.script_package_os_enabled()) {
    RegisterNamespaceWWIVOS(bas_);
  }
  RegisterNamespaceWWIVTIME(bas_);

  return true;
}

bool Basic::RunScript(const std::string& module, const std::string& text) {

  if (!config_.scripting_enabled()) {
    bout_.puts("WWIVbasic scripting is not enabled on this system.");
    return false;
  }
  script_userdata_->module = module;
  mb_set_userdata(bas_, script_userdata_.get());

  if (mb_load_string(bas_, text.c_str(), true) != MB_FUNC_OK) {
    LOG(ERROR) << "Unable to load text: '" << text << "'";
    return false;
  }

  static std::mutex script_mutex;
  std::lock_guard<std::mutex> guard(script_mutex);
  set_script_out(&bout_);
  set_script_in(&bin_);
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
  const auto path = FilePath(config_.scriptdir(), script_name);
  if (!File::Exists(path)) {
    bout_.print("|#6Unable to locate script: {}", script_name);
    return false;
  }

  auto o = ReadBasicFile(path, script_name);
  if (!o) {
    return false;
  }
  const auto module = ScriptBaseName(script_name);
  return RunScript(module, o.value());
}


bool RunBasicScript(const std::string& script_name) {
  Basic basic(bin, bout, *a()->config(), &a()->context());
  return basic.RunScript(script_name);
}

}

