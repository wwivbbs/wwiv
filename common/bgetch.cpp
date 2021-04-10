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
#include "common/common_events.h"
#include "common/context.h"
#include "common/input.h"
#include "common/output.h"
#include "core/eventbus.h"
#include "core/log.h"
#include "core/strings.h"
#include "local_io/keycodes.h"
#include "local_io/local_io.h"
#include "sdk/ansi/ansi.h"

#include <chrono>

using std::chrono::minutes;
using std::chrono::seconds;
using std::chrono::steady_clock;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::local::io;
using namespace wwiv::sdk;
using namespace wwiv::strings;

static steady_clock::time_point time_lastchar_pressed;
static void lastchar_pressed() { time_lastchar_pressed = steady_clock::now(); }

static bool so(const SessionContext& sess) { return (sess.effective_sl() == 255); }

void Output::RedrawCurrentLine() {
  const auto line = SaveCurrentLine();
  nl();
  RestoreCurrentLine(line);
}


bool Output::RestoreCurrentLine(const SavedLine& line) {
  if (wherex()) {
    nl();
  }
  for (const auto& c : line.line) {
    SystemColor(c.second);
    bputch(c.first, true);
  }
  flush();
  SystemColor(line.color);

  return true;
}

SavedLine Output::SaveCurrentLine() const { return {current_line_, curatr()}; }

void Output::dump() {
  if (sess().ok_modem_stuff()) {
    remoteIO()->purgeIn();
  }
}

int Output::wherex() const {
  const auto x = localIO()->WhereX();
  if (x != x_) {
    VLOG(1) << "x: " << x << " != x_: " << x_;
  }
  return x_;
}

std::chrono::duration<double> Input::key_timeout() const {
  if (so(sess())) {
    return sysop_key_timeout_;
  }
  return non_sysop_key_timeout_;
}

/* This function returns one character from either the local keyboard or
 * remote com port (if applicable).  After 1.5 minutes of inactivity, a
 * beep is sounded.  After 3 minutes of inactivity, the user is hung up.
 */
char Input::getkey(bool allow_extended_input) {
  resetnsp();
  auto beepyet = false;
  lastchar_pressed();

  const auto tv = key_timeout();
  auto tv1 = tv - minutes(1);
  if (tv1 < seconds(10)) {
    tv1 = seconds(10);
  }

  // Since were waiting for a key, reset the # of lines we've displayed since a pause.
  bout.clear_lines_listed();
  while (!sess().hangup()) {
    bus().invoke<CheckForHangupEvent>();
    while (!bkbhit() && !sess().hangup()) {
      // Try to make hangups happen faster.
      if (sess().incom() && sess().ok_modem_stuff() && !remoteIO()->connected()) {
        bus().invoke<HangupEvent>(HangupEvent{hangup_type_t::user_disconnected});
      }
      bus().invoke<CheckForHangupEvent>();
      bus().invoke<ProcessInstanceMessages>();
      bus().invoke<GiveupTimeslices>();
      
      auto dd = steady_clock::now();
      auto diff = dd - time_lastchar_pressed;
      if (diff > tv1 && !beepyet) {
        beepyet = true;
        bout.bputch(CG);
      }
      if (diff > tv) {
        bout.nl();
        bout << "Call back later when you are there.\r\n";
        bus().invoke<HangupEvent>();
      }
    }
    if (const auto ch = bgetch(allow_extended_input)) {
      return ch;
    }
  }
  DLOG(FATAL) << "getkey: Should not happen";
  return 0;
}

void Output::reset() {
  newline = true;
  ansi_->reset();
  curatr(0x07);
  clear_lines_listed();
}


