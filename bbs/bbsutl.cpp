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
#include "bbs/bbsutl.h"

#include "bbs/bbs.h"
#include "bbs/interpret.h"
#include "bbs/utility.h"
#include "common/datetime.h"
#include "common/input.h"
#include "common/output.h"
#include "core/stl.h"
#include "core/strings.h"
#include "local_io/keycodes.h"
#include "sdk/config.h"
#include "sdk/user.h"
#include <chrono>
#include <string>

using std::chrono::seconds;
using std::chrono::steady_clock;
using namespace wwiv::common;
using namespace wwiv::local::io;
using namespace wwiv::strings;
using namespace wwiv::stl;


bool inli(std::string* outBuffer, std::string* rollover, std::string::size_type maxlen, bool add_crlf,
          bool allow_previous, bool two_color, bool clear_previous_line) {
  char buffer[4096] = {0}, rollover_buffer[4096] = {0};
  to_char_array(buffer, *outBuffer);
  if (rollover) {
    to_char_array(rollover_buffer, *rollover);
  } else {
    memset(rollover_buffer, 0, sizeof(rollover_buffer));
  }
  bool ret = inli(buffer, rollover_buffer, maxlen, add_crlf, allow_previous, two_color, clear_previous_line);
  outBuffer->assign(buffer);
  if (rollover) {
    rollover->assign(rollover_buffer);
  }
  return ret;
}

// returns true if needs to keep inputting this line
bool inli(char *buffer, char *rollover, std::string::size_type nMaxLen, bool add_crlf, bool allow_previous,
          bool two_color, bool clear_previous_line) {
  char rollover_buffer[255];

  CHECK_NOTNULL(buffer);
  CHECK_NOTNULL(rollover);
  if (buffer == nullptr || rollover == nullptr) {
    // Should never happen from previous CHECKs but makes the static
    // analysis happy in MSVC.
    return false;
  }

  const auto cm = a()->sess().chatting();

  const auto begx = bout.wherex();
  if (rollover[0] != 0) {
    char* ss = rollover_buffer;
    for (int i = 0; rollover[i]; i++) {
      if (rollover[i] == CC || rollover[i] == CO) {
        *ss++ = 'P' - '@';
      } else {
        *ss++ = rollover[i];
      }
      // Add a second ^P if we are working on RIP codes
      if (rollover[i] == CO && rollover[i + 1] != CO && rollover[i - 1] != CO) {
        *ss++ = 'P' - '@';
      }
    }
    *ss = '\0';
    if (bin.charbufferpointer_) {
      char szTempBuffer[255];
      strcpy(szTempBuffer, rollover_buffer);
      strcat(szTempBuffer, &bin.charbuffer[bin.charbufferpointer_]);
      strcpy(&bin.charbuffer[1], szTempBuffer);
      bin.charbufferpointer_ = 1;
    } else {
      strcpy(&bin.charbuffer[1], rollover_buffer);
      bin.charbufferpointer_ = 1;
    }
    rollover[0] = '\0';
  }
  std::string::size_type cp = 0;
  auto done = false;
  unsigned char ch;
  do {
    ch = bin.getkey();
    if (two_color) {
      bout.Color(bin.IsLastKeyLocal() ? 1 : 0);
    }
    if (cm != chatting_t::none && a()->sess().chatting() == chatting_t::none) {
        ch = RETURN;
    }
    if (ch >= SPACE) {
      if (bout.wherex() < (a()->user()->screen_width() - 1) && cp < nMaxLen) {
        buffer[cp++] = ch;
        bout.bputch(ch);
        if (bout.wherex() == (a()->user()->screen_width() - 1)) {
          done = true;
        }
      } else {
        if (bout.wherex() >= (a()->user()->screen_width() - 1)) {
          done = true;
        }
      }
    } else switch (ch) {
      case CG:
        break;
      case RETURN:                            // C/R
        buffer[cp] = '\0';
        done = true;
        break;
      case BACKSPACE:                             // Backspace
        if (cp) {
          if (buffer[cp - 2] == CC) {
            cp -= 2;
            bout.Color(0);
          } else {
            if (buffer[cp - 1] == BACKSPACE) {
              cp--;
              bout.bputch(SPACE);
            } else {
              cp--;
              bout.bs();
            }
          }
        } else if (allow_previous) {
          if (okansi()) {
            if (clear_previous_line) {
              bout.clear_whole_line();
            }
            bout << "\x1b[1A";
          } else {
            bout << "[*> Previous Line <*]\r\n";
          }
          return true;
        }
        break;
      case CX:                            // Ctrl-X
        while (bout.wherex() > begx) {
          bout.bs();
          cp = 0;
        }
        bout.Color(0);
        break;
      case CW:                            // Ctrl-W
        if (cp) {
          do {
            if (buffer[cp - 2] == CC) {
              cp -= 2;
              bout.Color(0);
            } else if (buffer[cp - 1] == BACKSPACE) {
              cp--;
              bout.bputch(SPACE);
            } else {
              cp--;
              bout.bs();
            }
          } while (cp && buffer[cp - 1] != SPACE && buffer[cp - 1] != BACKSPACE);
        }
        break;
      case CN:                            // Ctrl-N
        if (bout.wherex() && cp < nMaxLen) {
          bout.bputch(BACKSPACE);
          buffer[cp++] = BACKSPACE;
        }
        break;
      case CP:                            // Ctrl-P
        if (cp < nMaxLen - 1) {
          ch = bin.getkey();
          if (ch >= SPACE && ch <= 126) {
            buffer[cp++] = CC;
            buffer[cp++] = ch;
            bout.Color(ch - '0');
          } else if (ch == CP && cp < nMaxLen - 2) {
            ch = bin.getkey();
            if (ch != CP) {
              buffer[cp++] = CO;
              buffer[cp++] = CO;
              buffer[cp++] = ch;
              bout.bputch('\xf');
              bout.bputch('\xf');
              bout.bputch(ch);
            }
          }
        }
        break;
      case TAB: {                           // Tab
        int charsNeeded = 5 - (cp % 5);
        if ((cp + charsNeeded) < nMaxLen
            && (bout.wherex() + charsNeeded) < a()->user()->screen_width()) {
          charsNeeded = 5 - ((bout.wherex() + 1) % 5);
          for (auto j = 0; j < charsNeeded; j++) {
            buffer[cp++] = SPACE;
            bout.bputch(SPACE);
          }
        }
      }
      break;
      }
  } while (!done && !a()->sess().hangup());

  if (a()->sess().hangup()) {
    // Caller isn't here, so we are not saving any message.
    return false;
  }

  if (ch != RETURN) {
    auto lastwordstart = cp - 1;
    while (lastwordstart > 0 && buffer[lastwordstart] != SPACE && buffer[lastwordstart] != BACKSPACE) {
      lastwordstart--;
    }
    if (lastwordstart > static_cast<std::string::size_type>(bout.wherex() / 2)
        && lastwordstart != (cp - 1)) {
      const auto lastwordlen = cp - lastwordstart - 1;
      for (std::string::size_type j = 0; j < lastwordlen; j++) {
        bout.bputch(BACKSPACE);
      }
      for (std::string::size_type j = 0; j < lastwordlen; j++) {
        bout.bputch(SPACE);
      }
      for (std::string::size_type j = 0; j < lastwordlen; j++) {
        rollover[j] = buffer[cp - lastwordlen + j];
      }
      rollover[lastwordlen] = '\0';
      cp -= lastwordlen;
    }
    buffer[cp++] = CA;
    buffer[cp] = '\0';
  }
  if (add_crlf) {
    bout.nl();
  }
  return false;
}


