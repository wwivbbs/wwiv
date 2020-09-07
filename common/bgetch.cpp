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
#include "common/bgetch.h"

#include "bbs/bbsutl.h"
#include "common/com.h"
#include "common/common_events.h"
#include "common/context.h"
#include "common/datetime.h"
#include "common/input.h"
#include "common/output.h"
#include "core/eventbus.h"
#include "core/log.h"
#include "core/strings.h"
#include "local_io/keycodes.h"
#include "local_io/wconstants.h"
#include <chrono>
#include <cmath>
#include <string>

using std::chrono::minutes;
using std::chrono::seconds;
using std::chrono::steady_clock;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

static steady_clock::time_point time_lastchar_pressed;
static void lastchar_pressed() { time_lastchar_pressed = steady_clock::now(); }

void Output::RedrawCurrentLine() {
  SavedLine line = bout.SaveCurrentLine();
  bout.nl();
  bout.RestoreCurrentLine(line);
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

SavedLine Output::SaveCurrentLine() { return {current_line_, curatr()}; }

void Output::dump() {
  if (sess().ok_modem_stuff()) {
    remoteIO()->purgeIn();
  }
}

int Output::wherex() {
  int x = localIO()->WhereX();
  if (x != x_) {
    VLOG(1) << "x: " << x << " != x_: " << x_;
  }
  return x_;
}

std::chrono::duration<double> Input::key_timeout() const {
  if (so()) {
    return sysop_key_timeout_;
  }
  return non_sysop_key_timeout_;
}

/* This function returns one character from either the local keyboard or
 * remote com port (if applicable).  After 1.5 minutes of inactivity, a
 * beep is sounded.  After 3 minutes of inactivity, the user is hung up.
 */
char Input::getkey(bool allow_extended_input) {
  bout.resetnsp();
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
    wwiv::core::bus().invoke<CheckForHangupEvent>();
    while (!bin.bkbhit() && !sess().hangup()) {
      // Try to make hangups happen faster.
      if (sess().incom() && sess().ok_modem_stuff() && !remoteIO()->connected()) {
        wwiv::core::bus().invoke<HangupEvent>();
      }
      wwiv::core::bus().invoke<CheckForHangupEvent>();
      wwiv::core::bus().invoke<ProcessInstanceMessages>();
      wwiv::core::bus().invoke<GiveupTimeslices>();
      
      auto dd = steady_clock::now();
      auto diff = dd - time_lastchar_pressed;
      if (diff > tv1 && !beepyet) {
        beepyet = true;
        bout.bputch(CG);
      }
      if (diff > tv) {
        bout.nl();
        bout << "Call back later when you are there.\r\n";
        wwiv::core::bus().invoke<HangupEvent>();
      }
    }
    ch = bin.bgetch(allow_extended_input);
  } while (!ch);
  return ch;
}

void Output::reset() {
  newline = true;
  ansi_->reset();
  curatr(0x07);
  clear_lines_listed();
}

