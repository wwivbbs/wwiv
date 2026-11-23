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

#include "common/context.h"
#include "common/output.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "sdk/config.h"
#include "wwivbasic/interpreter.h"

#include <algorithm>
#include <optional>

namespace wwiv::bbs::basic {

using namespace wwiv::core;
using namespace wwiv::strings;

namespace {

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
// legacy engine (Basic::RunScript and ReadBasicFile in basic.cpp).
std::optional<std::string> ReadScriptFile(common::Output& out, const std::filesystem::path& dir,
                                          const std::string& script_name) {
  const auto path = FilePath(dir, script_name);
  if (!File::Exists(path)) {
    out.print("|#6Unable to locate script: {}", script_name);
    return std::nullopt;
  }
  if (script_name.find("..") != std::string::npos) {
    LOG(ERROR) << "Invalid script name: " << script_name;
    out.print("|#6Invalid script name: {}\r\n", script_name);
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
                       common::Context* ctx)
    : in_(in), out_(out), config_(config), ctx_(ctx) {}

NextEngine::~NextEngine() = default;

bool NextEngine::RunScript(const std::string& script_name) {
  const auto text = ReadScriptFile(out_, config_.scriptdir(), script_name);
  if (!text) {
    return false;
  }
  return RunText(script_name, text.value());
}

bool NextEngine::RunScript(const std::string& module, const std::string& text) {
  return RunText(module, text);
}

bool NextEngine::RunText(const std::string& name, const std::string& text) {
  if (!config_.scripting_enabled()) {
    out_.outstr("WWIVbasic scripting is not enabled on this system.");
    return false;
  }
  const auto module = ScriptBaseName(name);
  if (ctx_ && ctx_->session_context().debug_wwivbasic()) {
    // TODO(M6): the debugger (/debug/v1) for the new engine.
    LOG(WARNING) << "The WWIVbasic debugger isn't supported on the new engine yet; running '"
                 << module << "' without it.";
    out_.pl("|#6The debugger isn't supported on the new BASIC engine yet.");
  }

  BbsConsole console(in_, out_);
  wwivbasic::Interpreter interp(console);
  // TODO(M4): register the wwiv.* modules.

  // The full path, so that IMPORT "file.bas" is found next to the script.
  const auto path = FilePath(config_.scriptdir(), name);
  const auto result = interp.run_source(path.string(), text);
  if (result.ok()) {
    return true;
  }
  // Errors start with the script's full path; show it relative to scriptdir.
  // The prefix is whatever FilePath put before the name.
  const auto full = path.string();
  const auto prefix = full.substr(0, full.size() - std::min(full.size(), name.size()));
  for (const auto& e : result.errors) {
    const auto msg = starts_with(e, prefix) ? e.substr(prefix.size()) : e;
    LOG(ERROR) << msg;
    out_.print("|#6{}\r\n", msg);
  }
  return false;
}

} // namespace wwiv::bbs::basic
