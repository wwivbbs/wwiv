/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*               Copyright (C)2014-2021, WWIV Software Services           */
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
#include "core/textfile.h"
#include "fmt/format.h"
#include "fmt/printf.h"
#include "localui/edit_items.h"
#include "localui/input.h"
#include "localui/listbox.h"
#include "localui/wwiv_curses.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/instance.h"
#include "sdk/names.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
#include "sdk/vardec.h"
#include "wwivconfig/utility.h"

#include <cstdint>
#include <string>
#include <vector>

static constexpr int NONEDITABLE_DATA_POS = 50;

using namespace wwiv::core;
using namespace wwiv::local::ui;
using namespace wwiv::strings;

static bool IsUserDeleted(userrec* user) { return user->inact & wwiv::sdk::User::userDeleted; }

static void show_user(EditItems* items, userrec* user) {
  items->window()->SetColor(SchemeId::WINDOW_TEXT);
  const auto height = items->window()->GetMaxY() - 2;
  const auto width = items->window()->GetMaxX() - 2;
  const std::string blank(width - NONEDITABLE_DATA_POS, ' ');
  items->window()->SetColor(SchemeId::WINDOW_TEXT);
  for (int i = 1; i < height; i++) {
    items->window()->PutsXY(NONEDITABLE_DATA_POS, i, blank);
  }
  if (IsUserDeleted(user)) {
    items->window()->SetColor(SchemeId::ERROR_TEXT);
    items->window()->PutsXY(NONEDITABLE_DATA_POS, 1, "[[ DELETED USER ]] ");
  } else if (user->inact & wwiv::sdk::User::userInactive) {
    items->window()->SetColor(SchemeId::ERROR_TEXT);
    items->window()->PutsXY(NONEDITABLE_DATA_POS, 1, "[[ INACTIVE USER ]]");
  }
  items->window()->SetColor(SchemeId::WINDOW_TEXT);
  auto y = 2;
  items->window()->PutsXY(NONEDITABLE_DATA_POS, y++, StrCat("First on     : ", user->firston));
  items->window()->PutsXY(NONEDITABLE_DATA_POS, y++, StrCat("Last on      : ", user->laston));
  y++;
  items->window()->PutsXY(NONEDITABLE_DATA_POS, y++, StrCat("Total Calls  : ", user->logons));
  items->window()->PutsXY(NONEDITABLE_DATA_POS, y++, StrCat("Today Calls  : ", static_cast<unsigned>(user->ontoday)));
  items->window()->PutsXY(NONEDITABLE_DATA_POS, y++, StrCat("Bad Logins   : ", static_cast<unsigned>(user->illegal)));
  y++;
  items->window()->PutsXY(NONEDITABLE_DATA_POS, y++, StrCat("Num of Posts : ", user->msgpost));
  items->window()->PutsXY(NONEDITABLE_DATA_POS, y++, StrCat("Num of Emails: ", user->emailsent));
  items->window()->PutsXY(NONEDITABLE_DATA_POS, y++, StrCat("Feedback Sent: ", user->feedbacksent));
  items->window()->PutsXY(NONEDITABLE_DATA_POS, y++, StrCat("Msgs Waiting : ", static_cast<unsigned>(user->waiting)));
  items->window()->PutsXY(NONEDITABLE_DATA_POS, y++, StrCat("Netmail Sent : ", user->emailnet));
  items->window()->PutsXY(NONEDITABLE_DATA_POS, y++, StrCat("Deleted Posts: ", user->deletedposts));
  items->window()->PutsXY(NONEDITABLE_DATA_POS, y,     StrCat("Extra Time   : ", user->extratime));

  items->Display();
}

static void show_error_no_users(CursesWindow* window) {
  messagebox(window, "You must have users added before using user editor.");
}

static std::vector<HelpItem> create_extra_help_items() {
  std::vector<HelpItem> help_items = {{"D", "Delete"}, {"J", "Jump"}, {"R", "Restore"}};
  return help_items;
}

static int JumpToUser(CursesWindow* window, const std::string& datadir) {
  std::vector<ListBoxItem> items;
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
  list.set_help_items({{"Esc", "Exit"}, {"Enter", "Select"}, {"/", "Filter"}, {"I", "Insert"}});
  if (const auto result = list.Run(); result.type == ListBoxResultType::SELECTION) {
    return items[result.selected].data();
  }
  return -1;
}

static void write_semaphore_if_user_online(const wwiv::sdk::Config& config, int current_usernum) {
  wwiv::sdk::Instances instances(config);
  if (!instances) {
    std::cout << "Unable to read Instance information.";
    return;
  }

  for (const auto& inst : instances) {
    if (!inst.online()) {
      continue;
    }
    if (inst.user_number() != current_usernum) {
      continue;
    }
    // we have user.
    const auto path = config.scratch_dir(inst.node_number());
    const auto fn = FilePath(path, "readuser.wwiv");
    if (TextFile tf(fn, "wt"); tf) {
      tf.Write("User edited in wwivconfig usereditor");
    }
    return;
  }
}

