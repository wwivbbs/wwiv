/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services             */
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

#include <chrono>

#include "bbs/bbs.h"
#include "bbs/bgetch.h"
#include "bbs/com.h"
#include "bbs/instmsg.h"
#include "local_io/keycodes.h"
#include "bbs/pause.h"
#include "bbs/utility.h"

#include "core/os.h"
#include "core/strings.h"

extern char str_pause[];

using std::chrono::milliseconds;
using namespace wwiv::os;
using namespace wwiv::sdk;
using namespace wwiv::strings;

int nsp;

namespace wwiv {
namespace bbs {

TempDisablePause::TempDisablePause() : wwiv::core::Transaction([] {
    if (a()->context().disable_pause()) {
      a()->context().disable_pause(false);
      a()->user()->SetStatusFlag(User::pauseOnPage);
    }
  }, nullptr) {
  if (a()->user()->HasPause()) {
    a()->context().disable_pause(true);
    a()->user()->ClearStatusFlag(User::pauseOnPage);
  }
}

}  // namespace bbs
}  // namespace wwiv

static char GetKeyForPause() {
  char ch = 0;
  while (ch == 0 && !a()->hangup_) {
    ch = bgetch();
    sleep_for(milliseconds(50));
    CheckForHangup();
  }
  int nKey = to_upper_case<int>(ch);
  switch (nKey) {
  case ESC:
  case 'Q':
  case 'N':
    if (!bkbhit()) {
      nsp = -1;
    }
    break;
  case 'C':
  case '=':
    if (a()->user()->HasPause()) {
      nsp = 1;
      a()->user()->ToggleStatusFlag(User::pauseOnPage);
    }
    break;
  default:
    break;
  }
  return ch;
}

// This will pause output, displaying the [PAUSE] message, and wait a key to be hit.
void pausescr() {
  int i1, warned;
  int i = 0;
  char ch;
  double ttotal;
  time_t tstart, tstop;

  nsp = 0;
  auto oiia = setiia(std::chrono::milliseconds(0));
  char* ss = str_pause;
  int i2 = i1 = strlen(ss);
  bool com_freeze = a()->context().incom();

  if (!a()->context().incom() && a()->context().outcom()) {
    a()->context().incom(true);
  }

  if (okansi()) {
    bout.ResetColors();

    i1 = strlen(stripcolors(ss));
    i = bout.curatr();
    bout.SystemColor(a()->user()->color(3));
    bout << ss;
    bout.Left(i1);
    bout.SystemColor(i);

    time(&tstart);

    bout.clear_lines_listed();
    warned = 0;
    do {
      while (!bkbhit() && !a()->hangup_) {
        time(&tstop);
        ttotal = difftime(tstop, tstart);
        if (ttotal == 120) {
          if (!warned) {
            warned = 1;
            bout.bputch(CG);
            bout.SystemColor(a()->user()->color(6));
            bout << ss;
            for (int i3 = 0; i3 < i2; i3++) {
              if (ss[i3] == 3 && i1 > 1) {
                i1 -= 2;
              }
            }
            bout.Left(i1);
            bout.SystemColor(i);
          }
        } else {
          if (ttotal > 180) {
            bout.bputch(CG);
            for (int i3 = 0; i3 < i1; i3++) {
              bout.bputch(' ');
            }
            bout.Left(i1);
            bout.SystemColor(i);
            setiia(oiia);
            return;
          }
        }
        sleep_for(milliseconds(50));
        CheckForHangup();
      }
      ch = GetKeyForPause();
    } while (!ch && !a()->hangup_);
    for (int i3 = 0; i3 < i1; i3++) {
      bout.bputch(' ');
    }
    bout.Left(i1);
    bout.SystemColor(i);
    setiia(oiia);

  } else {
    // nonansi code path
    for (int i3 = 0; i3 < i2; i3++) {
      if ((ss[i3] == CC) && i1 > 1) {
        i1 -= 2;
      }
    }
    bout << ss;
    GetKeyForPause();
    for (int i3 = 0; i3 < i1; i3++) {
      bout.bs();
    }
  }

  if (!com_freeze) {
    a()->context().incom(false);
  }
}

void resetnsp() {
  if (nsp == 1 && !(a()->user()->HasPause())) {
    a()->user()->ToggleStatusFlag(User::pauseOnPage);
  }
  nsp = 0;
}

void clearnsp() { nsp = 0; }
