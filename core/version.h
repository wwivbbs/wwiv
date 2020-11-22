/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                Copyright (C)2015-2020, WWIV Software Services          */
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
#ifndef INCLUDED_CORE_VERSION_H
#define INCLUDED_CORE_VERSION_H

#include <cstdint>
#include <string>

namespace wwiv::core {

// Returns the full WWIV version (like 5.5.0.2020) as a string.
std::string full_version();

// Returns the short version (Like 5.5.0) as a string.
std::string short_version();

// Returns the wwiv modern config version #.  Used to run upgrade
// activities on WWIV.
uint16_t wwiv_config_version();

// Returns the network compatible version as an int. i.e. 53
int wwiv_network_compatible_version();

// Returns the compile date for WWIV
std::string wwiv_compile_datetime();

}

#endif
