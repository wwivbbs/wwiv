/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2026, WWIV Software Services                  */
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
#include "bbs/basic/next/next_engine.h"

#include "bbs/basic/next/bbs_console.h"
#include "bbs/basic/next/debug_server.h"
#include "bbs/basic/next/debug_session.h"
#include "bbs/basic/file_base.h"
#include "bbs/basic/message_base.h"
#include "bbs/basic/user_base.h"
#include "bbs/basic/next/wwiv_bindings.h"

#include "common/context.h"
#include "common/input.h"
#include "common/output.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/format.h"
#include "local_io/keycodes.h"
#include "sdk/config.h"
#include "wwivbasic/interpreter.h"

#include <algorithm>
#include <chrono>
#include <memory>
#include <utility>
#include <optional>

namespace wwiv::bbs::basic {

using namespace wwiv::core;
using namespace wwiv::strings;

namespace {

using namespace std::chrono_literals;

// The debugger listens only on this machine: anyone who can reach it can
// read the script's variables and stop it.
constexpr char kDebuggerHost[] = "127.0.0.1";
// How long a script waits for the debugger to attach, as the legacy engine.
constexpr auto kDebuggerAttachWait = 120s;

// The module name for a script file: its name in lower case without ".bas",
// as the legacy engine does.
std::string ScriptBaseName(const std::string& script_name) {
  auto basename = ToStringLowerCase(script_name);
  if (ends_with(basename, ".bas")) {
    return basename.substr(0, basename.find_last_of('.'));
  }
  return basename;
}

// Reads scriptdir/script_name, with the same checks and messages as the
// legacy engine (Basic::RunScript and ReadBasicFile in basic.cpp), except
// that ".." is rejected before the file is looked for.
std::optional<std::string> ReadScriptFile(common::Output& out, const std::filesystem::path& dir,
                                          const std::string& script_name) {
  // Reject ".." before looking for the file, so the messages don't tell the
  // caller which files exist outside scriptdir.
  if (script_name.find("..") != std::string::npos) {
    LOG(ERROR) << "Invalid script name: " << script_name;
    out.print("|#6Invalid script name: {}\r\n", script_name);
    return std::nullopt;
  }
  const auto path = FilePath(dir, script_name);
  if (!File::Exists(path)) {
    out.print("|#6Unable to locate script: {}", script_name);
    return std::nullopt;
  }
  TextFile file(path, "r");
  if (!file) {
    LOG(ERROR) << "Unable to read script: " << path;
    out.print("|#6Unable to read script: {}\r\n", script_name);
    return std::nullopt;
  }
  return file.ReadFileIntoString();
}

} // namespace

NextEngine::NextEngine(common::Input& in, common::Output& out, const sdk::Config& config,
                       common::Context* ctx, setup_fn on_setup)
    : in_(in), out_(out), config_(config), ctx_(ctx), on_setup_(std::move(on_setup)) {}

NextEngine::~NextEngine() = default;

void NextEngine::set_message_base(std::unique_ptr<MessageBase> msgs) { msgs_ = std::move(msgs); }

void NextEngine::set_user_base(std::unique_ptr<UserBase> users) { users_ = std::move(users); }

void NextEngine::set_file_base(std::unique_ptr<FileBase> files) { files_ = std::move(files); }

bool NextEngine::RunScript(const std::string& script_name) {
  const auto text = ReadScriptFile(out_, config_.scriptdir(), script_name);
  if (!text) {
    return false;
  }
  return RunText(script_name, ScriptBaseName(script_name), text.value());
}

bool NextEngine::RunScript(const std::string& module, const std::string& text) {
  // The legacy engine keeps this name as given (in upper case; wwiv.data
  // lowers it again).
  return RunText(module, ToStringLowerCase(module), text);
}

bool NextEngine::RunText(const std::string& name, const std::string& module,
                         const std::string& text) {
  if (!config_.scripting_enabled()) {
    out_.outstr("WWIVbasic scripting is not enabled on this system.");
    return false;
  }
  // Stop the script (and end input) once the caller hangs up.
  auto hung_up = [ctx = ctx_]() { return ctx && ctx->session_context().hangup(); };

  // The full path, so that IMPORT "file.bas" is found next to the script.
  const auto path = FilePath(config_.scriptdir(), name);
  // Paths in errors and the debugger are shown relative to scriptdir: the
  // prefix is whatever FilePath put before the name.
  const auto full = path.string();
  const auto prefix = full.substr(0, full.size() - std::min(full.size(), name.size()));
  auto relative = [&prefix](const std::string& file) {
    return !prefix.empty() && starts_with(file, prefix) ? file.substr(prefix.size()) : file;
  };

  // With bbs --debug, the script waits for a debugger (wwivdbg) to attach.
  // The server goes before the session it uses.
  std::unique_ptr<DebugSession> debug;
  std::unique_ptr<DebugServer> debug_server;
  // While paused, Ctrl-C on the local or remote side ends the script.
  auto debug_cancel = [&]() {
    if (hung_up()) {
      return true;
    }
    return in_.bkbhit() && in_.bgetchraw() == wwiv::local::io::CC;
  };
  if (ctx_ && ctx_->session_context().debug_wwivbasic()) {
    const auto port = ctx_->session_context().debug_wwivbasic_port();
    debug = std::make_unique<DebugSession>(name);
    debug->AddSource(name, text);
    debug_server = std::make_unique<DebugServer>(*debug);
    if (debug_server->Start(kDebuggerHost, port)) {
      LOG(INFO) << "Waiting for the script debugger to attach on " << kDebuggerHost << ":" << port;
      out_.print("|#9Waiting for the debugger to attach on port {} (Ctrl-C to skip)...\r\n",
                 port);
      if (!debug->WaitForAttach(kDebuggerAttachWait, debug_cancel)) {
        out_.pl("|#6No debugger attached; running the script without it.");
      }
    } else {
      out_.print("|#6Unable to start the debugger on port {}; running without it.\r\n", port);
      debug_server.reset();
      debug.reset();
    }
  }

  BbsConsole console(in_, out_, hung_up);
  wwivbasic::Interpreter interp(console);
  interp.set_stop_check(hung_up);
  // As with the legacy engine, scripts can only import files in scriptdir.
  interp.set_import_root(config_.scriptdir());
  const WwivBindings bindings{in_, out_, console, config_, ctx_, module,
                               msgs_.get(), users_.get(), files_.get()};
  RegisterWwiv(interp, bindings);
  RegisterWwivIo(interp, bindings);
  RegisterWwivScreen(interp, bindings);
  RegisterWwivData(interp, bindings);
  // As in the legacy engine, these exist only when the sysop enables them.
  if (config_.script_package_file_enabled()) {
    RegisterWwivIoFile(interp, bindings);
  }
  if (config_.script_package_os_enabled()) {
    RegisterWwivOs(interp, bindings);
  }
  RegisterWwivTime(interp, bindings);
  // Read-only, and limited to what the user can read, so it's on unless the
  // sysop turns it off.
  if (config_.script_package_msg_enabled()) {
    RegisterWwivMsg(interp, bindings);
  }
  // Like wwiv.msg, it shows only what the user could see in the BBS.
  if (config_.script_package_user_enabled()) {
    RegisterWwivUser(interp, bindings);
  }
  // Read-only, and limited to the areas the user can list.
  if (config_.script_package_files_enabled()) {
    RegisterWwivFiles(interp, bindings);
  }
  if (on_setup_) {
    on_setup_(interp);
  }
  if (debug) {
    interp.set_statement_hook([&](const wwivbasic::DebugLocation& loc) {
      const auto module = relative(loc.file);
      if (!debug->has_source(module)) {
        if (const auto src = interp.source(loc.file)) {
          debug->AddSource(module, *src);
        }
      }
      auto snapshot = [&]() {
        DebugSession::Snapshot snap;
        for (const auto& f : interp.call_stack()) {
          const auto where = fmt::format("{}:{}", relative(f.file), f.line);
          snap.stack.push_back(f.function.empty() ? where
                                                  : fmt::format("{} ({})", f.function, where));
        }
        for (const auto& v : interp.variables()) {
          snap.vars.push_back({v.name, v.type, v.value, v.frame});
        }
        return snap;
      };
      return debug->OnStatement(module, loc.line, loc.column, loc.depth, snapshot, debug_cancel);
    });
  }

  const auto result = interp.run_source(path.string(), text);
  if (debug) {
    debug->Finished();
  }
  if (result.stopped) {
    LOG(INFO) << "Stopped script '" << module << "': "
              << (hung_up() ? "the caller hung up." : "the debugger stopped it.");
    return false;
  }
  if (result.ok()) {
    return true;
  }
  for (const auto& e : result.errors) {
    auto msg = e;
    if (!prefix.empty()) {
      StringReplace(&msg, prefix, "");
    }
    LOG(ERROR) << msg;
    // An error can span several lines (one from an IMPORTed file does).
    for (const auto& line : SplitString(msg, "\n")) {
      ShowError(line);
    }
  }
  return false;
}

void NextEngine::ShowError(const std::string& line) {
  // Shown as text: pipe or heart codes in the message (from the script's
  // source, say) must not change colors.
  out_.outstr("|#6");
  for (const auto ch : line) {
    out_.outchr(ch);
  }
  out_.nl();
}

} // namespace wwiv::bbs::basic