namespace wwiv::common {


static char HandleControlKey(const char c, const SessionContext& context, wwiv::sdk::User& user) {
  if (c == CBACKSPACE) {
    return BACKSPACE;
  }
  if (!bin.okskey()) {
    return c;
  }
  switch (c) {
  case CA: // CTRL-A
  case CD: // CTRL-D
  case CF: // CTRL-F
    if (context.okmacro() && !bin.charbufferpointer_) {
      static constexpr int MACRO_KEY_TABLE[] = {0, 2, 0, 0, 0, 0, 1};
      const auto macroNum = MACRO_KEY_TABLE[static_cast<int>(c)];
      to_char_array(bin.charbuffer, user.macro(macroNum));
      const auto nextchar = bin.charbuffer[0];
      if (nextchar) {
        bin.charbufferpointer_ = 1;
      }
      return nextchar;
    }
    break;
  case CT: // CTRL - T
    bus().invoke<DisplayTimeLeft>();
    break;
  case CU: { // CTRL-U
    const auto line = bout.SaveCurrentLine();
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
    if (so(context)) {
      bus().invoke<ToggleInvisble>();
    }
    break;
  case CN: // CTRL - N
    bus().invoke<ToggleAvailable>();
    break;
  case CY:
    user.toggle_flag(User::pauseOnPage);
    break;
  default:
    return c;
  }
  // Nothing to do here.
  return 0;
}

/* This function checks both the local keyboard, and the remote terminal
 * (if any) for input.  If there is input, the key is returned.  If there
 * is no input, a zero is returned.  Function keys hit are interpreted as
 * such within the routine and not returned.
 */
char Input::bgetch(bool allow_extended_input) {
  char ch = 0;

  if (charbufferpointer_) {
    if (!charbuffer[charbufferpointer_]) {
      charbufferpointer_ = 0;
      charbuffer[0] = 0;
    } else {
      if (charbuffer[charbufferpointer_] == CC) {
        charbuffer[charbufferpointer_] = CP;
      }
      return charbuffer[charbufferpointer_++];
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
    return HandleControlKey(ch, sess(), user());
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
    return remoteIO()->incoming() || localIO()->KeyPressed();
  }
  return localIO()->KeyPressed();
}

bool Input::bkbhit() {
  if (localIO()->KeyPressed() || (sess().incom() && bkbhitraw()) ||
      (charbufferpointer_ && charbuffer[charbufferpointer_])) {
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
  const auto esc_time1 = time(nullptr);
  auto esc_time2 = time(nullptr);
  do {
    esc_time2 = time(nullptr);
    if (bkbhitraw()) {
      key = static_cast<int>(getkey(true));
      if (key == OB || key == O) {
        key = static_cast<int>(getkey(true));

        // Check for a second set of brackets
        if (key == OB || key == O) {
          key = static_cast<int>(getkey(true));
        }
        return get_command_for_ansi_key(key);
      }
      return GET_OUT;
    }
  } while (difftime(esc_time2, esc_time1) < 1);

  if (difftime(esc_time2, esc_time1) >= 1) { // if no keys followed ESC
    return GET_OUT;
  }
  return key;
}

int Input::bgetch_handle_key_translation(int key, numlock_status_t numlock_mode) {
  if (key == CBACKSPACE) {
    return BACKSPACE;
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
    if (const auto ret = get_numpad_command(key); ret)
      return ret;
  }
  if (key < 127) {
    return HandleControlKey(static_cast<const char>(key), sess(), user());
  }
  return key;
}

int Input::bgetch_event(numlock_status_t numlock_mode, bgetch_callback_fn cb) {
  return bgetch_event(numlock_mode, std::chrono::hours(24), cb);
}

int Input::bgetch_event(numlock_status_t numlock_mode, std::chrono::duration<double> idle_time,
                 bgetch_callback_fn cb) {
  bus().invoke<UpdateTimeLeft>(UpdateTimeLeft{true});
  resetnsp();
  lastchar_pressed();
  bout.clear_lines_listed();


  auto beepyet{false};
  const auto tv = key_timeout();
  const auto tv1 = tv - std::chrono::minutes(1);
  bool once = true;

  while (true) {
    bus().invoke<CheckForHangupEvent>();
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
      bus().invoke<HangupEvent>();
      return 0;
    }
    if (once && diff > idle_time) {
      once = false;
      cb(bgetch_timeout_status_t::IDLE, 0);
    }

    if (!bkbhitraw() && !localIO()->KeyPressed()) {
      bus().invoke<GiveupTimeslices>();
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
      const auto key = static_cast<int>(getkey(true));
      return key == ESC ? bgetch_handle_escape(key)
               : bgetch_handle_key_translation(key, numlock_mode);
    }
  }
}

} // namespace wwiv::common
