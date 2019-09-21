/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*               Copyright (C)2014-2019, WWIV Software Services           */
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
#include "localui/input.h"
#include "localui/wwiv_curses.h"
#include "sdk/vardec.h"
#include "wwivconfig/utility.h"
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
  configrec cfg = *config.config();

  items.add_items({
      new FilePathItem(COL1_POSITION, 1, 60, config.root_directory(), cfg.msgsdir),
      new FilePathItem(COL1_POSITION, 2, 60, config.root_directory(), cfg.gfilesdir),
      new FilePathItem(COL1_POSITION, 3, 60, config.root_directory(), cfg.menudir),
      new FilePathItem(COL1_POSITION, 4, 60, config.root_directory(), cfg.datadir),
      new FilePathItem(COL1_POSITION, 5, 60, config.root_directory(), cfg.logdir),
      new FilePathItem(COL1_POSITION, 6, 60, config.root_directory(), cfg.scriptdir),
      new FilePathItem(COL1_POSITION, 7, 60, config.root_directory(), cfg.dloadsdir),
  });

  int y = 1;
  items.add_labels({new Label(LABEL1_POS, y++, LABEL1_WIDTH, "Messages:"),
                    new Label(LABEL1_POS, y++, LABEL1_WIDTH, "GFiles:"),
                    new Label(LABEL1_POS, y++, LABEL1_WIDTH, "Menus:"),
                    new Label(LABEL1_POS, y++, LABEL1_WIDTH, "Data:"),
                    new Label(LABEL1_POS, y++, LABEL1_WIDTH, "Logs:"),
                    new Label(LABEL1_POS, y++, LABEL1_WIDTH, "Scripts:"),
                    new Label(LABEL1_POS, y++, LABEL1_WIDTH, "Downloads:")});
  y+=2;
  items.add_labels({new Label(LABEL1_POS, y++, LABEL1_WIDTH,
                              "CAUTION: ONLY EXPERIENCED SYSOPS SHOULD MODIFY THESE SETTINGS.")});
  y+=1;
  items.add_labels({new Label(LABEL1_POS + 2, y++, LABEL1_WIDTH,
                              "Changing any of these requires YOU to MANUALLY move files and/or"),
                    new Label(LABEL1_POS + 2, y++, LABEL1_WIDTH, "directory structures.")});

  if (!cfg.scriptdir[0]) {
    // This is added in 5.3
    auto sdir = File::EnsureTrailingSlash("scripts");
    to_char_array(cfg.scriptdir, sdir);
  }

  items.Run("System Paths");
  config.set_config(&cfg, true);
}

