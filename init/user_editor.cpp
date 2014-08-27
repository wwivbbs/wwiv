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
#include <ctime>
#include <string>
#include <vector>

#include "ifcns.h"
#include "init.h"
#include "input.h"
#include "utility.h"
#include "wwivinit.h"

static const int COL1_POSITION = 19;
static const int COL2_POSITION = 50;
static const int PROMPT_LINE = 18;

using std::vector;

static bool IsUserDeleted(userrec *user) {
  return user->inact & inact_deleted;
}

void show_user(EditItems* items, userrec* user) {
  items->Display();

  for (int i=0; i<13; i++) {
    std::string blank(30, ' ');
    out->window()->PutsXY(50, i, blank.c_str());
  }
  out->window()->SetColor(out->color_scheme(), SchemeId::NORMAL);
  if (user->inact & inact_deleted) {
    out->window()->SetColor(out->color_scheme(), SchemeId::ERROR_TEXT);
    out->window()->PutsXY(COL2_POSITION, 0, "[[ DELETED USER ]]");
  } else if (user->inact & inact_inactive) {
    out->window()->SetColor(out->color_scheme(), SchemeId::ERROR_TEXT);
    out->window()->PutsXY(COL2_POSITION, 0, "[[ INACTIVE USER ]]");
  }
  out->window()->SetColor(out->color_scheme(), SchemeId::NORMAL);
  int y = 2;
 out->window()->PrintfXY(COL2_POSITION, y++, "First on     : %s", user->firston);
 out->window()->PrintfXY(COL2_POSITION, y++, "Last on      : %s", user->laston);
  y++;
 out->window()->PrintfXY(COL2_POSITION, y++, "Total Calls  : %d", user->logons);
 out->window()->PrintfXY(COL2_POSITION, y++, "Today Calls  : %d", user->ontoday);
 out->window()->PrintfXY(COL2_POSITION, y++, "Bad Logins   : %d", user->illegal);
  y++;
 out->window()->PrintfXY(COL2_POSITION, y++, "Num of Posts : %d", user->msgpost);
 out->window()->PrintfXY(COL2_POSITION, y++, "Num of Emails: %d", user->emailsent);
 out->window()->PrintfXY(COL2_POSITION, y++, "Feedback Sent: %d", user->feedbacksent);
 out->window()->PrintfXY(COL2_POSITION, y++, "Msgs Waiting : %d", user->waiting);
 out->window()->PrintfXY(COL2_POSITION, y++, "Netmail Sent : %d", user->emailnet);
 out->window()->PrintfXY(COL2_POSITION, y++, "Deleted Posts: %d", user->deletedposts);
}

static void show_error_no_users() {
  out->window()->SetColor(out->color_scheme(), SchemeId::ERROR_TEXT);
  out->window()->Printf("You must have users added before using user editor.");
  out->window()->Printf("\n\n");
  pausescr(out->window());
}

static vector<HelpItem> create_help_items() {
  vector<HelpItem> help_items = EditItems::StandardNavigationHelpItems();
  help_items.push_back({ "A", "Add" });
  return help_items;
}

