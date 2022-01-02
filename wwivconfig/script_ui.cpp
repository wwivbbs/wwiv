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

#include "core/strings.h"
#include "localui/edit_items.h"
#include "localui/input.h"

using namespace wwiv::core;
using namespace wwiv::local::ui;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

void script_ui(Config& config) {
  auto allow_script = config.scripting_enabled();
  auto enable_file = config.script_package_file_enabled();
  auto enable_os = config.script_package_os_enabled();

  auto y = 1;
  EditItems items{};
  items.add(new Label("Enable Scripting:"),
            new BooleanEditItem(&allow_script),
    "Are WWIVbasic scripts allowed to be executed anywhere.", 1, y);
  ++y;
  items.add(new Label("Enable Package FILE:"),
            new BooleanEditItem(&enable_file),
    "Enable package wwiv.io.file.", 1, y);
  ++y;
  items.add(new Label("Enable Package OS:"),
            new BooleanEditItem(&enable_os),
    "Enable package wwiv.os (allows executing external binaries).", 1, y);
  ++y;

  items.relayout_items_and_labels();
  items.Run("Scripting Configuration");

  config.scripting_enabled(allow_script);
  config.script_package_os_enabled(enable_os);
  config.script_package_file_enabled(enable_file);
}
