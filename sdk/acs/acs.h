/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*              Copyright (C)2020-2021, WWIV Software Services            */
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
/**************************************************************************/
#ifndef INCLUDED_SDK_ACS_ACS_H
#define INCLUDED_SDK_ACS_ACS_H

#include "sdk/config.h"
#include "sdk/user.h"
#include "sdk/value/valueprovider.h"

#include <string>
#include <tuple>
#include <vector>

namespace wwiv::sdk::acs {

enum class acs_debug_t { local, remote, none };

// Result: (true|false), debug lines
std::tuple<bool, std::vector<std::string>> check_acs(const Config& config, const value::ValueProvider& user,
                                                     const std::string& expression);
std::tuple<bool, std::vector<std::string>> check_acs(const Config& config, const std::vector<const value::ValueProvider*>& providers,
                                                     const std::string& expression);


// Result: (true|false), exception message (if any), debug lines
std::tuple<bool, std::string, std::vector<std::string>> validate_acs(const value::ValueProvider& user,
                                                                     const std::string& expression);

std::tuple<bool, std::string, std::vector<std::string>> validate_acs(const std::vector<const value::ValueProvider*>& providers,
                                                                     const std::string& expression);

} // namespace wwiv::sdk::acs

#endif
