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
#include "bbs/basic/next/wwiv_bindings.h"

#include "common/context.h"
#include "common/output.h"
#include "core/cereal_utils.h"
#include "core/file.h"
#include "core/jsonfile.h"
#include "core/log.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "sdk/config.h"
#include "wwivbasic/interpreter.h"

#include <optional>
#include <string>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv::bbs::basic {

using wwivbasic::BasicError;
using wwivbasic::Value;
using Args = std::vector<Value>;

namespace {

// The on-disk format of wwiv.data files. This must stay the same as in the
// legacy engine (bbs/basic/wwiv_data.cpp), so both engines read each other's
// files: {"data": [{"type": 0, "s": "..."}, {"type": 1, "i": 1}, ...]}.
enum class next_script_data_type_t { STRING, INT, REAL };

struct next_script_data_t {
  next_script_data_type_t type;
  std::string s;
  int i{};
  // my_basic used 32-bit floats; keep the same type so the files match.
  float r{};
};

template <class Archive> void serialize(Archive& ar, next_script_data_t& t) {
  SERIALIZE(t, type);
  if (t.type == next_script_data_type_t::INT) {
    SERIALIZE(t, i);
  } else if (t.type == next_script_data_type_t::REAL) {
    SERIALIZE(t, r);
  } else if (t.type == next_script_data_type_t::STRING) {
    SERIALIZE(t, s);
  }
}

Value to_value(const next_script_data_t& d) {
  switch (d.type) {
  case next_script_data_type_t::INT:
    return Value(d.i);
  case next_script_data_type_t::REAL:
    return Value(static_cast<double>(d.r));
  case next_script_data_type_t::STRING:
    return Value(d.s);
  }
  return Value(d.s);
}

next_script_data_t to_script_data(const Value& v) {
  next_script_data_t d{};
  switch (v.value_type()) {
  case Value::Type::INTEGER:
    d.type = next_script_data_type_t::INT;
    d.i = v.toInt();
    return d;
  // my_basic had no booleans; TRUE and FALSE were the integers 1 and 0.
  case Value::Type::BOOLEAN:
    d.type = next_script_data_type_t::INT;
    d.i = v.toBool() ? 1 : 0;
    return d;
  case Value::Type::REAL:
    d.type = next_script_data_type_t::REAL;
    d.r = static_cast<float>(v.toDouble());
    return d;
  case Value::Type::STRING:
    d.type = next_script_data_type_t::STRING;
    d.s = v.toString();
    return d;
  case Value::Type::LIST:
  case Value::Type::ITERATOR:
  case Value::Type::OBJECT:
    // As in the legacy engine, saved as an empty string.
    LOG(ERROR) << "wwiv.data: can't save a " << v.a_type_name() << "; saving \"\" instead";
    return d;
  }
  return d;
}

enum class data_scope_t { global, user };

// "USER" (any case) is the user's data; anything else is global.
data_scope_t to_scope(const std::string& s) {
  return iequals(s, "USER") ? data_scope_t::user : data_scope_t::global;
}

std::filesystem::path DataFileName(const WwivBindings& b, data_scope_t scope) {
  if (scope == data_scope_t::global) {
    return FilePath(b.config.datadir(), StrCat(b.module, ".script.json"));
  }
  const auto user_num = b.ctx ? b.ctx->session_context().user_num() : 0;
  return FilePath(b.config.datadir(), fmt::format("{}.user.{}.script.json", b.module, user_num));
}

} // namespace

// The wwiv.data module (bbs/basic/wwiv_data.cpp): SAVE and LOAD a list of
// integers, reals and strings, for all users (scope "GLOBAL") or for the
// current user (scope "USER").
void RegisterWwivData(wwivbasic::Interpreter& interp, const WwivBindings& b) {
  auto& m = interp.add_module("wwiv.data");

  m.native_function("MODULE_NAME", []() { return std::string("wwiv.data"); });

  // SAVE([scope [, list]]) replaces the scope's saved data with the list.
  m.native_functionl("SAVE", [b](const Args& args) {
    if (args.size() > 2) {
      throw BasicError(fmt::format("SAVE: expected 0 to 2 argument(s), got {}", args.size()));
    }
    const auto scope =
        args.empty() ? data_scope_t::global : to_scope(wwivbasic::from_value<std::string>(args[0]));
    if (args.size() < 2) {
      return Value();
    }
    if (!args[1].is_list()) {
      // The legacy engine stops the script here too.
      throw BasicError("SAVE: Only saving a LIST is currently supported. (not DICT)");
    }
    std::vector<next_script_data_t> data;
    for (const auto& v : args[1].list("SAVE")->items) {
      data.emplace_back(to_script_data(v));
    }
    JsonFile json(DataFileName(b, scope), "data", data);
    if (!json.Save()) {
      b.out.outstr("|#6Error saving data.\r\n");
    }
    return Value();
  });

  // LOAD([scope [, list]]) appends the scope's saved data to the list.
  m.native_functionl("LOAD", [b](const Args& args) {
    if (args.size() > 2) {
      throw BasicError(fmt::format("LOAD: expected 0 to 2 argument(s), got {}", args.size()));
    }
    const auto scope =
        args.empty() ? data_scope_t::global : to_scope(wwivbasic::from_value<std::string>(args[0]));
    if (args.size() < 2) {
      return Value();
    }
    if (!args[1].is_list()) {
      throw BasicError("LOAD: Only loading a LIST is currently supported. (not DICT)");
    }
    std::vector<next_script_data_t> data;
    JsonFile json(DataFileName(b, scope), "data", data);
    if (!json.Load()) {
      return Value();
    }
    auto& items = args[1].list("LOAD")->items;
    for (const auto& d : data) {
      items.push_back(to_value(d));
    }
    return Value();
  });
}

} // namespace wwiv::bbs::basic
