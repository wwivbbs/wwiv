/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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

#include "wwiv.h"
#include "core/os.h"
#include "core/strings.h"
#include "pause.h"
#include "bbs/keycodes.h"

extern char str_pause[];

using std::chrono::milliseconds;
using namespace wwiv::os;

namespace wwiv {
namespace bbs {

TempDisablePause::TempDisablePause() : wwiv::core::Transaction([] {
    if (g_flags & g_flag_disable_pause) {
      g_flags &= ~g_flag_disable_pause;
      GetSession()->GetCurrentUser()->SetStatusFlag(WUser::pauseOnPage);
    }
  }, nullptr) {
  if (GetSession()->GetCurrentUser()->HasPause()) {
    g_flags |= g_flag_disable_pause;
    GetSession()->GetCurrentUser()->ClearStatusFlag(WUser::pauseOnPage);
  }
}

}  // namespace bbs
}  // namespace wwiv

static char GetKeyForPause() {
  char ch = 0;
  while (ch == 0) {
    ch = bgetch();
    sleep_for(milliseconds(50));
    CheckForHangup();
  }
  int nKey = wwiv::UpperCase<int>(ch);
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
    if (GetSession()->GetCurrentUser()->HasPause()) {
      nsp = 1;
      GetSession()->GetCurrentUser()->ToggleStatusFlag(WUser::pauseOnPage);
    }
    break;
  default:
    break;
  }
  return ch;
}
// This will pause output, displaying the [PAUSE] message, and wait a key to be hit.
void pausescr() {
  int i1, i3, warned;
  int i = 0;
  char s[81];
  char ch;
  double ttotal;
  time_t tstart, tstop;

  if (x_only) {
    return;
  }

  nsp = 0;

  int oiia = setiia(0);

  char* ss = str_pause;
  int i2 = i1 = strlen(ss);

  bool com_freeze = incom;

  if (!incom && outcom) {
    incom = true;
  }

  if (okansi()) {
    bout.ResetColors();

    i1 = strlen(stripcolors(ss));
    i = curatr;
    bout.SystemColor(GetSession()->GetCurrentUser()->HasColor() ? GetSession()->GetCurrentUser()->GetColor(
                                     3) :
                                   GetSession()->GetCurrentUser()->GetBWColor(3));
    bout << ss << "\x1b[" << i1 << "D";
    bout.SystemColor(i);

    time(&tstart);

    lines_listed = 0;
    warned = 0;
    do {
      while (!bkbhit() && !hangup) {
        time(&tstop);
        ttotal = difftime(tstop, tstart);
        if (ttotal == 120) {
          if (!warned) {
            warned = 1;
            bputch(CG);
            bout.SystemColor(GetSession()->GetCurrentUser()->HasColor() ? GetSession()->GetCurrentUser()->GetColor(
                                             6) :
                                           GetSession()->GetCurrentUser()->GetBWColor(6));
            bout << ss;
            for (int i3 = 0; i3 < i2; i3++) {
              if (s[i3] == 3 && i1 > 1) {
                i1 -= 2;
              }
            }
            bout << "\x1b[" << i1 << "D";
            bout.SystemColor(i);
          }
        } else {
          if (ttotal > 180) {
            bputch(CG);
            for (i3 = 0; i3 < i1; i3++) {
              bputch(' ');
            }
            bout << "\x1b[" << i1 << "D";
            bout.SystemColor(i);
            setiia(oiia);
            return;
          }
        }
        sleep_for(milliseconds(50));
        CheckForHangup();
      }
      ch = GetKeyForPause();
    } while (!ch && !hangup);
    for (i3 = 0; i3 < i1; i3++) {
      bputch(' ');
    }
    bout << "\x1b[" << i1 << "D";
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
    incom = false;
  }
}


