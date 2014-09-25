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

static const string& up_str(int cursl, int an) {
  static const string y = "Y";
  static const string n = "N";

  return (syscfg.sl[cursl].ability & an) ? y : n;
}

static void pr_st(CursesWindow* window, int cursl, int ln, int an) {
  window->PutsXY(19, ln, up_str(cursl, an));
}

static void up_sl(CursesWindow* window, int cursl) {
  slrec ss = syscfg.sl[cursl];
  window->GotoXY(19, 0);
  window->Printf("%-3u", cursl);
  window->GotoXY(19, 1);
  window->Printf("%-5u", ss.time_per_day);
  window->GotoXY(19, 2);
  window->Printf("%-5u", ss.time_per_logon);
  window->GotoXY(19, 3);
  window->Printf("%-5u", ss.messages_read);
  window->GotoXY(19, 4);
  window->Printf("%-3u", ss.emails);
  window->GotoXY(19, 5);
  window->Printf("%-3u", ss.posts);
  pr_st(window, cursl, 6, ability_post_anony);
  pr_st(window, cursl, 7, ability_email_anony);
  pr_st(window, cursl, 8, ability_read_post_anony);
  pr_st(window, cursl, 9, ability_read_email_anony);
  pr_st(window, cursl, 10, ability_limited_cosysop);
  pr_st(window, cursl, 11, ability_cosysop);
}

static void ed_slx(CursesWindow* window, int *sln) {
  int i;

  int cursl = *sln;
  up_sl(window, cursl);
  bool done = false;
  int cp = 0;
  do {
    int i1 = 0;
    window->GotoXY(19, cp);
    switch (cp) {
    case 0: {
      string s = StringPrintf("%d", cursl);
      editline(window, &s, 3, NUM_ONLY, &i1, "");
      i = atoi(s.c_str());
      while (i < 0) {
        i += 255;
      }
      while (i > 255) {
        i -= 255;
      }
      if (i != cursl) {
        cursl = i;
        up_sl(window, cursl);
      }
    } break;
    case 1: {
      string s = StringPrintf("%u", syscfg.sl[cursl].time_per_day);
      editline(window, &s, 5, NUM_ONLY, &i1, "");
      int i = atoi(s.c_str());
      syscfg.sl[cursl].time_per_day = i;
      window->Printf("%-5u", i);
    } break;
    case 2: {
      string s = StringPrintf("%u", syscfg.sl[cursl].time_per_logon);
      editline(window, &s, 5, NUM_ONLY, &i1, "");
      int i = atoi(s.c_str());
      syscfg.sl[cursl].time_per_logon = i;
      window->Printf("%-5u", i);
    } break;
    case 3: {
      string s = StringPrintf("%u", syscfg.sl[cursl].messages_read);
      editline(window, &s, 5, NUM_ONLY, &i1, "");
      int i = atoi(s.c_str());
      syscfg.sl[cursl].messages_read = i;
      window->Printf("%-5u", i);
    } break;
    case 4: {
      string s = StringPrintf("%u", syscfg.sl[cursl].emails);
      editline(window, &s, 3, NUM_ONLY, &i1, "");
      i = atoi(s.c_str());
      syscfg.sl[cursl].emails = i;
      window->Printf("%-3u", i);
    } break;
    case 5: {
      string s = StringPrintf("%u", syscfg.sl[cursl].posts);
      editline(window, &s, 3, NUM_ONLY, &i1, "");
      i = atoi(s.c_str());
      syscfg.sl[cursl].posts = i;
      window->Printf("%-3u", i);
    } break;
    case 6: {
      i = ability_post_anony;
      string s = up_str(cursl, i);
      editline(window, &s, 1, UPPER_ONLY, &i1, "");
      if (s[0] == 'Y') {
        syscfg.sl[cursl].ability |= i;
      } else {
        syscfg.sl[cursl].ability &= (~i);
      }
      pr_st(window, cursl, cp, i);
    } break;
    case 7: {
      i = ability_email_anony;
      string s = up_str(cursl, i);
      editline(window, &s, 1, UPPER_ONLY, &i1, "");
      if (s[0] == 'Y') {
        syscfg.sl[cursl].ability |= i;
      } else {
        syscfg.sl[cursl].ability &= (~i);
      }
      pr_st(window, cursl, cp, i);
    } break;
    case 8: {
      i = ability_read_post_anony;
      string s = up_str(cursl, i);
      editline(window, &s, 1, UPPER_ONLY, &i1, "");
      if (s[0] == 'Y') {
        syscfg.sl[cursl].ability |= i;
      } else {
        syscfg.sl[cursl].ability &= (~i);
      }
      pr_st(window, cursl, cp, i);
    } break;
    case 9: {
      i = ability_read_email_anony;
      string s = up_str(cursl, i);
      editline(window, &s, 1, UPPER_ONLY, &i1, "");
      if (s[0] == 'Y') {
        syscfg.sl[cursl].ability |= i;
      } else {
        syscfg.sl[cursl].ability &= (~i);
      }
      pr_st(window, cursl, cp, i);
    } break;
    case 10: {
      i = ability_limited_cosysop;
      string s = up_str(cursl, i);
      editline(window, &s, 1, UPPER_ONLY, &i1, "");
      if (s[0] == 'Y') {
        syscfg.sl[cursl].ability |= i;
      } else {
        syscfg.sl[cursl].ability &= (~i);
      }
      pr_st(window, cursl, cp, i);
    } break;
    case 11: {
      i = ability_cosysop;
      string s = up_str(cursl, i);
      editline(window, &s, 1, UPPER_ONLY, &i1, "");
      if (s[0] == 'Y') {
        syscfg.sl[cursl].ability |= i;
      } else {
        syscfg.sl[cursl].ability &= (~i);
      }
      pr_st(window, cursl, cp, i);
    } break;
    }
    cp = GetNextSelectionPosition(0, 11, cp, i1);
    if (i1 == DONE) {
      done = true;
    }
  } while (!done);
  *sln = cursl;
}