void user_editor(const wwiv::sdk::Config& config) {
  auto number_users = number_userrecs(config.datadir());
  curses_out->Cls(ACS_CKBOARD);
  auto need_names_list_rebuilt{false};

  if (number_users < 1) {
    std::unique_ptr<CursesWindow> window(curses_out->CreateBoxedWindow("User Editor", 18, 76));
    show_error_no_users(window.get());
    return;
  }

  auto current_usernum = 1;
  userrec user{};
  read_user(config, current_usernum, &user);

  auto* user_name_field =
      new StringEditItem<unsigned char*>(30, user.name, EditLineMode::UPPER_ONLY);
  user_name_field->set_displayfn(
      // Not sure why but fmt::format("{} #{}"...) was crashing on linux.
      [&]() -> std::string { return StrCat(user.name, " #", current_usernum); });

  auto* birthday_field = new CustomEditItem(
      10,
      [&]() -> std::string {
        return fmt::sprintf("%2.2d/%2.2d/%4.4d", user.month, user.day, user.year + 1900);
      },
      [&](const std::string& s) {
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
        if (const auto current_year = dt.year(); year < 1900 || year > current_year) {
          return;
        }
        user.month = month;
        user.day = day;
        user.year = static_cast<uint8_t>(year - 1900);
      });

  auto* gender_field = new CustomEditItem(
      1,
      [&]() -> std::string {
        return std::string(1, user.sex);
      },
      [&](const std::string& s) {
        if (s.empty()) {
          user.sex = 0;
          return;
        }
        const auto g = to_upper_case_char(s.front());
        user.sex = (g == 'U') ? 0 : g;
      });

  auto y = 1;
  EditItems items{};
  items.add(new Label("Name/Handle:"), user_name_field, "", 1, y);
  y++;
  items.add(new Label("Real Name:"),
            new StringEditItem<unsigned char*>(20, user.realname, EditLineMode::ALL), "", 1, y);

  // SL/DSL on same line now.
  y++;
  items.add(new Label("SL:"),
      new NumberEditItem<uint8_t>(&user.sl), "", 1, y);
  items.add(new Label("DSL:"),
      new NumberEditItem<uint8_t>(&user.dsl), "", 3, y);
  // AR/DAR
  y++;
  items.add(new Label("AR:"), new ArEditItem(&user.ar), 1, y);
  y++;
  items.add(new Label("DAR:"), new ArEditItem(&user.dar), 1, y);

  y++;
  items.add(new Label("Address:"),
      new StringEditItem<char*>(30, user.street, EditLineMode::ALL), "", 1, y);
  y++;
  items.add(  new Label("City:"),
      new StringEditItem<char*>(30, user.city, EditLineMode::ALL), "", 1, y);
  y++;
  items.add(new Label("State:"),
      new StringEditItem<char*>( 2, user.state, EditLineMode::ALL), "", 1, y);
  y++;
  items.add(new Label("Postal Code:"),
      new StringEditItem<char*>(10, user.zipcode, EditLineMode::UPPER_ONLY), "", 1, y);
  y++;
  items.add(new Label("Birthday:"),
      birthday_field, "", 1, y);
  y++;
  items.add(new Label("Password:"),
      new StringEditItem<char*>(8, user.pw, EditLineMode::UPPER_ONLY), "", 1, y);
  y++;
  items.add(new Label("Phone Number:"),
      new StringEditItem<char*>(12, user.phone, EditLineMode::UPPER_ONLY), "", 1, y);
  y++;
  items.add(new Label("Data Number:"),
      new StringEditItem<char*>(12, user.dataphone, EditLineMode::UPPER_ONLY), "", 1, y);
  y++;
  items.add(new Label("Computer Type:"),
      new NumberEditItem<int8_t>(&user.comp_type), "", 1, y);
  y++;
  items.add(new Label("Gender:"),
      gender_field, "Enter (M)ale, (F)emale, (N)eed to ask again, (U)nused", 1, y);
  y++;
  items.add(new Label("Restrictions:"),
      new RestrictionsEditItem(&user.restrict), "", 1, y);
  y++;
  items.add(new Label("WWIV Reg:"),
      new NumberEditItem<uint32_t>(&user.wwiv_regnum), "", 1, y);
  y++;
  items.add( new Label("Email Address:"),
      new StringEditItem<char*>(57, user.email, EditLineMode::ALL), "", 1, y);
  y++;
  items.add(new Label("Sysop Note:"),
      new StringEditItem<char*>(57, user.note, EditLineMode::ALL), "", 1, y);

  items.add_aligned_width_column(1);
  items.relayout_items_and_labels();
  items.set_navigation_extra_help_items(create_extra_help_items());
  items.create_window("User Editor");
  items.Display();
  show_user(&items, &user);

  const NavigationKeyConfig nav("DJR\r");
  for (;;) {
    switch (const auto ch = items.GetKeyWithNavigation(nav); ch) {
    case '\r': {
      if (IsUserDeleted(&user)) {
        items.window()->SetColor(SchemeId::ERROR_TEXT);
        messagebox(items.window(), "Can not edit a deleted user.");
      } else {
        items.Run();
        if (dialog_yn(items.window(), "Save User?")) {
          write_user(config, current_usernum, &user);
          write_semaphore_if_user_online(config, current_usernum);
          need_names_list_rebuilt = true;
        }
      }
      items.window()->Refresh();
    } break;
    case 'D': {
      // Delete user.
      wwiv::sdk::User u(user, current_usernum);
      if (u.deleted()) {
        break;
      }
      if (!dialog_yn(items.window(),
                     StrCat("Are you sure you want to delete ", u.name(), "? "))) {
        break;
      }
      if (wwiv::sdk::UserManager um(config); !um.delete_user(current_usernum)) {
        messagebox(items.window(), "Error trying to restore user.");
      }
      need_names_list_rebuilt = true;
    } break;
    case 'J': {
      if (const auto user_number = JumpToUser(items.window(), config.datadir()); user_number >= 1) {
        current_usernum = user_number;
      }
    } break;
    case 'R': {
      if (wwiv::sdk::User u(user, current_usernum); !u.deleted()) {
        break;
      }
      if (wwiv::sdk::UserManager um(config); !um.restore_user(current_usernum)) {
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
