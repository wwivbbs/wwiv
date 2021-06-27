/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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
#include "core/version.h"
#include "fmt/format.h"
#include "version_internal.h"
#include <cstdint>
#include <string>

// The statusrec.wwiv_version
static constexpr uint16_t wwiv_num_version = 520;
// The statusrec.network_version
static constexpr int wwiv_net_version = 53;

namespace wwiv::core {
std::string wwiv_compile_datetime() {
  return INTERNAL_BUILD_DATE;
}

std::string full_version() {
#ifdef WWIV_FULL_RELEASE
  return WWIV_FULL_RELEASE;
#else
  return {};
#endif
}

std::string short_version() {
#ifdef WWIV_RELEASE
  return WWIV_RELEASE; 
#else 
  return {};
#endif
}

uint16_t wwiv_config_version() {
  return wwiv_num_version;
}

int wwiv_network_compatible_version() {
  return wwiv_net_version;
}

}
