/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*               Copyright (C)2014-2020, WWIV Software Services           */
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
#include "fmt/format.h"
#include "fmt/printf.h"
#include "localui/edit_items.h"
#include "localui/input.h"
#include "localui/listbox.h"
#include "localui/wwiv_curses.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/names.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
#include "sdk/vardec.h"
#include "wwivconfig/utility.h"
#include <cstdint>
#include <string>
#include <vector>

static constexpr int LABEL_WIDTH = 14;
static constexpr int COL1_LINE = 2;
static constexpr int COL1_POSITION = COL1_LINE + LABEL_WIDTH + 1;
static constexpr int COL2_POSITION = 50;
static constexpr int DSL_LABEL_WIDTH = 4;
static constexpr int DSL_LABEL = COL1_POSITION + DSL_LABEL_WIDTH + 1;
static constexpr int DSL_POSITION = DSL_LABEL + DSL_LABEL_WIDTH + 1;

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
  items->window()->PutsXY(COL2_POSITION, y++, StrCat("Today Calls  : ", static_cast<unsigned>(user->ontoday)));
  items->window()->PutsXY(COL2_POSITION, y++, StrCat("Bad Logins   : ", static_cast<unsigned>(user->illegal)));
  y++;
  items->window()->PutsXY(COL2_POSITION, y++, StrCat("Num of Posts : ", user->msgpost));
  items->window()->PutsXY(COL2_POSITION, y++, StrCat("Num of Emails: ", user->emailsent));
  items->window()->PutsXY(COL2_POSITION, y++, StrCat("Feedback Sent: ", user->feedbacksent));
  items->window()->PutsXY(COL2_POSITION, y++, StrCat("Msgs Waiting : ", static_cast<unsigned>(user->waiting)));
  items->window()->PutsXY(COL2_POSITION, y++, StrCat("Netmail Sent : ", user->emailnet));
  items->window()->PutsXY(COL2_POSITION, y, StrCat("Deleted Posts: ", user->deletedposts));

  items->Display();
}

static void show_error_no_users(CursesWindow* window) {
  messagebox(window, "You must have users added before using user editor.");
}

static vector<HelpItem> create_extra_help_items() {
  vector<HelpItem> help_items = {{"D", "Delete"}, {"J", "Jump"}, {"R", "Restore"}};
  return help_items;
}

static int JumpToUser(CursesWindow* window, const std::string& datadir) {
  vector<ListBoxItem> items;
  {
    DataFile<smalrec> file(FilePath(datadir, NAMES_LST), File::modeReadOnly | File::modeBinary,
                           File::shareDenyWrite);
    if (!file) {
      show_error_no_users(window);
      return -1;
    }

    const auto num_records = file.number_of_records();
    for (auto i = 0; i < num_records; i++) {
      smalrec name{};
      if (!file.Read(&name)) {
        messagebox(window, "Error reading smalrec");
        return -1;
      }
      items.emplace_back(fmt::format("{} #{}", name.name, name.number), 0, name.number);
    }
  }

  ListBox list(window, "Select User", items);
  const auto result = list.Run();
  if (result.type == ListBoxResultType::SELECTION) {
    return items[result.selected].data();
  }
  return -1;
}

