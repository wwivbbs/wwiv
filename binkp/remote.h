/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2016-2020, WWIV Software Services             */
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
#ifndef __INCLUDED_NETWORKB_REMOTE_USER_H__
#define __INCLUDED_NETWORKB_REMOTE_USER_H__

#include "binkp/binkp_config.h"
#include <cstdint>
#include <string>

namespace wwiv::net {
  
class Remote {
public:
  Remote(BinkConfig* config, bool caller, const std::string& expected_remote_node);
  virtual ~Remote() = default;

  void set_system_name(const std::string& s) { system_name_ = s; }
  void set_sysop_name(const std::string& s) { sysop_name_ = s; }
  void set_version(const std::string& v) { version_ = v; }
  void set_address_list(const std::string& a);
  [[nodiscard]] std::string address_list() const { return address_list_; }
  [[nodiscard]] const std::string network_name() const;
  [[nodiscard]] const net_networks_rec& network() const;

  [[nodiscard]] int wwivnet_node() const { return wwivnet_node_; }
  [[nodiscard]] std::string ftn_address() const { return ftn_address_; }

private:
  BinkConfig* config_;
  const std::string default_network_name_;
  bool remote_is_caller_;
  const std::string expected_remote_node_;

  std::string system_name_;
  std::string sysop_name_;
  std::string version_;
  std::string ftn_address_;
  uint16_t wwivnet_node_;
  std::string network_name_;
  std::string address_list_;
};

std::string ftn_address_from_address_list(const std::string& network_list, const std::string& network_name);

// Returns just the node number (such as "1") from a FTN address like
// (such as "20000:20000/1@wwivnet")
uint16_t wwivnet_node_number_from_ftn_address(const std::string& address);

// Returns a FTN address like "20000:20000/1@wwivnet".
std::string ftn_address_from_address_list(const std::string& address_list, const std::string& network_name);

// Returns just the network name (such as "wwivnet") from a FTN address like
// (such as "20000:20000/1@wwivnet")
std::string network_name_from_single_address(const std::string& address_list);

}  // namespace wwiv::net

#endif  // __INCLUDED_NETWORKB_REMOTE_USER_H__