namespace wwiv::common {


static void HandleControlKey(char* ch, const SessionContext& context, wwiv::sdk::User& user) {
  char c = *ch;

  if (c == CBACKSPACE) {
    c = BACKSPACE;
  }
  if (bout.okskey()) {
    switch (c) {
    case CA: // CTRL-A
    case CD: // CTRL-D
    case CF: // CTRL-F
      if (context.okmacro() && !bout.charbufferpointer_) {
        static constexpr int MACRO_KEY_TABLE[] = {0, 2, 0, 0, 0, 0, 1};
        auto macroNum = MACRO_KEY_TABLE[(int)c];
        to_char_array(bout.charbuffer, user.GetMacro(macroNum));
        c = bout.charbuffer[0];
        if (c) {
          bout.charbufferpointer_ = 1;
        }
      }
      break;
    case CT: // CTRL - T
      bus().invoke<DisplayTimeLeft>();
      break;
    case CU: { // CTRL-U
      SavedLine line = bout.SaveCurrentLine();
      bout.Color(0);
      bout.nl(2);
      bus().invoke<DisplayMultiInstanceStatus>();
      bout.nl();
      bout.RestoreCurrentLine(line);
    } break;
    case 18: // CR
      bout.RedrawCurrentLine();
      break;
    case CL: // CTRL - L
      if (so()) {
        bus().invoke<ToggleInvisble>();
      }
      break;
    case CN: // CTRL - N
      bus().invoke<ToggleAvailable>();
      break;
    case CY:
      user.ToggleStatusFlag(User::pauseOnPage);
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
char Input::bgetch(bool allow_extended_input) {
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
  if (localIO()->KeyPressed()) {
    ch = localIO()->GetChar();
    bin.SetLastKeyLocal(true);
    if (!allow_extended_input) {
      if (!ch) {
        ch = localIO()->GetChar();
        bus().invoke<HandleSysopKey>(HandleSysopKey{static_cast<uint8_t>(ch)});
        ch = static_cast<char>(((ch == F10) || (ch == CF10)) ? 2 : 0);
      }
    }
    lastchar_pressed();
  } else if (sess().incom() && bkbhitraw()) {
    ch = bgetchraw();
    bin.SetLastKeyLocal(false);
  }

  if (!allow_extended_input) {
    HandleControlKey(&ch, sess(), user());
  }

  return ch;
}

char Input::bgetchraw() {
  if (sess().ok_modem_stuff() && nullptr != remoteIO()) {
    if (remoteIO()->incoming()) {
      return static_cast<char>(remoteIO()->getW());
    }
    if (localIO()->KeyPressed()) {
      return static_cast<char>(localIO()->GetChar());
    }
  }
  return 0;
}

bool Input::bkbhitraw() {
  if (sess().ok_modem_stuff()) {
    return (remoteIO()->incoming() || localIO()->KeyPressed());
  } else if (localIO()->KeyPressed()) {
    return true;
  }
  return false;
}

bool Input::bkbhit() {
  if (localIO()->KeyPressed() || (sess().incom() && bkbhitraw()) ||
      (bout.charbufferpointer_ && bout.charbuffer[bout.charbufferpointer_])) {
    return true;
  }
  return false;
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

int Input::bgetch_event(numlock_status_t numlock_mode) {
  return bgetch_event(numlock_mode, [](bgetch_timeout_status_t, int) {});
}

int Input::bgetch_handle_escape(int key) {
  time_t esc_time1 = time(nullptr);
  time_t esc_time2 = time(nullptr);
  do {
    esc_time2 = time(nullptr);
    if (bkbhitraw()) {
      key = static_cast<int>(bin.getkey(true));
      if (key == OB || key == O) {
        key = static_cast<int>(bin.getkey(true));

        // Check for a second set of brackets
        if (key == OB || key == O) {
          key = static_cast<int>(bin.getkey(true));
        }
        return get_command_for_ansi_key(key);
      } else {
        return GET_OUT;
      }
    }
  } while (difftime(esc_time2, esc_time1) < 1);

  if (difftime(esc_time2, esc_time1) >= 1) { // if no keys followed ESC
    return GET_OUT;
  }
  return key;
}

int Input::bgetch_handle_key_translation(int key, numlock_status_t numlock_mode) {
  if (key == CBACKSPACE) {
    return COMMAND_DELETE;
  }
  if (key == CV) {
    return COMMAND_INSERT;
  }
  if (key == RETURN || key == CL) {
    return EXECUTE;
  }
  if ((key == 0 || key == 224) && localIO()->KeyPressed()) {
    // 224 is E0. See https://msdn.microsoft.com/en-us/library/078sfkak(v=vs.110).aspx
    return localIO()->GetChar() + 256;
  }
  if (numlock_mode == numlock_status_t::NOTNUMBERS) {
    auto ret = get_numpad_command(key);
    if (ret)
      return ret;
  }
  return key;
}

int Input::bgetch_event(numlock_status_t numlock_mode, bgetch_callback_fn cb) {
  return bgetch_event(numlock_mode, std::chrono::hours(24), cb);
}

int Input::bgetch_event(numlock_status_t numlock_mode, std::chrono::duration<double> idle_time,
                 bgetch_callback_fn cb) {
  bus().invoke<UpdateTimeLeft>(UpdateTimeLeft{true});
  bout.resetnsp();
  lastchar_pressed();
  bout.clear_lines_listed();


  auto beepyet{false};
  auto tv = bin.key_timeout();
  auto tv1 = tv - std::chrono::minutes(1);
  bool once = true;

  while (true) {
    wwiv::core::bus().invoke<CheckForHangupEvent>();
    if (sess().hangup()) {
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
      wwiv::core::bus().invoke<HangupEvent>();
      return 0;
    }
    if (once && diff > idle_time) {
      once = false;
      cb(bgetch_timeout_status_t::IDLE, 0);
    }

    if (!bkbhitraw() && !localIO()->KeyPressed()) {
      wwiv::core::bus().invoke<GiveupTimeslices>();
      continue;
    }
    if (beepyet) {
      cb(bgetch_timeout_status_t::CLEAR, 0);
    }

    if (!sess().incom() || localIO()->KeyPressed()) {
      // Check for local keys
      return bgetch_handle_key_translation(localIO()->GetChar(), numlock_mode);
    }
    if (bkbhitraw()) {
      auto key = static_cast<int>(bin.getkey(true));
      return (key == ESC) ? bgetch_handle_escape(key)
               : bgetch_handle_key_translation(key, numlock_mode);
    }
  }
  return 0; // must have hung up
}


} // namespace wwiv::common
