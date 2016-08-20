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
#include "bbs/mmkey.h"
#include <string>

#include "bbs/input.h"
#include "bbs/bbs.h"
#include "bbs/fcns.h"
#include "bbs/keycodes.h"
#include "bbs/vars.h"
#include "core/strings.h"
#include "core/stl.h"
#include "core/wwivassert.h"

using std::string;
using namespace wwiv::strings;
using namespace wwiv::stl;

char *mmkey(int dl, int area, bool bListOption) {
  static char cmd1[10], cmd2[81], ch;
  int i, p, cp;

  do {
    do {
      ch = getkey();
      if (bListOption && (ch == RETURN || ch == SPACE)) {
        ch = upcase(ch);
        cmd1[0] = ch;
        return cmd1;
      }
    } while ((ch <= ' ' || ch == RETURN || ch > 126) && !hangup);
    ch = upcase(ch);
    bputch(ch);
    if (ch == RETURN) {
      cmd1[0] = '\0';
    } else {
      cmd1[0] = ch;
    }
    cmd1[1] = '\0';
    p = 0;
    switch (dl) {

    case 1:
      if (strchr(mmkey_dtc, ch) != nullptr) {
        p = 2;
      } else if (strchr(mmkey_dcd, ch) != nullptr) {
        p = 1;
      }
      break;
    case 2:
      if (strchr(odc, ch) != nullptr) {
        p = 1;
      }
      break;
    case 0:
      if (strchr(mmkey_tc, ch) != nullptr) {
        p = 2;
      } else if (strchr(mmkey_dc, ch) != nullptr) {
        p = 1;
      }
      break;
    }
    if (p) {
      cp = 1;
      do {
        do {
          ch = getkey();
        } while ((((ch < ' ') && (ch != RETURN) && (ch != BACKSPACE)) || (ch > 126)) && !hangup);
        ch = upcase(ch);
        if (ch == RETURN) {
          bout.nl();
          if (dl == 2) {
            bout.nl();
          }
          if (!session()->user()->IsExpert() && !okansi()) {
            newline = true;
          }
          return cmd1;
        } else {
          if (ch == BACKSPACE) {
            bout.bs();
            cmd1[ --cp ] = '\0';
          } else {
            cmd1[ cp++ ]  = ch;
            cmd1[ cp ]    = '\0';
            bputch(ch);
            if (ch == '/' && cmd1[0] == '/') {
              input(cmd2, 50);
              if (!newline) {
                if (isdigit(cmd2[0])) {
                  if (area == WSession::mmkeyMessageAreas && dl == 0) {
                    for (i = 0; i < size_int(session()->subboards) && session()->usub[i].subnum != -1; i++) {
                      if (IsEquals(session()->usub[i].keys, cmd2)) {
                        bout.nl();
                        break;
                      }
                    }
                  }
                  if (area == WSession::mmkeyFileAreas && dl == 1) {
                    for (i = 0; i < size_int(session()->directories); i++) {
                      if (IsEquals(session()->udir[i].keys, cmd2)) {
                        bout.nl();
                        break;
                      }
                    }
                  }
                  if (dl == 2) {
                    bout.nl();
                  }
                } else {
                  bout.nl();
                }
                newline = true;
              }
              return cmd2;
            } else if (cp == p + 1) {
              if (!newline) {
                if (isdigit(cmd1[ 0 ])) {
                  if (dl == 2 || !okansi()) {
                    bout.nl();
                  }
                  if (!session()->user()->IsExpert() && !okansi()) {
                    newline = true;
                  }
                } else {
                  bout.nl();
                  newline = true;
                }
              } else {
                bout.nl();
                newline = true;
              }
              return cmd1;
            }
          }
        }
      } while (cp > 0);
    } else {
      if (!newline) {
        switch (cmd1[0]) {
        case '>':
        case '+':
        case ']':
        case '}':
        case '<':
        case '-':
        case '[':
        case '{':
        case 'H':
          if (dl == 2 || !okansi()) {
            bout.nl();
          }
          if (!session()->user()->IsExpert() && !okansi()) {
            newline = true;
          }
          break;
        default:
          if (isdigit(cmd1[0])) {
            if (dl == 2 || !okansi()) {
              bout.nl();
            }
            if (!session()->user()->IsExpert() && !okansi()) {
              newline = true;
            }
          } else {
            bout.nl();
            newline = true;
          }
          break;
        }
      } else {
        bout.nl();
        newline = true;
      }
      return cmd1;
    }
  } while (!hangup);
  cmd1[0] = '\0';
  return cmd1;
}
