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
#include "bbs/basic/next/type_ahead_test_util.h"

#include "gtest/gtest.h"

#include "common/common_events.h"
#include "common/context.h"
#include "common/input.h"
#include "core/eventbus.h"

#include <cstring>
#include <string>

namespace wwiv::bbs::basic::type_ahead {

namespace {
// While set, a test is running a script with typed-ahead keys.
common::SessionContext* sessions_to_hang_up[2]{};
} // namespace

void HangUpWhenKeysRunOut(common::SessionContext& a, common::SessionContext& b) {
  [[maybe_unused]] static const auto registered = [] {
    // Input sends GiveupTimeslices only while it waits for a key that hasn't
    // been typed: here, that means the typed-ahead keys have run out.
    core::bus().add_handler<common::GiveupTimeslices>([] {
      if (!sessions_to_hang_up[0]) {
        return;
      }
      for (auto* s : sessions_to_hang_up) {
        s->hangup(true);
      }
      // Type one more key, so getkey returns it: with nothing to read after a
      // hangup, getkey reaches DLOG(FATAL) and aborts (issue #1758).
      std::strcpy(&bin.charbuffer[1], "\r");
      bin.charbufferpointer_ = 1;
    });
    return true;
  }();
  sessions_to_hang_up[0] = &a;
  sessions_to_hang_up[1] = &b;
}

void StopTypingAhead() {
  sessions_to_hang_up[0] = sessions_to_hang_up[1] = nullptr;
  bin.charbufferpointer_ = 0;
  bin.charbuffer[0] = '\0';
  bin.charbuffer[1] = '\0';
}

void TypeAhead(const std::string& keys) {
  ASSERT_LT(keys.size(), sizeof(bin.charbuffer) - 1);
  std::strcpy(&bin.charbuffer[1], keys.c_str());
  bin.charbufferpointer_ = 1;
}

} // namespace wwiv::bbs::basic::type_ahead
