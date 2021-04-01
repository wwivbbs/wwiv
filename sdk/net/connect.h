/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2014-2021, WWIV Software Services             */
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
#ifndef INCLUDED_SDK_CONNECT_H
#define INCLUDED_SDK_CONNECT_H

#include "sdk/net/net.h"
#include <filesystem>
#include <initializer_list>
#include <map>
#include <string>

namespace wwiv::sdk {

  
class Connect {
 public:
  explicit Connect(const std::filesystem::path& network_dir);
  // VisibleForTesting
  Connect(std::initializer_list<net::net_interconnect_rec> l);
  virtual ~Connect();
  const net::net_interconnect_rec* node_config_for(int node) const;
  Connect& operator=(const Connect& rhs) { node_config_ = rhs.node_config_; return *this; }
  std::string ToString() const;
  const std::map<uint16_t, net::net_interconnect_rec>& node_config() const { return node_config_; }

 private:
  std::map<uint16_t, net::net_interconnect_rec> node_config_;
};

bool ParseConnectNetLine(const std::string& line, net::net_interconnect_rec* config);

}  // namespace

#endif
