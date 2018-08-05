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
#ifndef __INCLUDED_WWIVD_CONNECTION_DATA_H__
#define __INCLUDED_WWIVD_CONNECTION_DATA_H__

#include <map>
#include <memory>
#include "core/net.h"
#include "sdk/config.h"
#include "sdk/wwivd_config.h"
#include "wwivd/ips.h"
#include "wwivd/node_manager.h"

namespace wwiv {
namespace wwivd {

struct ConnectionData {
  ConnectionData(const ::wwiv::sdk::Config* g, const wwiv::sdk::wwivd_config_t* t,
    std::map<const std::string, std::shared_ptr<NodeManager>>* n,
    std::shared_ptr<ConcurrentConnections> concurrent_connections)
      : config(g), c(t), nodes(n), concurrent_connections_(concurrent_connections) {}
  const wwiv::sdk::Config* config;
  const wwiv::sdk::wwivd_config_t* c;
  std::map<const std::string, std::shared_ptr<NodeManager>>* nodes;
  std::shared_ptr<ConcurrentConnections> concurrent_connections_;
  std::shared_ptr<wwiv::wwivd::GoodIp> good_ips_;
  std::shared_ptr<wwiv::wwivd::BadIp> bad_ips_;
  std::shared_ptr<wwiv::wwivd::AutoBlocker> auto_blocker_;
};

}  // namespace wwivd
}  // namespace wwiv

#endif  // __INCLUDED_WWIVD_CONNECTION_DATA_H__
