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
#include <algorithm>
#include <cmath>

#include "bbs/bbsovl3.h"
#include "bbs/datetime.h"
#include "bbs/keycodes.h"
#include "bbs/remote_io.h"
#include "bbs/wconstants.h"
#include "bbs/bbs.h"
#include "bbs/fcns.h"
#include "bbs/vars.h"
#include "core/strings.h"
#include "core/wwivassert.h"

using namespace wwiv::sdk;

extern char str_quit[];

void RestoreCurrentLine(const char *cl, const char *atr, const char *xl, const char *cc) {
  if (session()->localIO()->WhereX()) {
    bout.nl();
  }
  for (size_t i = 0; cl[i] != 0; i++) {
    bout.SystemColor(atr[i]);
    bputch(cl[i], true);
  }
  FlushOutComChBuffer();
  bout.SystemColor(*cc);
  strcpy(endofline, xl);
}

void dump() {
  if (ok_modem_stuff) {
    session()->remoteIO()->purgeIn();
  }
}

// This function checks to see if the user logged on to the com port has
// hung up.  Obviously, if no user is logged on remotely, this does nothing.
// returns the value of hangup
bool CheckForHangup() {
  if (!hangup && session()->using_modem && !session()->remoteIO()->carrier()) {
    hangup = true;
    if (session()->IsUserOnline()) {
      sysoplog("Hung Up.");
      std::cerr << "Hung Up!";
    }
  }
  return hangup;
}

static void addto(char *ansi_str, int num) {
  char szBuffer[ 20 ];

  strcat(ansi_str, (ansi_str[0]) ? ";" : "\x1b[");
  snprintf(szBuffer, sizeof(szBuffer), "%d", num);
  strcat(ansi_str, szBuffer);
}

/* Passed to this function is a one-byte attribute as defined for IBM type
* screens.  Returned is a string which, when printed, will change the
* display to the color desired, from the current function.
*/
void makeansi(int attr, char *out_buffer, bool forceit) {
  static const char *temp = "04261537";

  int catr = curatr;
  out_buffer[0] = '\0';
  if (attr != catr) {
    if ((catr & 0x88) ^ (attr & 0x88)) {
      addto(out_buffer, 0);
      addto(out_buffer, 30 + temp[attr & 0x07] - '0');
      addto(out_buffer, 40 + temp[(attr & 0x70) >> 4] - '0');
      catr = (attr & 0x77);
    }
    if ((catr & 0x07) != (attr & 0x07)) {
      addto(out_buffer, 30 + temp[attr & 0x07] - '0');
    }
    if ((catr & 0x70) != (attr & 0x70)) {
      addto(out_buffer, 40 + temp[(attr & 0x70) >> 4] - '0');
    }
    if ((catr & 0x08) ^ (attr & 0x08)) {
      addto(out_buffer, 1);
    }
    if ((catr & 0x80) ^ (attr & 0x80)) {
      // Italics will be generated
      addto(out_buffer, 5);
    }
  }
  if (out_buffer[0]) {
    strcat(out_buffer, "m");
  }
  if (!okansi() && !forceit) {
    out_buffer[0] = '\0';
  }
}

void resetnsp() {
  if (nsp == 1 && !(session()->user()->HasPause())) {
    session()->user()->ToggleStatusFlag(User::pauseOnPage);
  }
  nsp = 0;
}

/* This function returns one character from either the local keyboard or
* remote com port (if applicable).  After 1.5 minutes of inactivity, a
* beep is sounded.  After 3 minutes of inactivity, the user is hung up.
*/
char getkey() {
  resetnsp();
  bool beepyet = false;
  timelastchar1 = timer1();

  using namespace wwiv::strings;
  long tv = (so() || IsEqualsIgnoreCase(session()->GetCurrentSpeed().c_str(), "TELNET")) ? 10920L : 3276L;
  long tv1 = tv - 1092L;     // change 4.31 Build3

  if (!session()->tagging) {
    lines_listed = 0;
  }

  char ch = 0;
  do {
    while (!bkbhit() && !hangup) {
      giveup_timeslice();
      long dd = timer1();
      if (dd < timelastchar1 && ((dd + 1000) > timelastchar1)) {
        timelastchar1 = dd;
      }
      if (std::abs(dd - timelastchar1) > 65536L) {
        timelastchar1 -= static_cast<int>(floor(SECONDS_PER_DAY * 18.2));
      }
      if ((dd - timelastchar1) > tv1 && !beepyet) {
        beepyet = true;
        bputch(CG);
      }
      session()->UpdateShutDownStatus();
      if (std::abs(dd - timelastchar1) > tv) {
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

static void print_yn(bool yes) {
  bout << YesNoString(yes);
  bout.nl();
}

/* The keyboard is checked for either a Y, N, or C/R to be hit.  C/R is
* assumed to be the same as a N.  Yes or No is output, and yn is set to
* zero if No was returned, and yesno() is non-zero if Y was hit.
*/
bool yesno() {
  char ch = 0;

  bout.Color(1);
  while ((!hangup) && ((ch = wwiv::UpperCase<char>(getkey())) != *(YesNoString(true))) && (ch != *(YesNoString(false)))
         && (ch != RETURN))
    ;

  if (ch == *(YesNoString(true))) {
    print_yn(true);
  } else {
    print_yn(false);
  }
  return (ch == *(YesNoString(true))) ? true : false;
}

/**
 * This is the same as yesno(), except C/R is assumed to be "Y"
 */
bool noyes() {
  char ch = 0;

  bout.Color(1);
  while ((!hangup) && ((ch = wwiv::UpperCase<char>(getkey())) != *(YesNoString(true))) && (ch != *(YesNoString(false)))
         && (ch != RETURN))
    ;

  if (ch == *(YesNoString(false))) {
    print_yn(false);
  } else {
    print_yn(true);
  }
  return ch == *(YesNoString(true)) || ch == RETURN;
}

char ynq() {
  char ch = 0;

  bout.Color(1);
  while (!hangup &&
         (ch = wwiv::UpperCase<char>(getkey())) != *(YesNoString(true)) &&
         ch != *(YesNoString(false)) &&
         ch != *str_quit && ch != RETURN) {
    // NOP
    ;
  }
  if (ch == *(YesNoString(true))) {
    ch = 'Y';
    print_yn(true);
  } else if (ch == *str_quit) {
    ch = 'Q';
    bout << str_quit;
    bout.nl();
  } else {
    ch = 'N';
    print_yn(false);
  }
  return ch;
}

char onek(const char *allowable_chars, bool auto_mpl) {
  if (auto_mpl) {
    bout.mpl(1);
  }
  char ch = onek_ncr(allowable_chars);
  bout.nl();
  return ch;
}
