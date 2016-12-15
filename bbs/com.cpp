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
#include "bbs/exceptions.h"
#include "bbs/keycodes.h"
#include "bbs/pause.h"
#include "bbs/remote_io.h"
#include "bbs/wconstants.h"
#include "bbs/bbs.h"
#include "bbs/com.h"
#include "bbs/fcns.h"
#include "bbs/sysoplog.h"
#include "bbs/vars.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/wwivassert.h"

using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

extern char str_quit[];


// This function checks to see if the user logged on to the com port has
// hung up.  Obviously, if no user is logged on remotely, this does nothing.
// returns the value of hangup
bool CheckForHangup() {
  if (!hangup && a()->using_modem && !a()->remoteIO()->connected()) {
    if (a()->IsUserOnline()) {
      sysoplog() << "Hung Up.";
    }
    Hangup();
  }
  return hangup;
}

void Hangup() {
  if (hangup) { return; }
  hangup = true;
  VLOG(1) << "Invoked Hangup()";
  throw wwiv::bbs::hangup_error(a()->user()->GetName());
}

static void addto(char *ansi_str, int num) {
  strcat(ansi_str, (ansi_str[0]) ? ";" : "\x1b[");
  std::string s = std::to_string(num);
  strcat(ansi_str, s.c_str());
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
  while ((!hangup) && ((ch = wwiv::UpperCase<char>(bout.getkey())) != *(YesNoString(true))) && (ch != *(YesNoString(false)))
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
  while ((!hangup) && ((ch = wwiv::UpperCase<char>(bout.getkey())) != *(YesNoString(true))) && (ch != *(YesNoString(false)))
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
         (ch = wwiv::UpperCase<char>(bout.getkey())) != *(YesNoString(true)) &&
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

char onek(const std::string& allowable, bool auto_mpl) {
  if (auto_mpl) {
    bout.mpl(1);
  }
  char ch = onek_ncr(allowable);
  bout.nl();
  return ch;
}

// Like onek but does not put cursor down a line
// One key, no carriage return
char onek_ncr(const std::string& allowable) {
  while (true) {
    CheckForHangup();
    auto ch = wwiv::UpperCase(bout.getkey());
    if (contains(allowable, ch)) {
      return ch;
    }
  }
  return 0;
}
