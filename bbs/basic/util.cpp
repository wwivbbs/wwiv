/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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

namespace wwiv::bbs::basic {

using namespace wwiv::common;

static Output* script_out_{nullptr};

Output& script_out() { 
  CHECK_NOTNULL(script_out_);
  return *script_out_; 
}

void set_script_out(Output* o) { 
  script_out_ = o; 
}

static wwiv::common::Input* script_in_{nullptr};

wwiv::common::Input& script_in() {
  CHECK_NOTNULL(script_in_);
  return *script_in_;
}

void set_script_in(wwiv::common::Input* o) { 
  script_in_ = o;
}

char* BasicStrDup(const std::string& s) { 
  return mb_memdup(s.c_str(), s.size() + 1);
}

wwiv_script_userdata_t* get_wwiv_script_userdata(struct mb_interpreter_t* bas) {
  void* x{};
  const auto result = mb_get_userdata(bas, &x);
  if (result != MB_FUNC_OK) {
    LOG(ERROR) << "Error getting the script userdata";
    return nullptr;
  }
  return static_cast<wwiv_script_userdata_t*>(x);
}


}