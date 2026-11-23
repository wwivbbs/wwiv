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
#ifndef INCLUDED_BBS_BASIC_SCRIPT_ENGINE_H
#define INCLUDED_BBS_BASIC_SCRIPT_ENGINE_H

#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace wwiv::common {
class Context;
class Input;
class Output;
} // namespace wwiv::common

namespace wwiv::sdk {
class Config;
}

namespace wwiv::bbs::basic {

// The BASIC interpreters WWIV can run scripts on. "legacy" is my_basic
// (bbs/basic/basic.*), "next" is wwivbasic (bbs/basic/next/), which is only
// built with WWIV_WWIVBASIC_NEXT.
enum class script_engine_t { legacy, next };

// Parses "legacy" or "next" (case-insensitive).
std::optional<script_engine_t> to_script_engine_t(std::string_view s);
std::string to_string(script_engine_t e);

// Whether this build includes the engine.
bool script_engine_available(script_engine_t e);

// The engine to run scripts on: the bbs --basic_engine override if one is set
// (it must already have been validated), otherwise the config setting. If the
// config asks for an engine this build doesn't have, logs a warning and
// returns legacy.
script_engine_t SelectScriptEngine(std::string_view override_engine, const sdk::Config& config);

// Runs WWIVbasic scripts. Each engine checks config.scripting_enabled() and
// registers the same wwiv.* modules.
class ScriptEngine {
public:
  virtual ~ScriptEngine() = default;

  // Runs the script scriptdir/script_name.
  virtual bool RunScript(const std::string& script_name) = 0;
  // Runs script text as the module named module.
  virtual bool RunScript(const std::string& module, const std::string& text) = 0;
};

// Creates an engine. Returns nullptr if the engine isn't in this build.
std::unique_ptr<ScriptEngine> CreateScriptEngine(script_engine_t e, common::Input& in,
                                                 common::Output& out, const sdk::Config& config,
                                                 common::Context* ctx);

} // namespace wwiv::bbs::basic

#endif
