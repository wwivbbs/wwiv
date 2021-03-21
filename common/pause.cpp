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
/*                                                                        */
/**************************************************************************/
#include "common/pause.h"

#include "common/common_events.h"
#include "common/context.h"
#include "common/input.h"
#include "common/output.h"
#include "core/datetime.h"
#include "core/eventbus.h"
#include "core/os.h"
#include "core/scope_exit.h"
#include "core/strings.h"
#include "local_io/keycodes.h"
#include <chrono>

using std::chrono::milliseconds;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::local::io;
using namespace wwiv::os;
using namespace wwiv::sdk;
using namespace wwiv::strings;

namespace wwiv::common {

TempDisablePause::TempDisablePause(Output& out)
    : Transaction([&out] {
    if (out.sess().disable_pause()) {
      out.sess().disable_pause(false);
      out.user().set_flag(User::pauseOnPage);
    }
  }, nullptr) {
  if (out.user().pause()) {
    out.sess().disable_pause(true);
    out.user().clear_flag(User::pauseOnPage);
  }
}

char Output::GetKeyForPause() {
  char ch = 0;
  while (ch == 0 && !sess().hangup()) {
    ch = bin.bgetch();
    sleep_for(milliseconds(50));
    bus().invoke<CheckForHangupEvent>();
  }
  const auto k = to_upper_case<int>(ch);
  switch (k) {
  case ESC:
  case 'Q':
  case 'N':
    if (!bin.bkbhit()) {
      bin.nsp(-1);
    }
    break;
  case 'C':
  case '=':
    if (user().pause()) {
      bin.nsp(1);
      user().toggle_flag(User::pauseOnPage);
    }
    break;
  default:
    break;
  }
  return ch;
}

static bool okansi(const User& user) { return user.ansi(); }

void Output::pausescr_noansi() {
  const auto str_pause = bout.lang().value("PAUSE", "|#3More? [Y/n/c]"); 
  const auto stripped_size = ssize(stripcolors(str_pause));
  bputs(str_pause);
  GetKeyForPause();
  for (auto i = 0; i < stripped_size; i++) {
    bs();
  }
}

void Output::pausescr() {
  const auto saved_curatr = curatr();
  bin.clearnsp();
  bus().invoke<PauseProcessingInstanceMessages>();
  ScopeExit at_exit(
      [] { wwiv::core::bus().invoke<ResetProcessingInstanceMessages>(); });
  if (!okansi(user())) {
    pausescr_noansi();
    return;
  }

  if (!sess().incom() && sess().outcom()) {
    sess().incom(true);
  }

  const auto str_pause = bout.lang().value("PAUSE", "|#3More? [Y/n/c]"); 
  ResetColors();

  const auto stripped_size = ssize(stripcolors(str_pause));
  const auto com_freeze = sess().incom();
  bputs(str_pause);
  Left(stripped_size);
  SystemColor(saved_curatr);

  const auto tstart = time_t_now();

  clear_lines_listed();
  auto warned = false;
  char ch;
  do {
    while (!bin.bkbhit() && !sess().hangup()) {
      const auto tstop = time_t_now();
      const auto ttotal = static_cast<time_t>(difftime(tstop, tstart));
      if (ttotal == 120) {
        if (!warned) {
          warned = true;
          // Strip the colors and display the pause prompt all red here.
          bputch(CG);
          SystemColor(user().color(6));
          bputs(stripcolors(str_pause));
          Left(stripped_size);
          SystemColor(saved_curatr);
        }
      } else {
        if (ttotal > 180) {
          bputch(CG);
          for (auto i3 = 0; i3 < stripped_size; i3++) {
            bputch(' ');
          }
          Left(stripped_size);
          SystemColor(saved_curatr);
          return;
        }
      }
      sleep_for(milliseconds(50));
      bus().invoke<CheckForHangupEvent>();
    }
    ch = GetKeyForPause();
  } while (!ch && !sess().hangup());
  for (int i3 = 0; i3 < stripped_size; i3++) {
    bputch(' ');
  }
  Left(stripped_size);
  SystemColor(saved_curatr);

  if (!com_freeze) {
    sess().incom(false);
  }
}


} // namespace wwiv::common
