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
#include <string>

#include "ifcns.h"
#include "init.h"
#include "input.h"
#include "wwivinit.h"

static const int COL1_POSITION = 19;
static const int COL2_POSITION = 50;
static const int PROMPT_LINE = 22;

static bool IsUserDeleted(userrec *user) {
  return user->inact & inact_deleted;
}

void show_user(EditItems* items, userrec* user) {
  items->Display();

  for (int i=0; i<13; i++) {
    std::string blank(30, ' ');
    app->localIO->LocalXYPuts(50, i, blank.c_str());
  }
  textattr(COLOR_CYAN);
  if (user->inact & inact_deleted) {
    textattr(COLOR_RED);
    PutsXY(COL2_POSITION, 0, "[[ DELETED USER ]]");
  } else if (user->inact & inact_inactive) {
    textattr(COLOR_RED);
    PutsXY(COL2_POSITION, 0, "[[ INACTIVE USER ]]");
  }
  textattr(COLOR_CYAN);
  int y = 1;
  PrintfXY(COL2_POSITION, y++, "First on         : %s", user->firston);
  PrintfXY(COL2_POSITION, y++, "Last on          : %s", user->laston);
  y++;
  PrintfXY(COL2_POSITION, y++, "Total Calls      : %d", user->logons);
  PrintfXY(COL2_POSITION, y++, "Today Calls      : %d", user->ontoday);
  PrintfXY(COL2_POSITION, y++, "Illegal Logons   : %d", user->illegal);
  y++;
  PrintfXY(COL2_POSITION, y++, "Number of Posts  : %d", user->msgpost);
  PrintfXY(COL2_POSITION, y++, "Number of Emails : %d", user->emailsent);
  PrintfXY(COL2_POSITION, y++, "Num Feedback Sent: %d", user->feedbacksent);
  PrintfXY(COL2_POSITION, y++, "Messages Waiting : %d", user->waiting);
  PrintfXY(COL2_POSITION, y++, "Net Email Sent   : %d", user->emailnet);
  PrintfXY(COL2_POSITION, y++, "Num Deleted Posts: %d", user->deletedposts);
}

static void show_help(int start_line) {
  textattr(COLOR_YELLOW);
  PutsXY(0, start_line, "\n<ESC> to exit\n");
  textattr(COLOR_CYAN);
  Printf("[ = down one user  ] = up one user\n");
  Printf("{ = down 10 user   } = up 10 user\n");
  Printf("<C/R> = edit SL data\n");
  textattr(COLOR_CYAN);
}

static void clear_help(int start_line) {
  textattr(COLOR_CYAN);
  for (int y = start_line; y <= PROMPT_LINE-1; y++) {
    app->localIO->LocalGotoXY(0, y);
    app->localIO->LocalClrEol();
  }
}

static void show_error_no_users() {
  textattr(COLOR_RED);
  Printf("You must have users added before using user editor.");
  Printf("\n\n");
  pausescr();
}

void user_editor() {
  int number_users = number_userrecs();
  app->localIO->LocalCls();
  if (number_users < 1) {
    show_error_no_users();
    return;
  }

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
  Printf("Sysop Note       : \n");

  unsigned short int current_usernum = 1;
  userrec user;
  read_user(current_usernum, &user);

  auto user_name_field = new StringEditItem<unsigned char*>(COL1_POSITION, 0, 30, user.name, true);
  user_name_field->set_displayfn([&]() -> std::string {
    char name[81];
    sprintf(name, "%s #%d", user.name, current_usernum);
    return std::string(name);
  });

  auto birthday_field = new CustomEditItem(COL1_POSITION, 8, 10, 
      [&user]() -> std::string { 
        char birthday[81];
        sprintf(birthday, "%2.2d/%2.2d/%4.4d", user.month, user.day, user.year + 1900);
        return std::string(birthday);
      },
      [&user](const std::string& s) {
        if (s[3] != '/' || s[6] != '/') {
          return;
        }
        int month = std::stoi(s.substr(0, 2));
        if (month < 1 || month > 12) { return; }
        int day = std::stoi(s.substr(3, 2));
        if (day < 1 || day > 31) { return; }
        int year = std::stoi(s.substr(6, 4));
        if (year < 1900 || year > 2014) { return ; }

        user.month = month;
        user.day = day;
        user.year = year - 1900;
      });


  EditItems items{
    user_name_field,
    new StringEditItem<unsigned char*>(COL1_POSITION, 1, 20, user.realname, false),
    new NumberEditItem<uint8_t>(COL1_POSITION, 2, &user.sl),
    new NumberEditItem<uint8_t>(COL1_POSITION, 3, &user.dsl),
    new StringEditItem<unsigned char*>(COL1_POSITION, 4, 30, user.street, false),
    new StringEditItem<unsigned char*>(COL1_POSITION, 5, 30, user.city, false),
    new StringEditItem<unsigned char*>(COL1_POSITION, 6, 2, user.state, false),
    new StringEditItem<unsigned char*>(COL1_POSITION, 7, 10, user.zipcode, true),
    birthday_field,
    new StringEditItem<unsigned char*>(COL1_POSITION, 9, 8, user.pw, true),
    new StringEditItem<unsigned char*>(COL1_POSITION, 10, 12, user.phone, true),
    new StringEditItem<unsigned char*>(COL1_POSITION, 11, 12, user.dataphone, true),
    new NumberEditItem<int8_t>(COL1_POSITION, 12, &user.comp_type),
    new NumberEditItem<uint32_t>(COL1_POSITION, 13, &user.wwiv_regnum),
    new StringEditItem<unsigned char*>(COL1_POSITION, 14, 60, user.note, false),
  };

  show_user(&items, &user);

  for (;;)  {
    show_help(14 + 1);
    PutsXY(0, PROMPT_LINE, "Command: ");
    char ch = onek("\033Q[]{}\r");
    switch (ch) {
    case '\r': {
      if (IsUserDeleted(&user)) {
        textattr(COLOR_RED);
        PutsXY(0, PROMPT_LINE, "Can not edit a deleted user.\n\n");
        pausescr();
      } else {
        clear_help(14 + 1);
        items.Run();
        if (dialog_yn("Save User")) {
          write_user(current_usernum, &user);
        }
      }
      app->localIO->LocalGotoXY(0, PROMPT_LINE);
      app->localIO->LocalClrEol();
    } break;
    case 'Q':
    case '\033':
      return;
    case ']':
      if (++current_usernum > number_users) {
        current_usernum = 1;
      }
      break;
    case '[': {
      if (--current_usernum < 1) {
        current_usernum = number_users;
      }
    } break;
    case '}':
      current_usernum += 10;
      if (current_usernum > number_users) {
        current_usernum = 1;
      }
      break;
    case '{':
      current_usernum -= 10;
      if (current_usernum < 1) {
        current_usernum = number_users;
      }
      break;
    }

    read_user(current_usernum, &user);
    show_user(&items, &user);
  }
}

