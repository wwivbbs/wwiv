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

static constexpr int LABEL1_POS = 2;
static constexpr int LABEL1_WIDTH = 11;
static constexpr int COL1_POSITION = LABEL1_POS + LABEL1_WIDTH + 1;

/* change msgsdir, gfilesdir, datadir, dloadsdir, ramdrive, tempdir, scriptdir, logdir */
void setpaths(wwiv::sdk::Config& config) {
  EditItems items{};
  auto cfg = *config.config();

  auto y = 1;
  items.add(new Label(LABEL1_POS, y, LABEL1_WIDTH, "Messages:"),
            new FilePathItem(COL1_POSITION, y, 60, config.root_directory(), cfg.msgsdir));
  ++y;
  items.add(new Label(LABEL1_POS, y, LABEL1_WIDTH, "GFiles:"),
      new FilePathItem(COL1_POSITION, y, 60, config.root_directory(), cfg.gfilesdir));
  ++y;
  items.add(new Label(LABEL1_POS, y, LABEL1_WIDTH, "Menus:"),
      new FilePathItem(COL1_POSITION, y, 60, config.root_directory(), cfg.menudir));
  ++y;
  items.add(new Label(LABEL1_POS, y, LABEL1_WIDTH, "Data:"),
      new FilePathItem(COL1_POSITION, y, 60, config.root_directory(), cfg.datadir));
  ++y;
  items.add(new Label(LABEL1_POS, y, LABEL1_WIDTH, "Logs:"),
      new FilePathItem(COL1_POSITION, y, 60, config.root_directory(), cfg.logdir));
  ++y;
  items.add(new Label(LABEL1_POS, y, LABEL1_WIDTH, "Scripts:"),
      new FilePathItem(COL1_POSITION, y, 60, config.root_directory(), cfg.scriptdir));
  ++y;
  items.add(new Label(LABEL1_POS, y, LABEL1_WIDTH, "Downloads:"),
      new FilePathItem(COL1_POSITION, y, 60, config.root_directory(), cfg.dloadsdir));
  y+=2;
  items.add(new Label(LABEL1_POS, y, LABEL1_WIDTH,
                      "CAUTION: ONLY EXPERIENCED SYSOPS SHOULD MODIFY THESE SETTINGS."));
  y+=2;
  items.add(new Label(LABEL1_POS + 2, y++, LABEL1_WIDTH,
                              "Changing any of these requires YOU to MANUALLY move files and/or"));
  items.add(new Label(LABEL1_POS + 2, y++, LABEL1_WIDTH, "directory structures."));

  if (!cfg.scriptdir[0]) {
    // This is added in 5.3
    auto sdir = File::EnsureTrailingSlash("scripts");
    to_char_array(cfg.scriptdir, sdir);
  }

  items.Run("System Paths");
  config.set_config(&cfg, true);
}

