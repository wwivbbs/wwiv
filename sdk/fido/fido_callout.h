/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*               Copyright (C)2016-2020, WWIV Software Services           */
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
#ifndef __INCLUDED_SDK_FIDO_FIDO_CALLOUT_H__
#define __INCLUDED_SDK_FIDO_FIDO_CALLOUT_H__

#include "sdk/config.h"
#include "sdk/fido/fido_address.h"
#include "sdk/net/callout.h"
#include "sdk/net/net.h"
#include <initializer_list>
#include <map>
#include <memory>
#include <string>

namespace wwiv {
namespace sdk {
namespace fido {

class FidoCallout : public wwiv::sdk::Callout {
public:
  typedef int size_type;
  static const size_type npos = -1;
  FidoCallout(const wwiv::sdk::Config& config, const net_networks_rec& net);
  // [[ VisibleForTesting ]]
  virtual ~FidoCallout();

  [[nodiscard]] bool IsInitialized() const { return initialized_; }

  [[nodiscard]] fido_node_config_t fido_node_config_for(const FidoAddress& a) const;
  [[nodiscard]] fido_packet_config_t packet_config_for(const FidoAddress& a) const;
  [[nodiscard]] fido_packet_config_t packet_override_for(const FidoAddress& a) const;

  // wwiv::sdk::Callout implementation
  [[nodiscard]] const net_call_out_rec* net_call_out_for(int node) const override;
  [[nodiscard]] const net_call_out_rec* net_call_out_for(const FidoAddress& node) const;
  [[nodiscard]] const net_call_out_rec* net_call_out_for(const std::string& node) const override;

  bool insert(const FidoAddress& a, const fido_node_config_t& c);
  bool erase(const FidoAddress& a);
  bool Load();
  bool Save() override;
  [[nodiscard]] std::map<FidoAddress, fido_node_config_t> node_configs_map() const {
    return node_configs_;
  }

private:
  bool initialized_{false};
  const std::string root_dir_;
  net_networks_rec net_;
  std::map<wwiv::sdk::fido::FidoAddress, fido_node_config_t> node_configs_;
};

} // namespace fido
} // namespace sdk
} // namespace wwiv

#endif // __INCLUDED_SDK_FIDO_FIDO_CALLOUT_H__
