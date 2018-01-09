/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2017, WWIV Software Services             */
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

#include <iostream>
#include <map>
#include <string>
#include <vector>
#include "core/command_line.h"
#include "core/log.h"
#include "core/strings.h"
#include "networkb/net_util.h"
#include "networkb/packets.h"
#include "sdk/bbslist.h"
#include "sdk/config.h"
#include "sdk/callout.h"
#include "sdk/config.h"
#include "sdk/datetime.h"
#include "sdk/networks.h"

using std::cout;
using std::endl;
using std::map;
using std::string;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

namespace wwiv {
namespace wwivutil {

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
  Networks networks(*config()->config());
  if (!networks.IsInitialized()) {
    LOG(ERROR) << "Unable to load networks.";
    return 1;
  }

  auto r = this->remaining();
  if (r.size() < 3) {
    cout << GetUsage();
    return 2;
  }

  auto net = config()->networks().at(arg("net").as_int());
  string packet_filename = wwiv::net::create_pend(net.dir, false, 'r');
  uint16_t main_type = main_type_sub_add_req;
  auto add_drop = upcase(r.at(0).front());
  if (add_drop != 'A') {
    main_type = main_type_sub_drop_req;
  }

  net_header_rec nh = {};
  auto host = to_number<uint16_t>(r.at(2));
  nh.tosys = static_cast<uint16_t>(host);
  nh.touser = 1;
  nh.fromsys = net.sysnum;
  nh.fromuser = 1;
  nh.main_type = main_type_sub_add_req;
  // always use 0 since we use the stype
  nh.minor_type = 0;
  nh.list_len = 0;
  nh.daten = daten_t_now();
  nh.method = 0;
  // This is an alphanumeric sub type.
  auto subtype = r.at(1);
  nh.length = subtype.size() + 1;
  string text = subtype;
  StringUpperCase(&text);
  text.push_back('\0');
  wwiv::net::Packet packet(nh, {}, text);
  bool ok = wwiv::net::write_wwivnet_packet(packet_filename, net, packet);
  if (!ok) {
    LOG(ERROR) << "Error writing packet: " << packet_filename;
    return 1;
  }
  LOG(INFO) << "Wrote Packet: " << packet_filename;
  return 0;
}

}
}
