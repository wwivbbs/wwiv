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
#include <sys/stat.h>

#include "ifcns.h"
#include "init.h"
#include "input.h"
#include "bbs/wconstants.h"
#include "wwivinit.h"
#include "core/wwivport.h"
#include "utility.h"

static void up_str(char *s, int cursl, int an) {
  strcpy(s, (syscfg.sl[cursl].ability & an) ? "Y" : "N");
}

static void pr_st(CursesWindow* window, int cursl, int ln, int an) {
  char s[81];

  up_str(s, cursl, an);
  window->GotoXY(19, ln);
  window->Puts(s);
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
  char s[81];

  int cursl = *sln;
  up_sl(window, cursl);
  bool done = false;
  int cp = 0;
  do {
    int i1 = 0;
    window->GotoXY(19, cp);
    switch (cp) {
    case 0:
      sprintf(s, "%d", cursl);
      editline(window, s, 3, NUM_ONLY, &i1, "");
      i = atoi(s);
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
      break;
    case 1:
      sprintf(s, "%u", syscfg.sl[cursl].time_per_day);
      editline(window, s, 5, NUM_ONLY, &i1, "");
      i = atoi(s);
      syscfg.sl[cursl].time_per_day = i;
      window->Printf("%-5u", i);
      break;
    case 2:
      sprintf(s, "%u", syscfg.sl[cursl].time_per_logon);
      editline(window, s, 5, NUM_ONLY, &i1, "");
      i = atoi(s);
      syscfg.sl[cursl].time_per_logon = i;
      window->Printf("%-5u", i);
      break;
    case 3:
      sprintf(s, "%u", syscfg.sl[cursl].messages_read);
      editline(window, s, 5, NUM_ONLY, &i1, "");
      i = atoi(s);
      syscfg.sl[cursl].messages_read = i;
      window->Printf("%-5u", i);
      break;
    case 4:
      sprintf(s, "%u", syscfg.sl[cursl].emails);
      editline(window, s, 3, NUM_ONLY, &i1, "");
      i = atoi(s);
      syscfg.sl[cursl].emails = i;
      window->Printf("%-3u", i);
      break;
    case 5:
      sprintf(s, "%u", syscfg.sl[cursl].posts);
      editline(window, s, 3, NUM_ONLY, &i1, "");
      i = atoi(s);
      syscfg.sl[cursl].posts = i;
      window->Printf("%-3u", i);
      break;
    case 6:
      i = ability_post_anony;
      up_str(s, cursl, i);
      editline(window, s, 1, UPPER_ONLY, &i1, "");
      if (s[0] == 'Y') {
        syscfg.sl[cursl].ability |= i;
      } else {
        syscfg.sl[cursl].ability &= (~i);
      }
      pr_st(window, cursl, cp, i);
      break;
    case 7:
      i = ability_email_anony;
      up_str(s, cursl, i);
      editline(window, s, 1, UPPER_ONLY, &i1, "");
      if (s[0] == 'Y') {
        syscfg.sl[cursl].ability |= i;
      } else {
        syscfg.sl[cursl].ability &= (~i);
      }
      pr_st(window, cursl, cp, i);
      break;
    case 8:
      i = ability_read_post_anony;
      up_str(s, cursl, i);
      editline(window, s, 1, UPPER_ONLY, &i1, "");
      if (s[0] == 'Y') {
        syscfg.sl[cursl].ability |= i;
      } else {
        syscfg.sl[cursl].ability &= (~i);
      }
      pr_st(window, cursl, cp, i);
      break;
    case 9:
      i = ability_read_email_anony;
      up_str(s, cursl, i);
      editline(window, s, 1, UPPER_ONLY, &i1, "");
      if (s[0] == 'Y') {
        syscfg.sl[cursl].ability |= i;
      } else {
        syscfg.sl[cursl].ability &= (~i);
      }
      pr_st(window, cursl, cp, i);
      break;
    case 10:
      i = ability_limited_cosysop;
      up_str(s, cursl, i);
      editline(window, s, 1, UPPER_ONLY, &i1, "");
      if (s[0] == 'Y') {
        syscfg.sl[cursl].ability |= i;
      } else {
        syscfg.sl[cursl].ability &= (~i);
      }
      pr_st(window, cursl, cp, i);
      break;
    case 11:
      i = ability_cosysop;
      up_str(s, cursl, i);
      editline(window, s, 1, UPPER_ONLY, &i1, "");
      if (s[0] == 'Y') {
        syscfg.sl[cursl].ability |= i;
      } else {
        syscfg.sl[cursl].ability &= (~i);
      }
      pr_st(window, cursl, cp, i);
      break;
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
