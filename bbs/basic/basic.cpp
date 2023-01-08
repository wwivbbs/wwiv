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
#include "httplib.h"

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

#include <chrono>
#include <cstdarg>
#include <mutex>
#include <optional>
#include <string>

using namespace wwiv::core;
using namespace wwiv::strings;
using namespace std::chrono_literals;

namespace wwiv::bbs::basic {

static int my_print(struct mb_interpreter_t* bas, const char* fmt, ...) {
  char buf[1024];

  va_list argptr;
  va_start(argptr, fmt);
  vsnprintf(buf, sizeof(buf), fmt, argptr);  // NOLINT(clang-diagnostic-format-nonliteral)
  va_end(argptr);

  const auto* d = get_wwiv_script_userdata(bas);
  d->out->outstr(buf);
  d->out->nl();
  return MB_FUNC_OK;
}

static int my_input(struct mb_interpreter_t* bas, const char* prompt, char* buf, int size) {
  const auto* d = get_wwiv_script_userdata(bas);
  if (prompt && *prompt) {
    d->out->outstr(prompt);
  }
  const auto v = d->in->input_text("", size);
  strcpy(buf, v.c_str());
  return stl::size_int(v);
}

static bool RegisterMyBasicGlobals() {
  mb_init();
  return true;
}

static std::optional<std::string> ReadBasicFile(wwiv::common::Output& out,
                                                const std::filesystem::path& path,
                                                const std::string& script_name) {
    if (script_name.find("..") != std::string::npos) {
    LOG(ERROR) << "Invalid script name: " << script_name;
    out.print("|#6Invalid script name: {}\r\n", script_name);
    return std::nullopt;
  }

  if (!File::Exists(path)) {
    LOG(ERROR) << "Unable to locate script: " << path;
    out.print("|#6Unable to locate script: {}\r\n", script_name);
    return std::nullopt;
  }
  TextFile file(path, "r");
  if (!file) {
    LOG(ERROR) << "Unable to read script: " << path;
    out.print("|#6Unable to read script: {}\r\n", script_name);
    return std::nullopt;
  }
  const auto lines = file.ReadFileIntoString();
  return {lines};
}

bool Basic::LoadBasicFile(mb_interpreter_t* bas, const std::string& script_name) {
  const auto* d = get_wwiv_script_userdata(bas);
  const auto path = FilePath(d->script_dir, script_name);
  const auto o = ReadBasicFile(*d->out, path, script_name);
  if (!o) {
    return false;
  }
  d->basic->current_debug_state().AddSourceModule(script_name, o.value());
  const auto ret = mb_load_string(bas, o.value().c_str(), true);
  return ret == MB_FUNC_OK;
} 

static void _on_error(struct mb_interpreter_t* s, mb_error_e err, const char* msg, const char* func,
                      int p, uint16_t row, uint16_t col, int abort_code) {
  mb_unrefvar(s);
  mb_unrefvar(p);

  if (err == SE_NO_ERR) {
    // No erorr here.
    return;
  }

  if (!func) {
    printf("Error:\n    Line %d, Col %d\n    Code %d, Abort Code %d\n    Message: %s.\n", row, col,
           err, err == SE_EA_EXTENDED_ABORT ? abort_code - MB_EXTENDED_ABORT : abort_code, msg);
    return;
  }

  if (err == SE_RN_REACHED_TO_WRONG_FUNCTION) {
    printf(
        "Error:\n    Line %d, Col %d in Func: %s\n    Code %d, Abort Code %d\n    Message: %s.\n",
        row, col, func, err, abort_code, msg);
    return;
  }
  printf(
      "Error:\n    Line %d, Col %d in File: %s\n    Code %d, Abort Code %d\n    Message: %s.\n",
      row, col, func, err, err == SE_EA_EXTENDED_ABORT ? abort_code - MB_EXTENDED_ABORT : abort_code,
      msg);
}

static int _on_post_stepped(struct mb_interpreter_t* bas, void** ast, const char* src, int pos,
                            uint16_t row, uint16_t col) {

  mb_unrefvar(ast);
  const auto* data = get_wwiv_script_userdata(bas);
  auto& debug_state = data->basic->current_debug_state();
  if (!data->basic->debugger_attached()) {
    return MB_FUNC_OK;
  }
  if (src) {
    debug_state.SetCurrentModuleName(src);
  }
  LOG(INFO) << "pos: " << pos << "; source_file: " << ((src) ? src : "(null)") << "; row: " << row
            << "; col: " << col;

  if (debug_state.running_state() == RunningState::RUNNING) {
    // Check for breakpoints and stop or return MB_FUNC_OK here to keep running

  }

  auto num_stack_frames = mb_debug_count_stack_frames(bas);
  char* frames[16];
  memset(frames, 0, sizeof(frames));
  const auto fc = std::min<int>(num_stack_frames, 16);
  mb_debug_get_stack_trace(bas, frames, fc);

  debug_state.SetCallStackFrames(frames, fc);
  debug_state.SetLocation(pos, row, col);

  return MB_FUNC_OK;
}

static int _on_prev_stepped(struct mb_interpreter_t* bas, void** ast, const char* source_file,
                            int pos, uint16_t row, uint16_t col) {
  // here is where the logic of (a) was a breakpoint hit lives.
  mb_unrefvar(bas);
  mb_unrefvar(ast);
  mb_unrefvar(source_file);
  mb_unrefvar(pos);
  mb_unrefvar(row);
  mb_unrefvar(col);

  return MB_FUNC_OK;
}


std::string Receive(const httplib::Request& req, httplib::Response& res,
                    const httplib::ContentReader& content_reader) {
  if (req.is_multipart_form_data()) {
    return {};
  }
  std::string body;
  content_reader([&](const char* data, size_t data_length) {
    body.append(data, data_length);
    return true;
  });
  return body;
}

Basic::Basic(common::Input& i, common::Output& o, const sdk::Config& config, common::Context* ctx)
    : bin_(i), bout_(o), config_(config), ctx_(ctx) {

  // Creates the script user-data passed to the interpreter.  This data can be used
  // by the implementation of custom functions.  Values derived from the script name
  // must be set in RunScript.
  script_userdata_ = std::make_unique<BasicScriptState>(
    config_.datadir(), config_.scriptdir(), ctx, &bin_, &bout_, this);

  [[maybe_unused]] static auto once = RegisterMyBasicGlobals();

  bas_ = SetupBasicInterpreter();
  RegisterDefaultNamespaces();
}

// Having this here in a c++ file means we can delete basic without knowing 
// the complete types of httplib in basic.h
Basic::~Basic() {
  // Stop the server then join the thread to wait for it to exit.
  if (svr_) {
    svr_->stop();
    srv_thread_.join();
  }
}

static std::string ScriptBaseName(const std::string& script_name) {
  auto basename = ToStringLowerCase(script_name);
  if (ends_with(basename, ".bas")) {
    return basename.substr(0, basename.find_last_of('.'));
  }
  return basename;
}

mb_interpreter_t* Basic::SetupBasicInterpreter() {
  struct mb_interpreter_t* bas = nullptr;
  mb_open(&bas);

  mb_set_error_handler(bas, _on_error);
  mb_debug_set_stepped_handler(bas, _on_prev_stepped, _on_post_stepped);

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
    bout_.outstr("WWIVbasic scripting is not enabled on this system.");
    return false;
  }
  script_userdata_->module = module;
  auto& debug = current_debug_state();
  debug.SetInitialModule(module, text);
  debug.AddSourceModule(module, text);
  mb_set_userdata(bas_, script_userdata_.get());

