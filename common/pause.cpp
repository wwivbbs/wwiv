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
#include "common/pause.h"

#include "common/bgetch.h"
#include "common/input.h"
#include "common/com.h"
#include "common/common_events.h"
#include "common/context.h"
#include "bbs/instmsg.h"  // setiia
#include "local_io/keycodes.h"
#include "bbs/utility.h"
#include "core/datetime.h"
#include "core/eventbus.h"
#include "core/os.h"
#include "core/strings.h"
#include <chrono>

extern char str_pause[];

using std::chrono::milliseconds;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::os;
using namespace wwiv::sdk;
using namespace wwiv::strings;

namespace wwiv::common {

TempDisablePause::TempDisablePause(Output& out)
    : wwiv::core::Transaction([&out] {
    if (out.sess().disable_pause()) {
      out.sess().disable_pause(false);
      out.user().SetStatusFlag(User::pauseOnPage);
    }
  }, nullptr) {
  if (out.user().HasPause()) {
    out.sess().disable_pause(true);
    out.user().ClearStatusFlag(User::pauseOnPage);
  }
}

char Output::GetKeyForPause() {
  char ch = 0;
  while (ch == 0 && !sess().hangup()) {
    ch = bin.bgetch();
    sleep_for(milliseconds(50));
    bus().invoke<CheckForHangupEvent>();
  }
  int nKey = to_upper_case<int>(ch);
  switch (nKey) {
  case ESC:
  case 'Q':
  case 'N':
    if (!bin.bkbhit()) {
      bin.nsp(-1);
    }
    break;
  case 'C':
  case '=':
    if (user().HasPause()) {
      bin.nsp(1);
      user().ToggleStatusFlag(User::pauseOnPage);
    }
    break;
  default:
    break;
  }
  return ch;
}

void Output::pausescr() {
  bin.clearnsp();
  auto oiia = setiia(std::chrono::milliseconds(0));
  char* ss = str_pause;
  int i1;
  int i2 = i1 = strlen(ss);
  bool com_freeze = sess().incom();

  if (!sess().incom() && sess().outcom()) {
    sess().incom(true);
  }

  if (okansi()) {
    ResetColors();

    i1 = strlen(stripcolors(ss));
    auto i = curatr();
    SystemColor(user().color(3));
    bputs(ss);
    Left(i1);
    SystemColor(i);

    auto tstart = time_t_now();

    clear_lines_listed();
    int warned = 0;
    char ch;
    do {
      while (!bin.bkbhit() && !sess().hangup()) {
        auto tstop = time_t_now();
        auto ttotal = difftime(tstop, tstart);
        if (ttotal == 120) {
          if (!warned) {
            warned = 1;
            bputch(CG);
            SystemColor(user().color(6));
            bputs(ss);
            for (int i3 = 0; i3 < i2; i3++) {
              if (ss[i3] == 3 && i1 > 1) {
                i1 -= 2;
              }
            }
            Left(i1);
            SystemColor(i);
          }
        } else {
          if (ttotal > 180) {
            bputch(CG);
            for (int i3 = 0; i3 < i1; i3++) {
              bputch(' ');
            }
            Left(i1);
            SystemColor(i);
            setiia(oiia);
            return;
          }
        }
        sleep_for(milliseconds(50));
        bus().invoke<CheckForHangupEvent>();
      }
      ch = GetKeyForPause();
    } while (!ch && !sess().hangup());
    for (int i3 = 0; i3 < i1; i3++) {
      bputch(' ');
    }
    Left(i1);
    SystemColor(i);
    setiia(oiia);

  } else {
    // nonansi code path
    for (int i3 = 0; i3 < i2; i3++) {
      if ((ss[i3] == CC) && i1 > 1) {
        i1 -= 2;
      }
    }
    bputs(ss);
    GetKeyForPause();
    for (int i3 = 0; i3 < i1; i3++) {
      bs();
    }
  }

  if (!com_freeze) {
    sess().incom(false);
  }
}


} // namespace wwiv::common
