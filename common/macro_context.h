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
#ifndef __INCLUDED_COMMON_MACRO_CONTEXT_H__
#define __INCLUDED_COMMON_MACRO_CONTEXT_H__

#include "common/context.h"
#include <string>

namespace wwiv::common {

class MacroContext {
public:
  MacroContext(wwiv::common::Context* context) : context_(context) {}
  ~MacroContext() = default;

  virtual std::string interpret(char c) const = 0;
  wwiv::common::Context* context() { return context_; }

protected:
  wwiv::common::Context* context_{nullptr};
};

} // namespace wwiv::common

#endif // __INCLUDED_BBS_INTERPRET_H__
