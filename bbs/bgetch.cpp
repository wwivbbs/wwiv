/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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
#include "bbs/bgetch.h"

#include <cmath>
#include <string>

#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/chat.h"
#include "bbs/com.h"
#include "bbs/datetime.h"
#include "bbs/instmsg.h"
#include "bbs/keycodes.h"
#include "bbs/multinst.h"
#include "bbs/utility.h"
#include "bbs/vars.h"
#include "bbs/wconstants.h"

#include "core/log.h"
#include "core/strings.h"

using namespace wwiv::sdk;
using namespace wwiv::strings;

static long time_lastchar_pressed = 0;

static void lastchar_pressed() {
  time_lastchar_pressed = timer1();
}


extern int nsp;
static void resetnsp() {
  if (nsp == 1 && !(session()->user()->HasPause())) {
    session()->user()->ToggleStatusFlag(User::pauseOnPage);
  }
  nsp = 0;
}

static void PrintTime() {
  SavedLine line = bout.SaveCurrentLine();

  bout.Color(0);
  bout.nl(2);
  time_t l = time(nullptr);
  std::string currentTime = asctime(localtime(&l));

  // Remove the ending \n character.
  currentTime.erase(currentTime.find_last_of("\r\n"));

  bout << "|#2" << currentTime << wwiv::endl;
  if (session()->IsUserOnline()) {
    bout << "|#9Time on   = |#1" << ctim(timer() - timeon) << wwiv::endl;
    bout << "|#9Time left = |#1" << ctim(nsl()) << wwiv::endl;
  }
  bout.nl();

  bout.RestoreCurrentLine(line);
}

static void RedrawCurrentLine() {
  char ansistr_1[81];

  int ansiptr_1 = ansiptr;
  ansiptr = 0;
  ansistr[ansiptr_1] = 0;
  strncpy(ansistr_1, ansistr, sizeof(ansistr_1) - 1);

  SavedLine line = bout.SaveCurrentLine();
  bout.nl();
  bout.RestoreCurrentLine(line);

  strcpy(ansistr, ansistr_1);
  ansiptr = ansiptr_1;
}

static void HandleControlKey(char *ch) {
  char c = *ch;

  if (c == CBACKSPACE) {
    c = BACKSPACE;
  }
  if (okskey) {
    switch (c) {
    case CA:   // CTRL-A
    case CD:   // CTRL-D
    case CF:   // CTRL-F
      if (okmacro && (!charbufferpointer)) {
        static constexpr int MACRO_KEY_TABLE[] = {0, 2, 0, 0, 0, 0, 1};
        int macroNum = MACRO_KEY_TABLE[(int)c];
        strncpy(charbuffer, &(session()->user()->GetMacro(macroNum)[0]), sizeof(charbuffer) - 1);
        c = charbuffer[0];
        if (c) {
          charbufferpointer = 1;
        }
      }
      break;
    case CT:  // CTRL - T
      if (local_echo) {
        PrintTime();
      }
      break;
    case CU:  // CTRL-U
      if (local_echo) {
        SavedLine line = bout.SaveCurrentLine();
        bout.Color(0);
        bout.nl(2);
        multi_instance();
        bout.nl();
        bout.RestoreCurrentLine(line);
      }
      break;
    case 18: // CR
      if (local_echo) {
        RedrawCurrentLine();
      }
      break;
    case CL:  // CTRL - L
      if (so()) {
        toggle_invis();
      }
      break;
    case CN:  // CTRL - N
      toggle_avail();
      break;
    case CY:
      session()->user()->ToggleStatusFlag(User::pauseOnPage);
      break;
    }
  }
  *ch = c;
}

/* This function checks both the local keyboard, and the remote terminal
 * (if any) for input.  If there is input, the key is returned.  If there
 * is no input, a zero is returned.  Function keys hit are interpreted as
 * such within the routine and not returned.
 */
