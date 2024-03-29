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
#ifndef INCLUDED_WWIVCONFIG_WWIVD_UI_H
#define INCLUDED_WWIVCONFIG_WWIVD_UI_H

#include "sdk/config.h"
#include "sdk/wwivd_config.h"

void wwivd_ui(const wwiv::sdk::Config& config);
wwiv::sdk::wwivd_config_t LoadDaemonConfig(const wwiv::sdk::Config& config);
bool SaveDaemonConfig(const wwiv::sdk::Config& config, wwiv::sdk::wwivd_config_t& c);

#endif // INCLUDED_WWIVCONFIG_WWIVD_UI_H
