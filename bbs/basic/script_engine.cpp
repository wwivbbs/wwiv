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
#include "bbs/basic/script_engine.h"

#include "bbs/basic/basic.h"
#include "core/log.h"
#include "core/strings.h"
#include "sdk/config.h"

#include <mutex>

#ifdef WWIV_WWIVBASIC_NEXT
#include "bbs/basic/next/next_engine.h"
#endif

namespace wwiv::bbs::basic {

using namespace wwiv::strings;

std::optional<script_engine_t> to_script_engine_t(std::string_view s) {
  if (iequals(std::string(s), "legacy")) {
    return script_engine_t::legacy;
  }
  if (iequals(std::string(s), "next")) {
    return script_engine_t::next;
  }
  return std::nullopt;
}

std::string to_string(script_engine_t e) {
  switch (e) {
  case script_engine_t::legacy:
    return "legacy";
  case script_engine_t::next:
    return "next";
  }
  return "legacy";
}

bool script_engine_available(script_engine_t e) {
  switch (e) {
  case script_engine_t::legacy:
    return true;
  case script_engine_t::next:
#ifdef WWIV_WWIVBASIC_NEXT
    return true;
#else
    return false;
#endif
  }
  return false;
}

script_engine_t SelectScriptEngine(std::string_view override_engine, const sdk::Config& config) {
  if (const auto e = to_script_engine_t(override_engine)) {
    return e.value();
  }
  if (!config.script_engine_next()) {
    return script_engine_t::legacy;
  }
  if (!script_engine_available(script_engine_t::next)) {
    static std::once_flag warned;
    std::call_once(warned, [] {
      LOG(WARNING) << "The new BASIC engine is enabled in config.json, but this build doesn't "
                      "include it (WWIV_WWIVBASIC_NEXT); using the legacy engine.";
    });
    return script_engine_t::legacy;
  }
  return script_engine_t::next;
}

namespace {

// my_basic, through bbs/basic/basic.*.
class LegacyEngine final : public ScriptEngine {
public:
  LegacyEngine(common::Input& in, common::Output& out, const sdk::Config& config,
               common::Context* ctx)
      : basic_(in, out, config, ctx) {}

  bool RunScript(const std::string& script_name) override { return basic_.RunScript(script_name); }
  bool RunScript(const std::string& module, const std::string& text) override {
    return basic_.RunScript(module, text);
  }

private:
  Basic basic_;
};

} // namespace

std::unique_ptr<ScriptEngine>
CreateScriptEngine(script_engine_t e, common::Input& in, common::Output& out,
                   const sdk::Config& config, common::Context* ctx,
                   [[maybe_unused]] std::unique_ptr<MessageBase> msgs) {
  switch (e) {
  case script_engine_t::legacy:
    return std::make_unique<LegacyEngine>(in, out, config, ctx);
  case script_engine_t::next:
#ifdef WWIV_WWIVBASIC_NEXT
  {
    auto engine = std::make_unique<NextEngine>(in, out, config, ctx);
    engine->set_message_base(std::move(msgs));
    return engine;
  }
#else
    LOG(ERROR) << "The new BASIC engine isn't included in this build (WWIV_WWIVBASIC_NEXT).";
    return nullptr;
#endif
  }
  return nullptr;
}

} // namespace wwiv::bbs::basic
