/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2020-2021, WWIV Software Services             */
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
#ifndef INCLUDED_COMMON_MENUS_MENU_GENERATOR_H
#define INCLUDED_COMMON_MENUS_MENU_GENERATOR_H

#include "sdk/menus/menu.h"

#include <map>
#include <set>
#include <string>

namespace wwiv::sdk::value {
class ValueProvider;
}

namespace wwiv::common::menus {

std::vector<std::string>
GenerateMenuLines(const sdk::Config& config,
                  const wwiv::sdk::menus::MenuSet56& menu_set, const sdk::menus::menu_56_t& menu,
                  const sdk::User& user, const std::vector<const sdk::value::ValueProvider*>& providers,
                  sdk::menus::menu_type_t typ);

}

#endif 