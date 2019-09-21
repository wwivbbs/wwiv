/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*               Copyright (C)2014-2019, WWIV Software Services           */
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
#include "wwivconfig/user_editor.h"

#include "core/datafile.h"
#include "core/datetime.h"
#include "core/file.h"
#include "core/strings.h"
#include "localui/input.h"
#include "localui/listbox.h"
#include "localui/wwiv_curses.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
#include "sdk/vardec.h"
#include "wwivconfig/utility.h"
#include <cstdint>
#include <string>
#include <vector>

static constexpr int COL1_POSITION = 17;
static constexpr int COL2_POSITION = 50;
static constexpr int COL1_LINE = 2;

using std::string;
using std::unique_ptr;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::strings;

static bool IsUserDeleted(userrec* user) { return user->inact & inact_deleted; }

static void show_user(EditItems* items, userrec* user) {
  items->window()->SetColor(SchemeId::WINDOW_TEXT);
  const auto height = items->window()->GetMaxY() - 2;
  const auto width = items->window()->GetMaxX() - 2;
  const string blank(width - COL2_POSITION, ' ');
  items->window()->SetColor(SchemeId::WINDOW_TEXT);
  for (int i = 1; i < height; i++) {
    items->window()->PutsXY(COL2_POSITION, i, blank);
  }
  if (user->inact & inact_deleted) {
    items->window()->SetColor(SchemeId::ERROR_TEXT);
    items->window()->PutsXY(COL2_POSITION, 1, "[[ DELETED USER ]] ");
  } else if (user->inact & inact_inactive) {
    items->window()->SetColor(SchemeId::ERROR_TEXT);
    items->window()->PutsXY(COL2_POSITION, 1, "[[ INACTIVE USER ]]");
  }
  items->window()->SetColor(SchemeId::WINDOW_TEXT);
  int y = 2;
  items->window()->PutsXY(COL2_POSITION, y++, StrCat("First on     : ", user->firston));
  items->window()->PutsXY(COL2_POSITION, y++, StrCat("Last on      : ", user->laston));
  y++;
  items->window()->PutsXY(COL2_POSITION, y++, StrCat("Total Calls  : ", user->logons));
  items->window()->PutsXY(COL2_POSITION, y++, StrCat("Today Calls  : ", user->ontoday));
  items->window()->PutsXY(COL2_POSITION, y++, StrCat("Bad Logins   : ", user->illegal));
  y++;
  items->window()->PutsXY(COL2_POSITION, y++, StrCat("Num of Posts : ", user->msgpost));
  items->window()->PutsXY(COL2_POSITION, y++, StrCat("Num of Emails: ", user->emailsent));
  items->window()->PutsXY(COL2_POSITION, y++, StrCat("Feedback Sent: ", user->feedbacksent));
  items->window()->PutsXY(COL2_POSITION, y++, StrCat("Msgs Waiting : ", user->waiting));
  items->window()->PutsXY(COL2_POSITION, y++, StrCat("Netmail Sent : ", user->emailnet));
  items->window()->PutsXY(COL2_POSITION, y++, StrCat("Deleted Posts: ", user->deletedposts));

  items->Display();
}

static void show_error_no_users(CursesWindow* window) {
  messagebox(window, "You must have users added before using user editor.");
}

static vector<HelpItem> create_extra_help_items() {
  vector<HelpItem> help_items = {{"D", "Delete"}, {"J", "Jump"}, {"R", "Restore"}};
  return help_items;
}

static const int JumpToUser(CursesWindow* window, const std::string& datadir) {
  vector<ListBoxItem> items;
  {
    DataFile<smalrec> file(PathFilePath(datadir, NAMES_LST), File::modeReadOnly | File::modeBinary,
                           File::shareDenyWrite);
    if (!file) {
      show_error_no_users(window);
      return -1;
    }

    const int num_records = file.number_of_records();
    for (int i = 0; i < num_records; i++) {
      smalrec name;
      if (!file.Read(&name)) {
        messagebox(window, "Error reading smalrec");
        return -1;
      }
      items.emplace_back(StringPrintf("%s #%d", name.name, name.number), 0, name.number);
    }
  }

  ListBox list(window, "Select User", items);
  ListBoxResult result = list.Run();
  if (result.type == ListBoxResultType::SELECTION) {
    return items[result.selected].data();
  }
  return -1;
}

