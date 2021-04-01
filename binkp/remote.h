/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2016-2021, WWIV Software Services             */
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
#ifndef INCLUDED_NETWORKB_REMOTE_USER_H
#define INCLUDED_NETWORKB_REMOTE_USER_H

#include "binkp/binkp_config.h"
#include <cstdint>
#include <set>
#include <string>

namespace wwiv::net {
  
class Remote final {
public:
  Remote(BinkConfig* config, bool remote_is_caller, const std::string& expected_remote_addr);
  ~Remote() = default;

  void set_system_name(const std::string& s) { system_name_ = s; }
  void set_sysop_name(const std::string& s) { sysop_name_ = s; }
  void set_version(const std::string& v) { version_ = v; }
  void set_address_list(const std::string& a);
  [[nodiscard]] std::string address_list() const { return address_list_; }
  [[nodiscard]] std::string network_name() const;
  [[nodiscard]] std::string domain() const;
  void set_domain(const std::string& d);
  [[nodiscard]] const sdk::net::Network& network() const;

  [[nodiscard]] int wwivnet_node() const { return wwivnet_node_; }
  void set_wwivnet_node(int n, const std::string& domain) { wwivnet_node_ = static_cast<uint16_t>(n); domain_ = domain; }
  [[nodiscard]] std::string ftn_address() const;
  [[nodiscard]] std::set<sdk::fido::FidoAddress> ftn_addresses() const { return ftn_addresses_; }
  void set_ftn_addresses(const std::set<sdk::fido::FidoAddress>& a);

private:
  BinkConfig* config_;
  const std::string default_domain_;
  bool remote_is_caller_;
  const std::string expected_remote_node_;
  std::string domain_;
  std::string network_name_;

  std::string system_name_;
  std::string sysop_name_;
  std::string version_;
  std::set<sdk::fido::FidoAddress> ftn_addresses_;
  uint16_t wwivnet_node_{0};
  std::string address_list_;
};

// Returns just the node number (such as "1") from a FTN address like
// (such as "20000:20000/1@wwivnet")
uint16_t wwivnet_node_number_from_ftn_address(const std::string& address);

// Returns a FTN address like "20000:20000/1@wwivnet".
std::string ftn_address_from_address_list(const std::string& address_list, const std::string& domain);
std::set<sdk::fido::FidoAddress> ftn_addresses_from_address_list(const std::string& address_list, const std::set<sdk::fido::FidoAddress>& known_addresses);

}  // namespace wwiv::net

#endif
