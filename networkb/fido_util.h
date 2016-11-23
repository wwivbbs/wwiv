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
#include "sdk/fido/fido_callout.h"
#include "sdk/fido/fido_packets.h"

namespace wwiv {
namespace net {
namespace fido {

std::string packet_name(time_t now);
std::string bundle_name(const wwiv::sdk::fido::FidoAddress& source, const wwiv::sdk::fido::FidoAddress& dest, int dow, int bundle_number);
std::string bundle_name(const wwiv::sdk::fido::FidoAddress& source, const wwiv::sdk::fido::FidoAddress& dest, const std::string& extension);
std::string flo_name(const wwiv::sdk::fido::FidoAddress& source, const wwiv::sdk::fido::FidoAddress& dest, fido_bundle_status_t status);
std::string dow_extension(int dow, int bundle_number);
std::string control_file_name(const wwiv::sdk::fido::FidoAddress& dest, fido_bundle_status_t status);
std::string daten_to_fido(time_t t);
time_t fido_to_daten(std::string d);
std::string to_net_node(const wwiv::sdk::fido::FidoAddress& a);
std::string to_zone_net_node(const wwiv::sdk::fido::FidoAddress& a);
std::vector<std::string> split_message(const std::string& string);

std::string FidoToWWIVText(const std::string& ft, bool convert_control_codes = true);
std::string WWIVToFidoText(const std::string& wt);

wwiv::sdk::fido::FidoAddress get_address_from_line(const std::string& line);
wwiv::sdk::fido::FidoAddress get_address_from_origin(const std::string& text);

bool RoutesThroughAddress(const wwiv::sdk::fido::FidoAddress& a, const std::string& routes);
wwiv::sdk::fido::FidoAddress FindRouteToAddress(const wwiv::sdk::fido::FidoAddress& a, const wwiv::sdk::fido::FidoCallout& callout);
wwiv::sdk::fido::FidoAddress FindRouteToAddress(
  const wwiv::sdk::fido::FidoAddress& a, const wwiv::sdk::fido::FidoCallout& callout);

}  // namespace fido
}  // namespace net
}  // namespace wwiv

#endif  // __INCLUDED_NETWORKB_FIDO_UTIL_H__
