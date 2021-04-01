/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*               Copyright (C)2016-2021, WWIV Software Services           */
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
#ifndef INCLUDED_SDK_FIDO_FIDO_CALLOUT_H
#define INCLUDED_SDK_FIDO_FIDO_CALLOUT_H

#include "sdk/config.h"
#include "sdk/fido/fido_address.h"
#include "sdk/net/callout.h"
#include "sdk/net/net.h"
#include <map>
#include <string>

namespace wwiv::sdk::fido {

class FidoCallout final : public Callout {
public:
  typedef int size_type;
  static const size_type npos = -1;
  FidoCallout(const wwiv::sdk::Config& config, const net::Network& net);
  // [[ VisibleForTesting ]]
  ~FidoCallout() override;

  [[nodiscard]] bool IsInitialized() const { return initialized_; }

  [[nodiscard]] net::fido_node_config_t fido_node_config_for(const FidoAddress& a) const;
  // Returns the packet config for the node, or a default empty config
  // if none exist.
  [[nodiscard]] net::fido_packet_config_t packet_config_for(const FidoAddress& a) const;
  [[nodiscard]] net::fido_packet_config_t
  packet_config_for(const FidoAddress& a, const net::fido_packet_config_t& default_config) const;
  // Creates a merged config, starting with the base_config then adds in values that exist
  // in the node specific config.
  [[nodiscard]] net::fido_packet_config_t
  merged_packet_config_for(const FidoAddress& a, const net::fido_packet_config_t& base_config) const;

  // wwiv::sdk::Callout implementation
  [[nodiscard]] const net::net_call_out_rec* net_call_out_for(int node) const override;
  [[nodiscard]] const net::net_call_out_rec* net_call_out_for(const FidoAddress& node) const;
  [[nodiscard]] const net::net_call_out_rec* net_call_out_for(const std::string& node) const override;

  bool insert(const FidoAddress& a, const net::fido_node_config_t& c);
  bool erase(const FidoAddress& a);
  bool Load();
  bool Save() override;
  [[nodiscard]] const std::map<FidoAddress, net::fido_node_config_t>& node_configs_map() const{
    return node_configs_;
  }
  [[nodiscard]] std::map<FidoAddress, net::fido_node_config_t>& node_configs_map() {
    return node_configs_;
  }

private:
  bool initialized_{false};
  const std::string root_dir_;
  net::Network net_;
  std::map<FidoAddress, net::fido_node_config_t> node_configs_;
};

} // namespace

#endif
