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

#include <cmath>
#include <limits>
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

// The on-disk format of wwiv.data files, the same as the legacy (my_basic)
// engine wrote, so scripts keep their data: {"data": [{"type": 0, "s": "..."}, {"type": 1, "i": 1}, ...]}.
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
  case Value::Type::REAL: {
    // Data files hold 32-bit floats, like my_basic's REALs. Larger values
    // would be saved as infinity, and NaN or infinity can't be read back.
    const auto r = v.toDouble();
    if (!std::isfinite(r) || std::fabs(r) > std::numeric_limits<float>::max()) {
      throw BasicError(fmt::format("SAVE: can't save {}: a REAL in a data file must be a finite "
                                   "number no larger than {}",
                                   v.toString(), std::numeric_limits<float>::max()));
    }
    d.type = next_script_data_type_t::REAL;
    d.r = static_cast<float>(r);
    return d;
  }
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

// The scope argument: "USER" (any case) is the user's data, any other string
// is global. Like the legacy engine, the scope must be a STRING.
data_scope_t to_scope(const char* fn, const Args& args) {
  if (args.empty()) {
    return data_scope_t::global;
  }
  if (args[0].value_type() != Value::Type::STRING) {
    throw BasicError(
        fmt::format("{}: the scope must be a STRING, got {}", fn, args[0].a_type_name()));
  }
  return iequals(args[0].toString(), "USER") ? data_scope_t::user : data_scope_t::global;
}

std::filesystem::path DataFileName(const char* fn, const WwivBindings& b, data_scope_t scope) {
  // The module name comes from the script's name. Script files can't have
  // ".." in their names, but RunScript(module, text) takes any name, so keep
  // data files inside datadir. (Names with a subdirectory are kept, as in the
  // legacy engine.)
  const std::filesystem::path module(b.module);
  if (b.module.empty() || module.is_absolute() || module.has_root_path() ||
      b.module.find("..") != std::string::npos) {
    throw BasicError(fmt::format("{}: can't use data files for the module '{}'", fn, b.module));
  }
  if (scope == data_scope_t::global) {
    return FilePath(b.config.datadir(), StrCat(b.module, ".script.json"));
  }
  if (!b.ctx) {
    throw BasicError(fmt::format("{}: there is no current user for USER data", fn));
  }
  const auto user_num = b.ctx->session_context().user_num();
  return FilePath(b.config.datadir(), fmt::format("{}.user.{}.script.json", b.module, user_num));
}

} // namespace

// The wwiv.data module: SAVE and LOAD a list of
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
    const auto scope = to_scope("SAVE", args);
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
    JsonFile json(DataFileName("SAVE", b, scope), "data", data);
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
    const auto scope = to_scope("LOAD", args);
    if (args.size() < 2) {
      return Value();
    }
    if (!args[1].is_list()) {
      throw BasicError("LOAD: Only loading a LIST is currently supported. (not DICT)");
    }
    std::vector<next_script_data_t> data;
    JsonFile json(DataFileName("LOAD", b, scope), "data", data);
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
