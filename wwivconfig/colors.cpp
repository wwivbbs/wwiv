/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*               Copyright (C)2014-2022, WWIV Software Services           */
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
#include "wwivconfig/colors.h"

#include "core/strings.h"
#include "localui/curses_win.h"
#include "localui/edit_items.h"
#include "localui/input.h"
#include "sdk/config.h"

using namespace wwiv::local::ui;
using namespace wwiv::sdk;
using namespace wwiv::strings;

void colors(Config&, color_config_t& c, CursesWindow* ) {

  // Set defaults if needed
  if (c.msg.tear_color.empty()) {
    c.msg.tear_color = "|#5";
  }
  if (c.msg.origin_color.empty()) {
    c.msg.origin_color = "|#5";
  }
  if (c.msg.quote_color.empty()) {
    c.msg.quote_color = "|#1";
  }
  if (c.msg.text_color.empty()) {
    c.msg.text_color = "|#0";
  }
  if (c.msg.kludge_color.empty()) {
    c.msg.kludge_color = "|08";
  }

  constexpr int MAX_STRING_LEN = 10;
  auto y = 1;
  EditItems items{};
  items.add(new Label("Text Color:"),
            new StringEditItem<std::string&>(MAX_STRING_LEN, c.msg.text_color, EditLineMode::ALL),
            "Color for plain message text.", 1, y);
  ++y;
  items.add(new Label("Quote Color:"),
            new StringEditItem<std::string&>(MAX_STRING_LEN, c.msg.quote_color, EditLineMode::ALL),
            "Color for quoted message text.", 1, y);
  ++y;
  items.add(new Label("Kludge Color:"),
            new StringEditItem<std::string&>(MAX_STRING_LEN, c.msg.kludge_color, EditLineMode::ALL),
            "Color for kludge lines in the message text.", 1, y);
  ++y;
  items.add(new Label("Tear Color:"),
            new StringEditItem<std::string&>(MAX_STRING_LEN, c.msg.tear_color, EditLineMode::ALL),
            "Color for tear lines in the message text.", 1, y);
  ++y;
  items.add(new Label("Origin Color:"),
            new StringEditItem<std::string&>(MAX_STRING_LEN, c.msg.origin_color, EditLineMode::ALL),
            "Color for origin lines in the message text.", 1, y);
  ++y;

  items.add_aligned_width_column(1);
  items.relayout_items_and_labels();
  items.Run("System Colors");
}
