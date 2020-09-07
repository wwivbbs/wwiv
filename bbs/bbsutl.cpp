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
#include "bbs/bbsutl.h"

#include "bbs/bbs.h"
#include "bbs/interpret.h"
#include "bbs/utility.h"
#include "common/bgetch.h"
#include "common/com.h"
#include "common/datetime.h"
#include "common/input.h"
#include "common/pause.h"
#include "core/stl.h"
#include "core/strings.h"
#include "local_io/keycodes.h"
#include "sdk/config.h"
#include "sdk/user.h"
#include <chrono>
#include <string>

using std::string;
using std::chrono::seconds;
using std::chrono::steady_clock;
using namespace wwiv::bbs;
using namespace wwiv::common;
using namespace wwiv::strings;
using namespace wwiv::stl;

static char str_yes[81],
            str_no[81];
char  str_pause[81],
      str_quit[81];

bool inli(string* outBuffer, string* rollover, string::size_type maxlen, bool add_crlf,
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
bool inli(char *buffer, char *rollover, string::size_type nMaxLen, bool add_crlf, bool allow_previous,
          bool two_color, bool clear_previous_line) {
  char rollover_buffer[255];

  CHECK_NOTNULL(buffer);
  CHECK_NOTNULL(rollover);
  if (buffer == nullptr || rollover == nullptr) {
    // Should never happen from previous CHECKs but makes the static
    // analysis happy in MSVC.
    return false;
  }

  auto cm = a()->sess().chatting();

  auto begx = bout.wherex();
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
  string::size_type cp = 0;
  bool done = false;
  unsigned char ch = '\0';
  do {
    ch = bin.getkey();
    if (two_color) {
      bout.Color(bin.IsLastKeyLocal() ? 1 : 0);
    }
    if (cm != chatting_t::none && a()->sess().chatting() == chatting_t::none) {
        ch = RETURN;
    }
    if (ch >= SPACE) {
      if ((bout.wherex() < (a()->user()->GetScreenChars() - 1)) && (cp < nMaxLen)) {
        buffer[cp++] = ch;
        bout.bputch(ch);
        if (bout.wherex() == (a()->user()->GetScreenChars() - 1)) {
          done = true;
        }
      } else {
        if (bout.wherex() >= (a()->user()->GetScreenChars() - 1)) {
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
          } else if (buffer[cp - 2] == CO) {
            MacroContext ctx(&a()->context());
            const auto interpreted = ctx.interpret(buffer[cp - 1]);
            for (auto i = interpreted.size(); i > 0; i--) {
              bout.bs();
            }
            cp -= 2;
            if (buffer[cp - 1] == CO) {
              cp--;
            }
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
            && (bout.wherex() + charsNeeded) < a()->user()->GetScreenChars()) {
          charsNeeded = 5 - ((bout.wherex() + 1) % 5);
          for (int j = 0; j < charsNeeded; j++) {
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
    string::size_type lastwordstart = cp - 1;
    while (lastwordstart > 0 && buffer[lastwordstart] != SPACE && buffer[lastwordstart] != BACKSPACE) {
      lastwordstart--;
    }
    if (lastwordstart > static_cast<string::size_type>(bout.wherex() / 2)
        && lastwordstart != (cp - 1)) {
      string::size_type lastwordlen = cp - lastwordstart - 1;
      for (string::size_type j = 0; j < lastwordlen; j++) {
        bout.bputch(BACKSPACE);
      }
      for (string::size_type j = 0; j < lastwordlen; j++) {
        bout.bputch(SPACE);
      }
      for (string::size_type j = 0; j < lastwordlen; j++) {
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
  return (a()->effective_sl() == 255);
}

/**
 * Checks for Co-SysOp status
 * @return true if current user has limited co-sysop access (or better)
 */
bool cs() {
  if (so()) {
    return true;
  }

  return (a()->effective_slrec().ability & ability_cosysop) ? true : false;
}


/**
 * Checks for limitied Co-SysOp status
 * <em>Note: Limited co sysop status may be for this message area only.</em>
 *
 * @return true if current user has limited co-sysop access (or better)
 */
bool lcs() {
  if (cs()) {
    return true;
  }

  if (a()->effective_slrec().ability & ability_limited_cosysop) {
    if (*a()->sess().qsc == 999) {
      return true;
    }
    return (*a()->sess().qsc == static_cast<uint32_t>(a()->current_user_sub().subnum)) ? true : false;
  }
  return false;
}

/**
 * Checks to see if user aborted whatever he/she was doing.
 * Sets abort to true if control-C/X or Q was pressed.
 * returns the value of abort
 */
bool checka() {
  bool ignored_abort = false;
  bool ignored_next = false;
  return checka(&ignored_abort, &ignored_next);
}

/**
 * Checks to see if user aborted whatever he/she was doing.
 * Sets abort to true if control-C/X or Q was pressed.
 * returns the value of abort
 */
bool checka(bool *abort) {
  bool ignored_next = false;
  return checka(abort, &ignored_next);
}

/**
 * Checks to see if user aborted whatever he/she was doing.
 * Sets next to true if control-N was hit, for zipping past messages quickly.
 * Sets abort to true if control-C/X or Q was pressed.
 * returns the value of abort
 */
bool checka(bool *abort, bool *next) {
  if (bin.nsp() == -1) {
    *abort = true;
    bin.clearnsp();
  }
  while (bin.bkbhit() && !*abort && !a()->sess().hangup()) {
    a()->CheckForHangup();
    char ch = bin.bgetch();
    switch (ch) {
    case CN:
      bout.clear_lines_listed();
      *next = true;
    case CC:
    case SPACE:
    case CX:
    case 'Q':
    case 'q':
      bout.clear_lines_listed();
      *abort = true;
      break;
    case 'P':
    case 'p':
    case CS:
      bout.clear_lines_listed();
      ch = bin.getkey();
      break;
    }
  }
  return *abort;
}

// Returns 1 if sysop is "chattable", else returns 0. Takes into account
// current user's chat restriction (if any) and sysop high and low times,
// if any, as well as status of scroll-lock key.
bool sysop2() {
  if (!sysop1()) {
    return false;
  }
  if (a()->user()->IsRestrictionChat()) {
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
// If the user is asked and choosed NO, then -1 is returned.
int check_ansi() {
  if (!a()->sess().incom()) {
    return 1;
  }

  while (bin.bkbhitraw()) {
    bin.bgetchraw();
  }

  bout.rputs("\x1b[6n");
  const auto now = steady_clock::now();
  auto l = now + seconds(3);

  while (steady_clock::now() < l) {
    a()->CheckForHangup();
    auto ch = bin.bgetchraw();
    if (ch == '\x1b') {
      l = steady_clock::now() + seconds(1);
      while (steady_clock::now() < l) {
        a()->CheckForHangup();
        ch = bin.bgetchraw();
        if (ch) {
          if ((ch < '0' || ch > '9') && ch != ';' && ch != '[') {
            return 1;
          }
        }
      }
      return 1;
    } else if (ch == 'N') {
      return -1;
    }
  }
  return 0;
}


// Sets current language to language index n, reads in menus and help files,
// and initializes stringfiles for that language. Returns false if problem,
// else returns true.
bool set_language_1(int n) {
  size_t idx = 0;
  for (idx = 0; idx < a()->languages.size(); idx++) {
    if (a()->languages[idx].num == n) {
      break;
    }
  }

  if (idx >= a()->languages.size() && n == 0) {
    idx = 0;
  }

  if (idx >= a()->languages.size()) {
    return false;
  }

  a()->set_language_number(n);

  to_char_array(str_yes, "Yes");
  to_char_array(str_no, "No");
  to_char_array(str_quit, "Quit");
  to_char_array(str_pause, "More? [Y/n/c]");
  str_yes[0] = upcase(str_yes[0]);
  str_no[0] = upcase(str_no[0]);
  str_quit[0] = upcase(str_quit[0]);

  return true;
}

// Sets language to language #n, returns false if a problem, else returns true.
bool set_language(int n) {
  if (a()->language_number() == n) {
    return true;
  }

  int old_curlang = a()->language_number();

  if (!set_language_1(n)) {
    if (old_curlang >= 0) {
      if (!set_language_1(old_curlang)) {
        set_language_1(0);
      }
    }
    return false;
  }
  return true;
}

std::string YesNoString(bool bYesNo) {
  return bYesNo ? str_yes : str_no;
}

void *BbsAllocA(size_t size) {
  void* p = calloc(size + 1, 1);
  CHECK_NOTNULL(p);
  return p;
}
