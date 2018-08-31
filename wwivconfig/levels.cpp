/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*             Copyright (C)1998-2017, WWIV Software Services             */
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
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include <memory>
#include <string>
#include <sys/stat.h>

#include "local_io/wconstants.h"
#include "core/strings.h"
#include "core/wwivport.h"
#include "wwivconfig/wwivconfig.h"
#include "wwivconfig/utility.h"
#include "sdk/vardec.h"
#include "localui/input.h"
#include "localui/listbox.h"
#include "localui/wwiv_curses.h"

using std::string;
using std::unique_ptr;
using std::vector;
using namespace wwiv::sdk;
using namespace wwiv::strings;

static const int MAX_SL = 255;
static const int MIN_SL = 0;
static const int COL1_POSITION = 21;
static const int LABEL1_POSITION = 2;
static constexpr int LABEL1_WIDTH = 18;

static vector<HelpItem> create_extra_help_items() {
  vector<HelpItem> help_items = {{"J", "Jump"}};
  return help_items;
}

static const uint8_t JumpToSl(CursesWindow* window) {
  vector<ListBoxItem> items;
  for (int i = MIN_SL; i < MAX_SL; i++) {
    items.emplace_back(StringPrintf("SL #%d", i), 0, i);
  }

  ListBox list(window, "Select SL", items);
  ListBoxResult result = list.Run();
  if (result.type == ListBoxResultType::SELECTION) {
    return static_cast<uint8_t>(items[result.selected].data());
  }
  return 0;
}

void sec_levs(Config& config) {
  uint8_t cursl = 10;
  slrec sl = config.sl(cursl);
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
    char ch = onek(items.window(), "\033JQ[]{}\r");
    switch (ch) {
    case '\r': {
      items.Run();
      config.sl(cursl, sl);
      items.window()->Refresh();
    } break;
    case 'J': {
      uint8_t sl_number = JumpToSl(items.window());
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
      if (--cursl < MIN_SL) {
        cursl = MAX_SL;
      }
    } break;
    case '}':
      cursl += 10;
      if (cursl > MAX_SL) {
        cursl = MIN_SL;
      }
      break;
    case '{':
      cursl -= 10;
      if (cursl < MIN_SL) {
        cursl = MIN_SL;
      }
      break;
    }
    sl = config.sl(cursl);
    items.Display();
  }
}
