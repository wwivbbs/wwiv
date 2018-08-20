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
#include "local_io/keycodes.h"
#include "bbs/multinst.h"
#include "bbs/utility.h"
#include "local_io/wconstants.h"

#include "core/log.h"
#include "core/strings.h"

using std::chrono::minutes;
using std::chrono::seconds;
using std::chrono::steady_clock;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

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
  auto dt = DateTime::now();
  bout << "|#2" << dt.to_string() << wwiv::endl;
  if (a()->IsUserOnline()) {
    auto time_on = std::chrono::system_clock::now() - a()->system_logon_time();
    auto seconds_on = static_cast<long>(std::chrono::duration_cast<std::chrono::seconds>(time_on).count());
    bout << "|#9Time on   = |#1" << ctim(seconds_on) << wwiv::endl;
    bout << "|#9Time left = |#1" << ctim(nsl()) << wwiv::endl;
  }
  bout.nl();
  bout.RestoreCurrentLine(line);
}

void Output::RedrawCurrentLine() {
  SavedLine line = bout.SaveCurrentLine();
  bout.nl();
  bout.RestoreCurrentLine(line);
}

static void HandleControlKey(char *ch) {
  char c = *ch;

  if (c == CBACKSPACE) {
    c = BACKSPACE;
  }
  if (bout.okskey()) {
    switch (c) {
    case CA:   // CTRL-A
    case CD:   // CTRL-D
    case CF:   // CTRL-F
      if (a()->context().okmacro() && (!bout.charbufferpointer_)) {
        static constexpr int MACRO_KEY_TABLE[] = {0, 2, 0, 0, 0, 0, 1};
        auto macroNum = MACRO_KEY_TABLE[(int)c];
        to_char_array(bout.charbuffer, a()->user()->GetMacro(macroNum));
        c = bout.charbuffer[0];
        if (c) {
          bout.charbufferpointer_ = 1;
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
      bout.RedrawCurrentLine();
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
char bgetch(bool allow_extended_input) {
  char ch = 0;

  if (bout.charbufferpointer_) {
    if (!bout.charbuffer[bout.charbufferpointer_]) {
      bout.charbufferpointer_ = bout.charbuffer[0] = 0;
    } else {
      if ((bout.charbuffer[bout.charbufferpointer_]) == CC) {
        bout.charbuffer[bout.charbufferpointer_] = CP;
      }
      return bout.charbuffer[bout.charbufferpointer_++];
    }
  }
  if (a()->localIO()->KeyPressed()) {
    ch = a()->localIO()->GetChar();
    bout.SetLastKeyLocal(true);
    if (!allow_extended_input) {
      if (!ch) {
        ch = a()->localIO()->GetChar();
        a()->handle_sysop_key(static_cast<uint8_t>(ch));
        ch = static_cast<char>(((ch == F10) || (ch == CF10)) ? 2 : 0);
      }
    }
    lastchar_pressed();
  } else if (a()->context().incom() && bkbhitraw()) {
    ch = bgetchraw();
    bout.SetLastKeyLocal(false);
  }

  if (!allow_extended_input) {
    HandleControlKey(&ch);
  }

  return ch;
}

char bgetchraw() {
  if (a()->context().ok_modem_stuff() && nullptr != a()->remoteIO()) {
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
  if (a()->context().ok_modem_stuff()) {
    return (a()->remoteIO()->incoming() || a()->localIO()->KeyPressed());
  } else if (a()->localIO()->KeyPressed()) {
    return true;
  }
  return false;
}

bool bkbhit() {
  if ((a()->localIO()->KeyPressed() || (a()->context().incom() && bkbhitraw()) ||
       (bout.charbufferpointer_ && bout.charbuffer[bout.charbufferpointer_]))) {
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

  return true;
}

SavedLine Output::SaveCurrentLine() {
  return {current_line_, curatr()};
}

void Output::dump() {
  if (a()->context().ok_modem_stuff()) {
    a()->remoteIO()->purgeIn();
  }
}

int Output::wherex() { 
  int x = localIO()->WhereX();
  if (x != x_) {
    VLOG(1) << "x: " << x << " != x_: " << x_;
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
char Output::getkey(bool allow_extended_input) {
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
    while (!bkbhit() && !a()->hangup_) {
      // Try to make hangups happen faster.
      if (a()->context().incom() && a()->context().ok_modem_stuff() &&
          !a()->remoteIO()->connected()) {
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
    ch = bgetch(allow_extended_input);
  } while (!ch);
  return ch;
}

void Output::reset() {
  newline = true;
  ansi_->reset();
  curatr(0x07);
  clear_lines_listed();
  // Reset the error bit on bout since after a a()->hangup_ it can be set.
  bout.clear();
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

static int get_numpad_command(int key) {
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


static int get_command_for_ansi_key(int key) {
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
    if (a()->hangup_) {
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

    if (!a()->context().incom() || a()->localIO()->KeyPressed()) {
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
          auto ret = get_numpad_command(key);
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
      int key = static_cast<int>(bout.getkey(true));

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
            key = static_cast<int>(bout.getkey(true));
            if (key == OB || key == O) {
              key = static_cast<int>(bout.getkey(true));

              // Check for a second set of brackets
              if (key == OB || key == O) {
                key = static_cast<int>(bout.getkey(true));
              }
              return get_command_for_ansi_key(key);
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
          auto ret = get_numpad_command(key);
          if (ret) return ret;
        }
      }
      return key;
    }
  }
  return 0;                                 // must have hung up
}
