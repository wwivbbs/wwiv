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
#include <cstdint>
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

int editline(unsigned char *s, int len) {
  int return_code = 0;
  editline(reinterpret_cast<char*>(s), len, ALL, &return_code, "");
  return return_code;
}

int editline(char *s, int len) {
  int return_code = 0;
  editline(s, len, ALL, &return_code, "");
  return return_code;
}

int editline_byte(uint8_t* value) {
  char s[21];
  int return_code = 0;
  sprintf(s, "%-3u", *value);
  editline(s, 3, NUM_ONLY, &return_code, "");
  *value = atoi(s);
  return return_code;
}

int editline_int(uint32_t* value) {
  char s[21];
  int return_code = 0;
  sprintf(s, "%-7u", *value);
  editline(s, 7, NUM_ONLY, &return_code, "");
  *value = atoi(s);
  return return_code;
}

void show_user(int user_number, const userrec *user) {
  textattr(COLOR_CYAN);
  PrintfY(0, "%-30s %-4d", user->name, user_number);
  PrintfY(1, "%-20s", user->realname);
  PrintfY(2, "%-3d", user->sl);
  PrintfY(3, "%-3d", user->dsl);
  PrintfY(4, "%-30s", user->street);
  PrintfY(5, "%-30s", user->city);
  PrintfY(6, "%-2s", user->state);
  PrintfY(7, "%-10s", user->zipcode);
  PrintfY(8, "%2.2d/%2.2d/%4.4d", user->month, user->day, user->year + 1900);
  PrintfY(9, "%-8s", user->pw);
  PrintfY(10, "%-12s", user->phone);
  PrintfY(11, "%-12s", user->dataphone);
  PrintfY(12, "%-3d", user->comp_type);
  PrintfY(13, "%-12d", user->wwiv_regnum);
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
      i1 = editline(user->name, 30);
    } break;
    case 1:
      i1 = editline(user->realname, 20);
      break;
    case 2:
      i1 = editline_byte(&user->sl);
      break;
    case 3:
      i1 = editline_byte(&user->dsl);
      break;
    case 4:
      i1 = editline(user->street, 30);
      break;
    case 5:
      i1 = editline(user->city, 10);
      break;
    case 6:
      i1 = editline(user->state, 2);
      break;
    case 7:
      i1 = editline(user->zipcode, 10);
      break;
    case 8: {
      char birthday[81];
      sprintf(birthday, "%2.2d/%2.2d/%4.4d", user->month, user->day, user->year + 1900);
      i1 = editline(birthday, 10);
    } break;
    case 9:
      i1 = editline(user->pw, 8);
      break;
    case 10:
      i1 = editline(user->phone, 12);
      break;
    case 11:
      i1 = editline(user->dataphone, 12);
      break;
    case 12: {
      uint8_t c = user->comp_type;
      i1 = editline_byte(&c);
      user->comp_type = c;
    } break;
    case 13:
      i1 = editline_int(&user->wwiv_regnum);
      break;
    default:
      break;
    }
    cp = GetNextSelectionPosition(0, 13, cp, i1);
    if (i1 == DONE) {

      // write_user(user_number, &user);
      done = true;
    }
  } while (!done);
}


static void show_help() {
  app->localIO->LocalGotoXY(0, 14);
  textattr(COLOR_YELLOW);
  Puts("\n<ESC> to exit\n");
  textattr(11);
  Printf("[ = down one user  ] = up one user\n");
  Printf("{ = down 10 user   } = up 10 user\n");
  Printf("<C/R> = edit SL data\n");
  textattr(COLOR_CYAN);
}

static void clear_help() {
  textattr(COLOR_CYAN);
  for (int y = 14; y <= 17; y++) {
    app->localIO->LocalGotoXY(0, y);
    app->localIO->LocalClrEol();
  }
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

  unsigned short int current_usernum = 1;
  userrec user;
  read_user(current_usernum, &user);
  show_user(current_usernum, &user);
  bool done = false;

  show_help();
  do {
    app->localIO->LocalGotoXY(0, 20);
    Puts("Command: ");
    char ch = onek("\033Q[]{}\r");
    switch (ch) {
    case '\r':
      clear_help();
      edit_user(current_usernum, &user);
      app->localIO->LocalGotoXY(0, 20);
      textattr(COLOR_YELLOW);
      Puts("Save User?");
      if (yn()) {
        write_user(current_usernum, &user);
      }
      app->localIO->LocalGotoXY(0, 20);
      app->localIO->LocalClrEol();
      break;
    case 'Q':
    case '\033':
      done = true;
      return;
    case ']':
      if (++current_usernum > number_userrecs()) {
        current_usernum = 1;
      }
      break;
    case '[': {
      if (--current_usernum < 1) {
        current_usernum = number_userrecs();
      }
    } break;
    case '}':
      current_usernum += 10;
      if (current_usernum > number_userrecs()) {
        current_usernum = 1;
      }
      break;
    case '{':
      current_usernum -= 10;
      if (current_usernum < 1) {
        current_usernum = number_userrecs();
      }
      break;
    }

    read_user(current_usernum, &user);
    show_user(current_usernum, &user);

  } while (!done);
}

