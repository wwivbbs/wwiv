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
#include "wwivutil/net/req.h"

#include "core/command_line.h"
#include "core/datetime.h"
#include "core/log.h"
#include "core/strings.h"
#include "sdk/bbslist.h"
#include "sdk/networks.h"
#include "sdk/net/packets.h"
#include <iostream>
#include <map>
#include <string>
#include <vector>

using std::cout;
using std::endl;
using std::map;
using std::string;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::strings;

namespace wwiv::wwivutil {

std::string SubReqCommand::GetUsage() const {
  std::ostringstream ss;
  ss << "Usage:   req <A|D> <subtype> <host>" << endl;
  ss << "Example: req A GENCHAT 1" << endl;
  return ss.str();
}

bool SubReqCommand::AddSubCommands() {
  add_argument({"net", "Network number to use (i.e. 0).", "0"});

  return true;
}

int SubReqCommand::Execute() {
  const Networks networks(*config()->config());
  if (!networks.IsInitialized()) {
    LOG(ERROR) << "Unable to load networks.";
    return 1;
  }

  const auto r = this->remaining();
  if (r.size() < 3) {
    cout << GetUsage() << GetHelp();
    return 2;
  }

  const auto net = config()->networks().at(arg("net").as_int());
  const auto packet_filename = create_pend(net.dir, false, 'r');
  const auto add_drop = upcase(r.at(0).front());

  net_header_rec nh = {};
  auto host = to_number<uint16_t>(r.at(2));
  nh.tosys = static_cast<uint16_t>(host);
  nh.touser = 1;
  nh.fromsys = net.sysnum;
  nh.fromuser = 1;
  nh.main_type = add_drop == 'A' ?  main_type_sub_add_req : main_type_sub_drop_req;
  // always use 0 since we use the stype
  nh.minor_type = 0;
  nh.list_len = 0;
  nh.daten = daten_t_now();
  nh.method = 0;
  // This is an alphanumeric sub type.
  const auto& subtype = r.at(1);
  nh.length = subtype.size() + 1;
  auto text = ToStringUpperCase(subtype);
  text.push_back('\0');
  Packet packet(nh, {}, text);
  const auto ok = write_wwivnet_packet(packet_filename, net, packet);
  if (!ok) {
    LOG(ERROR) << "Error writing packet: " << packet_filename;
    return 1;
  }
  LOG(INFO) << "Wrote Packet: " << packet_filename;
  return 0;
}

}
