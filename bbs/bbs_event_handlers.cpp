/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2020-2022, WWIV Software Services             */
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
#include "bbs/bbs_event_handlers.h"

#include "bbs/application.h"
#include "bbs/bbs.h"
#include "common/common_events.h"
#include "core/eventbus.h"
#include "core/os.h"
#include "core/strings.h"
#include "instmsg.h"
#include "multinst.h"
#include "utility.h"
#include "common/datetime.h"
#include "common/output.h"

namespace wwiv::bbs {

using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::os;
using namespace wwiv::strings;

static void PrintTime() {
  const auto line = bout.SaveCurrentLine();

  bout.ansic(0);
  bout.nl(2);
  const auto dt = DateTime::now();
  bout.print("|#2{}\r\n", dt.to_string());
  if (a()->sess().IsUserOnline()) {
    const auto time_on = std::chrono::system_clock::now() - a()->sess().system_logon_time();
    const auto seconds_on =
        static_cast<long>(std::chrono::duration_cast<std::chrono::seconds>(time_on).count());
    bout.print("|#9Time on   = |#1{}\r\n", ctim(seconds_on));
    bout.print("|#9Time left = |#1{}\r\n", ctim(nsl()));
  }
  bout.nl();
  bout.RestoreCurrentLine(line);
}

static void CheckForHangup() {
  a()->CheckForHangup();
}

static void GiveupTimeSlices() {
  yield();
  if (inst_msg_waiting() && (!a()->sess().in_chatroom() || !a()->sess().chatline())) {
    process_inst_msgs();
  } else {
    sleep_for(std::chrono::milliseconds(100));
  }
  yield();
}

bool bbs_callbacks() {
  // Register Application Level Callbacks
  bus().add_handler<ProcessInstanceMessages>([]() {
    if (inst_msg_waiting() && !a()->sess().chatline()) {
      process_inst_msgs();
    }
  });
  bus().add_handler<ResetProcessingInstanceMessages>([]() { setiia(std::chrono::seconds(5)); });
  bus().add_handler<PauseProcessingInstanceMessages>([]() { setiia(std::chrono::seconds(0)); });
  bus().add_handler<CheckForHangupEvent>(CheckForHangup);
  bus().add_handler<HangupEvent>([]() { a()->Hangup(); });
  bus().add_handler<UpdateTopScreenEvent>([]() { a()->UpdateTopScreen(); });
  bus().add_handler<UpdateTimeLeft>(
      [](const UpdateTimeLeft& u) { a()->tleft(u.check_for_timeout); });
  bus().add_handler<HandleSysopKey>([](const HandleSysopKey& k) { a()->handle_sysop_key(k.key); });
  bus().add_handler<GiveupTimeslices>(GiveupTimeSlices);
  bus().add_handler<DisplayTimeLeft>(PrintTime);
  bus().add_handler<DisplayMultiInstanceStatus>(multi_instance);

  bus().add_handler<ToggleAvailable>(toggle_avail);
  bus().add_handler<ToggleInvisble>(toggle_invis);
  return true;
}

} // namespace wwiv::bbs
