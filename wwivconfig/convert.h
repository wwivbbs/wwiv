/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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
#ifndef INCLUDED_WWIVCONFIG_CONVERT_H
#define INCLUDED_WWIVCONFIG_CONVERT_H

#include <filesystem>

namespace wwiv::local::ui {
  class UIWindow;
}
namespace wwiv::sdk {
class Config430;
}
namespace wwiv::wwivconfig::convert {

enum class ShouldContinue { CONTINUE, EXIT };

enum class config_upgrade_state_t { already_latest, upgraded };
bool ensure_offsets_are_updated(local::ui::UIWindow* window, const sdk::Config430&);
bool convert_config_424_to_430(local::ui::UIWindow* window, const std::filesystem::path& config_filename);
config_upgrade_state_t convert_config_to_52(local::ui::UIWindow* window, const std::filesystem::path& config_filename);
int final_wwiv_config_dat_version();

ShouldContinue do_wwiv_upgrades(local::ui::UIWindow* window, const std::string& bbsdir);

}

#endif
