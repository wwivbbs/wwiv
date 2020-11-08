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

#include "core/strings.h"
#include "localui/input.h"
#include "sdk/wwivd_config.h"
#include <memory>
#include <string>

using std::string;
using std::unique_ptr;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

void script_ui(Config& config) {
  static constexpr auto LABEL1_POSITION = 2;
  static constexpr auto LABEL1_WIDTH = 25;
  static constexpr auto COL1_POSITION = LABEL1_POSITION + LABEL1_WIDTH + 1;

  auto allow_script = config.scripting_enabled();
  auto enable_file = config.script_package_file_enabled();
  auto enable_os = config.script_package_os_enabled();

  auto y = 1;
  EditItems items{};
  items.add(new Label(LABEL1_POSITION, y, LABEL1_WIDTH, "Enable Scripting Support:"),
            new BooleanEditItem(COL1_POSITION, y, &allow_script),
    "Are WWIVbasic scripts allowed to be executed anywhere.");
  ++y;
  items.add(new Label(LABEL1_POSITION, y, LABEL1_WIDTH, "Enable Package FILE:"),
            new BooleanEditItem(COL1_POSITION, y, &enable_file),
    "Enable package wwiv.io.file.");
  ++y;
  items.add(new Label(LABEL1_POSITION, y, LABEL1_WIDTH, "Enable Package OS:"),
            new BooleanEditItem(COL1_POSITION, y, &enable_os),
    "Enable package wwiv.os (allowes executing external binaries).");
  ++y;

  items.Run("Scripting Configuration");

  auto cfg = *config.config();
  cfg.script_flags |= script_flag_disable_script;
  cfg.script_flags &= ~script_flag_enable_file;
  cfg.script_flags &= ~script_flag_enable_os;
  if (allow_script) {
    cfg.script_flags &= ~script_flag_disable_script;
  }
  if (enable_file) {
    cfg.script_flags |= script_flag_enable_file;
  }
  if (enable_os) {
    cfg.script_flags |= script_flag_enable_os;
  }

  config.set_config(&cfg, true);
}