// Returns 1 if current user has sysop access (permanent or temporary), else
// returns 0.
bool so() {
  return a()->sess().effective_sl() == 255;
}

/**
 * Checks for Co-SysOp status
 * @return true if current user has limited co-sysop access (or better)
 */
bool cs() {
  if (so()) {
    return true;
  }

  return (a()->config()->sl(a()->sess().effective_sl()).ability & ability_cosysop) ? true : false;
}


/**
 * Checks for limited Co-SysOp status
 * <em>Note: Limited co sysop status may be for this message area only.</em>
 *
 * @return true if current user has limited co-sysop access (or better)
 */
bool lcs() {
  if (cs()) {
    return true;
  }

  if (a()->config()->sl(a()->sess().effective_sl()).ability & ability_limited_cosysop) {
    if (*a()->sess().qsc == 999) {
      return true;
    }
    return (*a()->sess().qsc == static_cast<uint32_t>(a()->current_user_sub().subnum)) ? true : false;
  }
  return false;
}


// Returns 1 if sysop is "chatable", else returns 0. Takes into account
// current user's chat restriction (if any) and sysop high and low times,
// if any, as well as status of scroll-lock key.
bool sysop2() {
  if (!sysop1()) {
    return false;
  }
  if (a()->user()->restrict_chat()) {
    return false;
  }
  if (a()->config()->sysop_low_time() != a()->config()->sysop_high_time()) {
    const auto m = minutes_since_midnight();
    if (a()->config()->sysop_high_time() > a()->config()->sysop_low_time()) {
      if (m <= a()->config()->sysop_low_time() || m >= a()->config()->sysop_high_time()) {
        return false;
      }
    } else if (m <= a()->config()->sysop_low_time() && m >= a()->config()->sysop_high_time()) {
      return false;
    }
  }
  return true;
}

// Returns 1 if ANSI detected, or if local user, else returns 0. Uses the
// cursor position interrogation ANSI sequence for remote detection.
int check_ansi() {
  if (!a()->sess().incom()) {
    return 1;
  }

  if (const auto o = bin.screen_size()) {
    const auto& ss = o.value();
    VLOG(1) << "Screen size: x: " << ss.x << "; y: " << ss.y;
  } else {
    LOG(WARNING) << "Unable to get screen size from terminal.";
  }

  if (const auto pos = bin.remoteIO()->screen_position()) {
    return 1;
  }

  return 0;
}

std::string YesNoString(bool bYesNo) {
  return bYesNo ? "Yes" : "No";
}

void *BbsAllocA(size_t size) {
  auto* p = calloc(size + 1, 1);
  CHECK_NOTNULL(p);
  return p;
}
