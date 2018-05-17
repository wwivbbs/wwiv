/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*               Copyright (C)2014-2017, WWIV Software Services           */
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
#include "init/paths.h"

#include <cstdint>
#include <memory>
#include <string>

#include "core/strings.h"
#include "core/wwivport.h"
#include "init/init.h"
#include "init/utility.h"
#include "init/wwivinit.h"
#include "localui/wwiv_curses.h"
#include "localui/input.h"

using std::unique_ptr;
using std::string;
using namespace wwiv::strings;

static constexpr int LABEL1_POS = 2;
static constexpr int LABEL1_WIDTH = 11;
static constexpr int COL1_POSITION = LABEL1_POS + LABEL1_WIDTH + 1;

/* change msgsdir, gfilesdir, datadir, dloadsdir, ramdrive, tempdir, scriptdir */
void setpaths(const std::string& bbsdir) {
  out->Cls(ACS_CKBOARD);

  EditItems items{};
  items.add_items({
    new FilePathItem(COL1_POSITION, 1, 60, bbsdir, syscfg.msgsdir),
    new FilePathItem(COL1_POSITION, 2, 60, bbsdir, syscfg.gfilesdir),
    new FilePathItem(COL1_POSITION, 3, 60, bbsdir, syscfg.menudir),
    new FilePathItem(COL1_POSITION, 4, 60, bbsdir, syscfg.datadir),
    new FilePathItem(COL1_POSITION, 5, 60, bbsdir, syscfg.scriptdir),
    new FilePathItem(COL1_POSITION, 6, 60, bbsdir, syscfg.dloadsdir),
  });

  int y = 1;
  items.add_labels({new Label(LABEL1_POS, y++, LABEL1_WIDTH, "Messages:"),
                    new Label(LABEL1_POS, y++, LABEL1_WIDTH, "GFiles:"),
                    new Label(LABEL1_POS, y++, LABEL1_WIDTH, "Menus:"),
                    new Label(LABEL1_POS, y++, LABEL1_WIDTH, "Data:"),
                    new Label(LABEL1_POS, y++, LABEL1_WIDTH, "Scripts:"),
                    new Label(LABEL1_POS, y++, LABEL1_WIDTH, "Downloads:")});
  y+=2;
  //window->SetColor(SchemeId::WARNING);
  items.add_labels({new Label(LABEL1_POS, y++, LABEL1_WIDTH,
                              "CAUTION: ONLY EXPERIENCED SYSOPS SHOULD MODIFY THESE SETTINGS.")});
  y+=1;
  //window->SetColor(SchemeId::WINDOW_TEXT);
  items.add_labels({new Label(LABEL1_POS + 2, y++, LABEL1_WIDTH,
                              "Changing any of these requires YOU to MANUALLY move files and/or"),
                    new Label(LABEL1_POS + 2, y++, LABEL1_WIDTH, "directory structures.")});

  if (!syscfg.scriptdir[0]) {
    // This is added in 5.3
    string sdir = StrCat("scripts", File::pathSeparatorString);
    to_char_array(syscfg.scriptdir, sdir);
  }

  items.Run("System Paths");
  save_config();
}

