/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
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
#include <cstdlib>
#include <cstring>
#include <curses.h>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include <string>
#include <sys/stat.h>

#include "ifcns.h"
#include "init.h"
#include "input.h"
#include "bbs/wconstants.h"
#include "wwivinit.h"
#include "core/strings.h"
#include "core/wwivport.h"
#include "utility.h"

using std::string;
using wwiv::strings::StringPrintf;

static void list_autoval() {
  char s3[81], ar[20], dar[20], r[20];
  int i;
  valrec v;

  out->Cls();
  CursesWindow* window(out->window());
  window->Printf("NUM  SL   DSL  AR                DAR               RESTRICTIONS\n");
  window->Printf("---  ---  ---  ----------------  ----------------  ----------------\n");
  strcpy(s3, restrict_string);
  for (int i1 = 0; i1 < 10; i1++) {
    v = syscfg.autoval[i1];
    for (i = 0; i <= 15; i++) {
      if (v.ar & (1 << i)) {
        ar[i] = 'A' + i;
      } else {
        ar[i] = 32;
      }
      if (v.dar & (1 << i)) {
        dar[i] = 'A' + i;
      } else {
        dar[i] = 32;
      }
      if (v.restrict & (1 << i)) {
        r[i] = s3[i];
      } else {
        r[i] = 32;
      }
    }
    r[16] = 0;
    ar[16] = 0;
    dar[16] = 0;

    window->Printf("%3d  %3d  %3d  %16s  %16s  %16s\n", i1 + 1, v.sl, v.dsl, ar, dar, r);
  }
  nlx(2);
}

static void edit_autoval(CursesWindow* window, int n) {
  char s[81], ar[20], dar[20], r[20];
  int i, cp;
  valrec v;

  v = syscfg.autoval[n];
  strcpy(s, restrict_string);
  for (i = 0; i <= 15; i++) {
    if (v.ar & (1 << i)) {
      ar[i] = 'A' + i;
    } else {
      ar[i] = 32;
    }
    if (v.dar & (1 << i)) {
      dar[i] = 'A' + i;
    } else {
      dar[i] = 32;
    }
    if (v.restrict & (1 << i)) {
      r[i] = s[i];
    } else {
      r[i] = 32;
    }
  }
  r[16] = 0;
  ar[16] = 0;
  dar[16] = 0;
  out->Cls();
  window->Printf("Auto-validation data for: Alt-F%d\n\n", n + 1);
  window->Printf("SL           : %d\n", v.sl);
  window->Printf("DSL          : %d\n", v.dsl);
  window->Printf("AR           : %s\n", ar);
  window->Printf("DAR          : %s\n", dar);
  window->Printf("Restrictions : %s\n", r);
  window->SetColor(SchemeId::PROMPT);
  window->Puts("\n\n<ESC> to exit\n");
  window->SetColor(SchemeId::NORMAL);
  bool done = false;
  cp = 0;
  do {
    int i1 = 0;
    window->GotoXY(15, cp + 2);
    switch (cp) {
    case 0:
      sprintf(s, "%u", v.sl);
      editline(window, s, 3, NUM_ONLY, &i1, "");
      i = atoi(s);
      if ((i < 0) || (i > 254)) {
        i = 10;
      }
      v.sl = i;
      window->Printf("%-3d", i);
      break;
    case 1:
      sprintf(s, "%u", v.dsl);
      editline(window, s, 3, NUM_ONLY, &i1, "");
      i = atoi(s);
      if ((i < 0) || (i > 254)) {
        i = 0;
      }
      v.dsl = i;
      window->Printf("%-3d", i);
      break;
    case 2:
      editline(window, ar, 16, SET, &i1, "ABCDEFGHIJKLMNOP");
      v.ar = 0;
      for (i = 0; i < 16; i++) {
        if (ar[i] != 32) {
          v.ar |= (1 << i);
        }
      }
      break;
    case 3:
      editline(window, dar, 16, SET, &i1, "ABCDEFGHIJKLMNOP");
      v.dar = 0;
      for (i = 0; i < 16; i++) {
        if (dar[i] != 32) {
          v.dar |= (1 << i);
        }
      }
      break;
    case 4:
      editline(window, r, 16, SET, &i1, restrict_string);
      v.restrict = 0;
      for (i = 0; i < 16; i++) {
        if (r[i] != 32) {
          v.restrict |= (1 << i);
        }
      }
      break;
    }
    cp = GetNextSelectionPosition(0, 4, cp, i1);
    if (i1 == DONE) {
      done = true;
    }
  } while (!done);
  syscfg.autoval[n] = v;
}

void autoval_levs() {
  bool done = false;
  CursesWindow* window(out->window());
  do {
    list_autoval();
    window->SetColor(SchemeId::PROMPT);
    window->Puts("Which (0-9, Q=Quit) ? ");
    window->SetColor(SchemeId::NORMAL);
    char ch = onek(window, "Q0123456789\033");
    if (ch == 'Q' || ch == '\033') {
      done = true;
    } else {
      if (ch == '0') {
        edit_autoval(window, 9);
      } else {
        edit_autoval(window, ch - '1');
      }
    }
  } while (!done);
  save_config();
}
