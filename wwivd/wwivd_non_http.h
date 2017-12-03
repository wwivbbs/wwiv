/**************************************************************************/
/*                                                                        */
/*                          WWIV BBS Software                             */
/*               Copyright (C)2017, WWIV Software Services                */
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
#ifndef __INCLUDED_WWIVD_WWIVD_NON_HTTP_H__
#define __INCLUDED_WWIVD_WWIVD_NON_HTTP_H__

#include "sdk/wwivd_config.h"

#include "wwivd/connection_data.h"
#include "wwivd/node_manager.h"

namespace wwiv {
namespace wwivd {

std::string to_string(const wwiv::sdk::wwivd_matrix_entry_t& e);
std::string to_string(const std::vector<wwiv::sdk::wwivd_matrix_entry_t>& elements);
const std::string node_file(const wwiv::sdk::Config& config, 
  ConnectionType ct, int node_number);
void HandleConnection(ConnectionData data);
void HandleBinkPConnection(ConnectionData data);

}  // namespace wwivd
}  // namespace wwiv

#endif  // __INCLUDED_WWIVD_WWIVD_NON_HTTP_H__
