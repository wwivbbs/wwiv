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
#include  "bbs/basic/wwiv_data.h"

#include "deps/my_basic/core/my_basic.h"
#include "bbs/basic/util.h"
#include "core/file.h"
#include "core/jsonfile.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/cereal_utils.h"
#include <cassert>
#include <optional>
#include <string>

using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv::bbs::basic {

enum class script_data_type_t { STRING, INT, REAL };

struct script_data_t {
  script_data_type_t type;
  std::string s;
  int i{};
  float r{};
};

template <class Archive> void serialize(Archive& ar, script_data_t& t) {
  SERIALIZE(t, type);
  if (t.type == script_data_type_t::INT) {
    SERIALIZE(t, i);
  } else if (t.type == script_data_type_t::REAL) {
    SERIALIZE(t, r);
  } else if (t.type == script_data_type_t::STRING) {
    SERIALIZE(t, s);
  }
}

static mb_value_t to_mb_value(const script_data_t& f) {
  switch (f.type) {
  case script_data_type_t::INT:
    return wwiv_mb_make_int(f.i);
  case script_data_type_t::REAL:
    return wwiv_mb_make_real(f.r);
  case script_data_type_t::STRING:
    return wwiv_mb_make_string(f.s);
  }
  DLOG(FATAL) << "Should not happen";
  return {};
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
    LOG(ERROR) << "Unable to convert type (unknown) for basic type: " << v.type;
    return {};
  }
  LOG(ERROR) << "Unable to convert type (unknown) for basic type: " << v.type;
  return {};
}

enum class wwiv_data_scope_t { global, user };

static std::filesystem::path DataFileName(const BasicScriptState* ud, wwiv_data_scope_t scope) {
  const auto module_name = ToStringLowerCase(ud->module);
  if (scope == wwiv_data_scope_t::global) {
    return FilePath(ud->datadir, StrCat(module_name, ".script.json"));
  }
  const auto fn =
      fmt::format("{}.user.{}.script.json", module_name, ud->ctx->session_context().user_num());
  return FilePath(ud->datadir, fn);
}


static bool SaveData(const BasicScriptState* ud,
                     wwiv_data_scope_t scope, const std::vector<script_data_t>& data) {
  const auto path = DataFileName(ud, scope);
  JsonFile json(path, "data", data);
  return json.Save();
}

static std::optional<std::vector<script_data_t>> LoadData(const BasicScriptState* ud,
                                                          wwiv_data_scope_t scope) {
  std::vector<script_data_t> data;
  const auto path = DataFileName(ud, scope);
  JsonFile json(path, "data", data);
  if (!json.Load()) {
    return std::nullopt;
  }
  return data;
}

bool RegisterNamespaceData(mb_interpreter_t* basi) {
  mb_begin_module(basi, "WWIV.DATA");

  mb_register_func(basi, "MODULE_NAME", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_empty_function(bas, l));
    return mb_push_string(bas, l, BasicStrDup("wwiv.data"));
  });

  mb_register_func(basi, "SAVE", [](struct mb_interpreter_t* bas, void** l) -> int {
    const auto* d = get_wwiv_script_userdata(bas);
    mb_assert(bas && l);
    mb_check(mb_attempt_open_bracket(bas, l));
    char* scope_str = nullptr;
    auto scope{wwiv_data_scope_t::global};
    if (mb_has_arg(bas, l)) {
      // Scope: GLOBAL OR USER
      mb_check(mb_pop_string(bas, l, &scope_str));
      scope = iequals("USER", scope_str) ? wwiv_data_scope_t::user : wwiv_data_scope_t::global;
    }
    if (mb_has_arg(bas, l)) {
      mb_value_t arg;
      mb_make_nil(arg);
      mb_check(mb_pop_value(bas, l, &arg));
      // arg should be coll.
      if (arg.type != MB_DT_LIST) {
        d->out->outstr("|#6Error: Only saving a LIST is currently supported. (not DICT)\r\n");
        return MB_FUNC_WARNING;
      }
      int count = 0;
      mb_check(mb_count_coll(bas, l, arg, &count));
      // *d->out << " with size: " << count;
      std::vector<script_data_t> data;
      for (auto i = 0; i < count; i++) {
        mb_value_t idx;
        mb_make_int(idx, i);
        mb_value_t val{};
        if (mb_get_coll(bas, l, arg, idx, &val) == MB_FUNC_OK) {
          data.emplace_back(to_script_data(val));
        }
      }

      if (!SaveData(d, scope, data)) {
        d->out->outstr("|#6Error saving data.\r\n");
      }
    }
    mb_check(mb_attempt_close_bracket(bas, l));
    return MB_FUNC_OK;
  });

  // l = LOAD("GLOBAL|USER")
  mb_register_func(basi, "LOAD", [](struct mb_interpreter_t* bas, void** l) -> int {
    const auto* sd = get_wwiv_script_userdata(bas);
    mb_assert(bas && l);
    mb_check(mb_attempt_open_bracket(bas, l));
    char* scope_str = nullptr;
    auto scope{wwiv_data_scope_t::global};
    if (mb_has_arg(bas, l)) {
      // Scope: GLOBAL OR USER
      mb_check(mb_pop_string(bas, l, &scope_str));
      scope = iequals("USER", scope_str) ? wwiv_data_scope_t::user : wwiv_data_scope_t::global;
    }
    if (mb_has_arg(bas, l)) {
      mb_value_t arg;
      mb_make_nil(arg);
      mb_check(mb_pop_value(bas, l, &arg));
      // arg should be coll.
      if (arg.type != MB_DT_LIST) {
        sd->out->outstr("|#6Error: Only loading a LIST is currently supported. (not DICT)\r\n");
        return MB_FUNC_WARNING;
      }

      auto current_count = 0;
      mb_check(mb_count_coll(bas, l, arg, &current_count));
      if (auto data = LoadData(sd, scope)) {
        for (const auto& d : data.value()) {
          const auto val = to_mb_value(d);
          const auto idx = wwiv_mb_make_int(current_count++);
          const auto ret = mb_set_coll(bas, l, arg, idx, val);
          if (ret != MB_FUNC_OK) {
            sd->out->outstr("[oops] ");
          }
        }
      }
    }
    mb_check(mb_attempt_close_bracket(bas, l));
    return MB_FUNC_OK;
  });

  return mb_end_module(basi) == MB_FUNC_OK;
}

}