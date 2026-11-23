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

#include "core/datetime.h"
#include "wwivbasic/interpreter.h"

#include <ctime>
#include <string>

namespace wwiv::bbs::basic {

// The wwiv.time module (bbs/basic/wwiv_time.cpp).
void RegisterWwivTime(wwivbasic::Interpreter& interp, const WwivBindings&) {
  auto& m = interp.add_module("wwiv.time");

  m.native_function("MODULE_NAME", []() { return std::string("wwiv.time"); });

  // FORMAT(t, format) formats the time_t t with a strftime-style format.
  m.native_function("FORMAT", [](time_t t, std::string format) {
    return core::DateTime::from_time_t(t).to_string(format);
  });
}

} // namespace wwiv::bbs::basic
