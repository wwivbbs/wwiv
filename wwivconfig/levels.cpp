/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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
#include "core/strings.h"
#include "core/wwivport.h"
#include "fmt/format.h"
#include "localui/edit_items.h"
#include "localui/input.h"
#include "localui/listbox.h"
#include "localui/wwiv_curses.h"
#include "sdk/vardec.h"
#include <memory>
#include <string>

using std::string;
using std::unique_ptr;
using std::vector;
using namespace wwiv::sdk;
using namespace wwiv::strings;

static constexpr int MAX_SL = 255;
static constexpr int MIN_SL = 0;

static vector<HelpItem> create_extra_help_items() {
  vector<HelpItem> help_items = {{"J", "Jump"}};
  return help_items;
}

static uint8_t JumpToSl(CursesWindow* window) {
  vector<ListBoxItem> items;
  for (int i = MIN_SL; i < MAX_SL; i++) {
    items.emplace_back(fmt::format("SL #{}", i), 0, i);
  }

  ListBox list(window, "Select SL", items);
  const auto result = list.Run();
  if (result.type == ListBoxResultType::SELECTION) {
    return static_cast<uint8_t>(items[result.selected].data());
  }
  return 0;
}

void sec_levs(Config& config) {
  uint8_t cursl = 10;
  auto sl = config.sl(cursl);
  EditItems items{};
  auto y = 1;
  items.add(new Label("Security level:"),
      new NumberEditItem<uint8_t>(&cursl), "", 1, y);
  ++y;
  items.add(new Label("Time per day:"),
      new NumberEditItem<uint16_t>(&sl.time_per_day), "", 1, y);
  ++y;
  items.add(new Label("Time per logon:"),
      new NumberEditItem<uint16_t>(&sl.time_per_logon), "", 1, y);
  ++y;
  items.add(new Label("Messages read:"),
      new NumberEditItem<uint16_t>(&sl.messages_read), "", 1, y);
  ++y;
  items.add(new Label("Emails per day:"),
      new NumberEditItem<uint16_t>(&sl.emails), "", 1, y);
  ++y;
  items.add(new Label("Posts per day:"),
      new NumberEditItem<uint16_t>(&sl.posts), "", 1, y);
  ++y;
  items.add(new Label("Post anony:"),
      new FlagEditItem<uint32_t>(ability_post_anony, "Yes", "No ",
                                 &sl.ability), "", 1, y);
  ++y;
  items.add(new Label("Email anony:"),
      new FlagEditItem<uint32_t>(ability_email_anony, "Yes", "No ",
                                 &sl.ability), "", 1, y);
  ++y;
  items.add(new Label("Read anony posts:"),
      new FlagEditItem<uint32_t>(ability_read_post_anony, "Yes", "No ",
                                 &sl.ability), "", 1, y);
  ++y;
  items.add(new Label("Read anony email:"),
      new FlagEditItem<uint32_t>(ability_read_email_anony, "Yes", "No ",
                                 &sl.ability), "", 1, y);
  ++y;
  items.add(new Label("Limited co-sysop:"),
      new FlagEditItem<uint32_t>(ability_limited_cosysop, "Yes", "No ",
                                 &sl.ability), "", 1, y);
  ++y;
  items.add(new Label("Co-sysop:"),
      new FlagEditItem<uint32_t>(ability_cosysop, "Yes", "No ",
                                 &sl.ability), "", 1, y);

  items.relayout_items_and_labels();
  items.set_navigation_extra_help_items(create_extra_help_items());
  curses_out->Cls(ACS_CKBOARD);
  items.create_window("Security Level Editor");
  items.Display();

  const NavigationKeyConfig nav("J\r");

  for (;;) {
    const auto ch = items.GetKeyWithNavigation(nav);
    switch (ch) {
    case '\r': {
      items.Run();
      config.sl(cursl, sl);
      items.window()->Refresh();
    } break;
    case 'J': {
      cursl = JumpToSl(items.window());;
    } break;
    case 'Q':
      return;
    case ']':
      if (++cursl > MAX_SL) {
        cursl = MIN_SL;
      }
      break;
    case '[': {
      // Wraps around at 0 == 255
      --cursl;
    } break;
    case '}':
      cursl += 10;
      if (cursl > MAX_SL) {
        cursl = MIN_SL;
      }
      break;
    case '{':
      // Auto wraps
      cursl -= 10;
      break;
    default:
      continue;
    }
    sl = config.sl(cursl);
    items.Display();
  }
}
