/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*          Copyright (C)2020-2021, WWIV Software Services                */
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
#ifndef INCLUDED_NETWORKF_NETWORKF_H
#define INCLUDED_NETWORKF_NETWORKF_H

#include "core/clock.h"
#include "net_core/net_cmdline.h"
#include "net_core/netdat.h"
#include "sdk/bbslist.h"
#include "sdk/fido/fido_callout.h"
#include "sdk/net/ftn_msgdupe.h"
#include "sdk/net/packets.h"
#include <string>

namespace wwiv::net::networkf {

class NetworkF final {
public:
  NetworkF(const NetworkCommandLine& cmdline, const sdk::BbsListNet& bbslist,
           core::Clock& clock);
  ~NetworkF();

  bool Run();

private:
  bool import_packet_file(const std::string& dir, const std::string& name);

  bool import_packets(const std::string& dir, const std::string& mask);

  bool import_bundle_file(const std::string& dir, const std::string& name);

  int import_bundles(const std::string& dir, const std::string& mask);

  bool create_ftn_bundle(const sdk::fido::FidoAddress& route_to, 
                         const std::string& fido_packet_name, std::string& out_bundle_name);

  // ReSharper disable once CppMemberFunctionMayBeConst
  bool create_ftn_packet(const sdk::fido::FidoAddress& dest, const sdk::fido::FidoAddress& route_to,
                         const sdk::net::Packet& wwivnet_packet,
                         std::string& fido_packet_name);

  bool create_ftn_packet_and_bundle(const sdk::fido::FidoAddress& dest,
                                    const sdk::fido::FidoAddress& route_to,
                                    const sdk::net::Packet& p,
                                    std::string& bundlename);

  bool CreateFloFile(const wwiv::sdk::fido::FidoAddress& dest,
                     const std::string& bundlename, const sdk::net::fido_packet_config_t& packet_config);

  bool CreateNetmailAttach(const sdk::fido::FidoAddress& dest,
                           const std::string& bundlename,
                           const sdk::net::fido_packet_config_t& packet_config);

  bool CreateNetmailAttachOrFloFile(const sdk::fido::FidoAddress& dest,
                                    const std::string& bundlename,
                                    const sdk::net::fido_packet_config_t& packet_config);

  bool export_main_type_new_post(std::set<std::string>& bundles, sdk::net::Packet& p);

  bool export_main_type_email_name(std::set<std::string>& bundles, sdk::net::Packet& p);

  sdk::FtnMessageDupe& dupe();

  const NetworkCommandLine& net_cmdline_;
  const sdk::BbsListNet& bbslist_;
  core::Clock& clock_;
  const sdk::net::Network& net_;
  sdk::fido::FidoCallout fido_callout_;
  NetDat netdat_;

  std::unique_ptr<sdk::FtnMessageDupe> dupe_;
  std::vector<int> colors_{7, 11, 14, 5, 31, 2, 12, 9, 6, 3};
};

} // namespace wwiv::net::networkf

#endif
