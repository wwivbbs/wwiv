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
#include "wwivconfig/toggles.h"

#include "core/strings.h"
#include "localui/curses_win.h"
#include "localui/edit_items.h"
#include "localui/input.h"

using namespace wwiv::local::ui;
using namespace wwiv::strings;

void toggles(wwiv::sdk::Config&, wwiv::sdk::system_toggles_t& t, CursesWindow*) {

  auto y = 1;
  EditItems items{};
  items.add(new Label("LastNet at Logon:"),
            new BooleanEditItem(&t.lastnet_at_logon),
    "Show the most recent network connections at logon", 1, y);
  ++y;
  items.add(new Label("Show Chain Usage:"),
            new BooleanEditItem(&t.show_chain_usage),
    "Show usage stats in chain display", 1, y);


  items.add_aligned_width_column(1);
  items.relayout_items_and_labels();
  items.Run("System Toggles");
}
