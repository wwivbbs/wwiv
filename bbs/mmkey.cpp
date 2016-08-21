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
#include <set>
#include <string>


#include "bbs/input.h"
#include "core/log.h"
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

string mmkey(std::set<char>& x, std::set<char>& xx, bool bListOption) {
  char ch;
  string cmd1;

  do {
    do {
      ch = getkey();
      if (bListOption && (ch == RETURN || ch == SPACE)) {
        ch = upcase(ch);
        return string(1, ch);
      }
    } while ((ch <= ' ' || ch == RETURN || ch > 126) && !hangup);
    ch = upcase(ch);
    bputch(ch);
    if (ch == RETURN) {
      cmd1.clear();
    } else {
      cmd1 = ch;
    }
    int p = 0;
    if (contains(xx, ch)) {
      p = 2;
    } else if (contains(x, ch)) {
      p = 1;
    } else {
      bout.nl();
      newline = true;
      return cmd1;
    }
    
    int cp = 1;
    do {
      do {
        ch = getkey();
      } while ((((ch < ' ') && (ch != RETURN) && (ch != BACKSPACE)) || (ch > 126)) && !hangup);
      ch = upcase(ch);
      if (ch == RETURN) {
        bout.nl();
        newline = true;
        return cmd1;
      } else {
        if (ch == BACKSPACE) {
          bout.bs();
          cmd1.pop_back();
          cp--;
        } else {
          cmd1.push_back(ch);
          bputch(ch);
          cp++;
          if (ch == '/' && cmd1[0] == '/') {
            newline = true;
            return input(50);
          } else if (cp == p + 1) {
            bout.nl();
            newline = true;
            return cmd1;
          }
        }
      }
    } while (cp > 0);
  } while (!hangup);
  return{};
}

string mmkey(std::set<char>& x) {
  std::set<char> xx;
  return mmkey(x, xx, false);
}

static int max_sub_key(std::vector<usersubrec>& container) {
  int key = 0;
  for (const auto& r : container) {
    int current_key = StringToInt(r.keys);
    if (current_key == 0) {
      return key;
    }
    key = current_key;
  }
  return key;
}
 
char *mmkey(int dl, bool bListOption) {
  std::set<char> x = {'/'};
  std::set<char> xx{};
  switch (dl) {
  case 0: {
    int max_key = max_sub_key(session()->usub);
    for (char i = 1; i <= max_key / 10; i++) {
      x.insert(i + '0');
    }
    for (char i = 1; i <= max_key / 100; i++) {
      xx.insert(i + '0');
    }
  } break;
  case 1: {
    int max_key = max_sub_key(session()->udir);
    for (char i = 1; i <= max_key / 10; i++) {
      x.insert(i);
    }
    for (char i = 1; i <= max_key / 100; i++) {
      xx.insert(i);
    }
  } break;
  default: {
    DLOG(FATAL) << "invalid mmkey dl: " << dl;
  }
  }

  static char s[81];
  string s1 = mmkey(x, xx, bListOption);
  strcpy(s, s1.c_str());
  return s;
}
