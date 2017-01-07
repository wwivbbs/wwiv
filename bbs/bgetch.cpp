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
#include "bbs/bgetch.h"

#include <chrono>
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

using std::chrono::minutes;
using std::chrono::seconds;
using std::chrono::steady_clock;
using namespace wwiv::sdk;
using namespace wwiv::strings;

//static long time_lastchar_pressed = 0;
static steady_clock::time_point time_lastchar_pressed;

static void lastchar_pressed() {
  time_lastchar_pressed = steady_clock::now();
}

extern int nsp;
static void resetnsp() {
  if (nsp == 1 && !(a()->user()->HasPause())) {
    a()->user()->ToggleStatusFlag(User::pauseOnPage);
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
  if (a()->IsUserOnline()) {
    auto time_on = std::chrono::system_clock::now() - a()->system_logon_time();
    auto seconds_on = static_cast<long>(std::chrono::duration_cast<std::chrono::seconds>(time_on).count());
    bout << "|#9Time on   = |#1" << ctim(seconds_on) << wwiv::endl;
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
        strncpy(charbuffer, &(a()->user()->GetMacro(macroNum)[0]), sizeof(charbuffer) - 1);
        c = charbuffer[0];
        if (c) {
          charbufferpointer = 1;
        }
      }
      break;
    case CT:  // CTRL - T
      PrintTime();
      break;
    case CU: {  // CTRL-U
      SavedLine line = bout.SaveCurrentLine();
      bout.Color(0);
      bout.nl(2);
      multi_instance();
      bout.nl();
      bout.RestoreCurrentLine(line);
    } break;
    case 18: // CR
      RedrawCurrentLine();
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
      a()->user()->ToggleStatusFlag(User::pauseOnPage);
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
  if (a()->localIO()->KeyPressed()) {
    ch = a()->localIO()->GetChar();
    bout.SetLastKeyLocal(true);
    if (!(g_flags & g_flag_allow_extended)) {
      if (!ch) {
        ch = a()->localIO()->GetChar();
        a()->handle_sysop_key(static_cast<uint8_t>(ch));
        ch = static_cast<char>(((ch == F10) || (ch == CF10)) ? 2 : 0);
      }
    }
    lastchar_pressed();
  } else if (incom && bkbhitraw()) {
    ch = bgetchraw();
    bout.SetLastKeyLocal(false);
  }

  if (!(g_flags & g_flag_allow_extended)) {
    HandleControlKey(&ch);
  }

  return ch;
}

char bgetchraw() {
  if (ok_modem_stuff && nullptr != a()->remoteIO()) {
    if (a()->remoteIO()->incoming()) {
      return (a()->remoteIO()->getW());
    }
    if (a()->localIO()->KeyPressed()) {
      return a()->localIO()->GetChar();
    }
  }
  return 0;
}

bool bkbhitraw() {
  if (ok_modem_stuff) {
    return (a()->remoteIO()->incoming() || a()->localIO()->KeyPressed());
  } else if (a()->localIO()->KeyPressed()) {
    return true;
  }
  return false;
}

bool bkbhit() {
  if ((a()->localIO()->KeyPressed() || (incom && bkbhitraw()) ||
       (charbufferpointer && charbuffer[charbufferpointer])) ||
      bquote) {
    return true;
  }
  return false;
}


bool Output::RestoreCurrentLine(const SavedLine& line) {
  if (bout.wherex()) {
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
    a()->remoteIO()->purgeIn();
  }
}

int Output::wherex() { 
  int x = localIO()->WhereX();
  if (x != x_) {
    LOG(INFO) << "x: " << x << " != x_: " << x_;
  }
  return x_; 
}

std::chrono::duration<double> Output::key_timeout() const { 
  if (so()) {
    return sysop_key_timeout_;
  }
  return non_sysop_key_timeout_; 
}

/* This function returns one character from either the local keyboard or
* remote com port (if applicable).  After 1.5 minutes of inactivity, a
* beep is sounded.  After 3 minutes of inactivity, the user is hung up.
*/
char Output::getkey() {
  resetnsp();
  bool beepyet = false;
  lastchar_pressed();

  auto tv = key_timeout();
  auto tv1 = tv - minutes(1);
  if (tv1 < seconds(10)) {
    tv1 = seconds(10);
  }

  // Since were waitig for a key, reset the # of lines we've displayed since a pause.
  bout.clear_lines_listed();
  char ch = 0;
  do {
    CheckForHangup();
    while (!bkbhit() && !hangup) {
      // Try to make hangups happen faster.
      if (incom && ok_modem_stuff && !a()->remoteIO()->connected()) {
        Hangup();
      }
      CheckForHangup();
      giveup_timeslice();
      auto dd = steady_clock::now();

      auto diff = dd - time_lastchar_pressed;
      if (diff > tv1 && !beepyet) {
        beepyet = true;
        bout.bputch(CG);
      }
      if (diff > tv) {
        bout.nl();
        bout << "Call back later when you are there.\r\n";
        Hangup();
      }
    }
    ch = bgetch();
  } while (!ch);
  return ch;
}

// The final character of an ansi sequence
#define OB ('[')
#define O ('O')
#define A_HOME ('H')
#define A_LEFT ('D')
#define A_END ('K')
#define A_UP ('A')
#define A_DOWN ('B')
#define A_RIGHT ('C')
#define A_PAGEUP ('V')
#define A_PAGEDOWN ('U')
#define A_INSERT ('r')
#define A_DELETE ('s')

static int pd_getkey() {
  g_flags |= g_flag_allow_extended;
  int x = bout.getkey();
  g_flags &= ~g_flag_allow_extended;
  return x;
}

static int GetNumPadCommand(int key) {
  switch (key) {
  case '8': return COMMAND_UP;
  case '4': return COMMAND_LEFT;
  case '2': return COMMAND_DOWN;
  case '6': return COMMAND_RIGHT;
  case '0': return COMMAND_INSERT;
  case '.': return COMMAND_DELETE;
  case '9': return COMMAND_PAGEUP;
  case '3': return COMMAND_PAGEDN;
  case '7': return COMMAND_HOME;
  case '1': return COMMAND_END;
  }
  return 0;
}


static int GetCommandForAnsiKey(int key) {
  switch (key) {
  case A_UP: return COMMAND_UP;
  case A_LEFT: return COMMAND_LEFT;
  case A_DOWN: return COMMAND_DOWN;
  case A_RIGHT: return COMMAND_RIGHT;
  case A_INSERT: return COMMAND_INSERT;
  case A_DELETE: return COMMAND_DELETE;
  case A_HOME: return COMMAND_HOME;
  case A_END: return COMMAND_END;
  case A_PAGEUP: return COMMAND_PAGEUP;
  case A_PAGEDOWN: return COMMAND_PAGEDN;
  default: return key;
  }
}

int bgetch_event(numlock_status_t numlock_mode) {
  return bgetch_event(numlock_mode, [](bgetch_timeout_status_t, int) {});
}

int bgetch_event(numlock_status_t numlock_mode, bgetch_timeout_callback_fn cb) {
  a()->tleft(true);
  bool beepyet = false;
  resetnsp();
  lastchar_pressed();

  auto tv = bout.key_timeout();
  auto tv1 = tv - std::chrono::minutes(1);

  while (true) {
    CheckForHangup();
    if (hangup) {
      return 0;
    }
    auto dd = steady_clock::now();
    auto diff = dd - time_lastchar_pressed;
    if (diff > tv1 && !beepyet) {
      beepyet = true;
      bout.bputch(CG);
      cb(bgetch_timeout_status_t::WARNING, 60);
    }
    if (diff > tv) {
      bout.nl();
      bout << "Call back later when you are there.\r\n";
      Hangup();
      return 0;
    }

    if (!bkbhitraw() && !a()->localIO()->KeyPressed()) {
      giveup_timeslice();
      continue;
    }
    else if (beepyet) {
      cb(bgetch_timeout_status_t::CLEAR, 0);
    }

    if (!incom || a()->localIO()->KeyPressed()) {
      // Check for local keys
      int key = a()->localIO()->GetChar();
      if (key == CBACKSPACE) {
        return COMMAND_DELETE;
      }
      if (key == CV) {
        return COMMAND_INSERT;
      }
      if (key == RETURN || key == CL) {
        return EXECUTE;
      }
      if ((key == 0 || key == 224) && a()->localIO()->KeyPressed()) {
        // 224 is E0. See https://msdn.microsoft.com/en-us/library/078sfkak(v=vs.110).aspx
        return a()->localIO()->GetChar() + 256;
      }
      else {
        if (numlock_mode == numlock_status_t::NOTNUMBERS) {
          auto ret = GetNumPadCommand(key);
          if (ret) return ret;
        }
        switch (key) {
        case TAB: return TAB;
        case ESC: return GET_OUT;
        default: return key;
        }
      }
      return key;
    }
    else if (bkbhitraw()) {
      int key = pd_getkey();

      if (key == CBACKSPACE) {
        return COMMAND_DELETE;
      }
      if (key == CV) {
        return COMMAND_INSERT;
      }
      if (key == RETURN || key == CL) {
        return EXECUTE;
      }
      else if (key == ESC) {
        time_t esc_time1 = time(nullptr);
        time_t esc_time2 = time(nullptr);
        do {
          esc_time2 = time(nullptr);
          if (bkbhitraw()) {
            key = pd_getkey();
            if (key == OB || key == O) {
              key = pd_getkey();

              // Check for a second set of brackets
              if (key == OB || key == O) {
                key = pd_getkey();
              }
              return GetCommandForAnsiKey(key);
            }
            else {
              return GET_OUT;
            }
          }
        } while (difftime(esc_time2, esc_time1) < 1);

        if (difftime(esc_time2, esc_time1) >= 1) {     // if no keys followed ESC
          return GET_OUT;
        }
        return key;
      }
      else {
        if (!key) {
          if (a()->localIO()->KeyPressed()) {
            key = a()->localIO()->GetChar();
            return (key + 256);
          }
        }
        if (numlock_mode == numlock_status_t::NOTNUMBERS) {
          auto ret = GetNumPadCommand(key);
          if (ret) return ret;
        }
      }
      return key;
    }
  }
  return 0;                                 // must have hung up
}