void user_editor(const wwiv::sdk::Config& config) {
  int number_users = number_userrecs(config.datadir());
  out->Cls(ACS_CKBOARD);
  static constexpr int LABEL_WIDTH = 14;

  if (number_users < 1) {
    unique_ptr<CursesWindow> window(out->CreateBoxedWindow("User Editor", 18, 76));
    show_error_no_users(window.get());
    return;
  }

  int current_usernum = 1;
  userrec user;
  read_user(config, current_usernum, &user);

  auto user_name_field =
      new StringEditItem<unsigned char*>(COL1_POSITION, 1, 30, user.name, EditLineMode::UPPER_ONLY);
  user_name_field->set_displayfn(
      [&]() -> string { return StringPrintf("%s #%d", user.name, current_usernum); });

  auto birthday_field = new CustomEditItem(COL1_POSITION, 9, 10,
                                           [&]() -> string {
                                             return StringPrintf("%2.2d/%2.2d/%4.4d", user.month,
                                                                 user.day, user.year + 1900);
                                           },
                                           [&](const string& s) {
                                             if (s[2] != '/' || s[5] != '/') {
                                               return;
                                             }
                                             auto month = to_number<uint8_t>(s.substr(0, 2));
                                             if (month < 1 || month > 12) {
                                               return;
                                             }

                                             auto day = to_number<uint8_t>(s.substr(3, 2));
                                             if (day < 1 || day > 31) {
                                               return;
                                             }

                                             auto year = to_number<int>(s.substr(6, 4));
                                             auto dt = DateTime::now();
                                             auto current_year = dt.year();
                                             if (year < 1900 || year > current_year) {
                                               return;
                                             }

                                             user.month = month;
                                             user.day = day;
                                             user.year = static_cast<uint8_t>(year - 1900);
                                           });

  EditItems items{};
  items.add_items({
      user_name_field,
      new StringEditItem<unsigned char*>(COL1_POSITION, 2, 20, user.realname, EditLineMode::ALL),
      new NumberEditItem<uint8_t>(COL1_POSITION, 3, &user.sl),
      new NumberEditItem<uint8_t>(COL1_POSITION, 4, &user.dsl),
      new StringEditItem<char*>(COL1_POSITION, 5, 30, user.street, EditLineMode::ALL),
      new StringEditItem<char*>(COL1_POSITION, 6, 30, user.city, EditLineMode::ALL),
      new StringEditItem<char*>(COL1_POSITION, 7, 2, user.state, EditLineMode::ALL),
      new StringEditItem<char*>(COL1_POSITION, 8, 10, user.zipcode, EditLineMode::UPPER_ONLY),
      birthday_field,
      new StringEditItem<char*>(COL1_POSITION, 10, 8, user.pw, EditLineMode::UPPER_ONLY),
      new StringEditItem<char*>(COL1_POSITION, 11, 12, user.phone, EditLineMode::UPPER_ONLY),
      new StringEditItem<char*>(COL1_POSITION, 12, 12, user.dataphone, EditLineMode::UPPER_ONLY),
      new NumberEditItem<int8_t>(COL1_POSITION, 13, &user.comp_type),
      new RestrictionsEditItem(COL1_POSITION, 14, &user.restrict),
      new NumberEditItem<uint32_t>(COL1_POSITION, 15, &user.wwiv_regnum),
      new StringEditItem<char*>(COL1_POSITION, 16, 57, user.note, EditLineMode::ALL),
  });
  items.set_navigation_extra_help_items(create_extra_help_items());

  int y = 1;
  items.add_labels({new Label(COL1_LINE, y++, LABEL_WIDTH, "Name/Handle:"),
                    new Label(COL1_LINE, y++, LABEL_WIDTH, "Real Name:"),
                    new Label(COL1_LINE, y++, LABEL_WIDTH, "SL:"),
                    new Label(COL1_LINE, y++, LABEL_WIDTH, "DSL:"),
                    new Label(COL1_LINE, y++, LABEL_WIDTH, "Address:"),
                    new Label(COL1_LINE, y++, LABEL_WIDTH, "City:"),
                    new Label(COL1_LINE, y++, LABEL_WIDTH, "State:"),
                    new Label(COL1_LINE, y++, LABEL_WIDTH, "Postal Code:"),
                    new Label(COL1_LINE, y++, LABEL_WIDTH, "Birthday:"),
                    new Label(COL1_LINE, y++, LABEL_WIDTH, "Password:"),
                    new Label(COL1_LINE, y++, LABEL_WIDTH, "Phone Number:"),
                    new Label(COL1_LINE, y++, LABEL_WIDTH, "Data Number:"),
                    new Label(COL1_LINE, y++, LABEL_WIDTH, "Computer Type:"),
                    new Label(COL1_LINE, y++, LABEL_WIDTH, "Restrictions:"),
                    new Label(COL1_LINE, y++, LABEL_WIDTH, "WWIV Reg:"),
                    new Label(COL1_LINE, y++, LABEL_WIDTH, "Sysop Note:")});

  items.create_window("User Editor");
  items.Display();
  show_user(&items, &user);

  for (;;) {
    char ch = onek(items.window(), "\033DJRQ[]{}\r");
    switch (ch) {
    case '\r': {
      if (IsUserDeleted(&user)) {
        items.window()->SetColor(SchemeId::ERROR_TEXT);
        messagebox(items.window(), "Can not edit a deleted user.");
      } else {
        items.Run();
        if (dialog_yn(items.window(), "Save User?")) {
          write_user(config, current_usernum, &user);
        }
      }
      items.window()->Refresh();
    } break;
    case 'D': {
      // Delete user.
      wwiv::sdk::User u(user);
      if (u.IsUserDeleted()) {
        break;
      }
      if (!dialog_yn(items.window(),
                     StrCat("Are you sure you want to delete ", u.GetName(), "? "))) {
        break;
      }
      wwiv::sdk::UserManager um(config);
      if (!um.delete_user(current_usernum)) {
        messagebox(items.window(), "Error trying to restore user.");
      }
    } break;
    case 'J': {
      int user_number = JumpToUser(items.window(), config.datadir());
      if (user_number >= 1) {
        current_usernum = user_number;
      }
    } break;
    case 'R': {
      // Restore Deleted User.
      wwiv::sdk::User u(user);
      if (!u.IsUserDeleted()) {
        break;
      }
      wwiv::sdk::UserManager um(config);
      if (!um.restore_user(current_usernum)) {
        messagebox(items.window(), "Error trying to restore user.");
      }
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

    read_user(config, current_usernum, &user);
    show_user(&items, &user);
  }
}
