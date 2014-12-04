/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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

#include "wwiv.h"
#include "core/strings.h"


void HopSub() {
  char s1[81], s2[81];

  bool abort = false;
  int nc = 0;
  while (uconfsub[nc].confnum != -1) {
    nc++;
  }

  if (okansi()) {
    bout << "\r\x1b[K";
  } else {
    bout.nl();
  }
  bout << "|#9Enter name or partial name of sub to hop to:\r\n:";
  if (okansi()) {
    newline = false;
  }
  input(s1, 40, true);
  if (!s1[0]) {
    return;
  }
  if (!okansi()) {
    bout.nl();
  }

  int c = 0;
  int oc = session()->GetCurrentConferenceMessageArea();
  int os = usub[session()->GetCurrentMessageArea()].subnum;

  while ((c < nc) && !abort) {
    if (okconf(session()->user())) {
      setuconf(CONF_SUBS, c, -1);
    }
    int i = 0;
    while ((i < session()->num_subs) && (usub[i].subnum != -1) && !abort) {
      strcpy(s2, subboards[usub[i].subnum].name);
      for (int i2 = 0; (s2[i2] = upcase(s2[i2])) != 0; i2++)
        ;
      if (strstr(s2, s1) != nullptr) {
        if (okansi()) {
          bout << "\r\x1b[K";
        }
        if (!okansi()) {
          bout.nl();
        }
        bout << "|#5Do you mean \"" << subboards[usub[i].subnum].name << "\" (Y/N/Q)? ";
        char ch = onek_ncr("QYN\r");
        if (ch == 'Y') {
          abort = true;
          session()->SetCurrentMessageArea(i);
          break;
        } else if (ch == 'Q') {
          abort = true;
          if (okconf(session()->user())) {
            setuconf(CONF_SUBS, oc, os);
          }
          break;
        }
      }
      ++i;
    }
    c++;
    if (!okconf(session()->user())) {
      break;
    }
  }
  if (okconf(session()->user()) && !abort) {
    setuconf(CONF_SUBS, oc, os);
  }
}


void HopDir() {
  char s1[81], s2[81];
  bool abort = false;

  int nc = 0;
  while (uconfdir[nc].confnum != -1) {
    nc++;
  }

  if (okansi()) {
    bout << "\r\x1b[K";
  } else {
    bout.nl();
  }
  bout << "|#9Enter name or partial name of dir to hop to:\r\n:";
  if (okansi()) {
    newline = false;
  }
  input(s1, 20, true);
  if (!s1[0]) {
    return;
  }
  if (!okansi()) {
    bout.nl();
  }

  int c = 0;
  int oc = session()->GetCurrentConferenceFileArea();
  int os = udir[session()->GetCurrentFileArea()].subnum;

  while (c < nc && !abort) {
    if (okconf(session()->user())) {
      setuconf(CONF_DIRS, c, -1);
    }
    int i = 0;
    while ((i < session()->num_dirs) && (udir[i].subnum != -1) && (!abort)) {
      strcpy(s2, directories[udir[i].subnum].name);
      for (int i2 = 0; (s2[i2] = upcase(s2[i2])) != 0; i2++)
        ;
      if (strstr(s2, s1) != nullptr) {
        if (okansi()) {
          bout << "\r\x1b[K";
        } else {
          bout.nl();
        }
        bout << "|#5Do you mean \"" << directories[udir[i].subnum].name << "\" (Y/N/Q)? ";
        char ch = onek_ncr("QYN\r");
        if (ch == 'Y') {
          abort = true;
          session()->SetCurrentFileArea(i);
          break;
        } else if (ch == 'Q') {
          abort = true;
          if (okconf(session()->user())) {
            setuconf(CONF_DIRS, oc, os);
          }
          break;
        }
      }
      ++i;
    }
    c++;
    if (!okconf(session()->user())) {
      break;
    }
  }
  if (okconf(session()->user()) && !abort) {
    setuconf(CONF_DIRS, oc, os);
  }
}


