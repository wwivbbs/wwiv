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
#include "bbs/basic/debug_model.h"
#include "bbs/basic/util.h"
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace wwiv::bbs::basic {

// JSON

void to_json(nlohmann::json& j, const Variable& v) {
  j = nlohmann::json{ 
    {"name", v.name}, 
    {"type", v.type}, 
    {"value", v.value},
    {"frame", v.frame}
  };
}

void from_json(const nlohmann::json& j, Variable& v) {
  j.at("name").get_to(v.name);
  j.at("type").get_to(v.type);
  j.at("value").get_to(v.value);
  j.at("frame").get_to(v.frame);
}


void to_json(nlohmann::json& j, const DebugLocation& v) {
  j = nlohmann::json{ 
    {"module", v.module}, 
    {"pos", v.pos},
    {"row", v.row},
    {"col", v.col}
  };
}

void from_json(const nlohmann::json& j, DebugLocation& v) {
  j.at("module").get_to(v.module);
  j.at("pos").get_to(v.pos);
  j.at("row").get_to(v.row);
  j.at("col").get_to(v.col);
}

}

