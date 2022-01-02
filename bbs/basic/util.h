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
#ifndef INCLUDED_BBS_BASIC_UTIL_H
#define INCLUDED_BBS_BASIC_UTIL_H

#include "common/context.h"
#include "common/input.h"
#include "common/output.h"
#include "core/textfile.h"
#include "deps/my_basic/core/my_basic.h"

#include <memory>
#include <string>

struct mb_interpreter_t;

namespace wwiv::bbs::basic {

char* BasicStrDup(std::string_view s);
void set_script_out(common::Output* o);
void set_script_in(common::Input* o);

common::Input& script_in();
common::Output& script_out();

enum class file_location_t { GFILES, MENUS, TEMP, BBS };
enum class chain_type_t { DOOR32, STDIO, FOSSIL, NETFOSS };
enum class dropfile_type_t { CHAIN_TXT, DOOR_SYS };

struct wwiv_file_handle_t {
  file_location_t loc{file_location_t::GFILES};
  std::unique_ptr<TextFile> file;
};

struct wwiv_exec_options_t {
  chain_type_t type{chain_type_t::DOOR32};
  file_location_t loc{file_location_t::TEMP};
  dropfile_type_t dropfile{dropfile_type_t::CHAIN_TXT};
};

class BasicScriptState {
public:
  BasicScriptState(std::string d, std::string s, common::Context* c, common::Input* i,
                   common::Output* o);
  std::string datadir;
  std::string script_dir;
  common::Context* ctx;
  common::Input* in;
  common::Output* out;
  std::string module;

  int allocate_handle() noexcept { return ++handle_; }

  int emplace_exec_options(chain_type_t, file_location_t, dropfile_type_t);
  std::map<int, wwiv_file_handle_t> files;
  std::map<int, wwiv_exec_options_t> exec_options;

private:
  int handle_{0};
};

/**
 * Gets the WWIV script "userdata" (BasicScriptState) that was applied at the
 * script level for the interpreter session.
 */
BasicScriptState* get_wwiv_script_userdata(struct mb_interpreter_t* bas);

/**
 * Attempts to open and close the brackets () for an empty
 * function, returning MB_FUNC_OK on success and an error
 * code on failure
 */
int mb_empty_function(struct mb_interpreter_t* s, void** l);

/**
 * Creates a mb_value_t from string s
 *
 * equivalent to:
 * mb_value_t v;
 * mb_make_string(v, BasicStrDup(s));
 */
mb_value_t wwiv_mb_make_string(std::string_view s);
mb_value_t wwiv_mb_make_int(int i);
mb_value_t wwiv_mb_make_real(float f);

std::optional<mb_value_t> wwiv_mb_pop_value(struct mb_interpreter_t* bas, void** l);
std::optional<mb_value_t> wwiv_mb_pop_usertype(struct mb_interpreter_t* bas, void** l);
std::optional<std::string> wwiv_mb_pop_string(struct mb_interpreter_t* bas, void** l);
std::optional<int> wwiv_mb_pop_int(struct mb_interpreter_t* bas, void** l);
std::optional<int> wwiv_mb_pop_handle(struct mb_interpreter_t* bas, void** l);

/** Pushes a WWIVbasic handle, which is a usertype wrapping an integer */
int wwiv_mb_push_handle(struct mb_interpreter_t* bas, void** l, int h);
/** Pushes a string onto the interpreter stack */
int wwiv_mb_push_string(struct mb_interpreter_t* bas, void** l, std::string_view s);

file_location_t to_file_location_t(const std::string& s);
dropfile_type_t to_dropfile_type_t(const std::string& s);
chain_type_t to_chain_type_t(const std::string& s);

#define MB_FUNC_ERR_IF_ABSENT(o) do { if (!o) return MB_FUNC_ERR; } while (0)
#define MB_FUNC_ERR_IF_ABSENT_MSG(o, msg) do { if (!o) { LOG(ERROR) << msg; return MB_FUNC_ERR; } } while (0)

}

#endif