char bgetch() {
  char ch = 0;
  static int qpointer = 0, cpointer;

  if (bquote) {
    if (!qpointer) {
      charbuffer[1] = '0';
      charbuffer[2] = RETURN;
      charbuffer[3] = '\0';
      cpointer = 0;
      qpointer = 1;
      while (qpointer < bquote + 2) {
        if (quotes_ind[cpointer++] == SOFTRETURN) {
          if (quotes_ind[cpointer] != CD) {
            ++qpointer;
          }
        }
      }
      charbufferpointer = 1;
    }
    while (quotes_ind[cpointer] == CD) {
      while (quotes_ind[cpointer++] != SOFTRETURN)
        // Do nothing...
        ;
    }
    if (quotes_ind[cpointer] == SOFTRETURN) {
      ++qpointer;
      if (qpointer > equote + 2) {
        qpointer = 0;
        bquote = 0;
        equote = 0;
        return CP;
      } else {
        ++cpointer;
      }
    }
    if (quotes_ind[cpointer] == CC) {
      ++cpointer;
      return CP;
    }
    if (quotes_ind[cpointer] == 0) {
      qpointer = 0;
      bquote = 0;
      equote = 0;
      return RETURN;
    }
    return quotes_ind[cpointer++];
  }

  if (charbufferpointer) {
    if (!charbuffer[charbufferpointer]) {
      charbufferpointer = charbuffer[0] = 0;
    } else {
      if ((charbuffer[charbufferpointer]) == CC) {
        charbuffer[charbufferpointer] = CP;
      }
      return charbuffer[charbufferpointer++];
    }
  }
  if (session()->localIO()->KeyPressed()) {
    ch = session()->localIO()->GetChar();
    session()->SetLastKeyLocal(true);
    if (!(g_flags & g_flag_allow_extended)) {
      if (!ch) {
        ch = session()->localIO()->GetChar();
        session()->handle_sysop_key(static_cast<uint8_t>(ch));
        ch = static_cast<char>(((ch == F10) || (ch == CF10)) ? 2 : 0);
      }
    }
    lastchar_pressed();
  } else if (incom && bkbhitraw()) {
    ch = bgetchraw();
    session()->SetLastKeyLocal(false);
  }

  if (!(g_flags & g_flag_allow_extended)) {
    HandleControlKey(&ch);
  }

  return ch;
}

char bgetchraw() {
  if (ok_modem_stuff && nullptr != session()->remoteIO()) {
    if (session()->remoteIO()->incoming()) {
      return (session()->remoteIO()->getW());
    }
    if (session()->localIO()->KeyPressed()) {
      return session()->localIO()->GetChar();
    }
  }
  return 0;
}

bool bkbhitraw() {
  if (ok_modem_stuff) {
    return (session()->remoteIO()->incoming() || session()->localIO()->KeyPressed());
  } else if (session()->localIO()->KeyPressed()) {
    return true;
  }
  return false;
}

bool bkbhit() {
  if ((session()->localIO()->KeyPressed() || (incom && bkbhitraw()) ||
       (charbufferpointer && charbuffer[charbufferpointer])) ||
      bquote) {
    return true;
  }
  return false;
}


bool Output::RestoreCurrentLine(const SavedLine& line) {
  if (session()->localIO()->WhereX()) {
    bout.nl();
  }
  for (const auto& c : line.line) {
    bout.SystemColor(c.second);
    bout.bputch(c.first, true);
  }
  bout.flush();
  bout.SystemColor(line.color);
  endofline_ = line.endofline;

  return true;
}

SavedLine Output::SaveCurrentLine() {
  return {current_line_, curatr, endofline_};
}

void Output::dump() {
  if (ok_modem_stuff) {
    session()->remoteIO()->purgeIn();
  }
}

int Output::wherex() { 
  int x = localIO()->WhereX();
  if (x != x_) {
    LOG(INFO) << "x: " << x << " != x_: " << x_;
  }
  return x_; 
}

/* This function returns one character from either the local keyboard or
* remote com port (if applicable).  After 1.5 minutes of inactivity, a
* beep is sounded.  After 3 minutes of inactivity, the user is hung up.
*/
char Output::getkey() {
  resetnsp();
  bool beepyet = false;
  lastchar_pressed();

  long tv = (so() || IsEqualsIgnoreCase(session()->GetCurrentSpeed().c_str(), "TELNET")) ? 10920L : 3276L;
  long tv1 = tv - 1092L;     // change 4.31 Build3

                             // Since were waitig for a key, reset the # of lines we've displayed since a pause.
  lines_listed = 0;
  char ch = 0;
  do {
    while (!bkbhit() && !hangup) {
      giveup_timeslice();
      long dd = timer1();
      if (dd < time_lastchar_pressed && ((dd + 1000) > time_lastchar_pressed)) {
        time_lastchar_pressed = dd;
      }
      if (std::abs(dd - time_lastchar_pressed) > 65536L) {
        time_lastchar_pressed -= static_cast<int>(floor(SECONDS_PER_DAY * 18.2));
      }
      if ((dd - time_lastchar_pressed) > tv1 && !beepyet) {
        beepyet = true;
        bout.bputch(CG);
      }
      if (std::abs(dd - time_lastchar_pressed) > tv) {
        bout.nl();
        bout << "Call back later when you are there.\r\n";
        hangup = true;
      }
      CheckForHangup();
    }
    ch = bgetch();
  } while (!ch && !hangup);
  return ch;
}

