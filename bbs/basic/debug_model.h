/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2023, WWIV Software Services                  */
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
#ifndef INCLUDED_BBS_BASIC_DEBUG_MODEL_H
#define INCLUDED_BBS_BASIC_DEBUG_MODEL_H

#include <nlohmann/json_fwd.hpp>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace wwiv::bbs::basic {

struct Variable {
  std::string name;
  std::string type;
  std::string value;
  int frame{ 0 };
};
void to_json(nlohmann::json& j, const Variable& p);
void from_json(const nlohmann::json& j, Variable& p);

struct Breakpoint {
  // Line breakpoints break at a specific line.
  // function breakpoints stop when the function name in the callstack first happens
  // step breakpoints will stop on the next time the callstack_depth is the same or less
  // than when created and also will auto_delete.
  enum class Type { line, function, step} ;
  int id{ 0 };
  Type typ{ Type::line };
  std::string module;
  std::string function_name;
  int callstack_depth{ -1 };
  int line{ 0 };
  bool auto_delete{ false };
  bool visible{ true };
  int hit_count{ 0 };
};

void to_json(nlohmann::json& j, const Breakpoint& p);
void from_json(const nlohmann::json& j, Breakpoint& p);

struct DebugLocation {
  std::string module;
  int pos;
  int row;
  int col;
};
void to_json(nlohmann::json& j, const DebugLocation& v);
void from_json(const nlohmann::json& j, DebugLocation& v);

}

#endif
