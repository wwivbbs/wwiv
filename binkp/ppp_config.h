/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2021, WWIV Software Services             */
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
#ifndef __INCLUDED_NETWORKB_PPP_CONFIG_H__
#define __INCLUDED_NETWORKB_PPP_CONFIG_H__

#include "sdk/config.h"
#include "sdk/net/networks.h"
#include <cstdint>
#include <map>
#include <string>

namespace wwiv {
namespace net {

struct PPPNodeConfig {
  std::string email_address;
};

class PPPConfig {
 public:
  PPPConfig(const std::string& network_name, const wwiv::sdk::Config& config, const wwiv::sdk::Networks& networks);
  // For Testing
  PPPConfig(int node_number, std::string system_name, const std::string& network_dir);
  virtual ~PPPConfig();
  [[nodiscard]] const PPPNodeConfig* ppp_node_config_for(int node) const;

  [[nodiscard]] int callout_node_number() const { return node_; }
  [[nodiscard]] const std::string& system_name() const { return system_name_; }
  [[nodiscard]] const std::string& callout_network_name() const { return callout_network_name_; }

 private:
  std::map<int, PPPNodeConfig> node_config_;
  std::string home_dir_;
  int node_;
  std::string system_name_;
  std::string callout_network_name_;
};

bool ParseAddressNetLine(const std::string& line,
			 uint16_t* node,
			 PPPNodeConfig* config);

}  // namespace net
}  // namespace wwiv


#endif  // __INCLUDED_NETWORKB_PPP_CONFIG_H__
