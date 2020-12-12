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
#include "wwivconfig/paths.h"

#include "core/strings.h"
#include "localui/edit_items.h"
#include "localui/input.h"
#include "sdk/vardec.h"
#include <memory>
#include <string>

using std::unique_ptr;
using std::string;
using namespace wwiv::core;
using namespace wwiv::strings;

/* change msgsdir, gfilesdir, datadir, dloadsdir, ramdrive, tempdir, scriptdir, logdir */
void setpaths(wwiv::sdk::Config& config) {
  EditItems items{};
  auto cfg = *config.config();

  auto y = 1;
  items.add(new Label("Messages:"),
            new FilePathItem(60, config.root_directory(), cfg.msgsdir), 1, y++);
  items.add(new Label("GFiles:"),
      new FilePathItem(60, config.root_directory(), cfg.gfilesdir), 1, y++);
  items.add(new Label("Menus:"),
      new FilePathItem(60, config.root_directory(), cfg.menudir), 1, y++);
  items.add(new Label("Data:"),
      new FilePathItem(60, config.root_directory(), cfg.datadir), 1, y++);
  items.add(new Label("Logs:"),
      new FilePathItem(60, config.root_directory(), cfg.logdir), 1, y++);
  items.add(new Label("Scripts:"),
      new FilePathItem(60, config.root_directory(), cfg.scriptdir), 1, y++);
  items.add(new Label("Downloads:"),
      new FilePathItem(60, config.root_directory(), cfg.dloadsdir), 1, y++);
  y++;
  items.add(new MultilineLabel(R"(CAUTION: ONLY EXPERIENCED SYSOPS SHOULD MODIFY THESE SETTINGS.
Changing any of these requires YOU to MANUALLY move files and/or
directory structures.)"), 1, y++)->set_right_justified(false);

  if (!cfg.scriptdir[0]) {
    // This is added in 5.3
    auto sdir = File::EnsureTrailingSlash("scripts");
    to_char_array(cfg.scriptdir, sdir);
  }
  items.relayout_items_and_labels();
  items.Run("System Paths");
  config.set_config(&cfg, true);
}

