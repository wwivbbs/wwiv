/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*              Copyright (C)2016 WWIV Software Services                  */
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
#ifndef __INCLUDED_NETWORKB_FIDO_UTIL_H__
#define __INCLUDED_NETWORKB_FIDO_UTIL_H__

#include <ctime>
#include <set>
#include <string>
#include <vector>

#include "core/file.h"
#include "sdk/fido/fido_address.h"
#include "sdk/fido/fido_packets.h"

namespace wwiv {
namespace net {
namespace fido {

std::string packet_name(time_t now);
std::string bundle_name(const wwiv::sdk::fido::FidoAddress& source, const wwiv::sdk::fido::FidoAddress& dest, int dow, int bundle_number);
std::string bundle_name(const wwiv::sdk::fido::FidoAddress& source, const wwiv::sdk::fido::FidoAddress& dest, const std::string& extension);
std::string dow_extension(int dow, int bundle_number);
std::string control_file_name(const wwiv::sdk::fido::FidoAddress& dest, wwiv::sdk::fido::FidoBundleStatus dow);
std::string daten_to_fido(time_t t);

}  // namespace fido
}  // namespace net
}  // namespace wwiv

#endif  // __INCLUDED_NETWORKB_FIDO_UTIL_H__
