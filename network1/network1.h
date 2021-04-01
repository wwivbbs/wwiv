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
#ifndef INCLUDED_NET_NETWORK1_H
#define INCLUDED_NET_NETWORK1_H

#include "core/clock.h"
#include "net_core/net_cmdline.h"
#include "net_core/netdat.h"
#include "sdk/net/packets.h"
#include <string>


namespace wwiv::sdk {
class BbsListNet;
}


class Network1 final {
public:
  Network1(const wwiv::net::NetworkCommandLine& cmdline, 
           const wwiv::sdk::BbsListNet& bbslist,
           wwiv::core::Clock& clock);
  ~Network1() = default;

  bool Run();

private:
  bool write_multiple_wwivnet_packets(const net_header_rec& nh, const std::vector<uint16_t>& list,
                                      const std::string& text);
  bool handle_packet(wwiv::sdk::net::Packet& p);
  bool handle_file(const std::string& name);
  const wwiv::net::NetworkCommandLine& net_cmdline_;
  const wwiv::sdk::BbsListNet& bbslist_;
  wwiv::core::Clock& clock_;
  const wwiv::sdk::net::Network& net_;
  wwiv::net::NetDat netdat_;
};

#endif // INCLUDED_NET_NETWORK1_H