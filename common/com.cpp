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
/*                                                                        */
/**************************************************************************/
#include "common/com.h"

#include "common/common_events.h"
#include "common/input.h"
#include "common/output.h"
#include "core/eventbus.h"
#include "core/stl.h"
#include "core/strings.h"

using namespace wwiv::common;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

char onek(const std::string& allowable, bool auto_mpl) {
  if (auto_mpl) {
    bout.mpl(1);
  }
  const auto ch = onek_ncr(allowable);
  bout.nl();
  return ch;
}

// Like onek but does not put cursor down a line
// One key, no carriage return
char onek_ncr(const std::string& allowable) {
  while (true) {
    wwiv::core::bus().invoke<CheckForHangupEvent>();
    auto ch = to_upper_case(bin.getkey());
    if (contains(allowable, ch)) {
      return ch;
    }
  }
}