void sec_levs() {
  out->Cls();
  CursesWindow* window(out->window());
  window->SetColor(SchemeId::NORMAL);
  window->Printf("Security level   : \n");
  window->Printf("Time per day     : \n");
  window->Printf("Time per logon   : \n");
  window->Printf("Messages read    : \n");
  window->Printf("Emails per day   : \n");
  window->Printf("Posts per day    : \n");
  window->Printf("Post anony       : \n");
  window->Printf("Email anony      : \n");
  window->Printf("Read anony posts : \n");
  window->Printf("Read anony email : \n");
  window->Printf("Limited co-sysop : \n");
  window->Printf("Co-sysop         : \n");
  int cursl = 10;
  up_sl(window, cursl);
  bool done = false;
  window->GotoXY(0, 12);
  window->SetColor(SchemeId::PROMPT);
  window->Puts("\n<ESC> to exit\n");
  window->SetColor(SchemeId::NORMAL);
  window->Printf("[ = down one SL    ] = up one SL\n");
  window->Printf("{ = down 10 SL     } = up 10 SL\n");
  window->Printf("<C/R> = edit SL data\n");
  window->SetColor(SchemeId::NORMAL);
  do {
    window->GotoXY(0, 18);
    window->Puts("Command: ");
    char ch = onek(window, "\033[]{}\r");
    switch (ch) {
    case '\r':
      window->GotoXY(0, 12);
      window->SetColor(SchemeId::PROMPT);
      window->Puts("\n<ESC> to exit\n");
      window->SetColor(SchemeId::NORMAL);
      window->Printf("                                \n");
      window->Printf("                               \n");
      window->Printf("                    \n");
      window->Puts("\n          \n");
      ed_slx(window, &cursl);
      window->GotoXY(0, 12);
      window->SetColor(SchemeId::PROMPT);
      window->Puts("\n<ESC> to exit\n");
      window->SetColor(SchemeId::NORMAL);
      window->Printf("[ = down one SL    ] = up one SL\n");
      window->Printf("{ = down 10 SL     } = up 10 SL\n");
      window->Printf("<C/R> = edit SL data\n");
      window->SetColor(SchemeId::NORMAL);
      break;
    case '\033':
      done = true;
      break;
    case ']':
      cursl += 1;
      if (cursl > 255) {
        cursl -= 256;
      }
      if (cursl < 0) {
        cursl += 256;
      }
      up_sl(window, cursl);
      break;
    case '[':
      cursl -= 1;
      if (cursl > 255) {
        cursl -= 256;
      }
      if (cursl < 0) {
        cursl += 256;
      }
      up_sl(window, cursl);
      break;
    case '}':
      cursl += 10;
      if (cursl > 255) {
        cursl -= 256;
      }
      if (cursl < 0) {
        cursl += 256;
      }
      up_sl(window, cursl);
      break;
    case '{':
      cursl -= 10;
      if (cursl > 255) {
        cursl -= 256;
      }
      if (cursl < 0) {
        cursl += 256;
      }
      up_sl(window, cursl);
      break;
    }
  } while (!done);
  save_config();
}
