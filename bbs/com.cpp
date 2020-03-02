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
#include <algorithm>

#include "bbs/bbsutl.h"
#include "bbs/datetime.h"
#include "bbs/exceptions.h"
#include "bbs/execexternal.h"
#include "bbs/stuffin.h"
#include "local_io/keycodes.h"
#include "bbs/remote_io.h"
#include "bbs/bbs.h"
#include "bbs/com.h"
#include "bbs/sysoplog.h"
#include "core/stl.h"
#include "core/strings.h"

using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

extern char str_quit[];


// This function checks to see if the user logged on to the com port has
// hung up.  Obviously, if no user is logged on remotely, this does nothing.
// returns the value of a()->hangup_
bool CheckForHangup() {
  if (!a()->hangup_ && a()->using_modem && !a()->remoteIO()->connected()) {
    if (a()->IsUserOnline()) {
      sysoplog() << "Hung Up.";
    }
    Hangup();
  }
  return a()->hangup_;
}

void Hangup() {
  if (!a()->cleanup_cmd.empty()) {
    bout.nl();
    const auto cmd = stuff_in(a()->cleanup_cmd, "", "", "", "");
    ExecuteExternalProgram(cmd, a()->spawn_option(SPAWNOPT_CLEANUP));
    }
    bout.nl(2);
  }
  if (a()->hangup_) { return; }
  a()->hangup_ = true;
  VLOG(1) << "Invoked Hangup()";
  throw wwiv::bbs::hangup_error(a()->user()->GetName());
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
  while ((!a()->hangup_) && ((ch = to_upper_case<char>(bout.getkey())) != *(YesNoString(true))) && (ch != *(YesNoString(false)))
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
  while ((!a()->hangup_) && ((ch = to_upper_case<char>(bout.getkey())) != *(YesNoString(true))) && (ch != *(YesNoString(false)))
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
  while (!a()->hangup_ &&
         (ch = to_upper_case<char>(bout.getkey())) != *(YesNoString(true)) &&
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
    auto ch = to_upper_case(bout.getkey());
    if (contains(allowable, ch)) {
      return ch;
    }
  }
  return 0;
}
