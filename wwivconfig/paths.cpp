/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*               Copyright (C)2014-2021, WWIV Software Services           */
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
#include <memory>
#include <string>

using std::unique_ptr;
using std::string;
using namespace wwiv::core;
using namespace wwiv::strings;

/* change msgsdir, gfilesdir, datadir, dloadsdir, ramdrive, tempdir, scriptdir, logdir */
void setpaths(wwiv::sdk::Config& config) {
  EditItems items{};

  auto y = 1;
  auto msgsdir = config.raw_config().msgsdir;
  auto gfilesdir = config.raw_config().gfilesdir;
  auto menudir = config.raw_config().menudir;
  auto datadir = config.raw_config().datadir;
  auto logdir = config.raw_config().logdir;
  auto scriptdir= config.raw_config().scriptdir;
  auto dloadsdir = config.raw_config().dloadsdir;

  auto tempdir = config.temp_format();
  auto batchdir = config.batch_format();
  auto scratchdir = config.scratch_format();

  items.add(new Label("Messages:"),
            new StringFilePathItem(60, config.root_directory(), msgsdir), 1, y++);
  items.add(new Label("GFiles:"),
      new StringFilePathItem(60, config.root_directory(), gfilesdir), 1, y++);
  items.add(new Label("Menus:"),
      new StringFilePathItem(60, config.root_directory(), menudir), 1, y++);
  items.add(new Label("Data:"),
      new StringFilePathItem(60, config.root_directory(), datadir), 1, y++);
  items.add(new Label("Logs:"),
      new StringFilePathItem(60, config.root_directory(), logdir), 1, y++);
  items.add(new Label("Scripts:"),
      new StringFilePathItem(60, config.root_directory(), scriptdir), 1, y++);
  items.add(new Label("Downloads:"),
      new StringFilePathItem(60, config.root_directory(), dloadsdir), 1, y++);
  y++;
  items.add(new Label("Temp:"),
      new StringEditItem<std::string&>(60, tempdir, EditLineMode::ALL), 
    "relative path to temp dir. '%n' replaced by instance # and is required.", 1, y++);
  items.add(new Label("Batch:"),
      new StringEditItem<std::string&>(60, batchdir, EditLineMode::ALL), 
    "relative path to batch dir. '%n' replaced by instance # and is required.", 1, y++);
  items.add(new Label("Scratch:"),
      new StringEditItem<std::string&>(60, scratchdir, EditLineMode::ALL), 
    "relative path to scratch space. '%n' replaced by instance # and is required.", 1, y++);
  y++;
  items.add(new MultilineLabel(R"(CAUTION: ONLY EXPERIENCED SYSOPS SHOULD MODIFY THESE SETTINGS.
Changing any of these requires YOU to MANUALLY move files and/or
directory structures.)"), 1, y++)->set_right_justified(false);

  if (config.scriptdir().empty()) {
    // This is added in 5.3
    config.scriptdir(FilePath(config.root_directory(), "scripts").string());
  }
  items.relayout_items_and_labels();
  items.Run("System Paths");
  config.msgsdir(msgsdir);
  config.gfilesdir(gfilesdir);
  config.menudir(menudir);
  config.datadir(datadir);
  config.logdir(logdir);
  config.scriptdir(scriptdir);
  config.dloadsdir(dloadsdir);

  config.temp_format(tempdir);
  config.batch_format(batchdir);
  config.scratch_format(scratchdir);
}