  if (mb_load_string(bas_, text.c_str(), true) != MB_FUNC_OK) {
    LOG(ERROR) << "Unable to load text: '" << text << "'";
    return false;
  }

  static std::mutex script_mutex;
  std::lock_guard<std::mutex> guard(script_mutex);
  const auto ret = mb_run(bas_, false);
  mb_close(&bas_);

  // We don't call mb_dispose since we only call mb_init once per execution.
  if (ret != MB_FUNC_OK) {
    LOG(INFO) << "Failure exiting script: '" << module << "' error code (MB_FUNC_XXXX) : " << ret;
    return false;
  }
  return true;
}

bool Basic::WaitForDebuggerToAttach() {
  bout_.pl("Waiting for debugger to attach...");
  int count = 0;
  while (true) {
    if (debugger_attached()) {
      return true;
    }
    wwiv::os::sleep_for(1s);
    if (++count % 5 == 0) {
      bout_.pl("Waiting for debugger to attach...");
    }
    if (count > 120) {
      return false;
    }
  }
}

std::string dump_headers(const httplib::Headers& headers) {
  std::string s;
  char buf[BUFSIZ];

  for (auto it = headers.begin(); it != headers.end(); ++it) {
    const auto& x = *it;
    snprintf(buf, sizeof(buf), "%s: %s\n", x.first.c_str(), x.second.c_str());
    s += buf;
  }

  return s;
}

