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
#ifndef INCLUDED_WWIVCONFIG_UTILITY_H
#define INCLUDED_WWIVCONFIG_UTILITY_H

#include <filesystem>
#include <string>

struct statusrec_t;
struct userrec;

namespace wwiv::sdk {
  class Config;
}

int number_userrecs(const std::filesystem::path& datadir);
void save_status(const std::filesystem::path& datadir, const statusrec_t& statusrec);
bool read_status(const std::filesystem::path& datadir, statusrec_t& statusrec);
void read_user(const wwiv::sdk::Config& config, int un, userrec* u);
void write_user(const wwiv::sdk::Config& config, int un, userrec* u);

#endif
