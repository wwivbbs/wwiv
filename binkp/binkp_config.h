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
#ifndef __INCLUDED_NETWORKB_BINKP_CONFIG_H__
#define __INCLUDED_NETWORKB_BINKP_CONFIG_H__

#include "sdk/net/binkp.h"
#include "sdk/net/callout.h"
#include "sdk/net/networks.h"
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>

namespace wwiv {
namespace net {

class BinkConfig final {
public:
  BinkConfig(const std::string& callout_network_name, const wwiv::sdk::Config& config,
             const wwiv::sdk::Networks& networks);
  BinkConfig(int node_number, const wwiv::sdk::Config& config, const std::string& network_dir);
  ~BinkConfig();
  // Gets the binkp_session_config_t or nullptr if one can not be found.
  [[nodiscard]] const sdk::net::binkp_session_config_t* binkp_session_config_for(const std::string& node) const;
  // Gets the binkp_session_config_t or nullptr if one can not be found.
  [[nodiscard]] const sdk::net::binkp_session_config_t* binkp_session_config_for(uint16_t node) const;

  [[nodiscard]] int callout_node_number() const { return callout_wwivnet_node_; }
  [[nodiscard]] std::string callout_fido_address() const { return callout_fido_node_; }
  [[nodiscard]] std::string system_name() const { return system_name_; }
  [[nodiscard]] std::string sysop_name() const { return sysop_name_; }
  [[nodiscard]] std::string gfiles_directory() const { return gfiles_directory_; }
  [[nodiscard]] std::string callout_network_name() const { return callout_network_name_; }
  /** Gets net.dir in absolute form for the network named network_name */
  [[nodiscard]] std::filesystem::path network_dir(const std::string& network_name) const;
  /** Get the directory to receive files into for network named network_name */
  [[nodiscard]] std::string receive_dir(const std::string& network_name) const;

  [[nodiscard]] const sdk::net::Network& network(const std::string& network_name) const;
  [[nodiscard]] const sdk::net::Network& callout_network() const;
  [[nodiscard]] const wwiv::sdk::Networks& networks() { return networks_; }
  /*
   * Key/Value mapping of domain to callout class.
   * The domain and network name *should* match, but not always, so prefer domain
   * if you have both.
   */
  std::map<const std::string, std::unique_ptr<sdk::Callout>>& callouts() { return callouts_; }

  void set_skip_net(bool skip_net) { skip_net_ = skip_net; }
  [[nodiscard]] bool skip_net() const { return skip_net_; }
  void set_verbose(int verbose) { verbose_ = verbose; }
  [[nodiscard]] int verbose() const { return verbose_; }
  void set_network_version(int network_version) { network_version_ = network_version; }
  [[nodiscard]] int network_version() const { return network_version_; }
  [[nodiscard]] bool crc() const { return crc_; }
  [[nodiscard]] bool cram_md5() const { return cram_md5_; }
  [[nodiscard]] const sdk::Config& config() const { return config_; }

  [[nodiscard]] std::string session_identifier() const { return session_identifier_; }
  void session_identifier(std::string id);

  std::map<sdk::fido::FidoAddress, std::string> address_pw_map;

private:
  const sdk::Config& config_;
  std::string home_dir_;

  int callout_wwivnet_node_{0};
  std::string callout_fido_node_;
  std::string system_name_;
  std::string callout_network_name_ = "wwivnet";
  std::string sysop_name_;
  std::string gfiles_directory_;
  const wwiv::sdk::Networks networks_;
  std::map<const std::string, std::unique_ptr<wwiv::sdk::Callout>> callouts_;
  std::unique_ptr<wwiv::sdk::Binkp> binkp_;

  bool skip_net_{false};
  int verbose_{0};
  int network_version_{38};
  bool crc_{true};
  bool cram_md5_{true};
  std::string session_identifier_;
};

} // namespace net
} // namespace wwiv

#endif // __INCLUDED_NETWORKB_BINKP_CONFIG_H__