static std::string log(const httplib::Request& req, const httplib::Response& res) {
  std::string s;
  char buf[BUFSIZ];

  s += "================================\n";

  snprintf(buf, sizeof(buf), "%s %s %s", req.method.c_str(), req.version.c_str(), req.path.c_str());
  s += buf;

  std::string query;
  for (auto it = req.params.begin(); it != req.params.end(); ++it) {
    const auto& x = *it;
    snprintf(buf, sizeof(buf), "%c%s=%s", (it == req.params.begin()) ? '?' : '&', x.first.c_str(),
             x.second.c_str());
    query += buf;
  }
  snprintf(buf, sizeof(buf), "%s\n", query.c_str());
  s += buf;

  s += dump_headers(req.headers);

  s += "--------------------------------\n";

  snprintf(buf, sizeof(buf), "%d %s\n", res.status, res.version.c_str());
  s += buf;
  s += dump_headers(res.headers);
  s += "\n";

  if (!res.body.empty()) {
    s += res.body;
  }

  s += "\n";

  return s;
}
bool Basic::StartDebugger(int port) { 
  svr_ = std::make_unique<httplib::Server>();

  svr_->Get("/debug/status", [](const httplib::Request&, httplib::Response& res) {
    res.set_content("ok", "text/plain");
  });
  svr_->Post("/debug/v1/attach", [&](const httplib::Request& req, httplib::Response& res,
                           const httplib::ContentReader& content_reader) {
    if (debugger_attached()) {
      res.status = 400;
      res.set_content("Already attached", "text/plain");
      return false;
    }
    auto msg = Receive(req, res, content_reader);
    LOG(INFO) << "Debugger Message: " << msg;
    res.set_content("attached", "text/plain");
    return AttachDebugger(msg);
  });
  svr_->Post("/debug/v1/detach", [&](const httplib::Request& req, httplib::Response& res,
                                  const httplib::ContentReader& content_reader) {
    if (!debugger_attached()) {
      res.status = 400;
      res.set_content("Not Attached", "text/plain");
      return false;
    }
    auto msg = Receive(req, res, content_reader);
    LOG(INFO) << "Debugger Message: " << msg;
    res.set_content("detached", "text/plain");
    return DetachDebugger(msg);
  });

  svr_->Get("/debug/v1/source", [this](const httplib::Request&, httplib::Response& res) {
    auto& d = current_debug_state();
    const auto source = d.module_source(d.current_module_name()).value_or("");
    res.set_content(source, "text/plain");
  });
  svr_->Get("/debug/v1/stack", [this](const httplib::Request&, httplib::Response& res) {
    auto& d = current_debug_state();
    const auto frames = d.stack_frames();
    std::string result;
    result.reserve(frames.size() * 80);
    for (const auto& f : frames) {
      result.append(f);
    }
    res.set_content(result, "text/plain");
  });
  svr_->Get("/debug/v1/watch", [this](const httplib::Request&, httplib::Response& res) {
    std::string result = "name: \"foo\" value=\"value\"";
    res.set_content(result, "text/plain");
  });
  svr_->Get("/debug/v1/breakpoints", [this](const httplib::Request&, httplib::Response& res) {
    res.set_content("none", "text/plain");
  });

  svr_->set_logger([](const httplib::Request& req, const httplib::Response& res) {
    LOG(INFO) << log(req, res);
  });
  srv_thread_ = std::thread(
      [&](int p) {
        svr_->listen("0.0.0.0", p);
        DetachDebugger("server stopped");
      },
      port);
  return true;
}

bool Basic::AttachDebugger(const std::string& /*body*/) {
  std::lock_guard lock(mu_);
  debugger_attached_ = true;
  return true;
}

bool Basic::DetachDebugger(const std::string& /*body*/) {
  std::lock_guard lock(mu_);
  debugger_attached_ = false;
  return true;
}

bool Basic::debugger_attached() const {
  std::lock_guard lock(mu_);
  return debugger_attached_;
}


DebugState& Basic::current_debug_state() {
  std::lock_guard lock(mu_);
  if (debug_state_) {
    return *debug_state_;
  }
  debug_state_ = std::make_unique<DebugState>();
  return *debug_state_;
}

// maybe this should be deleted.
DebugState& Basic::new_debug_state() { 
  std::lock_guard lock(mu_);
  debug_state_ = std::make_unique<DebugState>(); 
  return *debug_state_;
}


bool Basic::RunScript(const std::string& script_name) {
  const auto path = FilePath(config_.scriptdir(), script_name);
  if (!File::Exists(path)) {
    bout_.print("|#6Unable to locate script: {}", script_name);
    return false;
  }

  const auto o = ReadBasicFile(bout_, path, script_name);
  if (!o) {
    return false;
  }
  const auto module = ScriptBaseName(script_name);
  return RunScript(module, o.value());
}


bool RunBasicScript(const std::string& script_name) {
  Basic basic(bin, bout, *a()->config(), &a()->context());
  // Check for debugger enablement
  if (a()->sess().debug_wwivbasic()) {
    basic.StartDebugger(a()->sess().debug_wwivbasic_port());
    basic.WaitForDebuggerToAttach();
  }
  return basic.RunScript(script_name);
}

}