void user_editor(const wwiv::sdk::Config& config) {
  auto number_users = number_userrecs(config.datadir());
  curses_out->Cls(ACS_CKBOARD);
  auto need_names_list_rebuilt{false};

  if (number_users < 1) {
    unique_ptr<CursesWindow> window(curses_out->CreateBoxedWindow("User Editor", 18, 76));
    show_error_no_users(window.get());
    return;
  }

  auto current_usernum = 1;
  userrec user{};
  read_user(config, current_usernum, &user);

  auto user_name_field =
      new StringEditItem<unsigned char*>(COL1_POSITION, 1, 30, user.name, EditLineMode::UPPER_ONLY);
  user_name_field->set_displayfn(
      // Not sure why but fmt::format("{} #{}"...) was crashing on linux.
      [&]() -> string { return StrCat(user.name, " #", current_usernum); });

  auto birthday_field = new CustomEditItem(
      COL1_POSITION, 8, 10,
      [&]() -> string {
        return fmt::sprintf("%2.2d/%2.2d/%4.4d", user.month, user.day, user.year + 1900);
      },
      [&](const string& s) {
        if (s[2] != '/' || s[5] != '/') {
          return;
        }
        const auto month = to_number<uint8_t>(s.substr(0, 2));
        if (month < 1 || month > 12) {
          return;
        }
        const auto day = to_number<uint8_t>(s.substr(3, 2));
        if (day < 1 || day > 31) {
          return;
        }
        const auto year = to_number<int>(s.substr(6, 4));
        const auto dt = DateTime::now();
        const auto current_year = dt.year();
        if (year < 1900 || year > current_year) {
          return;
        }
        user.month = month;
        user.day = day;
        user.year = static_cast<uint8_t>(year - 1900);
      });

  int y = 1;
  EditItems items{};
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Name/Handle:"), user_name_field);
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Real Name:"),
            new StringEditItem<unsigned char*>(COL1_POSITION, 2, 20, user.realname, EditLineMode::ALL));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "SL:"),
      new NumberEditItem<uint8_t>(COL1_POSITION, 3, &user.sl));
  y++;
  items.add(new Label(DSL_LABEL, y, DSL_LABEL_WIDTH, "DSL:"),
      new NumberEditItem<uint8_t>(DSL_POSITION, 3, &user.dsl));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Address:"),
      new StringEditItem<char*>(COL1_POSITION, 4, 30, user.street, EditLineMode::ALL));
  y++;
  items.add(  new Label(COL1_LINE, y, LABEL_WIDTH, "City:"),
      new StringEditItem<char*>(COL1_POSITION, 5, 30, user.city, EditLineMode::ALL));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "State:"),
      new StringEditItem<char*>(COL1_POSITION, 6, 2, user.state, EditLineMode::ALL));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Postal Code:"),
      new StringEditItem<char*>(COL1_POSITION, 7, 10, user.zipcode, EditLineMode::UPPER_ONLY));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Birthday:"),
      birthday_field);
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Password:"),
      new StringEditItem<char*>(COL1_POSITION, 9, 8, user.pw, EditLineMode::UPPER_ONLY));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Phone Number:"),
      new StringEditItem<char*>(COL1_POSITION, 10, 12, user.phone, EditLineMode::UPPER_ONLY));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Data Number:"),
      new StringEditItem<char*>(COL1_POSITION, 11, 12, user.dataphone, EditLineMode::UPPER_ONLY));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Computer Type:"),
      new NumberEditItem<int8_t>(COL1_POSITION, 12, &user.comp_type));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Restrictions:"),
      new RestrictionsEditItem(COL1_POSITION, 13, &user.restrict));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "WWIV Reg:"),
      new NumberEditItem<uint32_t>(COL1_POSITION, 14, &user.wwiv_regnum));
  y++;
  items.add( new Label(COL1_LINE, y, LABEL_WIDTH, "Email Address:"),
      new StringEditItem<char*>(COL1_POSITION, 15, 57, user.email, EditLineMode::ALL));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Sysop Note:"),
      new StringEditItem<char*>(COL1_POSITION, 16, 57, user.note, EditLineMode::ALL));
  items.set_navigation_extra_help_items(create_extra_help_items());

  items.create_window("User Editor");
  items.Display();
  show_user(&items, &user);

  const NavigationKeyConfig nav("DJR\r");
  for (;;) {
    const auto ch = items.GetKeyWithNavigation(nav);
    switch (ch) {
    case '\r': {
      if (IsUserDeleted(&user)) {
        items.window()->SetColor(SchemeId::ERROR_TEXT);
        messagebox(items.window(), "Can not edit a deleted user.");
      } else {
        items.Run();
        if (dialog_yn(items.window(), "Save User?")) {
          write_user(config, current_usernum, &user);
          need_names_list_rebuilt = true;
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
      need_names_list_rebuilt = true;
    } break;
    case 'J': {
      auto user_number = JumpToUser(items.window(), config.datadir());
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
      need_names_list_rebuilt = true;
    } break;
    case 'Q': {
      if (need_names_list_rebuilt) {
        // If we made changes here, let's be safe and rebuild
        // the names.lst file.
        wwiv::sdk::UserManager um(config);
        wwiv::sdk::Names names(config);
        names.set_save_on_exit(false);
        names.Rebuild(um);
        names.Save();
      }      
      return;
    }
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
    default:
      continue;
    }

    read_user(config, current_usernum, &user);
    show_user(&items, &user);
  }

}
