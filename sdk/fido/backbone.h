/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*           Copyright (C)2022, WWIV Software Services                    */
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
#ifndef INCLUDED_SDK_FIDO_BACKBONE_H
#define INCLUDED_SDK_FIDO_BACKBONE_H

#include "core/inifile.h"
#include "sdk/config.h"
#include "sdk/net/net.h"
#include "sdk/subxtr.h"
#include <filesystem>
#include <string>
#include <vector>

/**
 * Classes to represent a FTN BACKBONE file.
 *
 * The format is:
 *
 * ECHO_TAG      DESCRIPTION
 *
 */

namespace wwiv::sdk::fido {

struct backbone_t {
  std::string tag;
  std::string desc;
};

std::vector<backbone_t> ParseBackboneFile(const std::filesystem::path& file);

struct import_result_t {
  bool success{false};
  bool subs_dirty{false};
};

import_result_t ImportSubsFromBackbone(wwiv::sdk::Subs& subs, const wwiv::sdk::net::Network& net,
                                       int16_t net_num, wwiv::core::IniFile& ini,
                                       const std::vector<backbone_t> backbone_echos);

} // namespace wwiv::sdk::fido

#endif
