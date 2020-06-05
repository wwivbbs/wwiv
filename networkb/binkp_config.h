/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2020, WWIV Software Services             */
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
#pragma once
#ifndef __INCLUDED_NETWORKB_BINKP_CONFIG_H__
#define __INCLUDED_NETWORKB_BINKP_CONFIG_H__

#include <cstdint>
#include <exception>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>

#include "core/inifile.h"
#include "networkb/config_exceptions.h"
#include "sdk/binkp.h"
#include "sdk/callout.h"
#include "sdk/networks.h"

namespace wwiv {
namespace net {

class BinkConfig {
public:
  BinkConfig(const std::string& callout_network_name, const wwiv::sdk::Config& config,
             const wwiv::sdk::Networks& networks);
  BinkConfig(int node_number, const wwiv::sdk::Config& config, const std::string& network_dir);
  virtual ~BinkConfig();
  // Gets the binkp_session_config_t or nullptr if one can not be found.
  const binkp_session_config_t* binkp_session_config_for(const std::string& node) const;
  // Gets the binkp_session_config_t or nullptr if one can not be found.
  const binkp_session_config_t* binkp_session_config_for(uint16_t node) const;

  uint16_t callout_node_number() const { return callout_wwivnet_node_; }
  const std::string callout_fido_address() const { return callout_fido_node_; }
  const std::string system_name() const { return system_name_; }
  const std::string sysop_name() const { return sysop_name_; }
  const std::string gfiles_directory() const { return gfiles_directory_; }
  const std::string callout_network_name() const { return callout_network_name_; }
  const std::string network_dir(const std::string& network_name) const;
  const net_networks_rec& network(const std::string& network_name) const;
  const net_networks_rec& callout_network() const;
  const wwiv::sdk::Networks& networks() { return networks_; }
  std::map<const std::string, std::unique_ptr<wwiv::sdk::Callout>>& callouts() { return callouts_; }

  void set_skip_net(bool skip_net) { skip_net_ = skip_net; }
  bool skip_net() const { return skip_net_; }
  void set_verbose(int verbose) { verbose_ = verbose; }
  int verbose() const { return verbose_; }
  void set_network_version(int network_version) { network_version_ = network_version; }
  int network_version() const { return network_version_; }
  bool crc() const { return crc_; }
  bool cram_md5() const { return cram_md5_; }
  const wwiv::sdk::Config& config() const { return config_; }

private:
  const wwiv::sdk::Config& config_;
  std::string home_dir_;

  uint16_t callout_wwivnet_node_ = 0;
  std::string callout_fido_node_;
  std::string system_name_;
  std::string callout_network_name_ = "wwivnet";
  std::string sysop_name_;
  std::string gfiles_directory_;
  const wwiv::sdk::Networks networks_;
  std::map<const std::string, std::unique_ptr<wwiv::sdk::Callout>> callouts_;
  std::unique_ptr<wwiv::sdk::Binkp> binkp_;

  bool skip_net_ = false;
  int verbose_ = 0;
  int network_version_ = 38;
  bool crc_ = false;
  bool cram_md5_ = true;
};

} // namespace net
} // namespace wwiv

#endif // __INCLUDED_NETWORKB_BINKP_CONFIG_H__
