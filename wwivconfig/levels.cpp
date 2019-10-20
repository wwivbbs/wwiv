/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*             Copyright (C)1998-2019, WWIV Software Services             */
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
#include "localui/input.h"
#include "localui/listbox.h"
#include "localui/wwiv_curses.h"
#include "sdk/vardec.h"
#include "wwivconfig/utility.h"
#include <memory>
#include <string>

using std::string;
using std::unique_ptr;
using std::vector;
using namespace wwiv::sdk;
using namespace wwiv::strings;

static constexpr int MAX_SL = 255;
static constexpr int MIN_SL = 0;
static constexpr int COL1_POSITION = 21;
static constexpr int LABEL1_POSITION = 2;
static constexpr int LABEL1_WIDTH = 18;

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
  auto result = list.Run();
  if (result.type == ListBoxResultType::SELECTION) {
    return static_cast<uint8_t>(items[result.selected].data());
  }
  return 0;
}

void sec_levs(Config& config) {
  uint8_t cursl = 10;
  auto sl = config.sl(cursl);
  EditItems items{};
  items.add_items({
      new NumberEditItem<uint8_t>(COL1_POSITION, 1, &cursl),
      new NumberEditItem<uint16_t>(COL1_POSITION, 2, &sl.time_per_day),
      new NumberEditItem<uint16_t>(COL1_POSITION, 3, &sl.time_per_logon),
      new NumberEditItem<uint16_t>(COL1_POSITION, 4, &sl.messages_read),
      new NumberEditItem<uint16_t>(COL1_POSITION, 5, &sl.emails),
      new NumberEditItem<uint16_t>(COL1_POSITION, 6, &sl.posts),
      new FlagEditItem<uint32_t>(COL1_POSITION, 7, ability_post_anony, "Yes", "No ",
                                 &sl.ability),
      new FlagEditItem<uint32_t>(COL1_POSITION, 8, ability_email_anony, "Yes", "No ",
                                 &sl.ability),
      new FlagEditItem<uint32_t>(COL1_POSITION, 9, ability_read_post_anony, "Yes", "No ",
                                 &sl.ability),
      new FlagEditItem<uint32_t>(COL1_POSITION, 10, ability_read_email_anony, "Yes", "No ",
                                 &sl.ability),
      new FlagEditItem<uint32_t>(COL1_POSITION, 11, ability_limited_cosysop, "Yes", "No ",
                                 &sl.ability),
      new FlagEditItem<uint32_t>(COL1_POSITION, 12, ability_cosysop, "Yes", "No ",
                                 &sl.ability),
  });
  int y = 1;
  items.add_labels({new Label(LABEL1_POSITION, y++, LABEL1_WIDTH, "Security level:"),
                    new Label(LABEL1_POSITION, y++, LABEL1_WIDTH, "Time per day:"),
                    new Label(LABEL1_POSITION, y++, LABEL1_WIDTH, "Time per logon:"),
                    new Label(LABEL1_POSITION, y++, LABEL1_WIDTH, "Messages read:"),
                    new Label(LABEL1_POSITION, y++, LABEL1_WIDTH, "Emails per day:"),
                    new Label(LABEL1_POSITION, y++, LABEL1_WIDTH, "Posts per day:"),
                    new Label(LABEL1_POSITION, y++, LABEL1_WIDTH, "Post anony:"),
                    new Label(LABEL1_POSITION, y++, LABEL1_WIDTH, "Email anony:"),
                    new Label(LABEL1_POSITION, y++, LABEL1_WIDTH, "Read anony posts:"),
                    new Label(LABEL1_POSITION, y++, LABEL1_WIDTH, "Read anony email:"),
                    new Label(LABEL1_POSITION, y++, LABEL1_WIDTH, "Limited co-sysop:"),
                    new Label(LABEL1_POSITION, y++, LABEL1_WIDTH, "Co-sysop:")});

  items.set_navigation_extra_help_items(create_extra_help_items());
  out->Cls(ACS_CKBOARD);
  items.create_window("Security Level Editor");
  items.Display();

  for (;;) {
    auto ch = onek(items.window(), "\033JQ[]{}\r");
    switch (ch) {
    case '\r': {
      items.Run();
      config.sl(cursl, sl);
      items.window()->Refresh();
    } break;
    case 'J': {
      auto sl_number = JumpToSl(items.window());
      if (sl_number >= MIN_SL) {
        cursl = sl_number;
      }
    } break;
    case 'Q':
    case '\033':
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
    }
    sl = config.sl(cursl);
    items.Display();
  }
}