void user_editor() {
  int number_users = number_userrecs();
  out->Cls();
  if (number_users < 1) {
    show_error_no_users();
    return;
  }

  out->window()->SetColor(out->color_scheme(), SchemeId::NORMAL);
  out->window()->GotoXY(0, 1);
  out->window()->Printf("    Name/Handle  : \n");
  out->window()->Printf("    Real Name    : \n");
  out->window()->Printf("    SL           : \n");
  out->window()->Printf("    DSL          : \n");
  out->window()->Printf("    Address      : \n");
  out->window()->Printf("    City         : \n");
  out->window()->Printf("    State        : \n");
  out->window()->Printf("    Postal Code  : \n");
  out->window()->Printf("    Birthday     : \n");
  out->window()->Printf("    Password     : \n");
  out->window()->Printf("    Phone Number : \n");
  out->window()->Printf("    Data Number  : \n");
  out->window()->Printf("    Computer Type: \n");
  out->window()->Printf("    WWIV Reg     : \n");
  out->window()->Printf("    Sysop Note   : \n");

  int current_usernum = 1;
  userrec user;
  read_user(current_usernum, &user);

  auto user_name_field = new StringEditItem<unsigned char*>(COL1_POSITION, 1, 30, user.name, true);
  user_name_field->set_displayfn([&]() -> std::string {
    char name[81];
    sprintf(name, "%s #%d", user.name, current_usernum);
    return std::string(name);
  });

  auto birthday_field = new CustomEditItem(COL1_POSITION, 9, 10, 
      [&user]() -> std::string { 
        char birthday[81];
        sprintf(birthday, "%2.2d/%2.2d/%4.4d", user.month, user.day, user.year + 1900);
        return std::string(birthday);
      },
      [&user](const std::string& s) {
        if (s[2] != '/' || s[5] != '/') {
          return;
        }
        time_t t;
        time(&t);
        struct tm * pTm = localtime(&t);
        int current_year = pTm->tm_year+1900;
        int month = std::stoi(s.substr(0, 2));
        if (month < 1 || month > 12) { return; }
        int day = std::stoi(s.substr(3, 2));
        if (day < 1 || day > 31) { return; }
        int year = std::stoi(s.substr(6, 4));
        if (year < 1900 || year > current_year) { return ; }

        user.month = month;
        user.day = day;
        user.year = year - 1900;
      });

  EditItems items{
    user_name_field,
    new StringEditItem<unsigned char*>(COL1_POSITION, 2, 20, user.realname, false),
    new NumberEditItem<uint8_t>(COL1_POSITION, 3, &user.sl),
    new NumberEditItem<uint8_t>(COL1_POSITION, 4, &user.dsl),
    new StringEditItem<unsigned char*>(COL1_POSITION, 5, 30, user.street, false),
    new StringEditItem<unsigned char*>(COL1_POSITION, 6, 30, user.city, false),
    new StringEditItem<unsigned char*>(COL1_POSITION, 7, 2, user.state, false),
    new StringEditItem<unsigned char*>(COL1_POSITION, 8, 10, user.zipcode, true),
    birthday_field,
    new StringEditItem<unsigned char*>(COL1_POSITION, 10, 8, user.pw, true),
    new StringEditItem<unsigned char*>(COL1_POSITION, 11, 12, user.phone, true),
    new StringEditItem<unsigned char*>(COL1_POSITION, 12, 12, user.dataphone, true),
    new NumberEditItem<int8_t>(COL1_POSITION, 13, &user.comp_type),
    new NumberEditItem<uint32_t>(COL1_POSITION, 14, &user.wwiv_regnum),
    new StringEditItem<unsigned char*>(COL1_POSITION, 15, 60, user.note, false),
  };
  items.set_navigation_help_items(create_help_items());
  items.set_curses_io(out, out->window());

  show_user(&items, &user);

  for (;;)  {
    char ch = onek(out->window(), "\033Q[]{}\r");
    switch (ch) {
    case '\r': {
      if (IsUserDeleted(&user)) {
        out->window()->SetColor(out->color_scheme(), SchemeId::ERROR_TEXT);
        out->window()->PutsXY(0, PROMPT_LINE, "Can not edit a deleted user.\n\n");
        pausescr(out->window());
      } else {
        items.Run();
        if (dialog_yn(out->window(), "Save User")) {
          write_user(current_usernum, &user);
        }
      }
      out->window()->Move(PROMPT_LINE, 0); 
      out->window()->ClrtoEol();
      out->window()->Refresh();
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
        current_usernum = number_users;
      }
      break;
    case '{':
      current_usernum -= 10;
      if (current_usernum < 1) {
        current_usernum = 1;
      }
      break;
    }

    read_user(current_usernum, &user);
    show_user(&items, &user);
  }
}

