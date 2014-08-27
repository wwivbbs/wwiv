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

static void pr_st(int cursl, int ln, int an) {
  char s[81];

  up_str(s, cursl, an);
  out->window()->GotoXY(19, ln);
  out->window()->Puts(s);
}

static void up_sl(int cursl) {
  slrec ss = syscfg.sl[cursl];
  out->window()->GotoXY(19, 0);
  out->window()->Printf("%-3u", cursl);
  out->window()->GotoXY(19, 1);
  out->window()->Printf("%-5u", ss.time_per_day);
  out->window()->GotoXY(19, 2);
  out->window()->Printf("%-5u", ss.time_per_logon);
  out->window()->GotoXY(19, 3);
  out->window()->Printf("%-5u", ss.messages_read);
  out->window()->GotoXY(19, 4);
  out->window()->Printf("%-3u", ss.emails);
  out->window()->GotoXY(19, 5);
  out->window()->Printf("%-3u", ss.posts);
  pr_st(cursl, 6, ability_post_anony);
  pr_st(cursl, 7, ability_email_anony);
  pr_st(cursl, 8, ability_read_post_anony);
  pr_st(cursl, 9, ability_read_email_anony);
  pr_st(cursl, 10, ability_limited_cosysop);
  pr_st(cursl, 11, ability_cosysop);
}

