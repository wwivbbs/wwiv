/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
/*                 Copyright (C)2014, WWIV Software Services              */
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
#include "user_editor.h"

#include <curses.h>
#include "ifcns.h"
#include "init.h"
#include "wwivinit.h"

static const int X_POSITION = 19;

/**
 * Printf sytle output function.  Most init output code should use this.
 */
void PrintfY(int y, const char *pszFormat, ...) {
  va_list ap;
  char szBuffer[1024];

  va_start(ap, pszFormat);
  vsnprintf(szBuffer, 1024, pszFormat, ap);
  va_end(ap);
  app->localIO->LocalGotoXY(X_POSITION, y);
  app->localIO->LocalPuts(szBuffer);
}


void show_user(int user_number, const userrec *user) {
  PrintfY(0, "%-30s", user->name);
  PrintfY(1, "%-20s", user->realname);
  PrintfY(2, "%-3d", user->sl);
  PrintfY(3, "%-3d", user->dsl);
  PrintfY(4, "%-30s", user->street);
  PrintfY(5, "%-30s", user->city);
  PrintfY(6, "%-2s", user->state);
  PrintfY(7, "%-10s", user->zipcode);
  PrintfY(8, "%-8s", user->pw);
  PrintfY(9, "%-12s", user->phone);
  PrintfY(10, "%-12s", user->dataphone);
  PrintfY(11, "%-3d", user->comp_type);
  PrintfY(12, "%-12d", user->wwiv_regnum);
}

void edit_user(int user_number, userrec *user) {
  show_user(user_number, user);
  bool done = false;
  int cp = 0;
  do {
    int i1 = 0;
    app->localIO->LocalGotoXY(X_POSITION, cp);
    switch (cp) {
    case 0: {
      char s[81];
      sprintf(s, "%s", user->name);
      editline(s, 30, ALL, &i1, "");
      //sprintf(s, "%d", cursl);
      //editline(s, 3, NUM_ONLY, &i1, "");
      //i = atoi(s);
      //while (i < 0) {
      //  i += 255;
      //}
      //while (i > 255) {
      //  i -= 255;
      //}
      //if (i != cursl) {
      //  cursl = i;
      //  up_sl(cursl);
      //}
    } break;
    case 1:
      //sprintf(s, "%u", syscfg.sl[cursl].time_per_day);
      //editline(s, 5, NUM_ONLY, &i1, "");
      //i = atoi(s);
      //syscfg.sl[cursl].time_per_day = i;
      //Printf("%-5u", i);
      break;
    case 2:
      //sprintf(s, "%u", syscfg.sl[cursl].time_per_logon);
      //editline(s, 5, NUM_ONLY, &i1, "");
      //i = atoi(s);
      //syscfg.sl[cursl].time_per_logon = i;
      //Printf("%-5u", i);
      break;
    case 3:
      //sprintf(s, "%u", syscfg.sl[cursl].messages_read);
      //editline(s, 5, NUM_ONLY, &i1, "");
      //i = atoi(s);
      //syscfg.sl[cursl].messages_read = i;
      //Printf("%-5u", i);
      break;
    default:
      break;
    }
    cp = GetNextSelectionPosition(0, 0, cp, i1);
    if (i1 == DONE) {

      // write_user(user_number, &user);
      done = true;
    }
  } while (!done);
}


void user_editor() {
  app->localIO->LocalCls();
  textattr(COLOR_CYAN);
  Printf("Name/Handle      : \n");
  Printf("Real Name        : \n");
  Printf("SL               : \n");
  Printf("DSL              : \n");
  Printf("Address          : \n");
  Printf("City             : \n");
  Printf("State            : \n");
  Printf("Postal Code      : \n");
  Printf("Birthday         : \n");
  Printf("Password         : \n");
  Printf("Phone Number     : \n");
  Printf("Data Phone Number: \n");
  Printf("Computer Type    : \n");
  Printf("WWIV Registration: \n");

  int current_usernum = 1;
  userrec user;
  read_user(current_usernum, &user);
  show_user(current_usernum, &user);
  bool done = false;
  app->localIO->LocalGotoXY(0, 12);
  textattr(COLOR_YELLOW);
  Puts("\n<ESC> to exit\n");
  textattr(11);
  Printf("[ = down one user  ] = up one user\n");
  Printf("{ = down 10 user   } = up 10 user\n");
  Printf("<C/R> = edit SL data\n");
  textattr(COLOR_CYAN);
  do {
    app->localIO->LocalGotoXY(0, 18);
    Puts("Command: ");
    char ch = onek("\033[]{}\r");
    switch (ch) {
    case '\r':
      textattr(COLOR_CYAN);
      app->localIO->LocalGotoXY(0, 13);
      app->localIO->LocalClrEol();
      app->localIO->LocalGotoXY(0, 14);
      app->localIO->LocalClrEol();
      app->localIO->LocalGotoXY(0, 15);
      app->localIO->LocalClrEol();
      app->localIO->LocalGotoXY(0, 16);
      app->localIO->LocalClrEol();
      app->localIO->LocalGotoXY(0, 17);
      app->localIO->LocalClrEol();
      edit_user(current_usernum, &user);
      app->localIO->LocalGotoXY(0, 13);
      textattr(COLOR_CYAN);
      Printf("[ = down one user  ] = up one user\n");
      Printf("{ = down 10 user   } = up 10 user\n");
      Printf("<C/R> = edit SL data\n");
      textattr(COLOR_CYAN);
      break;
    case '\033':
      done = true;
      break;
    case ']':
      current_usernum++;
      current_usernum %= (number_userrecs() + 1);
      read_user(current_usernum, &user);
      show_user(current_usernum, &user);
      break;
    case '[':
      current_usernum--;
      current_usernum %= (number_userrecs() + 1);
      read_user(current_usernum, &user);
      show_user(current_usernum, &user);
      break;
    case '}':
      current_usernum += 10;
      current_usernum %= (number_userrecs() + 1);
      read_user(current_usernum, &user);
      show_user(current_usernum, &user);
      break;
    case '{':
      current_usernum -= 10;
      current_usernum %= (number_userrecs() + 1);
      read_user(current_usernum, &user);
      show_user(current_usernum, &user);
      break;
    }
  } while (!done);
}

