/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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
#include "bbs/basic/util.h"

#include "common/input.h"
#include "common/output.h"
#include "core/log.h"
#include "deps/my_basic/core/my_basic.h"
#include <string>
#include <utility>

namespace wwiv::bbs::basic {

using namespace wwiv::common;

static Output* script_out_{nullptr};

Output& script_out() { 
  CHECK_NOTNULL(script_out_);
  return *script_out_; 
}

BasicScriptState::BasicScriptState(std::string d, std::string s, Context* c, Input* i,
                                   Output* o)
    : datadir(std::move(d)), script_dir(std::move(s)), ctx(c), in(i), out(o), module("none") {}

void set_script_out(Output* o) { 
  script_out_ = o; 
}

static Input* script_in_{nullptr};

Input& script_in() {
  CHECK_NOTNULL(script_in_);
  return *script_in_;
}

void set_script_in(Input* o) { 
  script_in_ = o;
}

char* BasicStrDup(std::string_view s) { 
  return mb_memdup(s.data(), s.size() + 1);
}

int BasicScriptState::emplace_exec_options(chain_type_t c, file_location_t f, dropfile_type_t d) {
  wwiv_exec_options_t o;
  o.type = c;
  o.loc = f;
  o.dropfile = d;

  const auto handle = allocate_handle();
  exec_options.emplace(handle, o);
  return handle;
}

BasicScriptState* get_wwiv_script_userdata(struct mb_interpreter_t* bas) {
  void* x{};
  const auto result = mb_get_userdata(bas, &x);
  if (result != MB_FUNC_OK) {
    LOG(ERROR) << "Error getting the script userdata";
    return nullptr;
  }
  return static_cast<BasicScriptState*>(x);
}

int mb_empty_function(mb_interpreter_t* s, void** l) {
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_attempt_close_bracket(s, l));
  return MB_FUNC_OK;
}

mb_value_t wwiv_mb_make_string(const std::string_view s) {
  mb_value_t t;
  mb_make_string(t, BasicStrDup(s));
  return t;
}

mb_value_t wwiv_mb_make_int(int i) {
  mb_value_t t;
  mb_make_int(t, i);
  return t;
}

mb_value_t wwiv_mb_make_real(float f) {
  mb_value_t t;
  mb_make_real(t, f);
  return t;
}

std::optional<mb_value_t> wwiv_mb_pop_value(mb_interpreter_t* bas, void** l) {
  mb_value_t arg;
  mb_make_nil(arg);
  if (mb_pop_value(bas, l, &arg) != MB_FUNC_OK) {
    return std::nullopt;
  }
  return {arg};
}

std::optional<mb_value_t> wwiv_mb_pop_usertype(mb_interpreter_t* bas, void** l) {
  mb_value_t arg;
  mb_make_nil(arg);
  if (mb_pop_value(bas, l, &arg) != MB_FUNC_OK) {
    return std::nullopt;
  }
  if (arg.type != MB_DT_USERTYPE) {
    return std::nullopt;
  }

  return {arg};
}

std::optional<std::string> wwiv_mb_pop_string(mb_interpreter_t* bas, void** l) {
  auto o = wwiv_mb_pop_value(bas, l);
  if (!o) {
    return std::nullopt;
  }
  if (o.value().type != MB_DT_STRING) {
    return std::nullopt;
  }
  return { o.value().value.string };
}

std::optional<int> wwiv_mb_pop_int(mb_interpreter_t* bas, void** l) {
  auto o = wwiv_mb_pop_value(bas, l);
  if (!o) {
    return std::nullopt;
  }
  if (o.value().type != MB_DT_INT) {
    return std::nullopt;
  }
  return { o.value().value.integer };
}

std::optional<int> wwiv_mb_pop_handle(mb_interpreter_t* bas, void** l) {
  mb_value_t arg;
  mb_make_nil(arg);
  const auto ret = mb_pop_value(bas, l, &arg);
  if (ret != MB_FUNC_OK || arg.type != MB_DT_USERTYPE) {
    return std::nullopt;
  }
  return { arg.value.integer };
}

int wwiv_mb_push_handle(mb_interpreter_t* bas, void** l, int h) {
  mb_value_t val;
  val.type = MB_DT_USERTYPE;
  val.value.integer = h;

  return mb_push_value(bas, l, val);
}

int wwiv_mb_push_string(mb_interpreter_t* bas, void** l, std::string_view s) {
  const auto v = wwiv_mb_make_string(s);
  return mb_push_value(bas, l, v);
}

file_location_t to_file_location_t(const std::string& s) {
  if (strings::iequals(s, "MENUS")) {
    return file_location_t::MENUS;
  }
  if (strings::iequals(s, "GFILES")) {
    return file_location_t::GFILES;
  }
  if (strings::iequals(s, "TEMP")) {
    return file_location_t::TEMP;
  }
  if (strings::iequals(s, "BBS")) {
    return file_location_t::BBS;
  }
  return file_location_t::GFILES;
}

dropfile_type_t to_dropfile_type_t(const std::string& s) {
  if (strings::iequals(s, "DOOR.SYS")) {
    return dropfile_type_t::DOOR_SYS;
  }
  return dropfile_type_t::CHAIN_TXT;
}

chain_type_t to_chain_type_t(const std::string& s) {
  if (strings::iequals(s, "FOSSIL")) {
    return chain_type_t::FOSSIL;
  }
  if (strings::iequals(s, "NETFOSS")) {
    return chain_type_t::NETFOSS;
  }
  if (strings::iequals(s, "STDIO")) {
    return chain_type_t::STDIO;
  }
  return chain_type_t::DOOR32;
}

}