static void ed_slx(int *sln) {
  int i;
  char s[81];

  int cursl = *sln;
  up_sl(cursl);
  bool done = false;
  int cp = 0;
  do {
    int i1 = 0;
    out->window()->GotoXY(19, cp);
    switch (cp) {
    case 0:
      sprintf(s, "%d", cursl);
      editline(out->window(), s, 3, NUM_ONLY, &i1, "");
      i = atoi(s);
      while (i < 0) {
        i += 255;
      }
      while (i > 255) {
        i -= 255;
      }
      if (i != cursl) {
        cursl = i;
        up_sl(cursl);
      }
      break;
    case 1:
      sprintf(s, "%u", syscfg.sl[cursl].time_per_day);
      editline(out->window(), s, 5, NUM_ONLY, &i1, "");
      i = atoi(s);
      syscfg.sl[cursl].time_per_day = i;
      out->window()->Printf("%-5u", i);
      break;
    case 2:
      sprintf(s, "%u", syscfg.sl[cursl].time_per_logon);
      editline(out->window(), s, 5, NUM_ONLY, &i1, "");
      i = atoi(s);
      syscfg.sl[cursl].time_per_logon = i;
      out->window()->Printf("%-5u", i);
      break;
    case 3:
      sprintf(s, "%u", syscfg.sl[cursl].messages_read);
      editline(out->window(), s, 5, NUM_ONLY, &i1, "");
      i = atoi(s);
      syscfg.sl[cursl].messages_read = i;
      out->window()->Printf("%-5u", i);
      break;
    case 4:
      sprintf(s, "%u", syscfg.sl[cursl].emails);
      editline(out->window(), s, 3, NUM_ONLY, &i1, "");
      i = atoi(s);
      syscfg.sl[cursl].emails = i;
      out->window()->Printf("%-3u", i);
      break;
    case 5:
      sprintf(s, "%u", syscfg.sl[cursl].posts);
      editline(out->window(), s, 3, NUM_ONLY, &i1, "");
      i = atoi(s);
      syscfg.sl[cursl].posts = i;
      out->window()->Printf("%-3u", i);
      break;
    case 6:
      i = ability_post_anony;
      up_str(s, cursl, i);
      editline(out->window(), s, 1, UPPER_ONLY, &i1, "");
      if (s[0] == 'Y') {
        syscfg.sl[cursl].ability |= i;
      } else {
        syscfg.sl[cursl].ability &= (~i);
      }
      pr_st(cursl, cp, i);
      break;
    case 7:
      i = ability_email_anony;
      up_str(s, cursl, i);
      editline(out->window(), s, 1, UPPER_ONLY, &i1, "");
      if (s[0] == 'Y') {
        syscfg.sl[cursl].ability |= i;
      } else {
        syscfg.sl[cursl].ability &= (~i);
      }
      pr_st(cursl, cp, i);
      break;
    case 8:
      i = ability_read_post_anony;
      up_str(s, cursl, i);
      editline(out->window(), s, 1, UPPER_ONLY, &i1, "");
      if (s[0] == 'Y') {
        syscfg.sl[cursl].ability |= i;
      } else {
        syscfg.sl[cursl].ability &= (~i);
      }
      pr_st(cursl, cp, i);
      break;
    case 9:
      i = ability_read_email_anony;
      up_str(s, cursl, i);
      editline(out->window(), s, 1, UPPER_ONLY, &i1, "");
      if (s[0] == 'Y') {
        syscfg.sl[cursl].ability |= i;
      } else {
        syscfg.sl[cursl].ability &= (~i);
      }
      pr_st(cursl, cp, i);
      break;
    case 10:
      i = ability_limited_cosysop;
      up_str(s, cursl, i);
      editline(out->window(), s, 1, UPPER_ONLY, &i1, "");
      if (s[0] == 'Y') {
        syscfg.sl[cursl].ability |= i;
      } else {
        syscfg.sl[cursl].ability &= (~i);
      }
      pr_st(cursl, cp, i);
      break;
    case 11:
      i = ability_cosysop;
      up_str(s, cursl, i);
      editline(out->window(), s, 1, UPPER_ONLY, &i1, "");
      if (s[0] == 'Y') {
        syscfg.sl[cursl].ability |= i;
      } else {
        syscfg.sl[cursl].ability &= (~i);
      }
      pr_st(cursl, cp, i);
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
  out->SetColor(SchemeId::NORMAL);
  out->window()->Printf("Security level   : \n");
  out->window()->Printf("Time per day     : \n");
  out->window()->Printf("Time per logon   : \n");
  out->window()->Printf("Messages read    : \n");
  out->window()->Printf("Emails per day   : \n");
  out->window()->Printf("Posts per day    : \n");
  out->window()->Printf("Post anony       : \n");
  out->window()->Printf("Email anony      : \n");
  out->window()->Printf("Read anony posts : \n");
  out->window()->Printf("Read anony email : \n");
  out->window()->Printf("Limited co-sysop : \n");
  out->window()->Printf("Co-sysop         : \n");
  int cursl = 10;
  up_sl(cursl);
  bool done = false;
  out->window()->GotoXY(0, 12);
  out->SetColor(SchemeId::PROMPT);
  out->window()->Puts("\n<ESC> to exit\n");
  out->SetColor(SchemeId::NORMAL);
  out->window()->Printf("[ = down one SL    ] = up one SL\n");
  out->window()->Printf("{ = down 10 SL     } = up 10 SL\n");
  out->window()->Printf("<C/R> = edit SL data\n");
  out->SetColor(SchemeId::NORMAL);
  do {
    out->window()->GotoXY(0, 18);
    out->window()->Puts("Command: ");
    char ch = onek(out->window(), "\033[]{}\r");
    switch (ch) {
    case '\r':
      out->window()->GotoXY(0, 12);
      out->SetColor(SchemeId::PROMPT);
      out->window()->Puts("\n<ESC> to exit\n");
      out->SetColor(SchemeId::NORMAL);
      out->window()->Printf("                                \n");
      out->window()->Printf("                               \n");
      out->window()->Printf("                    \n");
      out->window()->Puts("\n          \n");
      ed_slx(&cursl);
      out->window()->GotoXY(0, 12);
      out->SetColor(SchemeId::PROMPT);
      out->window()->Puts("\n<ESC> to exit\n");
      out->SetColor(SchemeId::NORMAL);
      out->window()->Printf("[ = down one SL    ] = up one SL\n");
      out->window()->Printf("{ = down 10 SL     } = up 10 SL\n");
      out->window()->Printf("<C/R> = edit SL data\n");
      out->SetColor(SchemeId::NORMAL);
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
      up_sl(cursl);
      break;
    case '[':
      cursl -= 1;
      if (cursl > 255) {
        cursl -= 256;
      }
      if (cursl < 0) {
        cursl += 256;
      }
      up_sl(cursl);
      break;
    case '}':
      cursl += 10;
      if (cursl > 255) {
        cursl -= 256;
      }
      if (cursl < 0) {
        cursl += 256;
      }
      up_sl(cursl);
      break;
    case '{':
      cursl -= 10;
      if (cursl > 255) {
        cursl -= 256;
      }
      if (cursl < 0) {
        cursl += 256;
      }
      up_sl(cursl);
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
  out->window()->Printf("NUM  SL   DSL  AR                DAR               RESTRICTIONS\n");
  out->window()->Printf("---  ---  ---  ----------------  ----------------  ----------------\n");
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

    out->window()->Printf("%3d  %3d  %3d  %16s  %16s  %16s\n", i1 + 1, v.sl, v.dsl, ar, dar, r);
  }
  nlx(2);
}

static void edit_autoval(int n) {
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
  out->window()->Printf("Auto-validation data for: Alt-F%d\n\n", n + 1);
  out->window()->Printf("SL           : %d\n", v.sl);
  out->window()->Printf("DSL          : %d\n", v.dsl);
  out->window()->Printf("AR           : %s\n", ar);
  out->window()->Printf("DAR          : %s\n", dar);
  out->window()->Printf("Restrictions : %s\n", r);
  out->SetColor(SchemeId::PROMPT);
  out->window()->Puts("\n\n<ESC> to exit\n");
  out->SetColor(SchemeId::NORMAL);
  bool done = false;
  cp = 0;
  do {
    int i1 = 0;
    out->window()->GotoXY(15, cp + 2);
    switch (cp) {
    case 0:
      sprintf(s, "%u", v.sl);
      editline(out->window(), s, 3, NUM_ONLY, &i1, "");
      i = atoi(s);
      if ((i < 0) || (i > 254)) {
        i = 10;
      }
      v.sl = i;
      out->window()->Printf("%-3d", i);
      break;
    case 1:
      sprintf(s, "%u", v.dsl);
      editline(out->window(), s, 3, NUM_ONLY, &i1, "");
      i = atoi(s);
      if ((i < 0) || (i > 254)) {
        i = 0;
      }
      v.dsl = i;
      out->window()->Printf("%-3d", i);
      break;
    case 2:
      editline(out->window(), ar, 16, SET, &i1, "ABCDEFGHIJKLMNOP");
      v.ar = 0;
      for (i = 0; i < 16; i++) {
        if (ar[i] != 32) {
          v.ar |= (1 << i);
        }
      }
      break;
    case 3:
      editline(out->window(), dar, 16, SET, &i1, "ABCDEFGHIJKLMNOP");
      v.dar = 0;
      for (i = 0; i < 16; i++) {
        if (dar[i] != 32) {
          v.dar |= (1 << i);
        }
      }
      break;
    case 4:
      editline(out->window(), r, 16, SET, &i1, restrict_string);
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
  do {
    list_autoval();
    out->SetColor(SchemeId::PROMPT);
    out->window()->Puts("Which (0-9, Q=Quit) ? ");
    out->SetColor(SchemeId::NORMAL);
    char ch = onek(out->window(), "Q0123456789\033");
    if (ch == 'Q' || ch == '\033') {
      done = true;
    } else {
      if (ch == '0') {
        edit_autoval(9);
      } else {
        edit_autoval(ch - '1');
      }
    }
  } while (!done);
  save_config();
}
