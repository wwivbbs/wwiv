/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*               Copyright (C)2026     , WWIV Software Services           */
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
#include "wwivconfig/spell_check.h"

#include "core/strings.h"
#include "localui/curses_win.h"
#include "localui/edit_items.h"
#include "localui/input.h"

using namespace wwiv::local::ui;
using namespace wwiv::strings;

void spell_check_settings(wwiv::sdk::Config&, wwiv::sdk::spell_config_t& s, CursesWindow*) {
  auto y = 1;
  EditItems items{};
  items.add(new Label("Enabled:"), new BooleanEditItem(&s.enabled),
            "Allow spell checking messages in the full screen editor (FSED)", 1, y);
  ++y;
  items.add(new Label("Check on Save:"), new BooleanEditItem(&s.check_on_save),
            "Spell check when a message is saved (users can turn this off in Defaults)", 1, y);
  ++y;
  items.add(new Label("Dictionary:"),
            new StringEditItem<std::string&>(20, s.dictionary, EditLineMode::ALL),
            "Default Hunspell dictionary name, i.e. en_US for en_US.aff and en_US.dic", 1, y);
  ++y;
  items.add(new Label("Dictionary Dir:"),
            new StringEditItem<std::string&>(60, s.dict_dir, EditLineMode::ALL),
            "Directory searched first for dictionaries. Empty uses BBSDIR/dict, "
            "/usr/share/hunspell",
            1, y);

  items.add_aligned_width_column(1);
  items.relayout_items_and_labels();
  items.Run("Spell Check");
}
