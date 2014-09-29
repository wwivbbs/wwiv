/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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
#include <curses.h>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include <memory>
#include <string>
#include <sys/stat.h>

#include "ifcns.h"
#include "init.h"
#include "input.h"
#include "initlib/listbox.h"
#include "bbs/wconstants.h"
#include "wwivinit.h"
#include "core/strings.h"
#include "core/wwivport.h"
#include "utility.h"

using std::string;
using std::vector;
using std::unique_ptr;
using wwiv::strings::StringPrintf;

static const int MAX_SL = 255;
static const int MIN_SL = 0;
static const int COL1_POSITION = 21;
static const int COL1_LINE = 2;

static vector<HelpItem> create_extra_help_items() {
  vector<HelpItem> help_items = { { "J", "Jump" } };
  return help_items;
}

static const int JumpToSl(CursesIO* io, CursesWindow* window) {
 vector<ListBoxItem> items;
  for (int i = MIN_SL; i < MAX_SL; i++) {
    items.emplace_back(StringPrintf("SL #%d", i), 0, i);
  }
  
  ListBox list(io, window, "Select SL", static_cast<int>(floor(window->GetMaxX() * 0.8)), 
    static_cast<int>(floor(window->GetMaxY() * 0.8)), items, out->color_scheme());
  ListBoxResult result = list.Run();
  if (result.type == ListBoxResultType::SELECTION) {
    return items[result.selected].data();
  }
  return -1;
}

void sec_levs() {
  out->Cls(ACS_CKBOARD);
  unique_ptr<CursesWindow> window(out->CreateBoxedWindow("Security Level Editor", 18, 76));
  int y = 1;
  window->PrintfXY(COL1_LINE, y++, "Security level   :");
  window->PrintfXY(COL1_LINE, y++, "Time per day     :");
  window->PrintfXY(COL1_LINE, y++, "Time per logon   :");
  window->PrintfXY(COL1_LINE, y++, "Messages read    :");
  window->PrintfXY(COL1_LINE, y++, "Emails per day   :");
  window->PrintfXY(COL1_LINE, y++, "Posts per day    :");
  window->PrintfXY(COL1_LINE, y++, "Post anony       :");
  window->PrintfXY(COL1_LINE, y++, "Email anony      :");
  window->PrintfXY(COL1_LINE, y++, "Read anony posts :");
  window->PrintfXY(COL1_LINE, y++, "Read anony email :");
  window->PrintfXY(COL1_LINE, y++, "Limited co-sysop :");
  window->PrintfXY(COL1_LINE, y++, "Co-sysop         :");

  uint8_t cursl = 10;
  slrec sl = syscfg.sl[cursl];
  EditItems items{
    new NumberEditItem<uint8_t>(COL1_POSITION, 1, &cursl),
    new NumberEditItem<uint16_t>(COL1_POSITION, 2, &sl.time_per_day),
    new NumberEditItem<uint16_t>(COL1_POSITION, 3, &sl.time_per_logon),
    new NumberEditItem<uint16_t>(COL1_POSITION, 4, &sl.messages_read),
    new NumberEditItem<uint16_t>(COL1_POSITION, 5, &sl.emails),
    new NumberEditItem<uint16_t>(COL1_POSITION, 6, &sl.posts),
    new FlagEditItem<uint32_t>(COL1_POSITION, 7, ability_post_anony, "Yes", "No ", &sl.ability),
    new FlagEditItem<uint32_t>(COL1_POSITION, 8, ability_email_anony, "Yes", "No ", &sl.ability),
    new FlagEditItem<uint32_t>(COL1_POSITION, 9, ability_read_post_anony, "Yes", "No ", &sl.ability),
    new FlagEditItem<uint32_t>(COL1_POSITION, 10, ability_read_email_anony, "Yes", "No ", &sl.ability),
    new FlagEditItem<uint32_t>(COL1_POSITION, 11, ability_limited_cosysop, "Yes", "No ", &sl.ability),
    new FlagEditItem<uint32_t>(COL1_POSITION, 12, ability_cosysop, "Yes", "No ", &sl.ability),
  };

  items.set_navigation_extra_help_items(create_extra_help_items());
  items.set_curses_io(out, window.get());
  items.Display();

  for (;;)  {
    char ch = onek(window.get(), "\033JQ[]{}\r");
    switch (ch) {
    case '\r': {
      items.Run();
      syscfg.sl[cursl] = sl;
      window->Refresh();
    } break;
    case 'J': {
      int sl_number = JumpToSl(out, window.get());
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
    sl = syscfg.sl[cursl];
    items.Display();
  }
  save_config();
}
